#!/bin/bash

function init_global_paths()
{
    DFDB_SH_ROOT="${1:-/media/popina/test/dfdb/LMFAO}"
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
    DFDB_SH_COMP_PATH="${DFDB_SH_LA}/comparisons"
    DFDB_SH_DUMP_PATH="${DFDB_SH_LA}/dumps"
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
    DFDB_SH_PRECISION=false

    DFDB_SH_DUMP=false
    DFDB_SH_NUM_REP=1
    DFDB_SH_HELP_SHOW=false
    DFDB_SH_HELP_TXT=$"
Usage: runTests [-h --help] [-b|--build =<OPTS>]
[-r|--root=<PATH>] [-u|--user=<NAME>] [-p|--port=<NUMBER>]
[-o|--operation=<NAME1>, <NAME2>,...>]
[-d|--data_sets=<NAME1>,<NAME2>,...>][-f|--full_exps]

Run experiments for matrix factorizations.

Mandatory arguments to long options are mandatory for short options too.
    -b, --build=<OPTS>           run the experiments stated in opts:
                                 j - join tables using PSQL and export the joined result
                                 l - factorizations using LA-F (linear algebra on top of LMFAO)
                                 r - factorizations implemented in r
                                 n - factorizations implemented in numpy
                                 s - factorizations implemented in scipy
                                 e - factorizations implemented using eigen and C++
                                 m - factorizations implemented using MADlib
                                 p - compare accuracies of factorizations using LA-F
                                   and other approaches.
    -r, --root=<PATH>            set the root path to PATH of LMFAO project
    -u, --user=<NAME>            connect to PSQL datatabase with the username NAME.
    -p, --port=<NUMBER>          connect to PSQL database on the port NUMBER.
    -o, --operation=<NAME1>,...> run experiments for the factorization NAME1, NAME2,...
                                 Possible operations are qr and svd.
    -d, --data_sets=<NAME1>,...> run experiments for data sets NAME1, NAME2,...
                                 located on root of LMFAO project/data.
    -f, --full_exps              run multiple times the same experiment to reduce cache
                                 locality and statistics problems. All times
                                 will be outputed.
    -h, --help                   show help.
"
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
            DFDB_SH_EIGEN=true
            ;;
            n)
            DFDB_SH_JOIN=true
            DFDB_SH_NUMPY=true
            ;;
            r)
            DFDB_SH_JOIN=true
            DFDB_SH_R=true
            ;;
            p)
            DFDB_SH_JOIN=true
            DFDB_SH_LMFAO=true
            DFDB_SH_NUMPY=true
            DFDB_SH_DUMP=true
            DFDB_SH_PRECISION=true
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
        --dump)
        DFDB_SH_DUMP=true
        ;;
        -f|--full_exps)
        DFDB_SH_NUM_REP=5
        ;;
        -h|--help)
        echo "$DFDB_SH_HELP_TXT"
        DFDB_SH_HELP_SHOW=true
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
        # Order features so last ones are categorical.
        if [[ $line =~ $regex_features ]]; then
            feature_name=${BASH_REMATCH[1]}
            if [[ $line =~ $regex_cat_features ]]; then
                feature_name=${BASH_REMATCH[1]}
                features_cat+=(${feature_name})
            else
                features+=(${feature_name})
            fi
        fi
    done < $file_name

    features+=("${features_cat[@]}")
    DFDB_SH_FEATURES=(${features[@]})
    DFDB_SH_FEATURES_CAT=(${features_cat[@]})
}

# Template style organization of execution of classes.
function run_test()
{
    local clean_up_fun="$2"
    local run_test_impl="$1"
    for i in $(seq 1 $DFDB_SH_NUM_REP); do
        echo "Iteration number $i / $DFDB_SH_NUM_REP"
        if [[ $i > 1 ]]; then
            "$clean_up_fun"
        fi
        "$run_test_impl"
    done
}

: '
cd $DFDB_SH_LA
echo $DFDB_SH_LA
cmake .
make -j8
'

