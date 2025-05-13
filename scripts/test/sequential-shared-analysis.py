#!/usr/bin/python3
"""
Sequential Shared Memory Analysis Tool

This script analyzes CPU trace files (trace_<timestamp>.txt) to identify shared memory access 
patterns between multiple CPUs. It looks at the recorded reads and writes, and filters out
stack-related accesses (since these are by definition not shared). Also detects double reads
(when an address is read a second time briefly after a jump instruction). 

Input:
- trace.txt: 

Output:
- write.txt: records memory write operations. Format:
    "<instruction_ptr> <mem_address> <value> <length> <type: 0=write>"
- read.txt: records memory read operations with double read flags. Format:
    "<instruction_ptr> <mem_address> <value> <length> <type: 1=read> <double_read: bool{0,1}>"
- result.txt: summarizes shared memory accesses. Format:
    "write <shared_access> <total_access>\n read <shared_access> <double_read_num> <total_access>"

Usage:
  python sequential-shared-analysis.py [data_path] [seed_start] [seed_end]

Environment variables:
  KERNEL_DIR: directory containing the kernel disassembly file (vmlinux.onlydisas)
"""


import os
import sys
import multiprocessing
from multiprocessing import Process
from collections import defaultdict
import time
from datetime import datetime

MEMORY_ACCESS_LENGTH = 10
CPU_STATE_LENGTH     = 15
STACK_MASK           = 0xFFFFE000
STACK_SIZE           = 0x2000

class MemoryAccess:
    def __init__(self, contents):
        #based on: "### MEM: 0 8049610 8fd9318 2b 1 32 L 6b867318" format
        #contents = ["###", "MEM:", "0", "8049610", "8fd9318", "2b", "1", "32", "L", "6b867318"]
        # contents[0] = "###" (format prefix)
        # contents[1] = "MEM:" (format prefix)
        # contents[2] = "0" (CPU index)
        # contents[3] = "8049610" (instruction pointer)
        # contents[4] = "8fd9318" (memory address)
        # contents[5] = "2b" (value read/written)
        # contents[6] = "1" (memory index)
        # contents[7] = "32" (bit length: 8, 16, or 32)
        # contents[8] = "L" (access type: 'L' for load/read, 'S' for store/write)
        # contents[9] = "6b867318" (physical address)
        if len(contents) >= MEMORY_ACCESS_LENGTH:
            self.cpu_index          = int(contents[2])
            self.instruction_ptr    = int(contents[3], 16)
            self.mem_address        = int(contents[4], 16)
            self.value              = int(contents[5], 16)
            self.mem_index          = int(contents[6])
            self.length             = int(contents[7])
            self.access_type        =     contents[8]  #L is load, S is store
            self.phys_addr          = int(contents[9], 16) if len(contents) > 9 else 0
        else:
            #fallback to prevent crash
            self.cpu_index          = 0
            self.instruction_ptr    = 0
            self.mem_address        = 0
            self.value              = 0
            self.mem_index          = 0
            self.length             = 0
            self.access_type        = "U"  #U is unknown
            self.phys_addr          = 0
    
    def to_string(self):
        return f"{self.instruction_ptr:x} {self.mem_address:x} {self.value:x} {self.length} {1 if self.access_type == 'L' else 0}"

class CPUState:
    def __init__(self, contents):
        #based on format: "0 8049610 1020304 8172000 8fd9300 80 bffff780 0 8fd9300 8fc0c40 73 7b 7b f5d2000 ff801000"
        # contents[0] = "0" (CPU index)
        # contents[1] = "8049610" (EIP - instruction pointer)
        # contents[2] = "1020304" (EAX register)
        # contents[3] = "8172000" (EBX register)
        # contents[4] = "8fd9300" (ECX register)
        # contents[5] = "80" (EDX register)
        # contents[6] = "bffff780" (ESP register - stack pointer)
        # contents[7] = "0" (EBP register - base pointer)
        # contents[8] = "8fd9300" (ESI register)
        # contents[9] = "8fc0c40" (EDI register)
        # contents[10] = "73" (CS register - code segment)
        # contents[11] = "7b" (SS register - stack segment)
        # contents[12] = "7b" (DS register - data segment)
        # contents[13] = "f5d2000" (CR3 register - page directory base)
        # contents[14] = "ff801000" (GDT base)
        if len(contents) >= CPU_STATE_LENGTH:
            self.cpu_index  = int(contents[0])
            self.eip        =     contents[1]  #raw string for dictionary lookups
            self.eax        = int(contents[2], 16)
            self.ebx        = int(contents[3], 16)
            self.ecx        = int(contents[4], 16)
            self.edx        = int(contents[5], 16)
            self.esp        = int(contents[6], 16)
            self.ebp        = int(contents[7], 16)
            self.esi        = int(contents[8], 16)
            self.edi        = int(contents[9], 16)
            self.cs         = int(contents[10], 16)
            self.ss         = int(contents[11], 16)
            self.ds         = int(contents[12], 16)
            self.cr3        = int(contents[13], 16)
            self.gdt        = int(contents[14], 16)
        else:
            #fallback to prevent crash
            self.cpu_index  = 0
            self.eip        = 0
            self.eax        = 0
            self.ebx        = 0
            self.ecx        = 0
            self.edx        = 0
            self.esp        = 0
            self.ebp        = 0
            self.esi        = 0
            self.edi        = 0
            self.cs         = 0
            self.ss         = 0
            self.ds         = 0
            self.cr3        = 0
            self.gdt        = 0
    
    def get_stack_range(self):
        #use ESP register to calculate stack range 
        #(with 2 constants that should be defined as actual constant vars)
        stack_begin = self.esp & STACK_MASK
        stack_end = stack_begin + STACK_SIZE
        return stack_begin, stack_end

