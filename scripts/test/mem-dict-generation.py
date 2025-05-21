#!/usr/bin/python3
"""
Memory Dictionary Generation Tool

This script processes shared memory access trace files to build a dictionary of 
memory operations. It reads access trace files (write.txt and read.txt) from multiple 
test seeds, tracks memory operations, and identifies double read patterns. 
The created pickle memory dictionary is the input of the PMC analysis script.

input:
- write.txt: records memory write operations found by sequential-shared-analysis.py. 
  Format:
    "<instruction_ptr> <mem_address> <value> <length> <type: 0=write>"

- read.txt:  records memory read operations with double read flags. 
  Format:
    "<instruction_ptr> <mem_address> <value> <length> <type: 1=read> <double_read: bool{0,1}>"

output:
- mem-dict-<timestamp>: pickle file memory dictionary formatted as:
    "mem_dict[addr][access_type][byte_length][instruction_ptr][value] -> [frequency, seed_set]"
    where:
    - addr: memory address accessed (only kernel addresses >= 0xC0000000)
    - access_type: 0 for write, 1 for read
    - byte_length: 0 for 8-bit, 1 for 16-bit, 2 for 32-bit access
    - instruction_ptr: instruction pointer that performed the memory operation
    - value: value read from or written to memory
    - stored data includes operation [frequency] and a [seed_set]
    
    for reads, double reads are also added:
    "mem_dict[addr][1][byte_length][instruction_ptr][value][double_read] -> [frequency, seed_set]"
    where double_read: 0 for normal reads, 1 for double reads

usage:
  python mem-dict-generation.py [data_path] [seed_start] [seed_end] [existing_mem_dict_file (optional)]
"""

import sys
import os
import multiprocessing
from collections import defaultdict
import random
import json
import pickle
import resource
from datetime import datetime

ACCESS_TYPE_WRITE   = 0
ACCESS_TYPE_READ    = 1

KERNEL_ADDR_MIN     = 0xC0000000

BIT_LENGTH_8        = 8
BIT_LENGTH_16       = 16
BIT_LENGTH_32       = 32

BYTE_INDEX_8BIT     = 0
BYTE_INDEX_16BIT    = 1
BYTE_INDEX_32BIT    = 2

def getFileName(path, prefix, postfix):
    target_list     = []

    f_list = os.listdir(path)
    for f in f_list:
        if os.path.splitext(f)[0].startswith(prefix) and f.endswith(postfix):
            target_list.append(f)
    # assert there is only one match in the folder
    assert(len(target_list) == 1)

    return target_list[0]

# mem_dict[addr][0] is for write
# mem_dict[addr][0][0] write acess for 1 bytes
# mem_dict[addr][0][1] 2bytes write access
# mem_dict[addr][0][2] 4bytes write access
def write_ins_list():
    return [0, set([])]

def write_ins_new_dict():
    return defaultdict(write_ins_list)

def read_ins_list():
    return [[0, set([])], [0, set([])]]

def read_ins_new_dict():
    return defaultdict(read_ins_list)

def access_dict():
    return [[defaultdict(write_ins_new_dict), defaultdict(write_ins_new_dict), defaultdict(write_ins_new_dict)], 
            [defaultdict(read_ins_new_dict), defaultdict(read_ins_new_dict), defaultdict(read_ins_new_dict)]]

def process_memory_access(contents, mem_dict, seed, set_limit, access_type):
    ins     = int(contents[0], 16)
    addr    = int(contents[1], 16)
    if addr < KERNEL_ADDR_MIN:
        return
    value   = int(contents[2], 16)
    bits    = int(contents[3]    )
    
    if bits   == BIT_LENGTH_8:
        byte_index = BYTE_INDEX_8BIT
    elif bits == BIT_LENGTH_16:
        byte_index = BYTE_INDEX_16BIT
    elif bits == BIT_LENGTH_32:
        byte_index = BYTE_INDEX_32BIT
    else:
        return  #invalid bit length
    
    if access_type == ACCESS_TYPE_READ:
        double_read = int(contents[5])
        mem_dict[addr][access_type][byte_index][ins][value][double_read][0] += 1

        if len(mem_dict[addr][access_type][byte_index][ins][value][double_read][1]) < set_limit:
            mem_dict[addr][access_type][byte_index][ins][value][double_read][1].add(int(seed))

    else: #access_type == ACCESS_TYPE_WRITE
        mem_dict[addr][access_type][byte_index][ins][value][0] += 1

        if len(mem_dict[addr][access_type][byte_index][ins][value][1]) < set_limit:
            mem_dict[addr][access_type][byte_index][ins][value][1].add(int(seed))

