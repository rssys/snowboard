#!/usr/bin/python3
import sys
import json
import os
from collections import defaultdict
import re
from multiprocessing import Process, Manager, Pool
import subprocess
import multiprocessing
import pickle
import time
from datetime import datetime
import random
from rq import Queue
from redis import Redis
import struct
from executor import concurrent_executor

redis_host = '127.0.0.1'
redis_port = 6380
redis_passwd = 'snowboard-testing'

def set_to_list():
    return set([])

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

verbose_debugging = True

# Build a mapping from every unique ins-pair to a list of PMC channels it belongs to
def ins_pair_2_channel(pmc_info_path):
    uncommon_channel_list = open(pmc_info_path + '/uncommon-channel.txt', 'r')
    time_start = time.time()
    ins_pair_to_channel = defaultdict(set_to_list)
    if verbose_debugging:
        print("Building a mapping from ins-pair to the channel it belongs to")
    channel_list = uncommon_channel_list.readlines()
    for line in channel_list:
        contents = line.strip().split(' ')
        write_ins = int(contents[0], 16)
        write_addr = int(contents[1], 16)
        write_byte = int(contents[2])
        read_ins = int(contents[3], 16)
        read_addr = int(contents[4], 16)
        read_byte=  int(contents[5])
        key = tuple([write_ins, read_ins])
        if key not in ins_pair_to_channel:
            ins_pair_to_channel[key] = set([])
        ins_pair_to_channel[key].add(tuple([write_addr, write_byte, read_addr, read_byte]))
    if verbose_debugging:
        print("Num of unique ins-pairs:", len(ins_pair_to_channel.keys()))
    time_end = time.time()
    if verbose_debugging:
        print('time cost',time_end-time_start, 's')
    uncommon_channel_list.close()
    return ins_pair_to_channel

# Build a mapping from a unique ins to a list of PMC channels it belongs to
def ins_2_channel(pmc_info_path):
    uncommon_channel_list = open(pmc_info_path + '/uncommon-channel.txt', 'r')
    time_start = time.time()
    write_ins_to_channel = defaultdict(set_to_list)
    read_ins_to_channel = defaultdict(set_to_list)
    if verbose_debugging:
        print("Building a mapping from a ins to the channel it belongs to")
    channel_list = uncommon_channel_list.readlines()
    for line in channel_list:
        contents = line.strip().split(' ')
        write_ins = int(contents[0], 16)
        write_addr = int(contents[1], 16)
        write_byte = int(contents[2])
        read_ins = int(contents[3], 16)
        read_addr = int(contents[4], 16)
        read_byte=  int(contents[5])
        key = tuple([write_ins, 0])
        if key not in write_ins_to_channel:
            write_ins_to_channel[key] = set([])
        write_ins_to_channel[key].add(tuple([read_ins, write_addr, write_byte, read_addr, read_byte]))
        key = tuple([read_ins, 1])
        if key not in read_ins_to_channel:
            read_ins_to_channel[key] = set([])
        read_ins_to_channel[key].add(tuple([write_ins, write_addr, write_byte, read_addr, read_byte]))
    time_end = time.time()
    if verbose_debugging:
        print("Number of unique write ins", len(write_ins_to_channel.keys()), "unique read ins", len(read_ins_to_channel.keys())) 
        print('time cost',time_end-time_start, 's')
    uncommon_channel_list.close()
    return write_ins_to_channel, read_ins_to_channel

# Generates concurrent inputs based on ins-pair strategy
# pmc_info_path: the folder path where 'uncommon-ins-pair' is stored
# log_path: the folder path where generated concurrent inputs will be logged
# max_queue_size: the maximum number of concurrent input batches allowed. A batch of concurrent inputs contains 500 inputs.
def ins_pair_strategy(pmc_info_path, log_path, max_queue_size):
    uncommon_ins_pair_list = open(pmc_info_path + '/uncommon-ins-pair.txt', 'r')
    time_now = datetime.now()
    timestamp = time_now.strftime("%Y-%m-%d-%H-%M-%S")
    result_filename = log_path + "/ins-pair-" + timestamp + ".txt"
    result_file = open(result_filename, 'w')
    testcase_pack = []
    random_addr_list = list(mem_dict.keys())
    redis_conn = Redis(host = redis_host, port = redis_port, password=redis_passwd)
    input_queue = Queue('ins-pair', connection = redis_conn)
    total_input = 0
    while 1:
        while len(input_queue) < max_queue_size:
            # find a batch for testing
            testcase_pack = []
            while len(testcase_pack) < 500:
                ins_pair_item = uncommon_ins_pair_list.readline()
                if not ins_pair_item:
                    print("INS-PAIR strategy has generated all concurrent inputs")
                    # push the rest testcase_pack
                    if len(testcase_pack) > 0:
                        job = input_queue.enqueue(concurrent_executor, args = (testcase_pack, "ins-pair", False), result_ttl=0, job_timeout=366000)
                    return
                contents = ins_pair_item.strip().split(' ')
                write_ins = int(contents[0], 16)
                read_ins = int(contents[1], 16)
                key = tuple([write_ins, read_ins])
                assert(key in ins_pair_to_channel)
                mem_area_list = list(ins_pair_to_channel[key])
                mem_area = random.choice(mem_area_list)
                mem_area = list(mem_area)
                write_addr = mem_area[0]
                write_byte = mem_area[1]
                read_addr = mem_area[2]
                read_byte = mem_area[3]
                total_input += 1
                testcase = channel_generator(write_ins, write_addr, write_byte, read_ins, read_addr, read_byte)
                testcase_pack.append(testcase)
                write_addr = testcase[0]
                read_addr = testcase[1]
                write_ins = testcase[2]
                read_ins = testcase[3]
                write_value = testcase[4]
                read_value = testcase[5]
                write_byte = testcase[6]
                read_byte = testcase[7]
                double_read = testcase[8]
                write_id = testcase[9]
                read_id = testcase[10]
                print(hex(write_addr), hex(read_addr), hex(write_ins), hex(read_ins), hex(write_value),
                    hex(read_value), write_byte, read_byte, double_read, write_id, read_id, contents[2], file=result_file)
                result_file.flush()
            print("[ins-pair] generating ", total_input, " inputs")
            job = input_queue.enqueue(concurrent_executor, args = (testcase_pack, "ins-pair", False), result_ttl=0, job_timeout=366000)