class DisassemblyEntry:
    def __init__(self, line):
        # Based on "c1000000:	mov    0x3da9c80,%ecx" format
        if len(line) > 9 and line[8] == ":":
            parts = line.split(":", 1)  # Split only on the first colon
            self.address = parts[0].strip()
            self.instruction = parts[1].strip()
        else:
            self.address = ""
            self.instruction = ""
    
    def is_jump_instruction(self):
        # Check if the instruction starts with 'j'
        return self.instruction and self.instruction[0] == 'j'

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

def open_output_files(seed, result_path):
    if not os.path.isdir(result_path + '/' + str(seed)):
        os.makedirs(result_path + '/' + str(seed))
    write_access_filename = result_path + '/' + str(seed) + '/write.txt'
    read_access_filename = result_path + '/' + str(seed) + '/read.txt'
    result_filename = result_path + '/' + str(seed) + '/result.txt'
    write_access_file = open(write_access_filename, 'w')
    read_access_file = open(read_access_filename, 'w')
    result_file = open(result_filename, 'w')
    
    return write_access_file, read_access_file, result_file

def get_trace_file_path(seed, test_folder, cpu, data_path):
    if cpu == 0:
        current_path = data_path + '/cpu0/' + test_folder + '/'
        target_folder = current_path + str(seed) + '_1/'
    else:  # cpu == 1
        current_path = data_path + '/cpu1/' + test_folder + '/'
        target_folder = current_path + '1_' + str(seed) + '/'
    
    if not os.path.isdir(target_folder):
        return None, None
    
    try:
        exec_filename = getFileName(target_folder, 'trace', 'txt')
        print(seed, "Found trace file", target_folder+exec_filename)
        return target_folder, exec_filename
    except:
        print("This test input is corrupted ", current_path)
        return None, None

def process_write_access(seed, test_list, data_path, write_access_file, result_file):
    process_memory_access(
        seed=seed, 
        test_list=test_list[0], 
        data_path=data_path, 
        access_file=write_access_file, 
        result_file=result_file, 
        cpu=0, 
        access_type='S'
    )

def process_read_access(seed, test_list, data_path, read_access_file, result_file):
    process_memory_access(
        seed=seed, 
        test_list=test_list[1], 
        data_path=data_path, 
        access_file=read_access_file, 
        result_file=result_file, 
        cpu=1, 
        access_type='L'
    )

def process_memory_access(seed, test_list, data_path, access_file, result_file, cpu, access_type):
    """
    process memory access events from trace files for a specific CPU and access type.
    
    args:
        seed (int): the seed value identifying the test case
        test_list (list): list of test folders to process
        data_path (str): base path to the test data
        access_file (file): open file to write memory access information
        result_file (file): open file to write summary results
        cpu (int): CPU identifier (0 (for write) or 1 (for read))
        access_type (str): type of memory access to process ('S' for store/write, 'L' for load/read)
    
    returns:
        none: results are written to provided files
    """
    
    total_access = 0
    shared_access = 0
    double_read_num = 0
    
    for test_folder in test_list:
        target_folder, exec_filename = get_trace_file_path(seed, test_folder, cpu, data_path)
        if not target_folder:
            continue
        current_cr3 = 0
        cr3 = 0
        current_path = target_folder
        exec_file = open(current_path + '/' + exec_filename, 'r')
        exec_content = exec_file.readlines()
        mem_access_pattern = '# MEM: ' + str(cpu) + ' c'
        ins_pattern = str(cpu) + ' c'
        stack_begin = 0
        stack_end = 0
        for line_index in range(0, len(exec_content)):
            line = exec_content[line_index]
            if line.find(mem_access_pattern) == -1:
                contents = line.strip().split(' ')
                if not line.startswith(ins_pattern):
                    continue

                if len(contents) != CPU_STATE_LENGTH:
                    continue

                cpu_state = CPUState(contents)
                current_cr3 = cpu_state.cr3
                if cr3 == 0:
                    cr3 = current_cr3
                stack_begin, stack_end = cpu_state.get_stack_range()
                continue
            
            if current_cr3 != cr3:
                # cr3: skipping since memory access is not from this process
                # this is a memory access emit from the target CPU
                continue

            contents = line.strip().split(' ')
            if len(contents) != MEMORY_ACCESS_LENGTH:
                # if the length is not 10, then something is wrong
                # but for now we ignore the error
                continue

            mem_access = MemoryAccess(contents)
            if mem_access.access_type != access_type:
                continue

            total_access += 1
            if mem_access.mem_address >= stack_begin and mem_access.mem_address < stack_end:
                continue

            shared_access += 1
            if access_type == 'S':  # write access
                type = 0
                print(f"{mem_access.instruction_ptr:x} {mem_access.mem_address:x} {mem_access.value:x} {mem_access.length} {type}", file=access_file)
            else:  # read access
                double_read = check_double_read(line_index, exec_content, mem_access, 
                                                    cr3, stack_begin, stack_end, mem_access_pattern, 
                                                    ins_pattern)
                if double_read:
                    double_read_num += 1  
                print(f"{mem_access.instruction_ptr:x} {mem_access.mem_address:x} {mem_access.value:x} {mem_access.length} 1 {double_read}", file=access_file)
        exec_file.close()
    
    if access_type == 'S':
        print("write", shared_access, total_access, file=result_file)
    else:
        print("read", shared_access, double_read_num, total_access, file=result_file)

