GCC_DIR=$MAIN_HOME/tools/gcc/gcc-5.4.0
if [ -f "$GCC_DIR/install/bin/gcc" ]; then
	echo "gcc-5.4 is already installed"
	exit 0
fi
pushd $MAIN_HOME/tools/gcc/ > /dev/null
# Download the source code of gcc-5.4.0
wget https://www.cs.purdue.edu/homes/sishuai/sosp21/gcc-5.4.0.tar.gz
tar -zxvf gcc-5.4.0.tar.gz
if [ ! -d "$GCC_DIR" ]; then
	echo "[Error] $GCC_DIR does not exist"
	exit 1
fi
cd $GCC_DIR > /dev/null
mkdir build
mkdir install
./contrib/download_prerequisites
# patch gcc-5.4.0 because its code is ancient
patch -p1 < ../gcc-patch
cd build
../configure --prefix=$GCC_DIR/install/ -disable-nls --enable-languages=c,c++ -disable-multilib 
RES=$?
if [ $RES -ne 0 ]; then
	echo "[Error] gcc configuration error $RES"
	exit 1
fi
make -j`nproc`
RES=$?
if [ $RES -ne 0 ]; then
	echo "[Error] gcc compilation error $RES"
	exit 1
fi
make install
RES=$?
if [ $RES -ne 0 ]; then
	echo "[Error] gcc installment error $RES"
	exit 1
fi
popd > /dev/null
