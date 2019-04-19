#!/bin/bash
DFDB_SH_DATA_SET=$1

cd $DFDB_SH_ROOT
./multifaq --path "data/${DFDB_SH_DATA_SET}/" --model covar --codegen sql

cd ${DFDB_SH_RUNTIME_SQL}
psql -d ${DFDB_SH_DB} -f drop_data.sql
eval ${DFDB_TIME} psql -d ${DFDB_SH_DB} -f load_data.sql
eval ${DFDB_TIME} psql -d ${DFDB_SH_DB} -f join_features.sql
eval ${DFDB_TIME} psql -d ${DFDB_SH_DB} -f export.sql