def random_ins_pair_strategy(pmc_info_path, log_path, max_queue_size):
    uncommon_ins_pair_list = open(pmc_info_path + '/uncommon-ins-pair.txt', 'r')
    time_now = datetime.now()
    timestamp = time_now.strftime("%Y-%m-%d-%H-%M-%S")
    result_filename = log_path + "/random-ins-pair-" + timestamp + ".txt"
    result_file = open(result_filename, 'w')
    testcase_pack = []
    random_addr_list = list(mem_dict.keys())
    redis_conn = Redis(host = redis_host, port = redis_port, password=redis_passwd) 
    input_queue = Queue('random-ins-pair', connection = redis_conn)
    ins_pair_item_list = uncommon_ins_pair_list.readlines()
    random.shuffle(ins_pair_item_list)
    total_input = 0
    while 1:
        while len(input_queue) < max_queue_size:
            # find a batch for testing
            testcase_pack = []
            while len(testcase_pack) < 500:
                try:
                    ins_pair_item = ins_pair_item_list.pop()
                except:
                    if len(testcase_pack) > 0:
                        job = input_queue.enqueue(concurrent_executor, args = (testcase_pack, "random-ins-pair", False), result_ttl=0, job_timeout=366000)
                try:
                    contents = ins_pair_item.strip().split(' ')
                except:
                    print("error happens", ins_pair_item)
                    continue
                write_ins = int(contents[0], 16)
                read_ins = int(contents[1], 16)
                key = tuple([write_ins, read_ins])
                if key not in ins_pair_to_channel:
                    return
                mem_area_list = list(ins_pair_to_channel[key])
                if len(mem_area_list) == 0:
                    print("error happens: mem_area_list should not be 0")
                    exit(1)
                # randomly select a mem_area on which the write and read instruction communicates
                mem_area = random.choice(mem_area_list)
                mem_area = list(mem_area)
                write_addr = mem_area[0]
                write_byte = mem_area[1]
                read_addr = mem_area[2]
                read_byte = mem_area[3]
                total_input += 1
                testcase = channel_generator(write_ins, write_addr, write_byte, read_ins, read_addr, read_byte)
                testcase_pack.append(testcase)
                write_addr = testcase[0]
                read_addr = testcase[1]
                write_ins = testcase[2]
                read_ins = testcase[3]
                write_value = testcase[4]
                read_value = testcase[5]
                write_byte = testcase[6]
                read_byte = testcase[7]
                double_read = testcase[8]
                write_id = testcase[9]
                read_id = testcase[10]
                print(hex(write_addr), hex(read_addr), hex(write_ins), hex(read_ins), hex(write_value),
                    hex(read_value), write_byte, read_byte, double_read, write_id, read_id, contents[2], file=result_file)
                result_file.flush()
            print("[random-ins-pair] generating ", total_input, " inputs")
            job = input_queue.enqueue(concurrent_executor, args = (testcase_pack, "random-ins-pair", False), result_ttl=0, job_timeout=366000)


def ins_strategy(pmc_info_path, log_path, max_queue_size):
    uncommon_ins_list = open(pmc_info_path + '/uncommon-ins.txt', 'r')
    time_now = datetime.now()
    timestamp = time_now.strftime("%Y-%m-%d-%H-%M-%S")
    result_filename = log_path + "/ins-" + timestamp + ".txt"
    result_file = open(result_filename, 'w')
    testcase_pack = []
    random_addr_list = list(mem_dict.keys())
    redis_conn = Redis(host = redis_host, port = redis_port, password=redis_passwd)
    input_queue = Queue('ins', connection = redis_conn)
    total_input = 0
    while 1:
        while len(input_queue) < max_queue_size:
            # find a batch for testing
            testcase_pack = []
            while len(testcase_pack) < 500:
                ins_item = uncommon_ins_list.readline()
                if not ins_item:
                    if len(testcase_pack) > 0:
                        job = input_queue.enqueue(concurrent_executor, args = (testcase_pack, "ins", False), result_ttl=0, job_timeout=366000)
                    return
                try:
                    contents = ins_item.strip().split(' ')
                except:
                    print("Error with line", ins_item)
                    continue
                ins = int(contents[0], 16)
                ins_type = int(contents[1])
                if ins_type == 0:
                    key = tuple([ins, 0])
                    assert(key in write_ins_to_channel)
                    if key not in write_ins_to_channel:
                        print("error happen: a strange write instruction that Snowboard does not know", key)
                        exit(1)
                    mem_area_list = list(write_ins_to_channel[key])
                    mem_area = list(random.choice(mem_area_list))
                    write_ins = ins
                    read_ins = mem_area[0]
                    write_addr = mem_area[1]
                    write_byte = mem_area[2]
                    read_addr = mem_area[3]
                    read_byte = mem_area[4]
                elif ins_type == 1:
                    key = tuple([ins, 1])
                    if key not in read_ins_to_channel:
                        print("error happen: a strange read instruction that Snowboard does not know", key)
                        exit(1)
                    mem_area_list = list(read_ins_to_channel[key])
                    mem_area = list(random.choice(mem_area_list))
                    write_ins = mem_area[0]
                    read_ins = ins
                    write_addr = mem_area[1]
                    write_byte = mem_area[2]
                    read_addr = mem_area[3]
                    read_byte = mem_area[4]
                total_input += 1
                testcase = channel_generator(write_ins, write_addr, write_byte, read_ins, read_addr, read_byte) 
                testcase_pack.append(testcase)
                write_addr = testcase[0]
                read_addr = testcase[1]
                write_ins = testcase[2]
                read_ins = testcase[3]
                write_value = testcase[4]
                read_value = testcase[5]
                write_byte = testcase[6]
                read_byte = testcase[7]
                double_read = testcase[8]
                write_id = testcase[9]
                read_id = testcase[10]
                print(hex(write_addr), hex(read_addr), hex(write_ins), hex(read_ins), hex(write_value),
                hex(read_value), write_byte, read_byte, double_read, write_id, read_id, contents[2], file=result_file)
                result_file.flush()
            print("[ins] generated", total_input, " inputs")
            job = input_queue.enqueue(concurrent_executor, args = (testcase_pack, "ins", False), result_ttl=0, job_timeout=366000)


