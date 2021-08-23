#!/usr/bin/python3
import os
import sys
import multiprocessing
from multiprocessing import Process
from collections import defaultdict
import time
from datetime import datetime

# Return the filename which matches both the prefiex and postfix
 # Warning: make sure there is only one such file in the folder, otherwise, assertion error
def getFileName(path, prefix, postfix):
    target_list = []

    f_list = os.listdir(path)
    for f in f_list:
        if os.path.splitext(f)[0].startswith(prefix) and f.endswith(postfix):
            target_list.append(f)
    # assert there is only one match in the folder
    assert(len(target_list) == 1)

    return target_list[0]

def filter_access(seed, data_path, test_list, result_path, logging=True):
    if not os.path.isdir(result_path + '/' + str(seed)):
        os.makedirs(result_path + '/' + str(seed))
    write_access_filename = result_path + '/' + str(seed) + '/write.txt'
    read_access_filename = result_path + '/' + str(seed) + '/read.txt'
    result_filename = result_path + '/' + str(seed) + '/result.txt'
    write_access_file = open(write_access_filename, 'w')
    read_access_file = open(read_access_filename, 'w')
    result_file = open(result_filename, 'w')
    total_write = 0
    shared_write = 0
    for test_folder in test_list[0]:
        current_path = data_path + '/cpu0/'+test_folder + '/'
        target_folder = current_path + str(seed) + '_1/'
        if os.path.isdir(target_folder):
            cpu = 0
            try:
                exec_filename = getFileName(target_folder, 'trace', 'txt')
                print(seed, "Found trace file", target_folder+exec_filename)
            except:
                print("This test input is corrupted ", current_path)
                return
            current_path = target_folder
            current_cr3 = 0
            cr3 = 0
            exec_file = open(current_path + '/' + exec_filename, 'r')
            exec_content = exec_file.readlines()
            mem_access_pattern = '# MEM: ' + str(cpu) + ' c'
            ins_pattern = str(cpu) + ' c'
            for line_index in range(0, len(exec_content)):
                line = exec_content[line_index]
                if line.find(mem_access_pattern) == -1:
                    if line.startswith(ins_pattern):
                        contents = line.strip().split(' ')
                        if len(contents) != 15:
                            continue
                        current_cr3 = int(contents[13], 16)
                        if cr3 == 0:
                            cr3 = current_cr3
                        esp_value = int(contents[6], 16)
                        stack_begin = esp_value & 0xFFFFE000
                        stack_end = stack_begin + 0x2000
                elif current_cr3 == cr3:
                    # this is a memory access emit from the target CPU
                    contents = line.strip().split(' ')
                    if len(contents) != 10:
                        # if the length is not 10, then something is wrong
                        # but for now we ignore the error
                        continue
                    if contents[8] == 'S':
                        total_write += 1
                        ins = int(contents[3], 16)
                        addr = int(contents[4], 16)
                        if addr >= stack_begin and addr < stack_end:
                            continue
                        shared_write += 1
                        value = int(contents[5], 16)
                        length = int(contents[7])
                        type = 0
                        print(contents[3], contents[4], contents[5], contents[7], type, file=write_access_file)
                    #print(contents[3], contents[4], contents[5], contents[7], type, file=simple_result_file)
                    #print(line.strip(), file=result_file)
                    # this access is "shared"
    print("write", shared_write, total_write, file=result_file)
    distance = 10
    total_read = 0
    shared_read = 0
    double_read_num = 0
    for test_folder in test_list[1]:
        current_path = data_path + '/cpu1/'+test_folder + '/'
        target_folder = current_path +'1_' + str(seed) + '/'
        if os.path.isdir(target_folder):
            cpu = 1
            try:
                exec_filename = getFileName(target_folder, 'trace', 'txt')
                print(seed, "Found trace file", target_folder+exec_filename)
            except:
                print("This test input is corrupted ", current_path)
                return
            current_cr3 = 0
            cr3 = 0
            current_path = target_folder
            exec_file = open(current_path + '/' + exec_filename, 'r')
            exec_content = exec_file.readlines()
            mem_access_pattern = '# MEM: ' + str(cpu) + ' c'
            ins_pattern = str(cpu) + ' c'
            for line_index in range(0, len(exec_content)):
                line = exec_content[line_index]
                if line.find(mem_access_pattern) == -1:
                    if line.startswith(ins_pattern):
                        contents = line.strip().split(' ')
                        if len(contents) != 15:
                            continue
                        current_cr3 = int(contents[13], 16)
                        if cr3 == 0:
                            cr3 = current_cr3
                        esp_value = int(contents[6], 16)
                        stack_begin = esp_value & 0xFFFFE000
                        stack_end = stack_begin + 0x2000
                elif current_cr3 == cr3:
                    # this is a memory access emit from the target CPU
                    contents = line.strip().split(' ')
                    if len(contents) != 10:
                        # if the length is not 10, then something is wrong
                        # but for now we ignore the rror
                        continue
                    if contents[8] == 'L':
                        total_read += 1
                        ins = int(contents[3], 16)
                        addr = int(contents[4], 16)
                        if addr >= stack_begin and addr < stack_end:
                            continue
                        shared_read += 1
                        value = int(contents[5], 16)
                        length = int(contents[7])
                        # We need to analyze double read for read access
                        jump_found = 0
                        double_read = 0
                        compare_index_end = line_index + 1 + distance
                        compare_current_cr3 = cr3
                        compare_stack_begin = stack_begin
                        compare_stack_end = stack_end
                        if compare_index_end > len(exec_content):
                            compare_index_end = len(exec_content)
                        for compare_index in range(line_index + 1, compare_index_end):
                            compare_line = exec_content[compare_index]
                            if compare_line.find(mem_access_pattern) == -1:
                                if compare_line.startswith(ins_pattern):
                                    compare_contents = compare_line.strip().split(' ')
                                    if len(compare_contents) != 15:
                                        continue
                                    compare_current_cr3 = int(compare_contents[13], 16)
                                    compare_esp_value = int(compare_contents[6], 16)
                                    compare_stack_begin = compare_esp_value & 0xFFFFE000
                                    compare_stack_end = compare_stack_begin + 0x2000
                                    compare_possible_ins = compare_contents[1]
                                    if len(ip_disas[compare_possible_ins])== 0:
                                        ## very strange c10d66b2 ins on 2_1, check it later
                                        continue
                                    if ip_disas[compare_possible_ins][0] == 'j':
                                        jump_found = 1
                            elif compare_current_cr3 == cr3:
                                compare_contents = compare_line.strip().split(' ')
                                if len(compare_contents) != 10:
                                    continue
                                compare_addr = int(compare_contents[4], 16)
                                if compare_addr != addr:
                                    continue
                                compare_ins = int(compare_contents[3], 16)
                                compare_value = int(compare_contents[5], 16)
                                compare_length = int(compare_contents[7])
                                if jump_found and compare_ins != ins and compare_value == value and compare_length == length:
                                    double_read = 1
                                    double_read_num += 1
                                    break
                        print(contents[3], contents[4], contents[5], contents[7], 1, double_read, file=read_access_file)
    print("read", shared_read, double_read_num, total_read, file=result_file)
    print("Finished analyzing seed ", seed, " result is saved to ", result_path)



