RESULT_DIR=$1
DATA_RACE_RESULT=`find $1 -name "*race_detect*.txt"`
echo "Found a data race detection result in $DATA_RACE_RESULT"
./data-race-analysis.py $KERNEL_DIR/vmlinux.map $DATA_RACE_RESULT | ./data-race-source-code.py 20 > $DATA_RACE_RESULT.source
echo "Find the source code information about the detected data races in $DATA_RACE_RESULT.source" 
