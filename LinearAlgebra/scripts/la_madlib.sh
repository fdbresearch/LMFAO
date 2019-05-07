#!/bin/bash
DFDB_SH_DATA_SET=$1

sql_madlib_drop=$"
DROP TABLE IF EXISTS joinres_one_hot;
DROP TABLE IF EXISTS svd_u;
DROP TABLE IF EXISTS svd_v;
DROP TABLE IF EXISTS svd_s;
DROP TABLE IF EXISTS svd_summary_table;"

# Generate categorical array output that suits to madlib
cat_features_str="'"
for cat_feat in "${DFDB_SH_FEATURES_CAT[@]}"
do
    cat_features_str+="$cat_feat,"
done
cat_features_str=${cat_features_str::-1}
cat_features_str+="'"
sql_madlib_one_hot_encode="SELECT madlib.encode_categorical_variables ('joinres', 'joinres_one_hot', #);"
#sql_madlib_one_hot_encode="SELECT madlib.create_indicator_variables('joinres', 'joinres_one_hot', #);"
sql_madlib_svd="SELECT madlib.svd( 'joinres_one_hot', 'svd', 'row_id', 34, NULL, 'svd_summary_table');"
sql_madlib_svd="SELECT madlib.matrix_qr( 'joinres_one_hot', 'svd', 'row_id', 34, NULL, 'svd_summary_table');"
sql_madlib_join_oh_cols=$"SELECT column_name FROM information_schema.columns WHERE table_name = 'joinres_one_hot';"
sql_madlib_drop_col=$"ALTER TABLE joinres_one_hot
DROP COLUMN #;
"
cd $DFDB_SH_ROOT
./multifaq --path "data/${DFDB_SH_DATA_SET}/" --model covar --codegen sql

cd ${DFDB_SH_RUNTIME_SQL}
sql_drop=$(cat drop_data.sql)
sql_madlib_drop="${sql_drop}${sql_madlib_drop}"
sql_join=$(cat join_features.sql)
sql_madlib_join="${sql_join/CREATE TABLE joinres AS (SELECT /CREATE TABLE joinres AS (SELECT ROW_NUMBER() over () AS row_id,}"

sql_madlib_one_hot_encode="${sql_madlib_one_hot_encode/\#/${cat_features_str}}"


echo "${sql_madlib_drop}" > drop_madlib.sql
echo "${sql_madlib_join}" > join_madlib.sql
echo "${sql_madlib_one_hot_encode}" > join_one_hot_madlib.sql
echo "${sql_madlib_join_oh_cols}" > sql_madlib_feats_list.sql
echo "${sql_madlib_svd}" > svd_madlib.sql

psql -U $DFDB_SH_USERNAME -p $DFDB_SH_PORT --d ${DFDB_SH_DB} -f drop_madlib.sql
eval ${DFDB_TIME} psql -U $DFDB_SH_USERNAME -p $DFDB_SH_PORT --d ${DFDB_SH_DB} -f load_data.sql

eval ${DFDB_TIME} psql -U $DFDB_SH_USERNAME -p $DFDB_SH_PORT -d ${DFDB_SH_DB} -f join_madlib.sql


/usr/local/madlib/bin/madpack -s madlib -p postgres \
   -c "$DFDB_SH_USERNAME@localhost:$DFDB_SH_PORT/$DFDB_SH_DATA_SET" install

#/home/max/installation/madlib-1.8/build/src/bin/madpack -s madlib -p postgres \
#	-c "$DFDB_SH_USERNAME@localhost:$DFDB_SH_PORT/$DFDB_SH_DATA_SET" install
eval ${DFDB_TIME} psql -U $DFDB_SH_USERNAME -p $DFDB_SH_PORT -d ${DFDB_SH_DB} -f join_one_hot_madlib.sql
sql_output=$( psql -U $DFDB_SH_USERNAME -p $DFDB_SH_PORT -d ${DFDB_SH_DB} -f sql_madlib_feats_list.sql )

cat_feats_a=()
readarray -t sql_output_a <<< "$sql_output"
for cat_feat in "${DFDB_SH_FEATURES_CAT[@]}"; do
    for column in "${sql_output_a[@]}"; do # access each element of array
        column_trimmed=$( echo "$column" | xargs )
        if [[ $column_trimmed =~ (${cat_feat})_([0-9]+) ]]; then
            cat_feats_a+=($column_trimmed)
            break
        fi
    done
done
echo "${cat_feats_a[@]}"
sql_madlib_drop_cols=""
for cat_feat in "${cat_feats_a[@]}"; do
    sql_madlib_drop_col_u+="${sql_madlib_drop_col/\#/${cat_feat}}"
done

echo "$sql_madlib_drop_col_u" > drop_madlib_cols.sql
psql -U $DFDB_SH_USERNAME -p $DFDB_SH_PORT -d ${DFDB_SH_DB} -f drop_madlib_cols.sql
eval ${DFDB_TIME} psql -U $DFDB_SH_USERNAME -p $DFDB_SH_PORT -d ${DFDB_SH_DB} -f svd_madlib.sql
psql -U $DFDB_SH_USERNAME -p $DFDB_SH_PORT -d ${DFDB_SH_DB} -f drop_madlib.sql
