#!/bin/bash
DFDB_SH_DATA_SET=$1

cd $DFDB_SH_ROOT
./multifaq --path "data/${DFDB_SH_DATA_SET}/" --model covar --codegen sql

cd ${DFDB_SH_RUNTIME_SQL}
psql -d usretailer -f drop_data.sql
eval ${DFDB_TIME} psql -d usretailer -f load_data.sql
eval ${DFDB_TIME} psql -d usretailer -f join_features.sql
eval ${DFDB_TIME} psql -d usretailer -f export.sql
