#!/usr/bin/python3
import sys
import os
import time
from datetime import datetime
import multiprocessing

def concurrent_executor(testcase_list, task_name, testing_mode):
    main_home = os.environ.get('MAIN_HOME')
    if main_home is None:
        print("Please source setup.sh")
        exit(1)
    script = main_home + "/scripts/test/concurrent-test.sh"
    time_now = datetime.now()
    timestamp = time_now.strftime("%Y-%m-%d-%H-%M-%S")
    input1 = 'SKI_INPUT1_RANGE=%'
    input2 = 'SKI_INPUT2_RANGE=%'
    preemp1 = 'SKI_CPU1_PREEMPTION='
    preemp2 = 'SKI_CPU2_PREEMPTION='
    value1 = 'SKI_CPU1_PREEMPTION_VALUE='
    value2 = 'SKI_CPU2_PREEMPTION_VALUE='
    addr = 'SKI_CHANNEL_ADDR='
    ski_mode = False
    if testing_mode == True:
        testcase_list = testcase_list[:50]
    for testcase in testcase_list:
        write_addr = testcase[0]
        if write_addr == -1 and ski_mode == False:
            ski_mode = True
        if ski_mode == True:
            input1 += str(testcase[8]) +','
            input2 += str(testcase[9]) +','
            continue
        read_addr = testcase[1]
        write_byte = testcase[6]
        read_byte = testcase[7]
        write_value = testcase[4]
        read_value = testcase[5]
        write_addr_set = set([])
        read_addr_set = set([])
        for addr_tmp in range(write_addr, write_addr + write_byte):
            write_addr_set.add(addr_tmp)
        for addr_tmp in range(read_addr, read_addr + read_byte):
            read_addr_set.add(addr_tmp)
        addr_begin = min(list(write_addr_set & read_addr_set))
        input1 += str(testcase[9]) +','
        input2 += str(testcase[10]) +','
        preemp1 += str(testcase[2]) +','
        preemp2 += str(testcase[3]) +','
        value1 += str(write_value) + ','
        value2 += str(read_value) + ','
        addr += str(addr_begin) +','
    if ski_mode == False:
        test_cmd = input1[:-1] + ' ' + input2[:-1] + ' ' + preemp1[:-1] + ' ' + preemp2[:-1]+' '+addr[:-1] + ' ' 
        test_cmd += value1[:-1] + ' ' + value2[:-1] + ' SKI_PREEMPTION_BY_ACCESS=1 SKI_FORKALL_CONCURRENCY='+str(multiprocessing.cpu_count()) + ' ' + script
    else:
        test_cmd = input1[:-1] + ' ' + input2[:-1] + ' SKI_PREEMPTION_BY_ACCESS=0 ' 
        test_cmd + 'SKI_FORKALL_CONCURRENCY='+str(multiprocessing.cpu_count()) + ' '
        test_cmd += script
    print(test_cmd)
    os.system(test_cmd)
