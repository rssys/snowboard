DIR=$(dirname $BASH_SOURCE)
echo $DIR
export MAIN_HOME=$(realpath $DIR/../)

# test prerequisite 
export SNAPSHOT=$MAIN_HOME/testsuite/image/snapshot.img
export KERNEL_DIR=$MAIN_HOME/testsuite/kernel/linux-5.12-rc3/
export SEQUENTIAL_CORPUS=$MAIN_HOME/testsuite/input/data
