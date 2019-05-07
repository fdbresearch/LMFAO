#!/bin/bash

function init_global_paths()
{
    #DFDB_SH_DATA="/media/popina/test/dfdb/benchmarking/datasets"
    DFDB_SH_ROOT="${1:-/media/popina/test/dfdb/LMFAO}"
    echo "Root path of LMFAO is: " $DFDB_SH_ROOT
    #DFDB_SH_ROOT="/home/popina/Documents/FDB/LMFAO"
    DFDB_SH_LA="$DFDB_SH_ROOT/LinearAlgebra"
    DFDB_SH_LA_SCRIPT="${DFDB_SH_LA}/scripts"
    DFDB_SH_DATA="$DFDB_SH_ROOT/data"
    DFDB_SH_RUNTIME="$DFDB_SH_ROOT/runtime"
    DFDB_SH_RUNTIME_CPP="$DFDB_SH_RUNTIME/cpp"
    DFDB_SH_RUNTIME_SQL="$DFDB_SH_RUNTIME/sql"
    DFDB_SH_RUNTIME_OUTPUT="$DFDB_SH_RUNTIME_CPP/output"
    DFDB_SH_JOIN_RES_PATH="${DFDB_SH_RUNTIME_SQL}/joinresult.txt"
    DFDB_SH_LOG_PATH="${DFDB_SH_LA}/logs"
}

function init_global_vars()
{
    DFDB_TIME="/usr/bin/time -f \"%e %P %I %O\""
    DFDB_SH_DB="usretailer"
    DFDB_SH_USERNAME=$(whoami)
    DFDB_SH_PORT=5432
    DFDB_SH_DATA_SETS=(usretailer_35f_1 usretailer_35f_10 usretailer_35f_100 usretailer_35f_1000 usretailer favorita)
    DFDB_SH_OPS=("svd" "qr")
    DFDB_SH_FEATURES=()
    DFDB_SH_FEATURES_CAT=()
    DFDB_SH_POSITIONAL=()
    DFDB_SH_JOIN=false
    DFDB_SH_LMFAO=false
    DFDB_SH_JOIN=false
    DFDB_SH_MADLIB=false
    DFDB_SH_R=false
    DFDB_SH_SCIPY=false
    DFDB_SH_NUMPY=false
}

function get_build_opts()
{
#TODO: Add iterate over values split by comma
    for i in $(seq 1 ${#1}); do
        build_option="${1:i-1:1}"
        case $build_option in
            m)
            DFDB_SH_MADLIB=true
            ;;
            j)
            DFDB_SH_JOIN=true
            ;;
            l)
            DFDB_SH_LMFAO=true
            ;;
            s)
            DFDB_SH_JOIN=true
            DFDB_SH_SCIPY=true;
            ;;
            e)
            DFDB_SH_JOIN=true
            DFDB_SH_EIGEN=true;
            ;;
            n)
            DFDB_SH_JOIN=true
            DFDB_SH_NUMPY=true;
            ;;
            r)
            DFDB_SH_JOIN=true
            DFDB_SH_R=true;
            ;;
            *)    # unknown option
            echo "Wrong build argument" $build_option
            ;;
        esac
    done
}

function get_data_sets()
{
    IFS=', ' read -r -a DFDB_SH_DATA_SETS <<< "$1"
}

function get_data_operations()
{
    IFS=', ' read -r -a DFDB_SH_OPS <<< "$1"
}

function get_str_args()
{
    local EXTENSION=""
    for option in "$@"
    do
    case $option in
        -b=*|--build=*)
        EXTENSION="${option#*=}"
        get_build_opts $EXTENSION
        ;;
        -r=*|--root=*)
        EXTENSION="${option#*=}"
        init_global_paths $EXTENSION
        ;;
        -u=*|--user=*)
        EXTENSION="${option#*=}"
        DFDB_SH_USERNAME=$EXTENSION
        ;;
        -p=*|--port=*)
        EXTENSION="${option#*=}"
        DFDB_SH_PORT=$EXTENSION
        ;;
        -o=*|--operation=*)
        EXTENSION="${option#*=}"
        get_data_operations $EXTENSION
        ;;
        -d=*|--data_sets=*)
        EXTENSION="${option#*=}"
        get_data_sets $EXTENSION
        ;;
        *)    # unknown option
        echo "Wrong  argument" $option
        ;;
    esac
    done
}


