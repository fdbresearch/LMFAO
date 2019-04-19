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
DFDB_SH_DB="usretailer"
DFDB_SH_FEATURES=()
DFDB_SH_FEATURES_CAT=()


function get_features() {
    local file_name=$1"/features.conf"
    echo $file_name
    
    local regex_num="^([0-9]+)(, ?)?([0-9]+)(, ?)?([0-9]+)(, ?)?$"
    local regex_features="^([a-zA-Z_]+):[0-9]+:[a-zA-Z_]+$"
    local regex_cat_features="^([a-zA-Z_]+):([1-9][0-9]*):[a-zA-Z_]+$"
    local features=()
    features_cat=()
    while IFS='' read -r line || [[ -n "$line" ]]; do
        if [[ $line =~ $regex_num ]]; then
            NUM_FEATURES=${BASH_REMATCH[1]}
            echo ${line}
            DUMB_1=${BASH_REMATCH[3]}
            DUMB_2=${BASH_REMATCH[5]}
        fi
        if [[ $line =~ $regex_features ]]; then
            feature_name=${BASH_REMATCH[1]}
            features+=(${feature_name})

            if [[ $line =~ $regex_cat_features ]]; then
                feature_name=${BASH_REMATCH[1]}
                features_cat+=(${feature_name})
            fi
        fi
    done < $file_name

    DFDB_SH_FEATURES=(${features[@]})
    DFDB_SH_FEATURES_CAT=(${features_cat[@]})
}

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

data_sets=(usretailer_35f_1)
#usretailer_36f_1 usretailer_36f_10 usretailer_36f_100 usretailer_36f_1000 usretailer_36f)
for data_set in ${data_sets[@]}; do
    echo data_set_name: "$data_set"; 
    data_path=$DFDB_SH_DATA"/"$data_set
    get_features $data_path

    features_out=$(printf "%s," ${DFDB_SH_FEATURES[@]})
    features_out=${features_out::-1}
    echo 'Features: ' ${features_out}
    echo 'Len: '${#DFDB_SH_FEATURES[@]}

    features_cat_out=$(printf "%s," ${DFDB_SH_FEATURES_CAT[@]})
    features_cat_out=${features_cat_out::-1}
    echo 'Features: ' ${features_cat_out}
    echo 'LenCat: '${#DFDB_SH_FEATURES_CAT[@]}

    log_psql=${DFDB_SH_LOG_PATH}/psql/log"${data_set}".txt

    log_lmfao=${DFDB_SH_LOG_PATH}/lmfao/log"${data_set}".txt
    log_madlib=${DFDB_SH_LOG_PATH}/madlib/log"${data_set}".txt
    log_r=${DFDB_SH_LOG_PATH}/r/log"${data_set}".txt
    log_numpy=${DFDB_SH_LOG_PATH}/numpy/log"${data_set}".txt
    log_scipy=${DFDB_SH_LOG_PATH}/scipy/log"${data_set}".txt

    (source svd_madlib.sh ${data_set} ${features_cat_out}  &> ${log_madlib})
    : '

    (source generate_join.sh ${data_set}  &> ${log_psql}) 

    #(source test_lmfaola.sh ${data_set} &> ${log_lmfao})
    #eval ${DFDB_TIME} Rscript "${DFDB_SH_LA_SCRIPT}/svd.R" &> ${log_r}
    eval ${DFDB_TIME} python3 "${DFDB_SH_LA_SCRIPT}/svd_numpy.py" \
                      -f ${features_out} -c ${features_cat_out} &> ${log_numpy}
    eval ${DFDB_TIME} python3 "${DFDB_SH_LA_SCRIPT}/svd_scipy.py" \
                      -f ${features_out} -c ${features_cat_out} &> ${log_scipy}
    '
done
