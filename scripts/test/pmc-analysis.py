#!/usr/bin/python3
"""
Processor Memory Communication (PMC) Analysis Tool

This script analyzes memory access patterns to identify potential memory channels (PMCs). 
It processes memory access data to detect different types of PMCs, such as unaligned accesses, 
double reads, and null object accesses. The analysis is parallelized to efficiently handle 
large memory trace data.

input:
- mem-dict-<timestamp>: pickle file that contains memory access data with read/write info.
  The dictionary maps memory addresses to access data, and contains instruction pointers, 
  memory values, and access types.

output:
- uncommon-channel.txt: 
    all potential memory communications (PMCs) sorted by frequency, 
    representing shared memory access patterns where one thread writes to a location and 
    another reads from it. 
    Format:
    "<writer_instruction_addr> <writer_memory_addr> <write_byte_length> <reader_instruction_addr> 
    <reader_memory_addr> <read_byte_length> <frequency>"

- uncommon-unaligned-channel.txt: 
    PMCs where memory access ranges differ between reader 
    and writer (different start addresses or lengths), which can cause bugs when readers 
    fetch partially updated data. 
    Format:
    "<writer_instruction_addr> <writer_memory_addr> <write_byte_length> <reader_instruction_addr> 
    <reader_memory_addr> <read_byte_length> <frequency>"

- uncommon-double-read-channel.txt: 
    PMCs involving double-fetch patterns where the same 
    memory location is read twice in sequence, which can lead to time-of-check-to-time-of-use 
    vulnerabilities. 
    Format:
    "<writer_instruction_addr> <writer_memory_addr> <write_byte_length> <reader_instruction_addr> 
    <reader_memory_addr> <read_byte_length> <double_read_flag> <frequency>"

- uncommon-object-null-channel.txt: 
    PMCs where a writer zeros out a shared object (writing value 0)
    that a reader later accesses, often leading to null pointer dereference bugs. 
    Format:
    "<writer_instruction_addr> <writer_memory_addr> <write_byte_length> <reader_instruction_addr> 
    <reader_memory_addr> <read_byte_length> <frequency>"

- uncommon-ins-pair.txt: write-read instruction pairs involved in PMCs, highlighting 
    specific code patterns that may trigger concurrency bugs. 
    Format:
    "<writer_instruction_addr> <reader_instruction_addr> <frequency>"

- uncommon-ins.txt: individual instructions (both reads and writes) participating in PMCs, 
    sorted by frequency to identify potentially vulnerable code. 
    Format:
    "<instruction_addr> <instruction_type{0=write,1=read}> <frequency>"

- uncommon-mem-addr.txt: memory regions where inter-thread communication occurs, 
    helping identify shared kernel objects prone to race conditions. Format:
    "<writer_memory_addr> <reader_memory_addr> <write_byte_length> <read_byte_length> <frequency>"

each output file follows a specific format documenting the memory communications detected

usage:
  python pmc-analysis.py [memory_dict_file] [output_path]
"""

import sys
import random
import os
from collections import defaultdict
from multiprocessing import Process, Manager, Pool
import multiprocessing
import pickle
import time
from datetime import datetime
import struct

class Channel:
    def __init__(self, write_ins, write_addr, write_byte, read_ins, read_addr, read_byte, double_read=0, write_value_non_zero=0):
        self.write_ins = write_ins
        self.write_addr = write_addr
        self.write_byte = write_byte
        self.read_ins = read_ins
        self.read_addr = read_addr
        self.read_byte = read_byte
        self.double_read = double_read
        self.write_value_non_zero = write_value_non_zero
        
    def __hash__(self):
        return hash((self.write_ins, self.write_addr, self.write_byte, 
                    self.read_ins, self.read_addr, self.read_byte,
                    self.double_read, self.write_value_non_zero))
    
    def __eq__(self, other):
        if not isinstance(other, Channel):
            return False
        return (self.write_ins, self.write_addr, self.write_byte, 
                self.read_ins, self.read_addr, self.read_byte,
                self.double_read, self.write_value_non_zero) == \
               (other.write_ins, other.write_addr, other.write_byte, 
                other.read_ins, other.read_addr, other.read_byte,
                other.double_read, other.write_value_non_zero)
                
    def get_channel_redux_key(self):
        #reduced tuple, analysis often disregards double_read and write_value_non_zero
        return (self.write_ins, self.write_addr, self.write_byte, 
                self.read_ins, self.read_addr, self.read_byte)

