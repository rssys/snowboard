#!/usr/bin/python3
import sys
import os
import multiprocessing
from collections import defaultdict
import random
import json
import pickle
import resource
from datetime import datetime

def getFileName(path, prefix, postfix):
    target_list = []

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
    return [[defaultdict(write_ins_new_dict), defaultdict(write_ins_new_dict), defaultdict(write_ins_new_dict)], [defaultdict(read_ins_new_dict), defaultdict(read_ins_new_dict), defaultdict(read_ins_new_dict)]]

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
    for f in test_list:
        seed = f
        if int(seed) not in seeds:
            print(seed + ' is not interesting for us')
            continue
        current_path = path + '/' + f
        write_set_name = getFileName(current_path, 'write', 'txt')
        read_set_name = getFileName(current_path, 'read', 'txt')
        allSets = [current_path + '/' + write_set_name, current_path + '/' + read_set_name]
        #for index in range(0, 2):
        write_file = open(allSets[0], 'r')
        for line in write_file:
            contents = line.strip().split(' ')
            try:
                ins = int(contents[0], 16)
                addr = int(contents[1], 16)
                if addr < 0xC0000000:
                    continue
                value = int(contents[2], 16)
                bits = int(contents[3])
                if bits == 8:
                    mem_dict[addr][0][0][ins][value][0] += 1
                    if len(mem_dict[addr][0][0][ins][value][1]) < set_limit:
                        mem_dict[addr][0][0][ins][value][1].add(int(seed))
                elif bits == 16:
                    mem_dict[addr][0][1][ins][value][0] += 1
                    if len(mem_dict[addr][0][1][ins][value][1]) < set_limit:
                        mem_dict[addr][0][1][ins][value][1].add(int(seed))
                elif bits == 32:
                    mem_dict[addr][0][2][ins][value][0] += 1
                    if len(mem_dict[addr][0][2][ins][value][1]) < set_limit:
                        mem_dict[addr][0][2][ins][value][1].add(int(seed))
            except Exception as e:
                print("Unexpected error:", sys.exc_info()[0])
                print(str(e))
                print("Error happens at line ", line)
                continue
        read_file = open(allSets[1], 'r')
        for line in read_file:
            contents = line.strip().split(' ')
            try:
                ins = int(contents[0], 16)
                addr = int(contents[1], 16)
                if addr < 0xC0000000:
                    continue
                value = int(contents[2], 16)
                bits = int(contents[3])
                double_read = int(contents[5])
                if bits == 8:
                    mem_dict[addr][1][0][ins][value][double_read][0] += 1
                    if len(mem_dict[addr][1][0][ins][value][double_read][1]) < set_limit:
                        mem_dict[addr][1][0][ins][value][double_read][1].add(int(seed))
                elif bits == 16:
                    mem_dict[addr][1][1][ins][value][double_read][0] += 1
                    if len(mem_dict[addr][1][1][ins][value][double_read][1]) < set_limit:
                        mem_dict[addr][1][1][ins][value][double_read][1].add(int(seed))
                elif bits == 32:
                    mem_dict[addr][1][2][ins][value][double_read][0] += 1
                    if len(mem_dict[addr][1][2][ins][value][double_read][1]) < set_limit:
                        mem_dict[addr][1][2][ins][value][double_read][1].add(int(seed))
            except:
                print("Error happens at line ", line)
        finished += 1
        print("Finished adding test " + str(f), finished, len(test_list))
    return mem_dict

def reduce_size(mem_dict):
    vma_list = list(mem_dict)
    for vma in vma_list:
        if vma < 0xC0000000:
            del mem_dict[vma]
    return mem_dict

# sys.argv[1] data path to shared memory access
# sys.argv[2] starting seed index
# sys.argv[3] ending seed index
# sys.argv[4] existing mem_dict file (optional)
if __name__ == '__main__':
    data_path = sys.argv[1]
    if len(sys.argv) == 5:
        mem_dict_file = open(sys.argv[4], 'rb')
        print("There is an existing mem_dict")
        print("Start to load mem_dict into memory")
        existing_mem_dict = pickle.load(mem_dict_file)
        print("mem_dict is loaded")
    old_method = False
    if old_method:
        interesting_seed = redundant_check(data_path, 3)
        mem_dict = small_load_interesting(data_path, interesting_seed)
    else:
        interesting_seed =[]
        for index in range(int(sys.argv[2]), int(sys.argv[3])):
            interesting_seed.append(index)
        if len(sys.argv) == 5:
            mem_dict = mem_dict_generation(data_path, interesting_seed, existing_mem_dict)
        else:
            mem_dict = mem_dict_generation(data_path, interesting_seed)
    time_now = datetime.now()
    timestamp = time_now.strftime("%Y-%m-%d-%H-%M-%S")
    mem_dict_name = "/../mem-dict-" + timestamp
    f = open(data_path + mem_dict_name,'wb')
    print("Dumping the mem_dict")
    pickle.dump(mem_dict, f, pickle.HIGHEST_PROTOCOL)