def process_write_access(contents, mem_dict, seed, set_limit):
    process_memory_access(contents, mem_dict, seed, set_limit, ACCESS_TYPE_WRITE)

def process_read_access(contents, mem_dict, seed, set_limit):
    process_memory_access(contents, mem_dict, seed, set_limit, ACCESS_TYPE_READ)

def mem_dict_generation(path, seeds, old_mem_dict=None):
    set_limit = 10
    if old_mem_dict:
        mem_dict = old_mem_dict
    else:
        mem_dict = defaultdict(access_dict)
    seed_list = os.scandir(path)
    test_list = []
    for seed in seed_list:
        if seed.is_dir() is not True:
            continue
        test_list.append(seed.name)
    random.shuffle(test_list)   
    finished = 0
    for seed_dir in test_list:
        seed = seed_dir
        if int(seed) not in seeds:
            print(seed + ' is not interesting for us')
            continue
        current_path = os.path.join(path, seed)
        process_seed_file(current_path, seed, mem_dict, set_limit)
        finished += 1
        print("Finished adding test " + str(seed), finished, len(test_list))
    
    return mem_dict

def process_seed_file(current_path, seed, mem_dict, set_limit):
    write_set_name  = getFileName(current_path, 'write', 'txt')
    read_set_name   = getFileName(current_path, 'read', 'txt')
    
    write_file_path = os.path.join(current_path, write_set_name)
    read_file_path  = os.path.join(current_path, read_set_name)
    
    with open(write_file_path, 'r') as write_file:
        for line in write_file:
            contents = line.strip().split(' ')
            try:
                process_write_access(contents, mem_dict, seed, set_limit)
            except Exception as e:
                print("Unexpected error:", sys.exc_info()[0])
                print(str(e))
                print("Error happens at line ", line)
                continue

    with open(read_file_path, 'r') as read_file:
        for line in read_file:
            contents = line.strip().split(' ')
            try:
                process_read_access(contents, mem_dict, seed, set_limit)
            except:
                print("Error happens at line ", line)

def reduce_size(mem_dict):
    vma_list = list(mem_dict)
    for vma in vma_list:
        if vma < KERNEL_ADDR_MIN:
            del mem_dict[vma]
    return mem_dict

# sys.argv[1] data path to shared memory access
# sys.argv[2] starting seed index
# sys.argv[3] ending seed index
# sys.argv[4] existing mem_dict file (optional)
if __name__ == '__main__':
    data_path = sys.argv[1]
    
    interesting_seed =[]
    for index in range(int(sys.argv[2]), int(sys.argv[3])):
        interesting_seed.append(index)
        
    if len(sys.argv) == 5:
        mem_dict_file = open(sys.argv[4], 'rb')
        print("There is an existing mem_dict")
        print("Start to load mem_dict into memory")
        existing_mem_dict = pickle.load(mem_dict_file)
        print("mem_dict is loaded")
        mem_dict = mem_dict_generation(data_path, interesting_seed, existing_mem_dict)
    else:
        mem_dict = mem_dict_generation(data_path, interesting_seed)
    
    time_now = datetime.now()
    timestamp = time_now.strftime("%Y-%m-%d-%H-%M-%S")
    mem_dict_name = "/../mem-dict-" + timestamp
    f = open(data_path + mem_dict_name,'wb')
    print("Dumping the mem_dict")
    pickle.dump(mem_dict, f, pickle.HIGHEST_PROTOCOL)
