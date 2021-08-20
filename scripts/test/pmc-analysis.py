#!/usr/bin/python3
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

def write_ins_list():
    return [0, set([])]

def write_ins_new_dict():
    return defaultdict(write_ins_list)

def read_ins_list():
    return [[0, set([])], [0, set([])]]

def read_ins_new_dict():
    return defaultdict(read_ins_list)

def access_dict():
    return [[defaultdict(write_ins_new_dict), defaultdict(write_ins_new_dict), defaultdict(write_ins_new_dict)], [defaultdict(read_ins_new_dict), defaultdict(read_ins_new_dict), defaultdict(read_ins_new_dict)]]

def partition(origin_list, n):
    if len(origin_list) % n == 0:
        cnt = len(origin_list) // n
    else:
        cnt = len(origin_list) // n + 1
    for i in range(0, n):
        yield origin_list[i*cnt:(i+1)*cnt]

def counting_frequency(shared_mem_addr_list, process_id, result_path):
    if verbose_mode:
        log_filename = result_path + '/PMC-' + str(process_id) +'.txt'
        raw_pmc_list_file = open(log_filename, 'w')
    access_index_to_byte = {0:1, 1:2, 2:4}
    num_pmc = 0
    finished_mem_addr = 0
    channel_freq = defaultdict(int)
    num_shared_mem_addr = len(shared_mem_addr_list)
    for write_addr in shared_mem_addr_list:
        if write_addr < 0xC0000000:
            finished_mem_addr += 1
            continue
        write_access_dict = mem_dict[write_addr][0]
        num_new_pmc = 0
        for write_length_index in range(0, 3):
            # map the length index to real bytes
            write_byte = access_index_to_byte[write_length_index]
            # if the length is 4 bytes, then the following address would be accessed
            # addr, addr+1, addr+2, addr+3
            write_addr_begin = write_addr
            write_addr_end = write_addr + write_byte - 1
            # Now we know the write address and write length
            write_ins_dict = write_access_dict[write_length_index]
            # enumerate all possible addressed that could overlap with the write access
            for read_addr in range(write_addr_begin - 3, write_addr_end + 1):
                if read_addr not in shared_mem_addr_set:
                    continue
                read_access_dict = mem_dict[read_addr][1]
                for read_length_index in range(0, 3):
                    read_byte = access_index_to_byte[read_length_index]
                    read_addr_begin = read_addr
                    read_addr_end = read_addr + read_byte - 1
                    write_addr_set = set([])
                    read_addr_set = set([])
                    for addr in range(write_addr_begin, write_addr_end + 1):
                        write_addr_set.add(addr)
                    for addr in range(read_addr_begin, read_addr_end + 1):
                        read_addr_set.add(addr)
                    # check if two memory access could overlap
                    if len(write_addr_set & read_addr_set) == 0:
                        continue
                    read_ins_dict = read_access_dict[read_length_index]
                    addr_begin = min(list(write_addr_set & read_addr_set))
                    addr_end = max(list(write_addr_set & read_addr_set))
                    read_begin_offset = addr_begin - read_addr_begin
                    read_end_offset = addr_end - read_addr_begin + 1
                    write_begin_offset = addr_begin - write_addr_begin
                    write_end_offset = addr_end - write_addr_begin + 1
                    for write_ins in write_ins_dict:
                        for read_ins in read_ins_dict:
                            write_value_dict = write_ins_dict[write_ins]
                            read_value_dict = read_ins_dict[read_ins]
                            for write_value in write_value_dict:
                                write_bytes_value = struct.pack('<I', write_value)
                                write_actual_value = write_bytes_value[write_begin_offset: write_end_offset]
                                for read_value in read_value_dict:
                                    read_bytes_value = struct.pack('<I', read_value)
                                    read_actual_value = read_bytes_value[read_begin_offset:read_end_offset]
                                    if write_actual_value != read_actual_value:
                                        write_candidate_list = write_value_dict[write_value]
                                        read_candidate_list = read_value_dict[read_value]
                                        if read_candidate_list[0][0] != 0:
                                            freq = write_candidate_list[0] * read_candidate_list[0][0]
                                            if freq != 0:
                                                if write_value == 0:
                                                    channel = tuple([write_ins, write_addr, write_byte, read_ins, read_addr, read_byte, 0, 0])
                                                else:
                                                    # last filed represents the write value is not zero
                                                    channel = tuple([write_ins, write_addr, write_byte, read_ins, read_addr, read_byte, 0, 1])
                                                if channel not in channel_freq:
                                                    channel_freq[channel] = 0
                                                channel_freq[channel] += freq
                                                if verbose_mode:
                                                    if write_value == 0:
                                                        print(hex(write_ins), hex(write_addr), write_byte, hex(read_ins), hex(read_addr), read_byte, 0, 0)
                                                    else:
                                                        print(hex(write_ins), hex(write_addr), write_byte, hex(read_ins), hex(read_addr), read_byte, 0, 1)
                                                num_new_pmc += 1
                                                num_pmc += 1
                                        if read_candidate_list[1][0] != 0:
                                            freq = write_candidate_list[0] * read_candidate_list[1][0]
                                            if freq != 0:
                                                if write_value == 0:
                                                    channel = tuple([write_ins, write_addr, write_byte, read_ins, read_addr, read_byte, 1, 0])
                                                else:
                                                    # last filed represents the write value is not zero
                                                    channel = tuple([write_ins, write_addr, write_byte, read_ins, read_addr, read_byte, 1, 1])
                                                if channel not in channel_freq:
                                                    channel_freq[channel] = 0
                                                channel_freq[channel] += freq
                                                if verbose_mode:
                                                    if write_value == 0:
                                                        print(hex(write_ins), hex(write_addr), write_byte, hex(read_ins), hex(read_addr), read_byte, 1, 0)
                                                    else:
                                                        print(hex(write_ins), hex(write_addr), write_byte, hex(read_ins), hex(read_addr), read_byte, 1, 1)
                                                num_new_pmc += 1
                                                num_pmc += 1
        finished_mem_addr += 1
        print("Process %d analyzed %f (%d/%d) addresses, added %d new PMCs, %d in total" % (process_id,  finished_mem_addr/num_shared_mem_addr, finished_mem_addr, num_shared_mem_addr, num_new_pmc, num_pmc))
    return channel_freq


