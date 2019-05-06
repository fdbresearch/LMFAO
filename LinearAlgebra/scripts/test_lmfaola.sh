#!/bin/bash

DFDB_SH_DATA_SET=$1
#"usretailer_1"

cd $DFDB_SH_ROOT
./multifaq --path "data/${DFDB_SH_DATA_SET}/" --model qrdecomp --parallel both
cd $DFDB_SH_RUNTIME_OUTPUT
rm *
cd $DFDB_SH_RUNTIME_CPP
make -j8
eval ${DFDB_TIME} ./lmfao

cd $DFDB_SH_ROOT
./multifaq --path "data/${DFDB_SH_DATA_SET}/" --model svdecomp --parallel both
cd $DFDB_SH_RUNTIME_OUTPUT
rm *
cd $DFDB_SH_RUNTIME_CPP
make -j8
eval ${DFDB_TIME} ./lmfao
: '
cd $DFDB_SH_LA
python3 ./scripts/transform.py -o $DFDB_SH_RUNTIME_OUTPUT -d "$DFDB_SH_DATA/$DFDB_SH_DATA_SET/" >test.in
./lmfaola
'

