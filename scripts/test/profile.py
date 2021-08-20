#!/usr/bin/python3
import os
import sys
from datetime import datetime
import multiprocessing
'''
This script runs the profiling execution for seuqential inputs whose id is within range start ~ end
data_path sys.argv[1] type:string
folder path where the profiling data will be stored
start_id sys.argv[2] type:int
starting sequential input test id
end_id sys.argv[3] type:int
ending sequential input test id
'''
start_id = int(sys.argv[2])
end_id = int(sys.argv[3])
input_range =[start_id, end_id]
# create a timestamp
time_now = datetime.now()
timestamp = time_now.strftime("%Y-%m-%d-%H-%M-%S")

result_path = sys.argv[1] + "/profile-" + timestamp + '/'
if not os.path.isdir(result_path):
    os.makedirs(result_path)
# Each sequential test will be profiled twice running on different CPUs
for cpu in range(0, 2):
    current_path = result_path + '/cpu'+str(cpu)+ '/'
    if not os.path.isdir(current_path):
        os.makedirs(current_path)
    for input_index in range(input_range[0], input_range[1], 5000):
        cmd = ""
        if cpu == 0:
            cmd += "SKI_INPUT1_RANGE=" + str(input_index)+'-' + str(min(input_index+4999, input_range[1])) + ' '
            cmd += "SKI_INPUT2_RANGE=1-1 "
            cmd += "SKI_TRACE_SET_CPU=0 "
        elif cpu == 1:
            cmd += "SKI_INPUT2_RANGE=" + str(input_index)+'-' + str(min(input_index+4999, input_range[1])) + ' '
            cmd += "SKI_INPUT1_RANGE=1-1 "
            cmd += "SKI_TRACE_SET_CPU=1 "
        cmd += 'SKI_OUTPUT_DIR='+current_path + ' '
        cmd += 'SKI_FORKALL_CONCURRENCY='+str(multiprocessing.cpu_count())
        cmd += ' ./profile.sh'
        print(cmd)
        ret = os.system(cmd)
        ret >>= 8
        if ret != 0:
            exit(1)