def create_channel_from_redux_key(channel_redux_key):
    if len(channel_redux_key) >= 6:
        write_ins, write_addr, write_byte, read_ins, read_addr, read_byte = channel_redux_key[:6]
        double_read = channel_redux_key[6] if len(channel_redux_key) > 6 else 0
        write_value_non_zero = channel_redux_key[7] if len(channel_redux_key) > 7 else 0
        return Channel(write_ins, write_addr, write_byte, read_ins, read_addr, read_byte, double_read, write_value_non_zero)
    else:
        raise ValueError("channel_redux_key tuple must have at least 6 elements")

############################
# PARALLELIZED ANALYSIS    #
############################

def write_ins_list():
    return [0, set([])]

def write_ins_new_dict():
    return defaultdict(write_ins_list)

def read_ins_list():
    return [[0, set([])], [0, set([])]]

def read_ins_new_dict():
    return defaultdict(read_ins_list)

def access_dict():
    #create a nested structure of defaultdicts
    #first level:  read/write distinction [0=write, 1=read]
    #second level: byte length buckets [0,1,2]
    #third level:  appropriate dict type
    result = [
        [defaultdict(write_ins_new_dict) for _ in range(3)],  # write access
        [defaultdict(read_ins_new_dict) for _ in range(3)]    # read access
    ]
    return result

def partition(origin_list, n):
    if len(origin_list) % n == 0:
        cnt = len(origin_list) // n
    else:
        cnt = len(origin_list) // n + 1
    for i in range(0, n):
        yield origin_list[i*cnt:(i+1)*cnt]

def initialize_counting(process_id, result_path):
    if verbose_mode:
        log_filename = result_path + '/PMC-' + str(process_id) +'.txt'
        raw_pmc_list_file = open(log_filename, 'w')
    access_index_to_byte = {0:1, 1:2, 2:4}
    num_pmc = 0
    finished_mem_addr = 0
    channel_freq = defaultdict(int)
    return access_index_to_byte, num_pmc, finished_mem_addr, channel_freq

def get_memory_access_sets(addr_begin, addr_end, read_addr_begin, read_addr_end):
    write_addr_set = set([])
    read_addr_set = set([])
    for addr in range(addr_begin, addr_end + 1):
        write_addr_set.add(addr)
    for addr in range(read_addr_begin, read_addr_end + 1):
        read_addr_set.add(addr)
    return write_addr_set, read_addr_set

def calculate_address_overlap(write_addr_begin, write_addr_end, read_addr_begin, read_addr_end):
    write_addr_set, read_addr_set = get_memory_access_sets(write_addr_begin, write_addr_end, read_addr_begin, read_addr_end)
    if len(write_addr_set & read_addr_set) == 0:
        return None
    addr_begin              = min(list(write_addr_set & read_addr_set))
    addr_end                = max(list(write_addr_set & read_addr_set))
    read_begin_offset       = addr_begin - read_addr_begin
    read_end_offset         = addr_end   - read_addr_begin  + 1
    write_begin_offset      = addr_begin - write_addr_begin
    write_end_offset        = addr_end   - write_addr_begin + 1
    return read_begin_offset, read_end_offset, write_begin_offset, write_end_offset

def process_read_candidate(candidate_index, write_value, write_candidate_list, read_candidate_list, write_ins, write_addr, write_byte, read_ins, read_addr, read_byte, channel_freq):
    if read_candidate_list[candidate_index][0] == 0:
        return 0
    freq = write_candidate_list[0] * read_candidate_list[candidate_index][0]
    if freq == 0:
        return 0
    write_value_flag = 1 if write_value != 0 else 0
    channel = Channel(write_ins, write_addr, write_byte, read_ins, read_addr, read_byte, candidate_index, write_value_flag)
    if channel not in channel_freq:
        channel_freq[channel] = 0
    channel_freq[channel] += freq
    if verbose_mode:
        print(hex(channel.write_ins), hex(channel.write_addr), channel.write_byte, hex(channel.read_ins), hex(channel.read_addr), channel.read_byte, channel.double_read, channel.write_value_non_zero)
    return 1