def channel_generator(write_ins, write_addr, write_byte, read_ins, read_addr, read_byte):
    access_byte_to_index = {1:0, 2:1, 4:2}
    write_access_dict = mem_dict[write_addr][0]
    read_access_dict = mem_dict[read_addr][1]
    write_length_index = access_byte_to_index[write_byte]
    read_length_index = access_byte_to_index[read_byte]
    write_ins_dict = write_access_dict[write_length_index]
    read_ins_dict = read_access_dict[read_length_index]
    write_addr_begin = write_addr
    write_addr_end = write_addr + write_byte - 1
    read_addr_begin = read_addr
    read_addr_end = read_addr + read_byte - 1
    write_addr_set = set([])
    read_addr_set = set([])
    for addr in range(write_addr_begin, write_addr_end + 1):
        write_addr_set.add(addr)
    for addr in range(read_addr_begin, read_addr_end + 1):
        read_addr_set.add(addr)
    # check if two memory access could overlap
    assert(len(write_addr_set & read_addr_set) != 0)
    addr_begin = min(list(write_addr_set & read_addr_set))
    addr_end = max(list(write_addr_set & read_addr_set))
    read_begin_offset = addr_begin - read_addr_begin
    read_end_offset = addr_end - read_addr_begin + 1
    write_begin_offset = addr_begin - write_addr_begin
    write_end_offset = addr_end - write_addr_begin + 1
    write_value_dict = write_ins_dict[write_ins]
    read_value_dict = read_ins_dict[read_ins]
    random_write_value_list = list(write_value_dict.keys())
    random.shuffle(random_write_value_list)
    random_read_value_list = list(read_value_dict.keys())
    random.shuffle(random_read_value_list)
    for write_value in random_write_value_list:
        write_bytes_value = struct.pack('<I', write_value)
        write_actual_value = write_bytes_value[write_begin_offset: write_end_offset]
        for read_value in random_read_value_list:
            read_bytes_value = struct.pack('<I', read_value)
            read_actual_value = read_bytes_value[read_begin_offset:read_end_offset]
            if write_actual_value != read_actual_value:
                write_candidate_list = write_value_dict[write_value]
                read_candidate_list = read_value_dict[read_value]
                # randonly decide if we try double-read first
                double_read_freq = write_candidate_list[0] * read_candidate_list[1][0]
                normal_freq = write_candidate_list[0] * read_candidate_list[0][0]
                if double_read_freq > 0 and normal_freq > 0:
                    try_double_read = random.choice([0, 1])
                    if try_double_read:
                        write_id_list = list(write_candidate_list[1])
                        read_id_list = list(read_candidate_list[1][1])
                        write_id = random.choice(write_id_list)
                        read_id = random.choice(read_id_list)
                        return [write_addr, read_addr, write_ins, read_ins, write_value, read_value, write_byte, read_byte, 1, write_id, read_id]
                    else:
                        write_id_list = list(write_candidate_list[1])
                        read_id_list = list(read_candidate_list[0][1])
                        write_id = random.choice(write_id_list)
                        read_id = random.choice(read_id_list)
                        return [write_addr, read_addr, write_ins, read_ins, write_value, read_value, write_byte, read_byte, 0, write_id, read_id]
                if double_read_freq > 0 and normal_freq == 0:
                    write_id_list = list(write_candidate_list[1])
                    read_id_list = list(read_candidate_list[1][1])
                    write_id = random.choice(write_id_list)
                    read_id = random.choice(read_id_list)
                    return [write_addr, read_addr, write_ins, read_ins, write_value, read_value, write_byte, read_byte, 1, write_id, read_id]
                if double_read_freq == 0 and normal_freq > 0:
                    write_id_list = list(write_candidate_list[1])
                    read_id_list = list(read_candidate_list[0][1])
                    write_id = random.choice(write_id_list)
                    read_id = random.choice(read_id_list)
                    return [write_addr, read_addr, write_ins, read_ins, write_value, read_value, write_byte, read_byte, 0, write_id, read_id]

def channel_strategy(pmc_info_path, log_path, max_queue_size):
    uncommon_channel_list = open(pmc_info_path + '/uncommon-channel.txt', 'r')
    time_now = datetime.now()
    timestamp = time_now.strftime("%Y-%m-%d-%H-%M-%S")
    result_filename = log_path + "/channel-" + timestamp + ".txt"
    result_file = open(result_filename, 'w')
    redis_conn = Redis(host = redis_host, port = redis_port, password=redis_passwd)
    input_queue = Queue('channel', connection = redis_conn)
    total_input = 0
    while 1:
        while len(input_queue) < max_queue_size:
            testcase_pack = []
            while len(testcase_pack) < 500:
                channel_item = uncommon_channel_list.readline()
                if not channel_item:
                    print("Channel has generate all communications, about to return")
                    if len(testcase_pack) > 0:
                        job = input_queue.enqueue(concurrent_executor, args = (testcase_pack, "channel", False), result_ttl=0, job_timeout=366000)
                    return
                contents = channel_item.strip().split(' ')
                write_ins = int(contents[0], 16)
                write_addr = int(contents[1], 16)
                write_byte = int(contents[2])
                read_ins = int(contents[3], 16)
                read_addr = int(contents[4], 16)
                read_byte=  int(contents[5])
                total_input += 1
                testcase = channel_generator(write_ins, write_addr, write_byte, read_ins, read_addr, read_byte)
                testcase_pack.append(testcase)
                write_addr = testcase[0]
                read_addr = testcase[1]
                write_ins = testcase[2]
                read_ins = testcase[3]
                write_value = testcase[4]
                read_value = testcase[5]
                write_byte = testcase[6]
                read_byte = testcase[7]
                double_read = testcase[8]
                write_id = testcase[9]
                read_id = testcase[10]
                print(hex(write_addr), hex(read_addr), hex(write_ins), hex(read_ins), hex(write_value),
                hex(read_value), write_byte, read_byte, double_read, write_id, read_id, contents[6], file=result_file)
                result_file.flush()
            print("[channel] generated ", total_input, " inputs")
            job = input_queue.enqueue(concurrent_executor, args = (testcase_pack, "channel", False), result_ttl=0, job_timeout=366000)