def pmc_analysis(output, testing_mode):
    time_now = datetime.now()
    timestamp = time_now.strftime("%Y-%m-%d-%H-%M-%S")
    output_dir = output + '/PMC-' + timestamp + '/'
    if not os.path.isdir(output_dir):
        os.makedirs(output_dir)
    time_start = time.time()
    shared_mem_addr_list = list(mem_dict.keys())
    random.shuffle(shared_mem_addr_list)
    if testing_mode:
        shared_mem_addr_list = shared_mem_addr_list[0:50000]
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
    channel_freq = each_freq_dict_result[0].get()
    for index in range(1, num_process):
        local_channel_freq = each_freq_dict_result[index].get()
        local_keys = set(local_channel_freq.keys())
        global_keys = set(channel_freq.keys())
        assert(global_keys.isdisjoint(local_keys) == True)
        for channel in list(local_keys):
            channel_freq[channel] = local_channel_freq[channel]
    del each_freq_dict_result
    # sort all communications based on their frequency
    sorted_channel = sorted(channel_freq.items(), key = lambda x: x[1])

    print("Saving uncommon PMC channels")
    output_file = open(output_dir + '/uncommon-channel.txt', 'w')
    reg_channel_freq = defaultdict(int)
    for item in sorted_channel:
        channel = list(item[0])
        write_ins = channel[0]
        write_addr = channel[1]
        write_byte = channel[2]
        read_ins = channel[3]
        read_addr = channel[4]
        read_byte = channel[5]
        channel_key = tuple([write_ins, write_addr, write_byte, read_ins, read_addr, read_byte])
        if channel_key not in reg_channel_freq:
            reg_channel_freq[channel_key] = 0
        reg_channel_freq[channel_key] += item[1]
    sorted_reg_channel = sorted(reg_channel_freq.items(), key = lambda x: x[1])
    for item in sorted_reg_channel:
        channel = list(item[0])
        write_ins = channel[0]
        write_addr = channel[1]
        write_byte = channel[2]
        read_ins = channel[3]
        read_addr = channel[4]
        read_byte = channel[5]
        print(hex(write_ins), hex(write_addr), write_byte, hex(read_ins), hex(read_addr), read_byte, item[1], file=output_file)

    print("Saving uncommon PMC unaligned-channels")
    output_file = open(output_dir + '/uncommon-unaligned-channel.txt', 'w')
    for item in sorted_reg_channel:
        channel = list(item[0])
        write_addr = channel[1]
        write_byte = channel[2]
        read_addr = channel[4]
        read_byte = channel[5]
        if write_addr == read_addr and write_byte == read_byte:
            continue
        write_ins = channel[0]
        read_ins = channel[3]
        print(hex(write_ins), hex(write_addr), write_byte, hex(read_ins), hex(read_addr), read_byte, item[1], file=output_file)

    print("Saving uncommon double-read channels")
    output_file = open(output_dir + '/uncommon-double-read-channel.txt', 'w')
    double_channel_freq = defaultdict(int)
    for item in sorted_channel:
        channel = list(item[0])
        double_read = channel[6]
        if double_read == 0:
            continue
        write_addr = channel[1]
        write_byte = channel[2]
        read_addr = channel[4]
        read_byte = channel[5]
        write_ins = channel[0]
        read_ins = channel[3]
        double_channel_key = tuple([write_ins, write_addr, write_byte, read_ins, read_addr, read_byte, 1])
        if double_channel_key not in double_channel_freq:
            double_channel_freq[double_channel_key] = 0
        double_channel_freq[double_channel_key] += item[1]
    sorted_double_channel = sorted(double_channel_freq.items(), key = lambda x: x[1])
    for item in sorted_double_channel:
        channel = list(item[0])
        write_ins = channel[0]
        write_addr = channel[1]
        write_byte = channel[2]
        read_ins = channel[3]
        read_addr = channel[4]
        read_byte = channel[5]
        double_read = channel[6]
        print(hex(write_ins), hex(write_addr), write_byte, hex(read_ins), hex(read_addr), read_byte, double_read, item[1], file=output_file)

    print("Saving uncommon object null channels")
    output_file = open(output_dir + '/uncommon-object-null-channel.txt', 'w')
    obj_null_channel_freq = defaultdict(int)
    for item in sorted_channel:
        channel = list(item[0])
        write_value_non_zero = channel[7]
        if write_value_non_zero == 1:
            continue
        write_addr = channel[1]
        write_byte = channel[2]
        read_addr = channel[4]
        read_byte = channel[5]
        write_ins = channel[0]
        read_ins = channel[3]
        obj_null_channel_key = tuple([write_ins, write_addr, write_byte, read_ins, read_addr, read_byte, write_value_non_zero])
        if obj_null_channel_key not in obj_null_channel_freq:
            obj_null_channel_freq[obj_null_channel_key] = 0
        obj_null_channel_freq[obj_null_channel_key] += item[1]
    sorted_obj_null_channel = sorted(obj_null_channel_freq.items(), key = lambda x: x[1])
    for item in sorted_obj_null_channel:
        channel = list(item[0])
        write_ins = channel[0]
        write_addr = channel[1]
        write_byte = channel[2]
        read_ins = channel[3]
        read_addr = channel[4]
        read_byte = channel[5]
        print(hex(write_ins), hex(write_addr), write_byte, hex(read_ins), hex(read_addr), read_byte, item[1], file=output_file)

    print("Saving uncommon ins-pairs")
    output_file = open(output_dir + '/uncommon-ins-pair.txt', 'w')
    ins_pair_freq = defaultdict(int)
    for item in sorted_channel:
        channel = list(item[0])
        write_ins = channel[0]
        read_ins = channel[3]
        ins_pair = tuple([write_ins, read_ins])
        if ins_pair not in ins_pair_freq:
            ins_pair_freq[ins_pair] = 0
        ins_pair_freq[ins_pair] += item[1]
    sorted_ins_pair = sorted(ins_pair_freq.items(), key = lambda x: x[1])
    for item in sorted_ins_pair:
        ins_pair_key = list(item[0])
        write_ins = ins_pair_key[0]
        read_ins = ins_pair_key[1]
        print(hex(write_ins), hex(read_ins), item[1], file=output_file)

    print("Saving uncommon ins")
    output_file = open(output_dir + '/uncommon-ins.txt', 'w')
    ins_freq = defaultdict(int)
    for item in sorted_channel:
        channel = list(item[0])
        write_ins = channel[0]
        read_ins = channel[3]
        write_ins_key = tuple([write_ins, 0])
        read_ins_key = tuple([read_ins, 1])
        if write_ins_key not in ins_freq:
            ins_freq[write_ins_key] = 0
        ins_freq[write_ins_key] += item[1]
        if read_ins_key not in ins_freq:
            ins_freq[read_ins_key] = 0
        ins_freq[read_ins_key] += item[1]
    sorted_ins = sorted(ins_freq.items(), key = lambda x: x[1])
    for item in sorted_ins:
        ins_key = list(item[0])
        ins = ins_key[0]
        ins_type = ins_key[1]
        print(hex(ins), ins_type, item[1], file=output_file)


    print("Saving uncommon mem area")
    output_file = open(output_dir + '/uncommon-mem-addr.txt', 'w')
    mem_area_freq = defaultdict(int)
    for item in sorted_channel:
        channel = list(item[0])
        write_ins = channel[0]
        write_addr = channel[1]
        write_byte = channel[2]
        read_ins = channel[3]
        read_addr = channel[4]
        read_byte = channel[5]
        mem_area_key = tuple([write_addr, read_addr, write_byte, read_byte])
        if mem_area_key not in mem_area_freq:
            mem_area_freq[mem_area_key] = 0
        mem_area_freq[mem_area_key] += item[1]
    sorted_mem_area = sorted(mem_area_freq.items(), key = lambda x: x[1])
    for item in sorted_mem_area:
        mem_area = list(item[0])
        write_addr = mem_area[0]
        read_addr = mem_area[1]
        write_byte = mem_area[2]
        read_byte = mem_area[3]
        print(hex(write_addr), hex(read_addr), write_byte, read_byte, item[1], file=output_file)

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
