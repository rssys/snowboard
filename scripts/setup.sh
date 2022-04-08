log_msg(){
	echo "[setup.sh] LOG: $1"
}

sudo apt-get update
sudo apt-get install build-essential python gcc python3-pip
sudo apt install libglib2.0-dev libfdt-dev libpixman-1-dev zlib1g-dev libgtk-3-dev
sudo apt-get install gcc-multilib g++-multilib
pip3 install rq
source common.sh
export SNOWBOARD_STORAGE=$1
# Compile gcc-5.4
$MAIN_HOME/tools/gcc/install.sh
RES=$?
if [ $RES -ne 0 ]; then 
	log_msg "GCC-5.4 compilation failed ($RES)." ;  
fi
source $MAIN_HOME/tools/gcc/env.sh

# Compile snowboard hypervisor
$MAIN_HOME/vmm/install.sh
RES=$?
if [ $RES -ne 0 ]; then 
	log_msg "VMM compilation failed ($RES)." ;  
fi
source $MAIN_HOME/vmm/env.sh

# Compile and run Redis
$MAIN_HOME/tools/redis/install.sh
RES=$?
if [ $RES -ne 0 ]; then
	log_msg "Failed to compile or run Redis.";
fi
source $MAIN_HOME/tools/redis/env.sh

# Download artifact data
pushd $MAIN_HOME > /dev/null 
if [ ! -d "$MAIN_HOME/testsuite/" ]; then
	wget https://www.cs.purdue.edu/homes/sishuai/sosp21/testsuite.tar.gz
	RES=$?
	if [ $RES -ne 0 ]; then 
		log_msg "Data package downloading failed ($RES)." ;  
	else
		tar -zxvf testsuite.tar.gz
	fi
fi
popd > /dev/null
# Download the kernel source code
$MAIN_HOME/testsuite/kernel/download.sh
# Download the pmc data
$MAIN_HOME/data/download-pmc.sh