def unaligned_channel_strategy(pmc_info_path, log_path, max_queue_size):
    uncommon_channel_list = open(pmc_info_path + '/uncommon-unaligned-channel.txt', 'r')
    time_now = datetime.now()
    timestamp = time_now.strftime("%Y-%m-%d-%H-%M-%S")
    result_filename = log_path + "/unaligned-channel-" + timestamp + ".txt"
    result_file = open(result_filename, 'w')
    redis_conn = Redis(host = redis_host, port = redis_port, password=redis_passwd)
    input_queue = Queue('unaligned-channel', connection = redis_conn)
    total_input = 0
    while 1: 
        while len(input_queue) < max_queue_size:
            testcase_pack = []
            while len(testcase_pack) < 500:
                channel_item = uncommon_channel_list.readline()
                if not channel_item:
                    print("unaligned-channel has generated all communications, about to return")
                    if len(testcase_pack) > 0:
                        job = input_queue.enqueue(concurrent_executor, args = (testcase_pack, "unaligned-channel", False), result_ttl=0, job_timeout=366000)
                    return
                contents = channel_item.strip().split(' ')
                write_ins = int(contents[0], 16)
                write_addr = int(contents[1], 16)
                write_byte = int(contents[2])
                read_ins = int(contents[3], 16)
                read_addr = int(contents[4], 16)
                read_byte=  int(contents[5])
                total_input += 1
                testcase = channel_generator(write_ins, write_addr, write_byte, read_ins, read_addr, read_byte)
                testcase_pack.append(testcase)
                write_addr = testcase[0]
                read_addr = testcase[1]
                write_ins = testcase[2]
                read_ins = testcase[3]
                write_value = testcase[4]
                read_value = testcase[5]
                write_byte = testcase[6]
                read_byte = testcase[7]
                double_read = testcase[8]
                write_id = testcase[9]
                read_id = testcase[10]
                print(hex(write_addr), hex(read_addr), hex(write_ins), hex(read_ins), hex(write_value),
                hex(read_value), write_byte, read_byte, double_read, write_id, read_id, contents[6], file=result_file)
                result_file.flush()
            print("[unaligned-channel] generated ", total_input, " inputs")
            job = input_queue.enqueue(concurrent_executor, args = (testcase_pack, "unaligned-channel", False), result_ttl=0, job_timeout=366000)


def object_null_generator(write_ins, write_addr, write_byte, read_ins, read_addr, read_byte):
    access_byte_to_index = {1:0, 2:1, 4:2}
    write_access_dict = mem_dict[write_addr][0]
    read_access_dict = mem_dict[read_addr][1]
    write_length_index = access_byte_to_index[write_byte]
    read_length_index = access_byte_to_index[read_byte]
    write_ins_dict = write_access_dict[write_length_index]
    read_ins_dict = read_access_dict[read_length_index]
    write_addr_begin = write_addr
    write_addr_end = write_addr + write_byte - 1
    read_addr_begin = read_addr
    read_addr_end = read_addr + read_byte - 1
    write_addr_set = set([])
    read_addr_set = set([])
    for addr in range(write_addr_begin, write_addr_end + 1):
        write_addr_set.add(addr)
    for addr in range(read_addr_begin, read_addr_end + 1):
        read_addr_set.add(addr)
    # check if two memory access could overlap
    if (len(write_addr_set & read_addr_set) == 0):
        print("The two memory areas do not overlap")
        exit(1)
    addr_begin = min(list(write_addr_set & read_addr_set))
    addr_end = max(list(write_addr_set & read_addr_set))
    read_begin_offset = addr_begin - read_addr_begin
    read_end_offset = addr_end - read_addr_begin + 1
    write_begin_offset = addr_begin - write_addr_begin
    write_end_offset = addr_end - write_addr_begin + 1
    write_value_dict = write_ins_dict[write_ins]
    read_value_dict = read_ins_dict[read_ins]
    random_write_value_list = list(write_value_dict.keys())
    if 0 not in random_write_value_list:
        print("No communication nullificate the memory address")
        exit(1)
    random_write_value_list = [0]
    random_read_value_list = list(read_value_dict.keys())
    random.shuffle(random_read_value_list)
    for write_value in random_write_value_list:
        write_bytes_value = struct.pack('<I', write_value)
        write_actual_value = write_bytes_value[write_begin_offset: write_end_offset]
        for read_value in random_read_value_list:
            read_bytes_value = struct.pack('<I', read_value)
            read_actual_value = read_bytes_value[read_begin_offset:read_end_offset]
            if write_actual_value != read_actual_value:
                write_candidate_list = write_value_dict[write_value]
                read_candidate_list = read_value_dict[read_value]
                # randonly decide if we try double-read first
                double_read_freq = write_candidate_list[0] * read_candidate_list[1][0]
                normal_freq = write_candidate_list[0] * read_candidate_list[0][0]
                if double_read_freq > 0 and normal_freq > 0:
                    try_double_read = random.choice([0, 1])
                    if try_double_read:
                        write_id_list = list(write_candidate_list[1])
                        read_id_list = list(read_candidate_list[1][1])
                        write_id = random.choice(write_id_list)
                        read_id = random.choice(read_id_list)
                        return [write_addr, read_addr, write_ins, read_ins, write_value, read_value, write_byte, read_byte, 1, write_id, read_id]
                    else:
                        write_id_list = list(write_candidate_list[1])
                        read_id_list = list(read_candidate_list[0][1])
                        write_id = random.choice(write_id_list)
                        read_id = random.choice(read_id_list)
                        return [write_addr, read_addr, write_ins, read_ins, write_value, read_value, write_byte, read_byte, 0, write_id, read_id]
                if double_read_freq > 0 and normal_freq == 0:
                    write_id_list = list(write_candidate_list[1])
                    read_id_list = list(read_candidate_list[1][1])
                    write_id = random.choice(write_id_list)
                    read_id = random.choice(read_id_list)
                    return [write_addr, read_addr, write_ins, read_ins, write_value, read_value, write_byte, read_byte, 1, write_id, read_id]
                if double_read_freq == 0 and normal_freq > 0:
                    write_id_list = list(write_candidate_list[1])
                    read_id_list = list(read_candidate_list[0][1])
                    write_id = random.choice(write_id_list)
                    read_id = random.choice(read_id_list)
                    return [write_addr, read_addr, write_ins, read_ins, write_value, read_value, write_byte, read_byte, 0, write_id, read_id]

