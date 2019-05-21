#!/bin/bash

# Generate categorical array output that suits to madlib
function prepare_str_cat_features() {
    cat_features_str="'"
    for cat_feat in "${DFDB_SH_FEATURES_CAT[@]}"
    do
        cat_features_str+="$cat_feat,"
    done
    cat_features_str=${cat_features_str::-1}
    cat_features_str+="'"
}

function sort_array_str() {
    t=$(echo "$1" |  sort -V)
    echo "$t"
}

function generate_madlib_sql() {
    prepare_str_cat_features
    cd $DFDB_SH_ROOT
    ./multifaq --path "data/${DFDB_SH_DATA_SET}/" --model covar --codegen sql
    cd ${DFDB_SH_RUNTIME_SQL}

    sql_cleanup=$(cat join_cleanup.sql)
    sql_madlib_cleanup=$"
DROP TABLE IF EXISTS joinres_one_hot;
DROP TABLE IF EXISTS svd_u;
DROP TABLE IF EXISTS svd_v;
DROP TABLE IF EXISTS svd_s;
DROP TABLE IF EXISTS svd_summary_table;"
    sql_madlib_cleanup="${sql_cleanup}${sql_madlib_cleanup}"
    echo "${sql_madlib_cleanup}" > cleanup_madlib.sql

    sql_join=$(cat join_features.sql)
    sql_madlib_join="${sql_join/CREATE TABLE joinres AS (SELECT /CREATE TABLE joinres AS (SELECT ROW_NUMBER() over () AS row_id,}"
    echo "${sql_madlib_join}" > join_madlib.sql

    # OHE only categorical variables.
    sql_madlib_one_hot_encode="SELECT madlib.encode_categorical_variables ('joinres', 'joinres_one_hot', #);"
    #sql_madlib_one_hot_encode="SELECT madlib.create_indicator_variables('joinres', 'joinres_one_hot', #);"
    sql_madlib_one_hot_encode="${sql_madlib_one_hot_encode/\#/${cat_features_str}}"
    echo "${sql_madlib_one_hot_encode}" > join_one_hot_madlib.sql

    sql_madlib_join_oh_cols=$"SELECT column_name FROM information_schema.columns WHERE table_name = 'joinres_one_hot';"
    echo "${sql_madlib_join_oh_cols}" > sql_madlib_feats_list.sql

    sql_madlib_drop_col=$"ALTER TABLE joinres_one_hot
DROP COLUMN #;
    "

    sql_madlib_svd="SELECT madlib.svd( 'joinres_one_hot', 'svd', 'row_id', #, NULL, 'svd_summary_table');"
    #sql_madlib_svd="SELECT madlib.matrix_qr( 'joinres_one_hot', 'svd', 'row_id', 34, NULL, 'svd_summary_table');"
}

function clean_up_impl() {
    psql -U $DFDB_SH_USERNAME -p $DFDB_SH_PORT -d ${DFDB_SH_DB} -f cleanup_madlib.sql
}

function dump_s() {
    sing_vals_cnt="$1"
    echo "$sing_vals_cnt" > $dump_file

    # Parses output of psql to get all singular values.
    sing_val_out=$(psql -U $DFDB_SH_USERNAME -p $DFDB_SH_PORT -d ${DFDB_SH_DB} \
                 -c "SELECT * FROM svd_s;")
    readarray -t sing_vals_lines <<< "$sing_val_out"
    for idx in $(seq 1 $sing_vals_cnt); do
        shifted_idx=$(($idx+1))
        svd_line="${sing_vals_lines[$shifted_idx]}"
        IFS='| ' read -r -a svd_line_a <<< "$svd_line"
        echo "${svd_line_a[2]}" >> $dump_file
    done
    psql -U $DFDB_SH_USERNAME -p $DFDB_SH_PORT -d ${DFDB_SH_DB} -c "SELECT * FROM svd_summary_table;"
}

function run_test_impl() {
    eval_time "Joining data" psql -U $DFDB_SH_USERNAME -p $DFDB_SH_PORT \
        -d ${DFDB_SH_DB} -f join_madlib.sql
    eval_time "OneHotEncode" psql -U $DFDB_SH_USERNAME -p $DFDB_SH_PORT \
        -d ${DFDB_SH_DB} -f join_one_hot_madlib.sql

    #Names of the first categories of columns.
    sql_output=$(psql -U $DFDB_SH_USERNAME -p $DFDB_SH_PORT -d ${DFDB_SH_DB} -f sql_madlib_feats_list.sql)


    #echo "Output is: $sql_output"
    # Limitations, what if column has number in its name two columns share the  same name?
    # TODO: If there is time to properly implement this.
    sql_output_sor=$(sort_array_str "$sql_output")
    cat_feats_rm_a=()
    readarray -t sql_output_a <<< "$sql_output_sor"
    for cat_feat in "${DFDB_SH_FEATURES_CAT[@]}"; do
        for column in "${sql_output_a[@]}"; do # access each element of array
            column_trimmed=$( echo "$column" | xargs )
            if [[ $column_trimmed =~ (${cat_feat})_([0-9]+) ]]; then
                cat_feats_rm_a+=($column_trimmed)
                break
            fi
        done
    done
    echo "${cat_feats_rm_a[@]}"
    sql_madlib_drop_cols=""
    for cat_feat in "${cat_feats_rm_a[@]}"; do
        sql_madlib_drop_cols+="${sql_madlib_drop_col/\#/${cat_feat}}"
    done
    echo "$sql_madlib_drop_cols" > drop_madlib_cols.sql

    #Compute number of singular values in a matrix, 4 due to dumb output.
    sing_vals_cnt=$(( ${#sql_output_a[@]} - 4 - ${#cat_feats_rm_a[@]} ))
    echo "Number of singular values is: $sing_vals_cnt"
    echo "${sql_madlib_svd/\#/${sing_vals_cnt}}" > svd_madlib.sql

    psql -U $DFDB_SH_USERNAME -p $DFDB_SH_PORT -d ${DFDB_SH_DB} -f drop_madlib_cols.sql
    eval_time "Linear algebra" psql -U $DFDB_SH_USERNAME -p $DFDB_SH_PORT \
            -d ${DFDB_SH_DB} -f svd_madlib.sql

    [[ $DFDB_SH_DUMP == true ]] && dump_s $sing_vals_cnt
 }

DFDB_SH_DATA_SET=$1
cat_feats_out=$2
DFDB_SH_DATA_OP=$3
DFDB_SH_DUMP=$4
dump_file=$5

generate_madlib_sql

eval_time "Loading data" psql -U $DFDB_SH_USERNAME -p $DFDB_SH_PORT -d ${DFDB_SH_DB} -f load_data.sql

# Adds madlib schemas to database.
/usr/local/madlib/bin/madpack -s madlib -p postgres \
   -c "$DFDB_SH_USERNAME/$DFDB_SH_PASSWORD@localhost:$DFDB_SH_PORT/$DFDB_SH_DATA_SET" install

run_test run_test_impl clean_up_impl