def process_value_pair(write_value, read_value, write_bytes_value, read_bytes_value, read_begin_offset, read_end_offset, write_begin_offset, write_end_offset, write_ins, write_addr, write_byte, read_ins, read_addr, read_byte, write_value_dict, read_value_dict, channel_freq):
    write_actual_value = write_bytes_value[write_begin_offset: write_end_offset]
    read_actual_value = read_bytes_value[read_begin_offset:read_end_offset]
    if write_actual_value != read_actual_value:
        write_candidate_list = write_value_dict[write_value]
        read_candidate_list = read_value_dict[read_value]
        num_new_pmc = 0
        
        num_new_pmc += process_read_candidate(0, write_value, write_candidate_list, read_candidate_list, 
                                       write_ins, write_addr, write_byte, read_ins, read_addr, read_byte, channel_freq)
        
        num_new_pmc += process_read_candidate(1, write_value, write_candidate_list, read_candidate_list, 
                                       write_ins, write_addr, write_byte, read_ins, read_addr, read_byte, channel_freq)
        
        return num_new_pmc
    return 0

def process_overlapping_addresses(write_addr, read_addr, write_byte, read_byte, write_ins_dict, read_ins_dict, channel_freq):
    write_addr_begin = write_addr
    write_addr_end = write_addr + write_byte - 1
    read_addr_begin = read_addr
    read_addr_end = read_addr + read_byte - 1
    
    overlap_result = calculate_address_overlap(write_addr_begin, write_addr_end, read_addr_begin, read_addr_end)
    if not overlap_result:
        return 0
        
    read_begin_offset, read_end_offset, write_begin_offset, write_end_offset = overlap_result
    num_new_pmc = 0
    
    for write_ins in write_ins_dict:
        for read_ins in read_ins_dict:
            write_value_dict = write_ins_dict[write_ins]
            read_value_dict = read_ins_dict[read_ins]
            for write_value in write_value_dict:
                write_bytes_value = struct.pack('<I', write_value)
                for read_value in read_value_dict:
                    read_bytes_value = struct.pack('<I', read_value)
                    pmc_count = process_value_pair(write_value, read_value, write_bytes_value, read_bytes_value, 
                                           read_begin_offset, read_end_offset, write_begin_offset, write_end_offset, 
                                           write_ins, write_addr, write_byte, read_ins, read_addr, read_byte, 
                                           write_value_dict, read_value_dict, channel_freq)
                    num_new_pmc += pmc_count
    
    return num_new_pmc

def counting_frequency(shared_mem_addr_list, process_id, result_path):
    access_index_to_byte, num_pmc, finished_mem_addr, channel_freq = initialize_counting(process_id, result_path)
    num_shared_mem_addr = len(shared_mem_addr_list)
    for write_addr in shared_mem_addr_list:
        if write_addr < 0xC0000000:
            finished_mem_addr += 1
            continue
        write_access_dict = mem_dict[write_addr][0]
        num_new_pmc = 0
        for write_length_index in range(0, 3):
            write_byte = access_index_to_byte[write_length_index]
            write_addr_begin = write_addr
            write_addr_end = write_addr + write_byte - 1
            write_ins_dict = write_access_dict[write_length_index]
            for read_addr in range(write_addr_begin - 3, write_addr_end + 1):
                if read_addr not in shared_mem_addr_set:
                    continue
                read_access_dict = mem_dict[read_addr][1]
                for read_length_index in range(0, 3):
                    read_byte = access_index_to_byte[read_length_index]
                    read_ins_dict = read_access_dict[read_length_index]
                    current_pmc = process_overlapping_addresses(write_addr, read_addr, write_byte, read_byte, 
                                                       write_ins_dict, read_ins_dict, channel_freq)
                    num_new_pmc += current_pmc
                    num_pmc += current_pmc
        finished_mem_addr += 1
        print("Process %d analyzed %f (%d/%d) addresses, added %d new PMCs, %d in total" % 
                (process_id, finished_mem_addr/num_shared_mem_addr, finished_mem_addr, 
                 num_shared_mem_addr, num_new_pmc, num_pmc))
    return channel_freq

#####################################
# PREPARE AND PARALLELIZE ANALYSIS  #
#####################################

def setup_output_directory(output_path):
    time_now = datetime.now()
    timestamp = time_now.strftime("%Y-%m-%d-%H-%M-%S")
    output_dir = output_path + '/PMC-' + timestamp + '/'
    if not os.path.isdir(output_dir):
        os.makedirs(output_dir)
    return output_dir