def object_null_channel_strategy(pmc_info_path, log_path, max_queue_size):
    uncommon_channel_list = open(pmc_info_path + '/uncommon-object-null-channel.txt', 'r')
    time_now = datetime.now()
    timestamp = time_now.strftime("%Y-%m-%d-%H-%M-%S")
    result_filename = log_path + "/object-null-" + timestamp + ".txt"
    result_file = open(result_filename, 'w')
    redis_conn = Redis(host = redis_host, port = redis_port, password=redis_passwd)
    input_queue = Queue('object-null', connection = redis_conn)
    total_input = 0
    while 1:
        while len(input_queue) < max_queue_size:
            testcase_pack = []
            while len(testcase_pack) < 500:
                channel_item = uncommon_channel_list.readline()
                if not channel_item:
                    print("object-null has generated all communications, about to return")
                    if len(testcase_pack) > 0:
                        job = input_queue.enqueue(concurrent_executor, args = (testcase_pack, "object-null", False), result_ttl=0, job_timeout=366000)
                    return
                contents = channel_item.strip().split(' ')
                write_ins = int(contents[0], 16)
                write_addr = int(contents[1], 16)
                write_byte = int(contents[2])
                read_ins = int(contents[3], 16)
                read_addr = int(contents[4], 16)
                read_byte=  int(contents[5])
                total_input += 1
                testcase = object_null_generator(write_ins, write_addr, write_byte, read_ins, read_addr, read_byte)
                testcase_pack.append(testcase)
                write_addr = testcase[0]
                read_addr = testcase[1]
                write_ins = testcase[2]
                read_ins = testcase[3]
                write_value = testcase[4]
                read_value = testcase[5]
                write_byte = testcase[6]
                read_byte = testcase[7]
                double_read = testcase[8]
                write_id = testcase[9]
                read_id = testcase[10]
                print(hex(write_addr), hex(read_addr), hex(write_ins), hex(read_ins), hex(write_value),
                hex(read_value), write_byte, read_byte, double_read, write_id, read_id, contents[6], file=result_file)
                result_file.flush()
            print("[object-null] generated ", total_input, " inputs")
            job = input_queue.enqueue(concurrent_executor, args = (testcase_pack, "object-null", False), result_ttl=0, job_timeout=366000)


def double_read_channel_generator(write_ins, write_addr, write_byte, read_ins, read_addr, read_byte):
    access_byte_to_index = {1:0, 2:1, 4:2}
    write_access_dict = mem_dict[write_addr][0]
    read_access_dict = mem_dict[read_addr][1]
    write_length_index = access_byte_to_index[write_byte]
    read_length_index = access_byte_to_index[read_byte]
    write_ins_dict = write_access_dict[write_length_index]
    read_ins_dict = read_access_dict[read_length_index]
    write_addr_begin = write_addr
    write_addr_end = write_addr + write_byte - 1
    read_addr_begin = read_addr
    read_addr_end = read_addr + read_byte - 1
    write_addr_set = set([])
    read_addr_set = set([])
    for addr in range(write_addr_begin, write_addr_end + 1):
        write_addr_set.add(addr)
    for addr in range(read_addr_begin, read_addr_end + 1):
        read_addr_set.add(addr)
    # check if two memory access could overlap
    assert(len(write_addr_set & read_addr_set) != 0)
    addr_begin = min(list(write_addr_set & read_addr_set))
    addr_end = max(list(write_addr_set & read_addr_set))
    read_begin_offset = addr_begin - read_addr_begin
    read_end_offset = addr_end - read_addr_begin + 1
    write_begin_offset = addr_begin - write_addr_begin
    write_end_offset = addr_end - write_addr_begin + 1
    write_value_dict = write_ins_dict[write_ins]
    read_value_dict = read_ins_dict[read_ins]
    random_write_value_list = list(write_value_dict.keys())
    random.shuffle(random_write_value_list)
    random_read_value_list = list(read_value_dict.keys())
    random.shuffle(random_read_value_list)
    for write_value in random_write_value_list:
        write_bytes_value = struct.pack('<I', write_value)
        write_actual_value = write_bytes_value[write_begin_offset: write_end_offset]
        for read_value in random_read_value_list:
            read_bytes_value = struct.pack('<I', read_value)
            read_actual_value = read_bytes_value[read_begin_offset:read_end_offset]
            if write_actual_value != read_actual_value:
                write_candidate_list = write_value_dict[write_value]
                read_candidate_list = read_value_dict[read_value]
                freq = write_candidate_list[0] * read_candidate_list[1][0]
                if freq:
                    write_id_list = list(write_candidate_list[1])
                    read_id_list = list(read_candidate_list[1][1])
                    write_id = random.choice(write_id_list)
                    read_id = random.choice(read_id_list)
                    return [write_addr, read_addr, write_ins, read_ins, write_value, read_value, write_byte, read_byte, 1, write_id, read_id]


