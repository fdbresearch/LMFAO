#!/bin/bash
DFDB_SH_DATA_SET=$1

cd $DFDB_SH_LMFAO_P
./multifaq --path "${DFDB_SH_DATA}/${DFDB_SH_DATA_SET}/" --model covar --codegen sql

cd ${DFDB_SH_RUNTIME_SQL}
eval_time "Loading data" psql -U $DFDB_SH_USERNAME -p $DFDB_SH_PORT -d ${DFDB_SH_DB} -f load_data.sql

function clean_up_impl() {
    psql -U $DFDB_SH_USERNAME -p $DFDB_SH_PORT -d ${DFDB_SH_DB} -f join_cleanup.sql
}

function run_test_impl() {
    eval_time "Joining data" psql -U $DFDB_SH_USERNAME -p $DFDB_SH_PORT -d ${DFDB_SH_DB} -f join_features.sql
    eval_time "Exporting data" psql -U $DFDB_SH_USERNAME -p $DFDB_SH_PORT -d ${DFDB_SH_DB} -f export.sql
}


run_test_impl
#run_test run_test_impl clean_up_impl
