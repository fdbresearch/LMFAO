#!/bin/bash
DFDB_SH_DATA_SET=$1

cd $DFDB_SH_ROOT
./multifaq --path "data/${DFDB_SH_DATA_SET}/" --model covar --codegen sql

cd ${DFDB_SH_RUNTIME_SQL}
psql -U $DFDB_SH_USERNAME -p $DFDB_SH_PORT -d ${DFDB_SH_DB} -f drop_data.sql
eval ${DFDB_TIME} psql -U $DFDB_SH_USERNAME -p $DFDB_SH_PORT -d ${DFDB_SH_DB} -f load_data.sql
eval ${DFDB_TIME} psql -U $DFDB_SH_USERNAME -p $DFDB_SH_PORT -d ${DFDB_SH_DB} -f join_features.sql
eval ${DFDB_TIME} psql -U $DFDB_SH_USERNAME -p $DFDB_SH_PORT -d ${DFDB_SH_DB} -f export.sql