def double_read_channel_strategy(pmc_info_path, log_path, max_queue_size):
    uncommon_double_read_list = open(pmc_info_path + '/uncommon-double-read-channel.txt', 'r')
    time_now = datetime.now()
    timestamp = time_now.strftime("%Y-%m-%d-%H-%M-%S")
    result_filename = log_path + "/double-read-" + timestamp + ".txt"
    result_file = open(result_filename, 'w')
    redis_conn = Redis(host = redis_host, port = redis_port, password=redis_passwd)
    input_queue = Queue('double-read', connection = redis_conn)
    total_input = 0
    while 1:
        while len(input_queue) < max_queue_size:
            testcase_pack = []
            while len(testcase_pack) < 500:
                channel_item = uncommon_double_read_list.readline()
                if not channel_item:
                    print("double-read has generated all communications, about to return")
                    if len(testcase_pack) > 0:
                        job = input_queue.enqueue(concurrent_executor, args = (testcase_pack, "double-read", False), result_ttl=0, job_timeout=366000)
                    return
                contents = channel_item.strip().split(' ')
                write_ins = int(contents[0], 16)
                write_addr = int(contents[1], 16)
                write_byte = int(contents[2])
                read_ins = int(contents[3], 16)
                read_addr = int(contents[4], 16)
                read_byte=  int(contents[5])
                total_input += 1
                testcase = double_read_channel_generator(write_ins, write_addr, write_byte, read_ins, read_addr, read_byte)
                testcase_pack.append(testcase)
                write_addr = testcase[0]
                read_addr = testcase[1]
                write_ins = testcase[2]
                read_ins = testcase[3]
                write_value = testcase[4]
                read_value = testcase[5]
                write_byte = testcase[6]
                read_byte = testcase[7]
                double_read = testcase[8]
                write_id = testcase[9]
                read_id = testcase[10]
                print(hex(write_addr), hex(read_addr), hex(write_ins), hex(read_ins), hex(write_value),
                hex(read_value), write_byte, read_byte, double_read, write_id, read_id, contents[7], file=result_file)
                result_file.flush()
            print("[double-read] generated", total_input, "inputs")
            job = input_queue.enqueue(concurrent_executor, args = (testcase_pack, "double-read", False), result_ttl=0, job_timeout=366000)

def mem_area_generator(write_addr, write_byte, read_addr, read_byte):
    access_byte_to_index = {1:0, 2:1, 4:2}
    write_access_dict = mem_dict[write_addr][0]
    read_access_dict = mem_dict[read_addr][1]
    write_length_index = access_byte_to_index[write_byte]
    read_length_index = access_byte_to_index[read_byte]
    write_ins_dict = write_access_dict[write_length_index]
    read_ins_dict = read_access_dict[read_length_index]
    write_addr_begin = write_addr
    write_addr_end = write_addr + write_byte - 1
    read_addr_begin = read_addr
    read_addr_end = read_addr + read_byte - 1
    write_addr_set = set([])
    read_addr_set = set([])
    for addr in range(write_addr_begin, write_addr_end + 1):
        write_addr_set.add(addr)
    for addr in range(read_addr_begin, read_addr_end + 1):
        read_addr_set.add(addr)
    # check if two memory access could overlap
    assert(len(write_addr_set & read_addr_set) != 0)
    addr_begin = min(list(write_addr_set & read_addr_set))
    addr_end = max(list(write_addr_set & read_addr_set))
    read_begin_offset = addr_begin - read_addr_begin
    read_end_offset = addr_end - read_addr_begin + 1
    write_begin_offset = addr_begin - write_addr_begin
    write_end_offset = addr_end - write_addr_begin + 1
    random_write_ins_list = list(write_ins_dict.keys())
    random.shuffle(random_write_ins_list)
    random_read_ins_list = list(read_ins_dict.keys())
    random.shuffle(random_read_ins_list)
    for write_ins in random_write_ins_list:
        for read_ins in random_read_ins_list:
            write_value_dict = write_ins_dict[write_ins]
            read_value_dict = read_ins_dict[read_ins]
            random_write_value_list = list(write_value_dict.keys())
            random.shuffle(random_write_value_list)
            random_read_value_list = list(read_value_dict.keys())
            random.shuffle(random_read_value_list)
            for write_value in random_write_value_list:
                write_bytes_value = struct.pack('<I', write_value)
                write_actual_value = write_bytes_value[write_begin_offset: write_end_offset]
                for read_value in random_read_value_list:
                    read_bytes_value = struct.pack('<I', read_value)
                    read_actual_value = read_bytes_value[read_begin_offset:read_end_offset]
                    if write_actual_value != read_actual_value:
                        write_candidate_list = write_value_dict[write_value]
                        read_candidate_list = read_value_dict[read_value]
                        # randonly decide if we try double-read first
                        double_read_freq = write_candidate_list[0] * read_candidate_list[1][0]
                        normal_freq = write_candidate_list[0] * read_candidate_list[0][0]
                        if double_read_freq > 0 and normal_freq > 0:
                            try_double_read = random.choice([0, 1])
                            if try_double_read:
                                write_id_list = list(write_candidate_list[1])
                                read_id_list = list(read_candidate_list[1][1])
                                write_id = random.choice(write_id_list)
                                read_id = random.choice(read_id_list)
                                return [write_addr, read_addr, write_ins, read_ins, write_value, read_value, write_byte, read_byte, 1, write_id, read_id]
                            else:
                                write_id_list = list(write_candidate_list[1])
                                read_id_list = list(read_candidate_list[0][1])
                                write_id = random.choice(write_id_list)
                                read_id = random.choice(read_id_list)
                                return [write_addr, read_addr, write_ins, read_ins, write_value, read_value, write_byte, read_byte, 0, write_id, read_id]
                        if double_read_freq > 0 and normal_freq == 0:
                            write_id_list = list(write_candidate_list[1])
                            read_id_list = list(read_candidate_list[1][1])
                            write_id = random.choice(write_id_list)
                            read_id = random.choice(read_id_list)
                            return [write_addr, read_addr, write_ins, read_ins, write_value, read_value, write_byte, read_byte, 1, write_id, read_id]
                        if double_read_freq == 0 and normal_freq > 0:
                            write_id_list = list(write_candidate_list[1])
                            read_id_list = list(read_candidate_list[0][1])
                            write_id = random.choice(write_id_list)
                            read_id = random.choice(read_id_list)
                            return [write_addr, read_addr, write_ins, read_ins, write_value, read_value, write_byte, read_byte, 0, write_id, read_id]

