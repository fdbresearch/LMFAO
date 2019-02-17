#DFDB_SH_DATA="/media/popina/test/dfdb/benchmarking/datasets"
#DFDB_SH_ROOT="/media/popina/test/dfdb/LMFAO"
DFDB_SH_ROOT="/home/popina/Documents/FDB/LMFAO"
DFDB_SH_LA="$DFDB_SH_ROOT/LinearAlgebra"
DFDB_SH_DATA="$DFDB_SH_ROOT/data"
DFDB_SH_RUNTIME_CPP="$DFDB_SH_ROOT/runtime/cpp"
DFDB_SH_RUNTIME_OUTPUT="$DFDB_SH_RUNTIME_CPP/output"
cd $DFDB_SH_LA
: '
cmake .
make -j8
'
cd $DFDB_SH_ROOT
cmake .
make -j8
./multifaq --path "data/usretailer/" --model qrdecomp --parallel both
cd $DFDB_SH_RUNTIME_OUTPUT
rm *
cd $DFDB_SH_RUNTIME_CPP
make dump -j8
./lmfao
: '
cd $DFDB_SH_LA
DFDB_SH_DATA_SET="usretailer"
python3 ./scripts/transform.py -o $DFDB_SH_RUNTIME_OUTPUT -d "$DFDB_SH_DATA/$DFDB_SH_DATA_SET/" >test.in
./lmfaola
'