def prepare_memory_addresses(testing_mode):
    shared_mem_addr_list = list(mem_dict.keys())
    random.shuffle(shared_mem_addr_list)
    if testing_mode:
        shared_mem_addr_list = shared_mem_addr_list[0:50000]
    return shared_mem_addr_list

def run_parallel_analysis(shared_mem_addr_list, output_dir):
    # divide the task into even pieces
    num_shared_mem_addr = len(shared_mem_addr_list)
    num_process = multiprocessing.cpu_count()
    process_pool = Pool(num_process)
    task_list = partition(shared_mem_addr_list, num_process)
    task_list = list(task_list)
    partition_check = 0
    for each_task in task_list:
        partition_check += len(each_task)
    assert(partition_check == num_shared_mem_addr)
    assert(len(task_list) == num_process)
    # launch several processes running the task in paralle
    each_freq_dict_result = []
    raw_multiprocess_output_dir = output_dir + '/raw/'
    if verbose_mode and not os.path.isdir(raw_multiprocess_output_dir):
        os.makedirs(raw_multiprocess_output_dir)
    for index in range(0, num_process):
        ret = process_pool.apply_async(counting_frequency, args=(task_list[index], index, raw_multiprocess_output_dir))
        each_freq_dict_result.append(ret)
        
    process_pool.close()
    process_pool.join()
    return each_freq_dict_result

def combine_results(each_freq_dict_result):
    channel_freq = each_freq_dict_result[0].get()
    for index in range(1, len(each_freq_dict_result)):
        local_channel_freq = each_freq_dict_result[index].get()
        local_keys = set(local_channel_freq.keys())
        global_keys = set(channel_freq.keys())
        assert(global_keys.isdisjoint(local_keys) == True)
        for channel in list(local_keys):
            channel_freq[channel] = local_channel_freq[channel]
    return channel_freq

#####################################
# SAVE ANALYSIS RESULTS             #
#####################################

def save_uncommon_pmc_channels(output_dir, sorted_channel):
    print("Saving uncommon PMC channels")
    output_file = open(output_dir + '/uncommon-channel.txt', 'w')
    reg_channel_freq = defaultdict(int)
    
    for channel, freq in sorted_channel:
        channel_redux_key = channel.get_channel_redux_key()
        reg_channel_freq[channel_redux_key] += freq
        
    sorted_reg_channel = sorted(reg_channel_freq.items(), key=lambda x: x[1])
    
    for channel_redux_key, freq in sorted_reg_channel:
        channel = create_channel_from_redux_key(channel_redux_key)
        print(hex(channel.write_ins), hex(channel.write_addr), channel.write_byte, 
              hex(channel.read_ins), hex(channel.read_addr), channel.read_byte, 
              freq, file=output_file)
              
    return sorted_reg_channel

def save_uncommon_unaligned_channels(output_dir, sorted_reg_channel):
    print("Saving uncommon PMC unaligned-channels")
    output_file = open(output_dir + '/uncommon-unaligned-channel.txt', 'w')
    
    for channel_redux_key, freq in sorted_reg_channel:
        channel = create_channel_from_redux_key(channel_redux_key)
        if channel.write_addr == channel.read_addr and channel.write_byte == channel.read_byte:
            continue
            
        print(hex(channel.write_ins), hex(channel.write_addr), channel.write_byte, 
              hex(channel.read_ins), hex(channel.read_addr), channel.read_byte, 
              freq, file=output_file)

def save_uncommon_double_read_channels(output_dir, sorted_channel):
    print("Saving uncommon double-read channels")
    output_file = open(output_dir + '/uncommon-double-read-channel.txt', 'w')
    double_channel_freq = defaultdict(int)
    
    for channel, freq in sorted_channel:
        if channel.double_read == 0:
            continue
        
        # create a key that only considers the 6 core attributes plus double_read
        # this ignores write_value_non_zero for grouping purposes
        key_tuple = (channel.write_ins, channel.write_addr, channel.write_byte,
                     channel.read_ins, channel.read_addr, channel.read_byte,
                     channel.double_read)
        double_channel_freq[key_tuple] += freq
        
    sorted_double_channel = sorted(double_channel_freq.items(), key=lambda x: x[1])
    
    for key_tuple, freq in sorted_double_channel:
        write_ins, write_addr, write_byte, read_ins, read_addr, read_byte, double_read = key_tuple
        print(hex(write_ins), hex(write_addr), write_byte, 
              hex(read_ins), hex(read_addr), read_byte, 
              double_read, freq, file=output_file)

