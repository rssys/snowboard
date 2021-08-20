#!/bin/bash

error_msg(){
        echo [CONCURRENT-INPUT.SH] ERROR: $1
        echo [CONCURRENT-INPUT.SH] ERROR: exiting!
        exit 1
}

log_msg(){
        echo [CONCURRENT-INPUT.SH] $1
}
## Default directories values: update or override them ##
SKI_TMP=${SKI_TMP-"/dev/shm/ski/"}
SKI_OUTPUT_DIR=$SNOWBOARD_STORAGE
# create dir if not exists

SKI_OUTPUT_DIR="$SKI_OUTPUT_DIR/concurrent-test-`date +'%Y-%m-%d-%H-%M-%S'`"
if ! [ -d "$SKI_OUTPUT_DIR" ]
then
        echo "dir not exists, creating $SKI_OUTPUT_DIR"
        mkdir $SKI_OUTPUT_DIR
fi
export SKI_OUTPUT_DIR
log_file="$SKI_OUTPUT_DIR/run-ski-`date +'%Y-%m-%d-%H-%M-%S'`.txt"
echo $log_file
#exec &> >(tee "$log_file")
echo "This will be logged to the file and to the screen"

VMM_BINARY=$SB_INSTALL_DIR/bin/qemu-system-i386
SKI_VM_FILENAME=$SNAPSHOT
VMM_RAM_MB=2048
VMM_CPUS=4
VMM_BOOTLOADER_LINUX_APPEND="root=/dev/sda1 rw -verbose console=tty0 console=ttyS0"
VMM_MISC_OPTIONS="-rtc base=utc,clock=vm -qmp tcp:localhost:10003,server,nowait -net nic -net user,hostfwd=tcp::10004-:22 -vnc :1,password -monitor telnet:127.0.0.1:1235,server,nowait"
VMM_SKI_ARGS="-ski 0,input_number=210:210,preemptions=119:97"

# some verification
if ! [ -x $VMM_BINARY ] ; then error_msg "Unable to find the binary at $VMM_BINARY. Make sure that SKI_DIR is correctly defined."; fi
if [ -z "$SKI_VM_FILENAME" ] || ! [ -f "$SKI_VM_FILENAME" ] ; then  error_msg "Need to set SKI_VM_FILENAME to a valid snapshot"; fi
if ! [ -d "$SKI_OUTPUT_DIR" ] ; then  mkdir $SKI_OUTPUT_DIR || error_msg "Need create the output directory (SKI_OUTPUT_DIR=$SKI_OUTPUT_DIR)."; fi

export
log_msg "Sleeping for a few seconds..."
sleep 3


# TODO: Misc: Ensure that this is sufficient to get the coredumps,
# Enable core dumps
ulimit -c unlimited
ulimit -a
# Note that coredumps can be extremely large, specially if not filtered because of the large address space
# echo 21 > /proc/self/coredump_filter
# Running concurrent test by resuming from a snapshot

VMM_SNAPSHOT=ski-vm-XXX
VMM_HDA_FILENAME=$SKI_TMP/tmp.$$.img
VMM_SERIAL_FILENAME=file:$SKI_OUTPUT_DIR/console.txt

mkdir -p $SKI_TMP
rm /dev/shm/ski/*img
log_msg "Copying the VM image to tmp"
cp $SKI_VM_FILENAME $VMM_HDA_FILENAME || error_msg "Unable to copy the VM image to the temporary directory (SKI_TMP=$SKI_TMP)!"


export SKI_PREEMPTION_BY_EIP=1
export SKI_PREEMPTION_BY_ACCESS
export SKI_TEST_CPU_1_MODE=3
export SKI_TEST_CPU_2_MODE=3
export SKI_RACE_DETECTOR_ENABLED=1
export SKI_TRACE_INSTRUCTIONS_ENABLED=0
export SKI_TRACE_MEMORY_ACCESSES_ENABLED=0
export SKI_TRACE_WRITE_SET_ENABLED=0
export SKI_TRACE_READ_SET_ENABLED=0
export SKI_TRACE_EXEC_SET_ENABLED=0
export SKI_TRACE_SET_CPU=0
# Other SKI parameters
export SKI_INTERLEAVING_RANGE=0-63
export SKI_RESCHEDULE_POINTS=1
export SKI_RESCHEDULE_K=1
export SKI_FORKALL_ENABLED=1
export SKI_WATCHDOG_SECONDS=15
export SKI_QUIT_HYPERCALL_THRESHOLD=1
export SKI_OUTPUT_DIR_PER_INPUT_ENABLED=1
export SKI_DEBUG_START_SLEEP_ENABLED=0
export SKI_DEBUG_CHILD_START_SLEEP_SECONDS=1
export SKI_DEBUG_CHILD_WAIT_START_SECONDS=0
export SKI_DEBUG_PARENT_EXECUTES_ENABLED=0
export SKI_DEBUG_EXIT_AFTER_HYPERCALL_ENABLED=0
export SKI_MEMFS_ENABLED=1
export SKI_MEMFS_TEST_MODE_ENABLED=0
export SKI_MEMFS_LOG_LEVEL=1
export SKI_PRIORITIES_FILENAME=$SB_INSTALL_DIR/../testcase.priorities
export SKI_KERNEL_FILENAME=$KERNEL_DIR/bzImage
export SKI_CORPUS_DIR=$SEQUENTIAL_CORPUS

log_msg "Running command"
#gdb -ex "set follow-fork-mode child" --args
$VMM_BINARY -m $VMM_RAM_MB -smp $VMM_CPUS -loadvm $VMM_SNAPSHOT -kernel $SKI_KERNEL_FILENAME -hda $VMM_HDA_FILENAME -serial $VMM_SERIAL_FILENAME $VMM_MISC_OPTIONS $VMM_SKI_ARGS


log_msg "Removing the VM image in tmp.."
rm $VMM_HDA_FILENAME
