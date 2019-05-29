#!/bin/bash

DFDB_SH_DATA_SET=$1
DFDB_SH_DATA_OP=$2
DFDB_SH_DUMP=$3
dump_file=$4
#"usretailer_1"

[[ $DFDB_SH_DUMP == true ]] && dump="--outputDecomp" || dump=""
cd $DFDB_SH_LMFAO_P
case $DFDB_SH_DATA_OP in
    svd)
    ./multifaq --path "${DFDB_SH_DATA}/${DFDB_SH_DATA_SET}/" --model svdecomp \
               --parallel both "${dump}" --dumpFile "${dump_file}"
    ;;
    qr)
    ./multifaq --path "${DFDB_SH_DATA}/${DFDB_SH_DATA_SET}/" --model qrdecomp  \
               --parallel both "${dump}" --dumpFile "${dump_file}"
    ;;
esac
cd $DFDB_SH_RUNTIME_OUTPUT
rm *
cd $DFDB_SH_RUNTIME_CPP
make -j8

function run_test_impl() {
   eval ${DFDB_TIME} ./lmfao
}

function run_test_cleanup() {
    :;
}

run_test run_test_impl run_test_cleanup

: '
cd $DFDB_SH_LA
python3 ./scripts/transform.py -o $DFDB_SH_RUNTIME_OUTPUT -d "$DFDB_SH_DATA/$DFDB_SH_DATA_SET/" >test.in
./lmfaola
'

