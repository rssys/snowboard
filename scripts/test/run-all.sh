# $1: start id
# $2: end id
SB_OUTPUT_DIR="$SNOWBOARD_STORAGE/sequential-analysis-`date +'%Y-%m-%d-%H-%M-%S'`"
#echo "dir not exists, creating $SB_OUTPUT_DIR"
mkdir $SB_OUTPUT_DIR

./profile.py $SB_OUTPUT_DIR $1 $2

PROFILE_DATA=$(find $SB_OUTPUT_DIR -name "profile*" -type d)

./sequential-shared-analysis.py $PROFILE_DATA $1 $2

SHARED_ACCESS_DATA=$(find $PROFILE_DATA -name "shared*" -type d)

./mem-dict-generation.py $SHARED_ACCESS_DATA $1 $2

MEM_DICT=$(find $PROFILE_DATA -name "mem-dict*")

./pmc-analysis.py $MEM_DICT $SB_OUTPUT_DIR
