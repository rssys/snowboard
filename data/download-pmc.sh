PMC_DIR=$MAIN_HOME/data/pmc
if [ -d "$PMC_DIR" ]; then
	echo "PMC data is already downloaded"
	exit 0
fi
pushd $MAIN_HOME/data/ > /dev/null
wget https://www.cs.purdue.edu/homes/sishuai/sosp21/pmc.tar.gz
tar -zxvf pmc.tar.gz
RES=$?
if [ $RES -ne 0 ]; then
	echo "[error] failed to decompress the pmc tar"
	exit 1
fi
rm pmc.tar.gz
popd > /dev/null
