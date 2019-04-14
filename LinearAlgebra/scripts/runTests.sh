#!/bin/bash

#DFDB_SH_DATA="/media/popina/test/dfdb/benchmarking/datasets"
DFDB_SH_ROOT="/media/popina/test/dfdb/LMFAO"
#DFDB_SH_ROOT="/home/popina/Documents/FDB/LMFAO"
DFDB_SH_LA="$DFDB_SH_ROOT/LinearAlgebra"
DFDB_SH_LA_SCRIPT="${DFDB_SH_LA}/scripts"
DFDB_SH_DATA="$DFDB_SH_ROOT/data"
DFDB_SH_RUNTIME="$DFDB_SH_ROOT/runtime"
DFDB_SH_RUNTIME_CPP="$DFDB_SH_RUNTIME/cpp"
DFDB_SH_RUNTIME_SQL="$DFDB_SH_RUNTIME/sql"
DFDB_SH_RUNTIME_OUTPUT="$DFDB_SH_RUNTIME_CPP/output"
DFDB_SH_LOG_PATH="${DFDB_SH_LA}/logs"
DFDB_TIME="/usr/bin/time -f \"%e %P %I %O\""

: '
cd $DFDB_SH_LA
echo $DFDB_SH_LA
cmake .
make -j8
'
cd $DFDB_SH_ROOT
cmake .
make -j8

cd $DFDB_SH_LA_SCRIPT

data_sets=(usretailer_35f_100)
#usretailer_36f_1 usretailer_36f_10 usretailer_36f_100 usretailer_36f_1000 usretailer_36f)
for data_set in ${data_sets[@]}; do
    echo data_set_name: "$data_set"; 
    
    log_psql=${DFDB_SH_LOG_PATH}/psql/log"${data_set}".txt
    #(source generate_join.sh ${data_set} &> ${log_psql}) 

    log_lmfao=${DFDB_SH_LOG_PATH}/lmfao/log"${data_set}".txt
    log_r=${DFDB_SH_LOG_PATH}/r/log"${data_set}".txt
    log_numpy=${DFDB_SH_LOG_PATH}/numpy/log"${data_set}".txt
    log_scipy=${DFDB_SH_LOG_PATH}/scipy/log"${data_set}".txt

    #(source testLmfaola.sh ${data_set} &> ${log_lmfao})
    #eval ${DFDB_TIME} Rscript "${DFDB_SH_LA_SCRIPT}/svd.R" &> ${log_r}
    eval ${DFDB_TIME} python3 "${DFDB_SH_LA_SCRIPT}/svd_numpy.py" &> ${log_numpy}
    #eval ${DFDB_TIME} python3 "${DFDB_SH_LA_SCRIPT}/svd_scipy.py" &> ${log_scipy}
done