function get_features()
{
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

function run_test() {
    data_set=$1
    data_op=$2
    local features_out=$(printf "%s," ${DFDB_SH_FEATURES[@]})
    features_out=${features_out::-1}
    echo 'Features: ' ${features_out}
    echo 'Number of categorical feats: '${#DFDB_SH_FEATURES[@]}

    local features_cat_out=$(printf "%s," ${DFDB_SH_FEATURES_CAT[@]})
    local features_cat_out=${features_cat_out::-1}
    echo 'Categorical features: ' ${features_cat_out}
    echo 'Number of categorical feats: '${#DFDB_SH_FEATURES_CAT[@]}

    local log_madlib=${DFDB_SH_LOG_PATH}/madlib/log"${data_set}${data_op}".txt
    local log_eigen=${DFDB_SH_LOG_PATH}/eigen/log"${data_set}${data_op}".txt
    local log_lmfao=${DFDB_SH_LOG_PATH}/lmfao/log"${data_set}${data_op}".txt
    local log_r=${DFDB_SH_LOG_PATH}/r/log"${data_set}${data_op}".txt
    local log_numpy=${DFDB_SH_LOG_PATH}/numpy/log"${data_set}${data_op}".txt
    local log_scipy=${DFDB_SH_LOG_PATH}/scipy/log"${data_set}${data_op}".txt

    [[ $DFDB_SH_MADLIB  == true ]] && {
        echo '*********Madlib test started**********'
        (source la_madlib.sh ${data_set} ${features_cat_out}  &> ${log_madlib})
        echo '*********Madlib test finished**********'
    }

    [[ $DFDB_SH_EIGEN  == true ]] && {
        echo '*********Eigen test started**********'
        (source la_eigen.sh ${data_set} ${data_op} ${features_cat_out}  &> ${log_eigen})
        echo '*********Eigen test finished**********'
    }

    [[ $DFDB_SH_LMFAO  == true ]] && {
        echo '*********LMFAO test started**********'
        (source test_lmfaola.sh ${data_set} ${data_op} &> ${log_lmfao})
        echo '*********LMFAO test finished**********'
    }

    [[ $DFDB_SH_R  == true ]] && {
        echo '*********R test started**********'
        eval ${DFDB_TIME} Rscript "${DFDB_SH_LA_SCRIPT}/la.R ${DFDB_SH_JOIN_RES_PATH} \
                ${data_op} ${#DFDB_SH_FEATURES[@]} ${#DFDB_SH_FEATURES_CAT[@]}         \
                ${DFDB_SH_FEATURES[@]} ${DFDB_SH_FEATURES_CAT[@]}" &> ${log_r}
        echo '*********R test finished**********'
    }
    [[ $DFDB_SH_NUMPY  == true ]] && {
        echo '*********numpy test started**********'
        eval ${DFDB_TIME} python3 "${DFDB_SH_LA_SCRIPT}/la_numpy.py" \
                          -f ${features_out} -c ${features_cat_out}   \
                          -d "${DFDB_SH_JOIN_RES_PATH}" -o "$data_op" &> ${log_numpy}
        echo '*********numpy test finished**********'
    }
    [[ $DFDB_SH_SCIPY  == true ]] && {
        echo '*********scipy test started**********'
        eval ${DFDB_TIME} python3 "${DFDB_SH_LA_SCRIPT}/la_scipy.py" \
                          -f ${features_out} -c ${features_cat_out}    \
                          -d "${DFDB_SH_JOIN_RES_PATH}" -o "$data_op" &> ${log_scipy}
        echo '*********scipy test finished**********'
    }
}

#TODO: Add qr for eigen, madlib.

function main() {
    init_global_paths
    init_global_vars

    get_str_args "$@"
    cd $DFDB_SH_ROOT
    cmake .
    make -j8

    cd $DFDB_SH_LA_SCRIPT

    for data_set in ${DFDB_SH_DATA_SETS[@]}; do
        DFDB_SH_DB=${data_set}
        echo tests for data set "$data_set" are starting;
        data_path=$DFDB_SH_DATA"/"$data_set
        get_features $data_path

        dropdb $DFDB_SH_DB -U $DFDB_SH_USERNAME -p $DFDB_SH_PORT || \
        createdb $DFDB_SH_DB -U $DFDB_SH_USERNAME -p $DFDB_SH_PORT
        local log_psql=${DFDB_SH_LOG_PATH}/psql/log"${data_set}".txt
        [[ $DFDB_SH_JOIN  == true ]] && {
            echo '*********Join started**********'
            #(source generate_join.sh ${data_set}  &> ${log_psql})
            echo '*********Join finished**********'
        }

        for data_op in ${DFDB_SH_OPS[@]}; do
            echo "*********${data_op} decomposition**********"
            run_test $data_set $data_op
            echo "*********${data_op} decomposition**********"
        done

        dropdb $DFDB_SH_DB -U $DFDB_SH_USERNAME -p $DFDB_SH_PORT
    done
}
# Example:  ./runTests.sh --build=n -o=svd -d=usretailer_35f_1
main $@
