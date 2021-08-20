#!/usr/bin/python

import os
import sys
import subprocess

f=sys.stdin
kernel_source_directory = ""

if os.getenv("KERNEL_DIR") != None:
    kernel_source_directory = os.getenv("KERNEL_DIR")

def locate_source_code(full_path):
    loc = full_path.find("linux")
    linux_relative_path = full_path[loc:]
    linux_source = kernel_source_directory + "/source/" + linux_relative_path[linux_relative_path.find("/"):]
    return linux_source


## Usage ./script.py <context_lines>
## Recives input in the stdin and produces ouput to the stdout

buff=""
context=sys.argv[1]

for line in f:
    if "=====================" in line:
        print buff
        print line,
        buff=""
    elif "/" in line:
        print line,
        file_location = line.split()[4]
        file_lineno = locate_source_code(file_location)
        #file_lineno = kernel_source_directory + file_location
        #print(file_lineno, file_location)
        file_field = file_lineno.split(":")[0]
        lineno_field = file_lineno.split(":")[1]
        cmd_str = """cat -n """ + file_field + """ |  awk "(FNR==""" +lineno_field+"""){print \\"==>\\", \$0, \\"      \\"}((FNR>=""" + lineno_field + '-' + context + """) && (FNR<=""" + lineno_field + '+' + context + """) && (FNR != """ + lineno_field + """)){print \\\"   \\\" , \$0} " """
        #print cmd_str
        #print " command: " + cmd_str
        #exit(1)
        p = subprocess.Popen(cmd_str, shell=True,  stdout=subprocess.PIPE)
        buff += file_lineno.strip() + "\n"
        buff += p.communicate()[0] + "\n"
        #print ""
    else:
        print line,

print buff
