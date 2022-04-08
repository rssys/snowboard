#!/bin/bash

#set -x
#set -v

error_msg(){
    echo [LOAD-TESTSUITE.SH] ERROR: $1
    echo [LOAD-TESTSUITE.SH] ERROR: Exiting!!
    exit 1
}

log_msg(){
    echo [LOAD-TESTSUITE.SH] $1
}


RUN_SCRIPT=ski-testcase-run.sh
SSH_OPTIONS="-i $MAIN_HOME/scripts/prerequisite/id_dsa_vm -o StrictHostKeyChecking=no  -o UserKnownHostsFile=/dev/null"
SSH_DESTINATION="root@localhost"
VMM_PROC_DIR=/proc/$SKI_VMM_PID/

TESTCASE_PACKAGE_BASEDIR="`basename $SKI_TESTCASE_DIR`"
TESTCASE_PACKAGE_FILENAME=$TESTCASE_PACKAGE_BASEDIR.tgz


# Make some checks on the input env. variables
if [ -z "$SKI_VMM_SSH_LOCAL_PORT" ] ; then error_msg "VMM SSH local port is not set (SKI_VMM_SSH_LOCAL_PORT = $SKI_VMM_SSH_LOCAL_PORT). Make sure that load-testsuit.sh is launched by create-snapshot.sh."; fi
if [ -z "$SKI_VMM_PID" ] ; then error_msg "VMM PID is not set (SKI_VMM_PID = $SKI_VMM_PID). Make sure that load-testsuit.sh is launched by create-snapshot.sh."; fi
if ! [ -d "$VMM_PROC_DIR" ] ; then error_msg "Unable to read the proc directory for the VMM (SKI_VMM_PID = $SKI_VMM_PID). Make sure that QEMU is still running."; fi
if ! [ -d "$SKI_TESTCASE_DIR" ] ; then error_msg "Unable to read the testcase directory (SKI_TESTCASE_DIR = $SKI_TESTCASE_DIR)."; fi
if ! [ -d "$SKI_OUTPUT_DIR" ] ; then error_msg "Unable to read the SKI output directory (SKI_OUTPUT_DIR = $SKI_OUTPUT_DIR)."; fi
if ! [ -f "$MAIN_HOME/scripts/prerequisite/id_dsa_vm" ] ; then error_msg "Unable to read the SSH private key for the VM ($MAIN_HOME/scripts/prerequisite/id_dsa_vm)."; fi
if ! [ -d "$SKI_TESTCASE_DIR" ] ; then error_msg "Unable to read the testcase directory (SKI_TESTCASE_DIR = $SKI_TESTCASE_DIR)."; fi
if ! [ -f "$SKI_TESTCASE_DIR/../$TESTCASE_PACKAGE_FILENAME" ] ; then error_msg "Unable to read the testcase package ($SKI_TESTCASE_DIR/../$TESTCASE_PACKAGE_FILENAME)."; fi



MAX_SECONDS=20
SECONDS_WAITING=0
while true;
do
	# Find the QEMU process and the respective msg file (with the hypercall messages)
	log_msg "Finding the QEMU fd for the msg file... ($SECONDS_WAITING/$MAX_SECONDS)"
	VMM_MSG_FILENAME=$(find $VMM_PROC_DIR/fd/ -lname \*msg\*)
	VMM_CONSOLE_FILENAME=$(find $VMM_PROC_DIR/fd/ -lname \*console\* )
	if [ -r "$VMM_MSG_FILENAME" ] && [ -r $VMM_CONSOLE_FILENAME ] ;
	then
		log_msg "Found QEMU's msg filename ($VMM_MSG_FILENAME) and console filename ($VMM_CONSOLE_FILENAME)"
		break
	fi

	if [ $SECONDS_WAITING -gt $MAX_SECONDS ] ; then error_msg "Unable to find QEMU's msg filename ($VMM_MSG_FILENAME). Waited for $MAX_SECONDS seconds."; fi;

	let "SECONDS_WAITING=$SECONDS_WAITING+1"
	sleep 1
done

log_msg "Waiting for VM to boot...Might take several minutes"
log_msg " ...to get booting information run: tail -f $(readlink -f $VMM_CONSOLE_FILENAME)"
log_msg " ...or use VNC: vncviewer :1"

while read LINE;
do
	#log_msg "VM message: $LINE"

    if [[ "$LINE" == *"Guest finished booting"* ]]
    then
		log_msg "Sleep a while in case the vm is still in booting"
		sleep 50s
		log_msg "Transfering testcase to VM:"
		log_msg "scp -P ${SKI_VMM_SSH_LOCAL_PORT} ${SSH_OPTIONS} $SKI_TESTCASE_DIR/../$TESTCASE_PACKAGE_FILENAME ${SSH_DESTINATION}:/root/"
		scp -P ${SKI_VMM_SSH_LOCAL_PORT} ${SSH_OPTIONS} -v -l 2048 $SKI_TESTCASE_DIR/../$TESTCASE_PACKAGE_FILENAME ${SSH_DESTINATION}:/root/
		SCP_RESULT=$?
		log_msg "Finished transfering testcase (Exit code: $SCP_RESULT)"
		log_msg "Establishing an SSH connection to unpack and run the script"
		log_msg "ssh -p ${SKI_VMM_SSH_LOCAL_PORT} ${SSH_OPTIONS} ${SSH_DESTINATION} \"cd /root/ && tar -xzvf $TESTCASE_PACKAGE_FILENAME && cd ./$TESTCASE_PACKAGE_BASEDIR && ./$RUN_SCRIPT > $TESTCASE_PACKAGE_BASEDIR.log &\""
		SSH_REMOTE_COMMAND="echo 0 > /proc/sys/kernel/randomize_va_space && sysctl -w kernel.randomize_va_space=0 && cd /root/ && tar -xzvf $TESTCASE_PACKAGE_FILENAME && cd ./$TESTCASE_PACKAGE_BASEDIR/ && ./$RUN_SCRIPT"
		ssh -f -p ${SKI_VMM_SSH_LOCAL_PORT} ${SSH_OPTIONS} ${SSH_DESTINATION} 'nohup bash -c "'"$SSH_REMOTE_COMMAND"'" </dev/null >'$TESTCASE_PACKAGE_BASEDIR.log' 2>&1 &'
		SSH_RESULT=$?
		log_msg "Finished the SSH connection (Exit code: $SSH_RESULT)"
		exit
		#)&
    fi
done < <(tail -f $VMM_MSG_FILENAME)