def check_double_read(line_index, exec_content, mem_access, cr3, stack_begin, stack_end, mem_access_pattern, ins_pattern):
    """
    detect if a memory read operation is followed by another read to the same address after a jump instruction
    
    this function examines a window of trace lines after the current memory read to determine if 
    there's a pattern indicating a "double read" scenario (where the same memory location is read 
    again after a jump instruction)
    
    args:
        line_index (int): current index in the execution trace
        exec_content (list): list of all trace file lines
        mem_access (MemoryAccess): current memory access being checked
        cr3 (int): current page directory base register value
        stack_begin (int): start address of current stack
        stack_end (int): end address of current stack
        mem_access_pattern (str): pattern to identify memory access lines
        ins_pattern (str): pattern to identify instruction lines
        
    returns:
        int: 1 if a double read pattern is detected, 0 otherwise
    """
    max_double_read_distance = 10
    jump_found = 0
    compare_index_end = line_index + 1 + max_double_read_distance
    compare_current_cr3 = cr3
    if compare_index_end > len(exec_content):
        compare_index_end = len(exec_content)
    
    for compare_index in range(line_index + 1, compare_index_end):
        compare_line = exec_content[compare_index]

        if compare_line.find(mem_access_pattern) == -1:
            if not compare_line.startswith(ins_pattern):
                continue

            compare_contents = compare_line.strip().split(' ')
            if len(compare_contents) != CPU_STATE_LENGTH:
                continue

            cpu_state = CPUState(compare_contents)
            compare_current_cr3 = cpu_state.cr3
            compare_possible_ins = cpu_state.eip

            if len(disas_ins_dict[compare_possible_ins]) == 0:
                ## very strange c10d66b2 ins on 2_1, check it later
                continue

            if disas_ins_dict[compare_possible_ins][0] == 'j':
                jump_found = 1
            continue

        if compare_current_cr3 != cr3:
            # cr3: skipping since memory access is not from this process
            continue
        
        compare_contents = compare_line.strip().split(' ')
        if len(compare_contents) != MEMORY_ACCESS_LENGTH:
            continue

        compare_mem_access = MemoryAccess(compare_contents)
        if compare_mem_access.mem_address != mem_access.mem_address:
            continue
        
        if (jump_found 
            and compare_mem_access.instruction_ptr != mem_access.instruction_ptr 
            and compare_mem_access.value == mem_access.value 
            and compare_mem_access.length == mem_access.length):
            return 1 #found a double read
                
    return 0 #found no double read

def filter_access(seed, data_path, test_list, result_path, logging=True):
    write_access_file, read_access_file, result_file = open_output_files(seed, result_path)

    process_write_access(seed, test_list, data_path, write_access_file, result_file)

    process_read_access(seed, test_list, data_path, read_access_file, result_file)

    write_access_file.close()
    read_access_file.close()
    result_file.close()
    
    print("Finished analyzing seed ", seed, " result is saved to ", result_path)

def get_test_lists(data_path):
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
    
    return test_list

def load_disassembly_data():
    kernel_dir = os.environ.get('KERNEL_DIR')
    if kernel_dir is None:
        print("error happens when accessing environment vairable KERNEL_DIR")
        exit(1)
    linux_disas_filename = kernel_dir + "/vmlinux.onlydisas"
    ip_disas = defaultdict(str)
    linux_disas_file = open(linux_disas_filename, 'r')
    for line in linux_disas_file:
        disas_entry = DisassemblyEntry(line)
        if disas_entry.address != "":
            ip_disas[disas_entry.address] = disas_entry.instruction
            # print(disas_entry.address, disas_entry.instruction)
        
    return ip_disas

disas_ins_dict = load_disassembly_data()

if __name__ == '__main__':
    data_path = sys.argv[1]
    seed_start = int(sys.argv[2])
    seed_end = int(sys.argv[3])

    test_list = get_test_lists(data_path)
    
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