def save_uncommon_object_null_channels(output_dir, sorted_channel):
    print("Saving uncommon object null channels")
    output_file = open(output_dir + '/uncommon-object-null-channel.txt', 'w')
    obj_null_channel_freq = defaultdict(int)
    
    for channel, freq in sorted_channel:
        if channel.write_value_non_zero == 1:
            continue
            
        obj_null_channel_freq[channel] += freq
        
    sorted_obj_null_channel = sorted(obj_null_channel_freq.items(), key=lambda x: x[1])
    
    for channel, freq in sorted_obj_null_channel:
        print(hex(channel.write_ins), hex(channel.write_addr), channel.write_byte, 
              hex(channel.read_ins), hex(channel.read_addr), channel.read_byte, 
              freq, file=output_file)

def save_uncommon_ins_pairs(output_dir, sorted_channel):
    print("Saving uncommon ins-pairs")
    output_file = open(output_dir + '/uncommon-ins-pair.txt', 'w')
    ins_pair_freq = defaultdict(int)
    
    for channel, freq in sorted_channel:
        ins_pair = (channel.write_ins, channel.read_ins)
        ins_pair_freq[ins_pair] += freq
        
    sorted_ins_pair = sorted(ins_pair_freq.items(), key=lambda x: x[1])
    
    for (write_ins, read_ins), freq in sorted_ins_pair:
        print(hex(write_ins), hex(read_ins), freq, file=output_file)

def save_uncommon_ins(output_dir, sorted_channel):
    print("Saving uncommon ins")
    output_file = open(output_dir + '/uncommon-ins.txt', 'w')
    ins_freq = defaultdict(int)
    
    for channel, freq in sorted_channel:
        write_ins_key = (channel.write_ins, 0)
        read_ins_key = (channel.read_ins, 1)
        ins_freq[write_ins_key] += freq
        ins_freq[read_ins_key] += freq
        
    sorted_ins = sorted(ins_freq.items(), key=lambda x: x[1])
    
    for (ins, ins_type), freq in sorted_ins:
        print(hex(ins), ins_type, freq, file=output_file)

def save_uncommon_mem_area(output_dir, sorted_channel):
    print("Saving uncommon mem area")
    output_file = open(output_dir + '/uncommon-mem-addr.txt', 'w')
    mem_area_freq = defaultdict(int)
    
    for channel, freq in sorted_channel:
        mem_area_key = (channel.write_addr, channel.read_addr, channel.write_byte, channel.read_byte)
        mem_area_freq[mem_area_key] += freq
        
    sorted_mem_area = sorted(mem_area_freq.items(), key=lambda x: x[1])
    
    for (write_addr, read_addr, write_byte, read_byte), freq in sorted_mem_area:
        print(hex(write_addr), hex(read_addr), write_byte, read_byte, freq, file=output_file)
#####################################
# MAIN                              #
#####################################

def pmc_analysis(output, testing_mode):
    output_dir = setup_output_directory(output)
    time_start = time.time()
    
    shared_mem_addr_list = prepare_memory_addresses(testing_mode)
    each_freq_dict_result = run_parallel_analysis(shared_mem_addr_list, output_dir)
    channel_freq = combine_results(each_freq_dict_result)
    del each_freq_dict_result
    # sort all communications based on their frequency
    sorted_channel = sorted(channel_freq.items(), key = lambda x: x[1])

    sorted_reg_channel = save_uncommon_pmc_channels(output_dir, sorted_channel)
    save_uncommon_unaligned_channels(  output_dir, sorted_reg_channel)
    save_uncommon_double_read_channels(output_dir, sorted_channel)
    save_uncommon_object_null_channels(output_dir, sorted_channel)
    save_uncommon_ins_pairs(           output_dir, sorted_channel)
    save_uncommon_ins(                 output_dir, sorted_channel)
    save_uncommon_mem_area(            output_dir, sorted_channel)

    time_end = time.time()
    print('time cost',time_end-time_start, 's')
    print("finish sorting")

    return



'''
# argument 1 memory dictionary file
# argument 2 path where to store the result
'''
verbose_mode = False 
mem_dict_file = open(sys.argv[1], 'rb')
print("Loading python dictionary into memory")
mem_dict = pickle.load(mem_dict_file)
print("Memory dictionary is loaded")
shared_mem_addr_set = set(mem_dict.keys())
testing_mode = False
pmc_analysis(sys.argv[2], testing_mode)
