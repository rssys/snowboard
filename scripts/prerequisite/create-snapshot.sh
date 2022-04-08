#!/bin/bash


##################################################################################
############                     CREATE SNAPSHOT                       ###########
##################################################################################

## Default directories values: update or override them ##

error_msg(){
	echo "[CREATE-SNAPSHOT.SH] ERROR: $1"
	echo "[CREATE-SNAPSHOT.SH] ERROR: Exiting!!"
	exit 1
}

log_msg(){
	echo "[CREATE-SNAPSHOT.SH] $1"
}

SKI_TMP=${SKI_TMP-"/dev/shm/ski/"}
SKI_OUTPUT_DIR=$SNOWBOARD_STORAGE
SKI_OUTPUT_DIR="$SKI_OUTPUT_DIR/snapshot-`date +'%Y-%m-%d-%H-%M-%S'`"
# create dir if not exists
echo "SKI_OUTPUT_DIR = $SKI_OUTPUT_DIR"
if ! [ -d "$SKI_OUTPUT_DIR" ]
then
	echo "directory not exists, creating $SKI_OUTPUT_DIR"
	mkdir $SKI_OUTPUT_DIR
fi

SKI_OUTPUT_DIR="$SKI_OUTPUT_DIR/snapshot-`date +'%Y-%m-%d-%H-%M-%S'`"
if ! [ -d "$SKI_OUTPUT_DIR" ]
then
        echo "dir not exists, creating $SKI_OUTPUT_DIR"
        mkdir $SKI_OUTPUT_DIR
fi
export SKI_OUTPUT_DIR
log_file="$SKI_OUTPUT_DIR/snapshot-`date +'%Y-%m-%d-%H-%M-%S'`.txt"
echo $log_file
exec &> >(tee "$log_file")
echo "This will be logged to the file and to the screen"

SKI_VM_FILENAME=$BASEIMAGE
# Used by the load-testsuite.sh to ssh into the client
export SKI_VMM_SSH_LOCAL_PORT=20001
VMM_BINARY=$SB_INSTALL_DIR/bin/qemu-system-i386
VMM_RAM_MB=2048
VMM_CPUS=4
VMM_BOOTLOADER_LINUX_APPEND="root=/dev/sda1 rw -verbose console=tty0 console=ttyS0"
VMM_MISC_OPTIONS="-rtc base=utc,clock=vm -qmp tcp:localhost:20000,server,nowait -net nic -net user,hostfwd=tcp::$SKI_VMM_SSH_LOCAL_PORT-:22 -vnc :1600,password"
VMM_SKI_ARGS="-ski 0,input_number=210:210,preemptions=119:97"

LOADTESTSUITE_SCRIPT=$MAIN_HOME/scripts/prerequisite/load-testsuite.sh
RUN_SCRIPT=ski-testcase-run.sh
PACK_SCRIPT=ski-testcase-pack.sh

# point to testcase dir
# sishuai snowboard: fix this
export SKI_TESTCASE_DIR=$MAIN_HOME/testsuite/executor/gopath/src/github.com/google/syzkaller/
#export SKI_TESTCASE_DIR=$SKI_DIR/testcases/$TESTCASE/gopath/src/github.com/google/syzkaller/

TESTCASE_PACKAGE_BASEDIR="`basename $SKI_TESTCASE_DIR`"
TESTCASE_PACKAGE_FILENAME=$TESTCASE_PACKAGE_BASEDIR.tgz
TESTCASE_PACKAGE_RESULT_FILENAME=${TESTCASE_PACKAGE_BASEDIR}_result.tgz