def mem_addr_strategy(pmc_info_path, log_path, max_queue_size):
    uncommon_mem_area_list = open(pmc_info_path + '/uncommon-mem-addr.txt', 'r')
    time_now = datetime.now()
    timestamp = time_now.strftime("%Y-%m-%d-%H-%M-%S")
    result_filename = log_path + "/mem-addr-" + timestamp + ".txt"
    result_file = open(result_filename, 'w')
    redis_conn = Redis(host = redis_host, port = redis_port, password=redis_passwd)
    input_queue = Queue('mem-addr', connection = redis_conn)
    total_input = 0
    while 1:
        while len(input_queue) < max_queue_size:
            testcase_pack = []
            while len(testcase_pack) < 500:
                mem_area_item = uncommon_mem_area_list.readline()
                if not mem_area_item:
                    print("mem-addr has generated all communications, about to return")
                    if len(testcase_pack) > 0:
                        job = input_queue.enqueue(concurrent_executor, args = (testcase_pack, "mem-addr", False), result_ttl=0, job_timeout=366000)
                    return
                contents = mem_area_item.strip().split(' ')
                write_addr = int(contents[0], 16)
                read_addr = int(contents[1], 16)
                write_byte = int(contents[2])
                read_byte = int(contents[3])
                total_input += 1
                testcase = mem_area_generator(write_addr, write_byte, read_addr, read_byte)
                testcase_pack.append(testcase)
                write_addr = testcase[0]
                read_addr = testcase[1]
                write_ins = testcase[2]
                read_ins = testcase[3]
                write_value = testcase[4]
                read_value = testcase[5]
                write_byte = testcase[6]
                read_byte = testcase[7]
                double_read = testcase[8]
                write_id = testcase[9]
                read_id = testcase[10]
                print(hex(write_addr), hex(read_addr), hex(write_ins), hex(read_ins), hex(write_value),
                hex(read_value), write_byte, read_byte, double_read, write_id, read_id, contents[4], file=result_file)
                result_file.flush()
            print("[mem-addr] generated", total_input, "inputs")
            job = input_queue.enqueue(concurrent_executor, args = (testcase_pack, "mem-addr", False), result_ttl=0, job_timeout=366000)

def full_pmc_strategy(pmc_info_path, log_path, max_queue_size):
    testing_mode = False
    time_start = time.time()
    communication_list = open(pmc_info_path + '/uncommon-pmc.txt', 'r')
    time_now = datetime.now()
    timestamp = time_now.strftime("%Y-%m-%d-%H-%M-%S")
    result_filename = log_path + "/communication-" + timestamp + ".txt"
    result_file = open(result_filename, 'w')
    communication_set = set([])
    testcase_pack_list = []
    testcase_pack = []
    access_index_to_byte = {1:0, 2:1, 4:2}
    num_comm_checked = 0
    num_comm_found = 0
    communication = communication_list.readline()
    redis_conn = Redis(host = redis_host, port = redis_port, password=redis_passwd)
    input_queue = Queue('comm', connection = redis_conn)
    total = 5000000
    while communication and num_comm_found < total:
        # Check if the current_testpack is full
        if len(testcase_pack) == 500:
            testcase_pack_list.append(testcase_pack)
            #print("Added a batch to the list, total packs in the list ", len(testcase_pack_list))
            testcase_pack = []
        # every loop we will check the queue first in case workers are in short of inputs
        if len(input_queue) < max_queue_size:
            while len(testcase_pack_list) > 0:
                new_testcase_pack = testcase_pack_list.pop()
                job = input_queue.enqueue(concurrent_executor, args = (new_testcase_pack, "comm", False), result_ttl=0, job_timeout=366000)
                if len(input_queue) == max_queue_size:
                    break
        # Now we are back to the work
        contents = communication.strip().split(' ')
        num_comm_checked += 1
        assert(len(contents) == 10)
        write_ins = int(contents[0], 16)
        read_ins = int(contents[4], 16)
        write_addr = int(contents[1], 16)
        read_addr = int(contents[5], 16)
        write_value = int(contents[2], 16)
        read_value = int(contents[6], 16)
        write_byte = int(contents[3])
        read_byte = int(contents[7])
        double_read = int(contents[8])
        comm = tuple([write_addr, read_addr, write_ins, read_ins, write_value, read_value, write_byte, read_byte])
        if comm in communication_set:
            communication = communication_list.readline()
            continue
        communication_set.add(comm)
        write_id_list = list(mem_dict[write_addr][0][access_index_to_byte[write_byte]][write_ins][write_value][1])
        read_id_list = list(mem_dict[read_addr][1][access_index_to_byte[read_byte]][read_ins][read_value][double_read][1])
        assert(len(write_id_list) != 0 and len(read_id_list) != 0)
        write_id = random.choice(write_id_list)
        read_id = random.choice(read_id_list)
        testcase = [write_addr, read_addr, write_ins, read_ins, write_value, read_value, write_byte, read_byte, double_read, write_id, read_id]
        num_comm_found += 1
        time_now = datetime.now()
        found_timestamp = time_now.strftime("%Y-%m-%d-%H-%M-%S")
        print(communication.strip(), write_id, read_id, num_comm_found, num_comm_checked, found_timestamp, file=result_file)
        result_file.flush()
        testcase_pack.append(testcase)
        #print("Checked ", num_comm_checked, " and found ", num_comm_found, "communications")
        if testing_mode:
            if num_comm_found == 2000:
                break
        communication = communication_list.readline()
    result_file.close()
    communication_list.close()
    time_end = time.time()
    print('time cost',time_end-time_start, 's')
    # enqueue the last pack anyway in case its length is < 500
    if len(testcase_pack) != 0:
        testcase_pack_list.append(testcase_pack)
    # The process will wait here for here to add testcase into the queue
    while 1:
        if len(input_queue) < max_queue_size:
            if len(testcase_pack_list) == 0:
                    print("[comm] No packs available, return")
                    return
            while len(testcase_pack_list) > 0:
                new_testcase_pack = testcase_pack_list.pop()
                print("[comm] List reading finished, but added 1 pack into the queue")
                job = input_queue.enqueue(concurrent_executor, args = (new_testcase_pack, "comm", False), result_ttl=0, job_timeout=366000)
                if len(input_queue) == max_queue_size:
                    break

