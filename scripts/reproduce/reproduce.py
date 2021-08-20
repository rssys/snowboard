#!/usr/bin/python3
import sys
import os
import re
import multiprocessing
import json

# this script can reproduce a bug by executing the concurrent input
if __name__ =='__main__':
    # path to store the testing result
	output_path = os.environ.get('SNOWBOARD_STORAGE')
    # read concurrent input file
    concurrent_input_filename = sys.argv[1]
    concurrent_input_file = open(concurrent_input_filename, 'r')
    input_list = concurrent_input_file.readlines()
    input1 = 'SKI_INPUT1_RANGE=%'
    input2 = 'SKI_INPUT2_RANGE=%'
    preemp1 = 'SKI_CPU1_PREEMPTION='
    preemp2 = 'SKI_CPU2_PREEMPTION='
    addr = 'SKI_CHANNEL_ADDR='
    value1 = 'SKI_CPU1_PREEMPTION_VALUE='
    value2 = 'SKI_CPU2_PREEMPTION_VALUE='
    for concurrent_input in input_list:
        testcase = concurrent_input.strip().split(' ')
        write_addr = int(testcase[0][2:], 16)
        read_addr = int(testcase[1][2:], 16)
        # compute the starting address of the overlapped area
        write_byte = int(testcase[6])
        read_byte = int(testcase[7])
        write_addr_set = set([])
        read_addr_set = set([])
        for addr_tmp in range(write_addr, write_addr + write_byte):
            write_addr_set.add(addr_tmp)
        for addr_tmp in range(read_addr, read_addr + read_byte):
            read_addr_set.add(addr_tmp)
        addr_begin = min(list(write_addr_set & read_addr_set))
        # read the pair of sequential tests
        input1 += str(testcase[8]) +','
        input2 += str(testcase[9]) +','
        # specify the PMC instruction information
        preemp1 += str(int(testcase[2][2:], 16)) +','
        preemp2 += str(int(testcase[3][2:], 16)) +','
        addr += str(addr_begin) +','
        value1 += str(int(testcase[4][2:], 16)) + ','
        value2 += str(int(testcase[5][2:], 16)) + ','
    test_cmd = input1[:-1] + ' ' + input2[:-1] + ' ' + preemp1[:-1] + ' ' + preemp2[:-1]+' '+addr[:-1] + ' ' +value1[:-1] + ' ' + value2[:-1] + ' '
    test_cmd += ' SKI_INTERLEAVING_RANGE=0-600 SKI_FORKALL_CONCURRENCY=' + str(multiprocessing.cpu_count())
    test_cmd += ' SKI_OUTPUT_DIR=' + output_path
    test_cmd += ' ./test.sh'
    print(test_cmd)
    os.system(test_cmd)