function build_and_run_tests() {
    data_set=$1
    data_op=$2
    local features_out=$(printf "%s," ${DFDB_SH_FEATURES[@]})
    features_out=${features_out::-1}
    echo 'Features: ' ${features_out}
    echo 'Number of categorical feats: '${#DFDB_SH_FEATURES[@]}

    local features_cat_out=$(printf "%s," ${DFDB_SH_FEATURES_CAT[@]})
    features_cat_out=${features_cat_out::-1}
    echo 'Categorical features: ' ${features_cat_out}
    echo 'Number of categorical feats: '${#DFDB_SH_FEATURES_CAT[@]}

    local log_madlib=${DFDB_SH_LOG_PATH}/madlib/log"${data_set}${data_op}".txt
    local log_eigen=${DFDB_SH_LOG_PATH}/eigen/log"${data_set}${data_op}".txt
    local log_lmfao=${DFDB_SH_LOG_PATH}/lmfao/log"${data_set}${data_op}".txt
    local log_r=${DFDB_SH_LOG_PATH}/r/log"${data_set}${data_op}".txt
    local log_numpy=${DFDB_SH_LOG_PATH}/numpy/log"${data_set}${data_op}".txt
    local log_scipy=${DFDB_SH_LOG_PATH}/scipy/log"${data_set}${data_op}".txt
    local path_comp=${DFDB_SH_COMP_PATH}/comp"${data_set}${data_op}"

    local dump_madlib=${DFDB_SH_DUMP_PATH}/madlib/dump"${data_set}${data_op}".txt
    local dump_eigen=${DFDB_SH_DUMP_PATH}/eigen/dump"${data_set}${data_op}".txt
    local dump_lmfao=${DFDB_SH_DUMP_PATH}/lmfao/dump"${data_set}${data_op}".txt
    local dump_r=${DFDB_SH_DUMP_PATH}/r/dump"${data_set}${data_op}".txt
    local dump_numpy=${DFDB_SH_DUMP_PATH}/numpy/dump"${data_set}${data_op}".txt
    local dump_scipy=${DFDB_SH_DUMP_PATH}/scipy/dump"${data_set}${data_op}".txt

    local dump_opt=""
    [[ $DFDB_SH_DUMP == true ]] && dump_opt="--dump"

    [[ $DFDB_SH_MADLIB  == true ]] && {
        echo '*********Madlib test started**********'
        (source la_madlib.sh ${data_set} ${features_cat_out}  &> ${log_madlib})
        echo '*********Madlib test finished**********'
    }

    [[ $DFDB_SH_EIGEN  == true ]] && {
        echo '*********Eigen test started**********'
        (source la_eigen.sh ${data_set} ${data_op} ${features_out} ${features_cat_out} &> ${log_eigen})
        echo '*********Eigen test finished**********'
    }

    [[ $DFDB_SH_LMFAO  == true ]] && {
        echo '*********LMFAO test started**********'
        (source test_lmfaola.sh ${data_set} ${data_op} ${DFDB_SH_DUMP} \
                "${dump_lmfao}" &> ${log_lmfao})
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
        eval ${DFDB_TIME} python3 "${DFDB_SH_LA_SCRIPT}/la_python.py" \
                          -f ${features_out} -c ${features_cat_out}  \
                          -d "${DFDB_SH_JOIN_RES_PATH}" -o "$data_op"\
                          -n "${DFDB_SH_NUM_REP}"  "${dump_opt}"     \
                          --dump_file "${dump_numpy}" -s "numpy"     \
                           &> ${log_numpy}
        echo '*********numpy test finished**********'
    }
    # TODO: Add tests for scipy, R, Madlib
    [[ $DFDB_SH_SCIPY  == true ]] && {
        echo '*********scipy test started**********'
        eval ${DFDB_TIME} python3 "${DFDB_SH_LA_SCRIPT}/la_python.py" \
                  -f ${features_out} -c ${features_cat_out}  \
                  -d "${DFDB_SH_JOIN_RES_PATH}" -o "$data_op"\
                  -n "${DFDB_SH_NUM_REP}"  "${dump_opt}"     \
                  --dump_file "${dump_scipy}" -s "scipy"     \
                   &> ${log_scipy}

        #eval ${DFDB_TIME} python3 "${DFDB_SH_LA_SCRIPT}/la_scipy.py" \
        #                  -f ${features_out} -c ${features_cat_out}    \
        #                  -d "${DFDB_SH_JOIN_RES_PATH}" -o "$data_op" &> ${log_scipy}
        echo '*********scipy test finished**********'
    }

    # This one has to be always the last
    [[ $DFDB_SH_PRECISION  == true ]] && {
      echo '*********comparison test started**********'
      python3 "${DFDB_SH_LA_SCRIPT}/precision_comparison.py" \
              -lp "${dump_lmfao}" -cp "${dump_numpy}" \
              -pr 1e-10 --output_path "${path_comp}"
      echo '*********comparison test started**********'
    }
}

#TODO: Add qr for eigen, madlib.

function main() {
    init_global_paths
    init_global_vars

    get_str_args "$@"
    if [[ $DFDB_SH_HELP_SHOW == true ]]; then
        return
    fi

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
            build_and_run_tests $data_set $data_op
            echo "*********${data_op} decomposition**********"
        done

        dropdb $DFDB_SH_DB -U $DFDB_SH_USERNAME -p $DFDB_SH_PORT
    done
}
# Example:  ./runTests.sh --build=n -o=svd -d=usretailer_35f_1 -r=/home/max/LMFAO -f
main $@