def random_pairing(communication_list_filename, log_path, max_queue_size):
    time_now = datetime.now()
    timestamp = time_now.strftime("%Y-%m-%d-%H-%M-%S")
    result_filename = log_path + "/random-" + timestamp + ".txt"
    result_file = open(result_filename, 'w')
    total = 10000000
    testcase_generated = 0
    testcase_pack_list = []
    testcase_pack = []
    id_list = []
    redis_conn = Redis(host = redis_host, port = redis_port, password=redis_passwd)
    input_queue = Queue('random', connection = redis_conn)
    for index in range(2, 129876):
        id_list.append(index)
    while (testcase_generated < total):
        write_id = random.choice(id_list)
        read_id = random.choice(id_list)
        testcase = [-1, -1, -1, -1, -1, -1, -1, -1, -1, write_id, read_id]
        if len(testcase_pack) == 500:
            testcase_pack_list.append(testcase_pack)
            testcase_pack = []
        testcase_pack.append(testcase)
        testcase_generated += 1
        time_now = datetime.now()
        found_timestamp = time_now.strftime("%Y-%m-%d-%H-%M-%S")
        print(write_id, read_id, testcase_generated, found_timestamp, file=result_file)
    result_file.close()
    while 1:
        if len(input_queue) < max_queue_size:
            if len(testcase_pack_list) == 0:
                    print("[random] No batches are available, return")
                    return
            while len(testcase_pack_list) > 0:
                new_testcase_pack = testcase_pack_list.pop()
                print("[random] List reading finished, but added 1 pack into the queue")
                job = input_queue.enqueue(concurrent_executor, args = (new_testcase_pack, "random", False), result_ttl=0, job_timeout=366000)
                if len(input_queue) == max_queue_size:
                    break

def duplicate_pairing(communication_list_filename, log_path, max_queue_size):
    time_now = datetime.now()
    timestamp = time_now.strftime("%Y-%m-%d-%H-%M-%S")
    result_filename = log_path + "/duplicate-" + timestamp + ".txt"
    result_file = open(result_filename, 'w')
    total = 10000000
    testcase_generated = 0
    testcase_pack_list = []
    testcase_pack = []
    id_list = []
    redis_conn = Redis(host = redis_host, port = redis_port, password=redis_passwd)
    input_queue = Queue('duplicate', connection = redis_conn)
    for index in range(2, 129876):
        id_list.append(index)
    while (testcase_generated < total):
        write_id = random.choice(id_list)
        read_id = write_id
        testcase = [-1, -1, -1, -1, -1, -1, -1, -1, -1, write_id, read_id]
        if len(testcase_pack) == 500:
            testcase_pack_list.append(testcase_pack)
            testcase_pack = []
        testcase_pack.append(testcase)
        testcase_generated += 1
        time_now = datetime.now()
        found_timestamp = time_now.strftime("%Y-%m-%d-%H-%M-%S")
        print(write_id, read_id, testcase_generated, found_timestamp, file=result_file)
    result_file.close()
    while 1:
        if len(input_queue) < max_queue_size:
            if len(testcase_pack_list) == 0:
                    print("[duplicate] No batches are available, return")
                    return
            while len(testcase_pack_list) > 0:
                new_testcase_pack = testcase_pack_list.pop()
                print("[duplicate] List reading finished, but added 1 pack into the queue")
                job = input_queue.enqueue(concurrent_executor, args = (new_testcase_pack, "duplicate", False), result_ttl=0, job_timeout=366000)
                if len(input_queue) == max_queue_size:
                    break


strategy_list = [
    channel_strategy, 
    double_read_channel_strategy, 
    object_null_channel_strategy, 
    unaligned_channel_strategy,
    ins_pair_strategy,
    ins_strategy, 
    mem_addr_strategy
    ]

prompt_msg = "The generator can generate concurrent inputs based on the following strategies:\n[1]: M-CH strategy\n[2]: M-CH-DOUBLE strategy\n[3]: M-CH-NULL strategy\n[4]: M-CH-UNALIGNED strategy\n[5]: M-INS-PAIR strategy\n[6]: M-INS strategy\n[7]: M-OBJ strategy"
print(prompt_msg)
strategy_choice_input = input("Please select strategy(s) by entering the strategy ID(s) and delimiting them by a space (e.g. 1 2 3)\n")
strategy_choice = list(map(int, strategy_choice_input.split()))

# Find mem-dict-file and pmc folder
findCMD = 'find ' + sys.argv[1] + ' -name "mem-dict-*"'
out = subprocess.Popen(findCMD, shell=True,stdin=subprocess.PIPE, 
                        stdout=subprocess.PIPE,stderr=subprocess.PIPE)
(stdout, stderr) = out.communicate()
mem_dict_filename = stdout.decode().strip()

findCMD = 'find ' + sys.argv[1] + ' -name "PMC-*" -type d'
out = subprocess.Popen(findCMD, shell=True,stdin=subprocess.PIPE, 
                        stdout=subprocess.PIPE,stderr=subprocess.PIPE)
(stdout, stderr) = out.communicate()
pmc_dir = stdout.decode().strip()
print(mem_dict_filename, pmc_dir)
mem_dict_file = open(mem_dict_filename, 'rb')
print("Loading python dictionary into memory")
mem_dict = pickle.load(mem_dict_file)
ins_pair_to_channel = ins_pair_2_channel(pmc_dir)
write_ins_to_channel, read_ins_to_channel = ins_2_channel(pmc_dir)
time_now = datetime.now()
timestamp = time_now.strftime("%Y-%m-%d-%H-%M-%S")
snowboard_storage = os.environ.get('SNOWBOARD_STORAGE')
if snowboard_storage is None:
    print("[Error] Please source scripts/setup.sh $FOLDER_TO_STORE_DATA first")
    exit(1)
output_dir = snowboard_storage + '/generator-log-' + timestamp + '/'
if not os.path.isdir(output_dir):
    os.makedirs(output_dir)
process_pool = Pool(len(strategy_choice))
result_list = []
for index in strategy_choice:
    result = process_pool.apply_async(strategy_list[index-1], args=(pmc_dir, output_dir, 10))
    result_list.append(result)
for result in result_list:
    result.get()
process_pool.close()
process_pool.join()
