#!/usr/bin/python3
import os
import sys
import multiprocessing
from multiprocessing import Process
from collections import defaultdict
import re
import subprocess
import json

kernel_dir = os.environ.get('KERNEL_DIR')
linux_map = kernel_dir + "/vmlinux.map"

def locate_source_code(full_path):
    loc = full_path.find("linux")
    linux_relative_path = full_path[loc:]
    linux_source = kernel_dir + "/source/" + linux_relative_path[linux_relative_path.find("/"):] 
    return linux_source

def ins_2_code(point, map_ip_source, ip_list, output=None):
    write_ins = point[0][2:]
    read_ins = point[1][2:]
    if output is None:
        print("### Instruction pair ###")
    else:
        print("### Instruction pair ", '[' + write_ins+', ' + read_ins + ']', "###", file=output)
    context = '10'
    #print(map_ip_source[write_ins])
    # Print the source code of the write instruction
    file_location = map_ip_source[write_ins].split()[3]
    previous_ip = write_ins
    if map_ip_source[write_ins].find("_once_size") != -1:
        previous_ip = last_ip(ip_list, write_ins)
        file_location = map_ip_source[previous_ip].split()[3]
    print(file_location)
    file_lineno = locate_source_code(file_location)
    #file_lineno = kernel_source_directory + file_location[5:]
    file_field = file_lineno.split(":")[0]
    lineno_field = file_lineno.split(":")[1]
    cmd_str = """cat -n """ + file_field + """ |  awk "(FNR==""" +lineno_field+"""){print \\"==>\\", \$0, \\"      \\"}((FNR>=""" + lineno_field + '-' + context + """) && (FNR<=""" + lineno_field + '+' + context + """) && (FNR != """ + lineno_field + """)){print \\\"   \\\" , \$0} " """
    #print(" command: ", cmd_str)
    p = subprocess.Popen(cmd_str, shell=True,  stdout=subprocess.PIPE)
    #buff += file_lineno.strip() + "\n"
    if output is None:
        print('write ',write_ins, file_location)
        print(p.communicate()[0].decode())
    else:
        if previous_ip == write_ins:
            print("Write instruction",write_ins, file_location, file=output)
        else:
            print("Write instruction", write_ins+'->'+previous_ip, file_location, file=output)
        print(p.communicate()[0].decode(), file=output)
    # Print the source code of the read instruction
    file_location = map_ip_source[read_ins].split()[3]
    previous_ip = read_ins
    if map_ip_source[read_ins].find("_once_size") != -1:
        previous_ip = last_ip(ip_list, read_ins)
        file_location = map_ip_source[previous_ip].split()[3]
    file_lineno = locate_source_code(file_location)
    ##file_lineno = kernel_source_directory + file_location[5:]
    file_field = file_lineno.split(":")[0]
    lineno_field = file_lineno.split(":")[1]
    cmd_str = """cat -n """ + file_field + """ |  awk "(FNR==""" +lineno_field+"""){print \\"==>\\", \$0, \\"      \\"}((FNR>=""" + lineno_field + '-' + context + """) && (FNR<=""" + lineno_field + '+' + context + """) && (FNR != """ + lineno_field + """)){print \\\"   \\\" , \$0} " """
    #print(" command: ", cmd_str)
    p = subprocess.Popen(cmd_str, shell=True,  stdout=subprocess.PIPE)
    #buff += file_lineno.strip() + "\n"
    if output is None:
        print(read_ins, file_location)
        print(p.communicate()[0].decode())
    else:
        if read_ins == previous_ip:
            print("Read instruction", read_ins, file_location, file=output)
        else:
            print("Read instruction", read_ins+'->'+previous_ip, file_location, file=output)
        print(p.communicate()[0].decode(), file=output)


if __name__ == '__main__':

    ins_pair_file = open(sys.argv[1], 'r')
    result_file = open(sys.argv[1] + ".source", 'w')

    fmap_ip_source = open(linux_map, 'r')
    map_ip_source={}
    ip_list = []
    for line in fmap_ip_source:
        a = line.split(":")
        a[0] = a[0][2:len(a[0])]
        #print(a[0])
        map_ip_source[a[0]] = line.strip()
        ip_list.append(a[0])

    for entry in ins_pair_file:
        contents = entry.split(' ')
        write_ins = contents[0]
        read_ins = contents[1]
        ins_2_code([write_ins, read_ins], map_ip_source, ip_list, result_file)
        print("\n\n\n", file=result_file)
