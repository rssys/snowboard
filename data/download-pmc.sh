wget https://www.cs.purdue.edu/homes/sishuai/sosp21/pmc.tar.gz
tar -zxvf pmc.tar.gz
RES=$?
if [ $RES -ne 0 ]; then
	echo "[error] failed to decompress the pmc tar"
	exit 1
fi
rm pmc.tar.gz