if ! [ -x $VMM_BINARY ] ; then error_msg "Unable to find the binary at $VMM_BINARY. Make sure that SKI_DIR is correctly defined (SKI_DIR=$SKI_DIR)."; fi
if [ -z "$SKI_VM_FILENAME" ] || ! [ -f "$SKI_VM_FILENAME" ] ; then  error_msg "Need to set SKI_VM_FILENAME to a valid VM image (SKI_VM_FILENAME=$SKI_VM_FILENAME)"; fi
if ! [ -d "$SKI_TESTCASE_DIR" ] ; then error_msg "Unable to read the testcase directory (SKI_TESTCASE_DIR = $SKI_TESTCASE_DIR)."; fi
if ! [ -x "$SKI_TESTCASE_DIR/$RUN_SCRIPT" ] ; then error_msg "Unable to find the executable run script in the testcase directory ($SKI_TESTCASE_DIR/$RUN_SCRIPT)."; fi
if ! [ -x "$SKI_TESTCASE_DIR/$PACK_SCRIPT" ] ; then error_msg "Unable to find the executable pack script in the testcase directory ($SKI_TESTCASE_DIR/$PACK_SCRIPT)."; fi
if [ -z $TESTCASE_PACKAGE_BASEDIR ] || [[ "$TESTCASE_PACKAGE_BASEDIR" == *"/"* ]] ; then error_msg "Invalid package filename  ($TESTCASE_PACKAGE_BASEDIR)."; fi
if ! [ -x $LOADTESTSUITE_SCRIPT ] ; then error_msg "Unable to find load-testsuite.sh ($LOADTESTSUITE_SCRIPT)" ; fi

log_msg "Running the testcase packing script ($SKI_TESTCASE_DIR/$PACK_SCRIPT)..." > /dev/null
pushd $SKI_TESTCASE_DIR/ > /dev/null
./$PACK_SCRIPT
PACK_RESULT=$?
if [ $PACK_RESULT -ne 0 ]; then error_msg "Unable to sucessfully run the pack script ($SKI_TESTCASE_DIR/$PACK_SCRIPT)"; fi
popd > /dev/null

# make testcase into a compress
log_msg "Creating the testcase package..."
pushd $SKI_TESTCASE_DIR/.. > /dev/null
if [ -f $TESTCASE_PACKAGE_FILENAME ] ; then rm $TESTCASE_PACKAGE_FILENAME; log_msg "Deleted the existing testcase package file";  fi
log_msg "Packing directory $TESTCASE_PACKAGE_BASEDIR into archive $TESTCASE_PACKAGE_FILENAME..."
tar --totals  -czf $TESTCASE_PACKAGE_FILENAME $TESTCASE_PACKAGE_BASEDIR/bin/linux_386/ski-executor $TESTCASE_PACKAGE_BASEDIR/executor/empty/empty $TESTCASE_PACKAGE_BASEDIR/ski-testcase-run.sh $TESTCASE_PACKAGE_BASEDIR/setup.sh $TESTCASE_PACKAGE_BASEDIR/misc > /dev/null

TAR_RESULT=$?
if [ $TAR_RESULT -ne 0 ]; then error_msg "Unable to create the testcase package tar file"; fi
chmod 777 $TESTCASE_PACKAGE_FILENAME
ls -l $TESTCASE_PACKAGE_FILENAME
TESTCASE_PACKAGE_PATH=`pwd`/$TESTCASE_PACKAGE_FILENAME
popd > /dev/null
log_msg " -> Created the testcase package: $TESTCASE_PACKAGE_PATH "

log_msg "Checking if there are other QEMU instances running..."
ps -All -f | grep qemu-system-i386
log_msg "Killing other instances of SKI..."
killall -9 qemu-system-i386
log_msg "Sleeping for a few seconds..."
sleep 3


# TODO: Misc: Ensure that this is sufficient to get the coredumps,
# Enable core dumps
ulimit -c unlimited
ulimit -a
# Note that coredumps can be extremely large, specially if not filtered because of the large address space

VMM_HDA_FILENAME=$SKI_TMP/tmp.$$.img
VMM_SERIAL_FILENAME=file:$SKI_OUTPUT_DIR/console.txt

