# to-do check the existance of redis server
REDIS_DIR=$MAIN_HOME/tools/redis/redis-6.2.4/
if [ -f "$REDIS_DIR/src/redis-server" ]; then
	echo "redis-server is already installed"
	echo "Try to run redis-server anyway. It is possible that redis-server is already running"
	$REDIS_DIR/src/redis-server $REDIS_DIR/../redis.conf --pidfile $REDIS_DIR/running.pid
	exit 0
fi
pushd $MAIN_HOME/tools/redis/ > /dev/null
# Prepare redis if the script is running for the first time
wget https://www.cs.purdue.edu/homes/sishuai/sosp21/redis.tar.gz
tar -zxvf redis.tar.gz
if [ ! -d "$REDIS_DIR" ]; then
	echo "[Error] $REDIS_DIR does not exist"
	exit 1
fi

cd $REDIS_DIR > /dev/null
make -j`nproc`
RES=$?
if [ $RES -ne 0 ]; then
	echo "[Error} failed to compile redis"
	exit 1
fi
# start running redis server
echo "Start running redis-server"
$REDIS_DIR/src/redis-server $REDIS_DIR/../redis.conf --pidfile $REDIS_DIR/../running.pid
popd > /dev/null