kernel_dir = os.environ.get('KERNEL_DIR')
if kernel_dir is None:
    print("error happens when accessing environment vairable KERNEL_DIR")
    exit(1)
linux_disas_filename = kernel_dir + "/vmlinux.onlydisas"
ip_disas = defaultdict(str)
linux_disas_file = open(linux_disas_filename, 'r')
for line in linux_disas_file:
    if len(line) > 9 and line[8] == ":":
        contents = line.split(":")
        ins_addr = contents[0]
        ip_disas[ins_addr] = contents[1].strip()
        print(ins_addr, ip_disas[ins_addr])

if __name__ == '__main__':
    data_path = sys.argv[1]
    cpu0_test_list = []
    cpu1_test_list = []
    for test in os.scandir(data_path + '/cpu0/'):
        if test.is_dir() is True:
            if test.name.find("test") > -1:
                cpu0_test_list.append(test.name)
    for test in os.scandir(data_path + '/cpu1/'):
        if test.is_dir() is True:
            if test.name.find("test") > -1:
                cpu1_test_list.append(test.name)
    test_list = [cpu0_test_list, cpu1_test_list]
    assert(len(cpu0_test_list) == len(cpu1_test_list))
    seed_start = int(sys.argv[2])
    seed_end = int(sys.argv[3])
    print("Analyzing sequential tests between ", seed_start, seed_end)
    time_now = datetime.now()
    timestamp = time_now.strftime("%Y-%m-%d-%H-%M-%S")
    result_path =  data_path + '/shared-' + timestamp + '/'
    if not os.path.isdir(result_path):
        os.makedirs(result_path)
    time_start = time.time()
    pool = multiprocessing.Pool(multiprocessing.cpu_count())
    for index in range(seed_start, seed_end):
        pool.apply_async(filter_access, (index, data_path, test_list, result_path,))
    pool.close()
    pool.join()
    time_end = time.time()
    print('time cost',time_end-time_start, 's')