# copy the image to a tmp folder
mkdir -p $SKI_TMP
log_msg "Copying the VM image to tmp"
cp $SKI_VM_FILENAME $VMM_HDA_FILENAME || error_msg "Unable to copy the VM image to the temporary directory (SKI_TMP=$SKI_TMP)!"


export SKI_TRACE_INSTRUCTIONS_ENABLED=1
export SKI_TRACE_MEMORY_ACCESSES_ENABLED=1

# Other SKI parameters
export SKI_INPUT1_RANGE=1-1
export SKI_INPUT2_RANGE=1-1
export SKI_INTERLEAVING_RANGE=1-1
export SKI_TEST_CPU_1_MODE=3
export SKI_TEST_CPU_2_MODE=3
export SKI_FORKALL_CONCURRENCY=1
export SKI_RACE_DETECTOR_ENABLED=0
export SKI_RESCHEDULE_POINTS=1
export SKI_RESCHEDULE_K=1
export SKI_FORKALL_ENABLED=0
export SKI_WATCHDOG_SECONDS=300
export SKI_QUIT_HYPERCALL_THRESHOLD=1
export SKI_OUTPUT_DIR_PER_INPUT_ENABLED=1
export SKI_DEBUG_START_SLEEP_ENABLED=0
export SKI_DEBUG_CHILD_START_SLEEP_SECONDS=1
export SKI_DEBUG_CHILD_WAIT_START_SECONDS=0
export SKI_DEBUG_PARENT_EXECUTES_ENABLED=0
export SKI_DEBUG_EXIT_AFTER_HYPERCALL_ENABLED=0
export SKI_MEMFS_ENABLED=0
export SKI_MEMFS_TEST_MODE_ENABLED=0
export SKI_MEMFS_LOG_LEVEL=1
export SKI_PRIORITIES_FILENAME=$SB_INSTALL_DIR/../testcase.priorities
export SKI_KERNEL_FILENAME=$KERNEL_DIR/bzImage

# XXX: This applies for linux case; for other OSs QEMU is not so convenient so need to modify the VM image to change kernel options

log_msg "Running SKI process in the background"
#./qemu-system-i386 -m "521" -smp "4" -kernel "/media/psf/Workspace/Project/Snowboard/ski/config" -append "root=/dev/hda1 rootfstype=ext4 rw -verbose console=tty0 console=ttyS0" &
#-hda vm.img -serial "file:/media/psf/Workspace/Project/Snowboard/ski/output/console.txt" -rtc base=utc,clock=vm -qmp tcp:localhost:10000,server,nowait -net nic -net user,hostfwd=tcp::$SKI_VMM_SSH_LOCAL_PORT-:22 -vnc :1
$VMM_BINARY -m "$VMM_RAM_MB" -smp "$VMM_CPUS" -kernel "$SKI_KERNEL_FILENAME" -append "$VMM_BOOTLOADER_LINUX_APPEND" -hda "$VMM_HDA_FILENAME" -serial "$VMM_SERIAL_FILENAME" $VMM_MISC_OPTIONS $VMM_SKI_ARGS &
# Save the QEMU PID for the testsuit loader
export SKI_VMM_PID=$!

# Lunch the testsuite loader
log_msg "Spawning in the background the testsuit loader..."
$LOADTESTSUITE_SCRIPT &

# Wait for QEMU to finish
log_msg "Waiting for QEMU to finish"
#sleep 3000
wait $SKI_VMM_PID

log_msg "Copying the final VM image..."
cp $VMM_HDA_FILENAME $SKI_OUTPUT_DIR/vm-image.img
log_msg "*********************************************************************************************************"
log_msg "** If message \"[SKI] Successfully wrote snapshot!!\" was displayed then snapshot was created successfuly"
log_msg "** and the VM image with the snapshot is stored in $SKI_OUTPUT_DIR/vm-image.img"
log_msg "*********************************************************************************************************"

# TODO:
log_msg "Removing the VM image in tmp.."
rm $VMM_HDA_FILENAME
