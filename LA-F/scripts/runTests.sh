#!/bin/bash

function init_global_paths()
{
    DFDB_SH_ROOT="${1:-/media/popina/test/dfdb/LMFAO}"
    #DFDB_SH_ROOT="/home/popina/Documents/FDB/LMFAO"
    DFDB_SH_LA="$DFDB_SH_ROOT/LA-F"
    DFDB_SH_LMFAO_P="$DFDB_SH_ROOT/LMFAO"
    DFDB_SH_LA_SCRIPT="${DFDB_SH_LA}/scripts"
    DFDB_SH_LA_BUILD="${DFDB_SH_LA}/build"
    DFDB_SH_LMFAO_BUILD="${DFDB_SH_LMFAO_P}/build"
    DFDB_SH_DATA="$DFDB_SH_ROOT/data"
    DFDB_SH_BUILD="$DFDB_SH_LMFAO_P/build"
    DFDB_SH_RUNTIME="$DFDB_SH_LMFAO_P/runtime"
    DFDB_SH_RUNTIME_CPP="$DFDB_SH_RUNTIME/cpp"
    DFDB_SH_RUNTIME_SQL="$DFDB_SH_RUNTIME/sql"
    DFDB_SH_RUNTIME_OUTPUT="$DFDB_SH_RUNTIME_CPP/output"
    DFDB_SH_JOIN_RES_PATH="${DFDB_SH_RUNTIME_SQL}/joinresult.txt"
    DFDB_SH_LOG_PATH="${DFDB_SH_LA}/logs"
    DFDB_SH_COMP_PATH="${DFDB_SH_LA}/comparisons"
    DFDB_SH_DUMP_PATH="${DFDB_SH_LA}/dumps"
    DFDB_SH_TIME_PATH="${DFDB_SH_LA}/times"
}

function init_global_vars()
{
    DFDB_TIME="/usr/bin/time -f \"%e\""
    DFDB_SH_DB="usretailer"
    DFDB_SH_USERNAME=$(whoami)
    DFDB_SH_PASSWORD=""
    DFDB_SH_PORT=5432
    declare -gA DFDB_SH_DATA_SETS_FULL
    DFDB_SH_DATA_SETS_FULL=( [usretailer_35f_1]=1 [usretailer_35f_10]=2 [usretailer_35f_100]=3 [usretailer_35f_1000]=4 [usretailer_35f]=5 [favorita]=6 [favorita_6f]=7 )
    DFDB_SH_DATA_SETS=("${!DFDB_SH_DATA_SETS_FULL[@]}")
    DFDB_SH_OPS=("qr_chol" "qr_mul_t" "svd" "svd_qr" "svd_qr_chol" "svd_eig_dec" "svd_alt_min")
    DFDB_SH_FEATURES=()
    DFDB_SH_FEATURES_CAT=()
    DFDB_SH_POSITIONAL=()
    DFDB_SH_CONF_ERROR=false

    DFDB_SH_SIN_VALS=false
    DFDB_SH_JOIN=false
    DFDB_SH_LMFAO=false
    DFDB_SH_EIGEN=false
    DFDB_SH_MADLIB=false
    DFDB_SH_R=false
    DFDB_SH_SCIPY=false
    DFDB_SH_NUMPY=false
    DFDB_SH_PRECISION=false
    DFDB_SH_PERF=false

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
            DFDB_SH_SCIPY=true;
            ;;
            e)
            DFDB_SH_EIGEN=true
            ;;
            n)
            DFDB_SH_NUMPY=true
            ;;
            r)
            DFDB_SH_R=true
            ;;
            p)
            DFDB_SH_LMFAO=true
            DFDB_SH_DUMP=true
            DFDB_SH_PRECISION=true
            ;;
            *)    # unknown option
            echo "Wrong build argument" $build_option
            ;;
        esac
    done
}

array_contains () {
    local search_el=$1;
    local exist="false"
    for element in "${@:2}"; do
        if [[ $element == $search_el ]]; then
            exist="true"
            break
        fi
    done
    echo "${exist}"
}

function get_data_sets()
{
    local data_sets_tmp=()
    IFS=', ' read -r -a data_sets_tmp <<< "$1"
    for data_set in "${data_sets_tmp[@]}"; do
        cont=$(array_contains $data_set ${DFDB_SH_DATA_SETS[@]})
        if [[ $cont != true ]]; then
            DFDB_SH_CONF_ERROR=true
            echo "Data set \"$data_set\" does not exist " 1>&2
        fi
    done
    DFDB_SH_DATA_SETS=("${data_sets_tmp[@]}")
}

function get_data_operations()
{
    local ops_tmp=()
    IFS=', ' read -r -a ops_tmp <<< "$1"
    for data_op in "${ops_tmp[@]}"; do
        cont=$(array_contains $data_op ${DFDB_SH_OPS[@]})
        if [[ $cont != true ]]; then
            DFDB_SH_CONF_ERROR=true
            echo "Data op \"$data_op\" does not exist " 1>&2
        fi
    done
    DFDB_SH_OPS=("${ops_tmp[@]}")
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
        -P=*|--port=*)
        EXTENSION="${option#*=}"
        DFDB_SH_PORT=$EXTENSION
        ;;
        -p=*|--password=*)
        EXTENSION="${option#*=}"
        DFDB_SH_PASSWORD=$EXTENSION
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
        -t|--time)
        DFDB_SH_PERF=true
        DFDB_SH_NUM_REP=5
        ;;
        -f|--full_exps)
        DFDB_SH_NUM_REP=5
        ;;
        -s|--sin_vals)
        DFDB_SH_SIN_VALS=true
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

function compare_precisions()
{
    local dump_test="$1"
    local path_comp="$2"
    local dump_lmfao="$3"
    local data_op="$4"
    [[ $DFDB_SH_PRECISION  == true ]] && {
      echo '*********comparison test started**********'
      echo $dump_test
      echo $path_comp
      python3 "${DFDB_SH_LA_SCRIPT}/precision_comparison.py" \
              -lp "${dump_lmfao}" -cp "${dump_test}"         \
              -pr 1e-10 --output_file "${path_comp}"         \
              --operation "$data_op"
      echo '*********comparison test finished**********'
    }
}
# TODO: Refactor this properly.

function update_times()
{
    local time_test="$1"
    local log_test="$2"
    local data_op="$3"
    local data_set="$4"
    local data_set_idx="$5"
    [[ $DFDB_SH_PERF  == true ]] && {
      echo '*********perf test update**********'
      echo $time_test
      echo $log_test
        python3 "${DFDB_SH_LA_SCRIPT}/time_comparison.py" \
        -t "${time_test}" -i "${log_test}"      \
        -op "$data_op" -ds "$data_set" -s "$data_set_idx"
      echo '*********perf test finished**********'
    }
}


function eval_time()
{
    module=$1
    # Redirects stderr (to which time is output) to stdout and drops stdout.
    eval_output=$(eval "${DFDB_TIME}" "${@:2}" 2>&1 1>/dev/null)
    # If there any other errors in the output, just extract the last line whic
    # contains the time.
    time_meas=$(echo $eval_output | grep -Eo '[0-9]+\.?[0-9]*$')
    echo '##LMFAO## ' $module ' ##' "$time_meas"
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

#TODO: If you have time to play, play with refactoring.
function build_and_run_tests() {
    local data_set=$1
    local data_op=$2
    local data_set_idx=$3

    echo "Data set id" $data_set_idx
    local features_out=$(printf "%s," ${DFDB_SH_FEATURES[@]})
    features_out=${features_out::-1}
    echo 'Features: ' ${features_out}
    echo 'Number of categorical feats: '${#DFDB_SH_FEATURES[@]}

    local features_cat_out=$(printf "%s," ${DFDB_SH_FEATURES_CAT[@]})
    features_cat_out=${features_cat_out::-1}
    echo 'Categorical features: ' ${features_cat_out}
    echo 'Number of categorical feats: '${#DFDB_SH_FEATURES_CAT[@]}
    local data_op_rec=$data_op
    [[ $data_op == svd* ]] && data_op="svd"
    [[ $data_op == qr* ]] && data_op="qr"

    local log_lmfao=${DFDB_SH_LOG_PATH}/lmfao/log"${data_set}${data_op}".txt
    local log_madlib=${DFDB_SH_LOG_PATH}/madlib/log"${data_set}${data_op}".txt
    local log_eigen=${DFDB_SH_LOG_PATH}/eigen/log"${data_set}${data_op}".txt
    local log_r=${DFDB_SH_LOG_PATH}/r/log"${data_set}${data_op}".txt
    local log_numpy=${DFDB_SH_LOG_PATH}/numpy/log"${data_set}${data_op}".txt
    local log_scipy=${DFDB_SH_LOG_PATH}/scipy/log"${data_set}${data_op}".txt

    local dump_lmfao=${DFDB_SH_DUMP_PATH}/lmfao/dump"${data_set}${data_op}".txt
    local dump_madlib=${DFDB_SH_DUMP_PATH}/madlib/dump"${data_set}${data_op}".txt
    local dump_eigen=${DFDB_SH_DUMP_PATH}/eigen/dump"${data_set}${data_op}".txt
    local dump_r=${DFDB_SH_DUMP_PATH}/r/dump"${data_set}${data_op}".txt
    local dump_numpy=${DFDB_SH_DUMP_PATH}/numpy/dump"${data_set}${data_op}".txt
    local dump_scipy=${DFDB_SH_DUMP_PATH}/scipy/dump"${data_set}${data_op}".txt

    local comp_madlib=${DFDB_SH_COMP_PATH}/madlib/comp"${data_set}${data_op}"
    local comp_numpy=${DFDB_SH_COMP_PATH}/numpy/comp"${data_set}${data_op}"
    local comp_eigen=${DFDB_SH_COMP_PATH}/eigen/comp"${data_set}${data_op}"
    local comp_scipy=${DFDB_SH_COMP_PATH}/scipy/comp"${data_set}${data_op}"
    local comp_r=${DFDB_SH_COMP_PATH}/r/comp"${data_set}${data_op}"

    local time_lmfao=${DFDB_SH_TIME_PATH}/timelmfao".xlsx"
    local time_madlib=${DFDB_SH_TIME_PATH}/timemadlib".xlsx"
    local time_eigen=${DFDB_SH_TIME_PATH}/timeigen".xlsx"
    local time_numpy=${DFDB_SH_TIME_PATH}/timenumpy".xlsx"
    local time_scipy=${DFDB_SH_TIME_PATH}/timescipy".xlsx"
    local time_r=${DFDB_SH_TIME_PATH}/timer".xlsx"

    local dump_opt=""
    [[ $DFDB_SH_DUMP == true ]] && dump_opt="--dump"

    [[ $DFDB_SH_LMFAO  == true ]] && {
        echo '*********LMFAO test started**********'
        (source test_lmfaola.sh ${data_set} ${data_op_rec} ${DFDB_SH_DUMP} \
                "${dump_lmfao}" &> ${log_lmfao})
        echo '*********LMFAO test finished**********'
        update_times $time_lmfao $log_lmfao $data_op $data_set $data_set_idx
    }


    [[ $DFDB_SH_MADLIB  == true ]] && {
        echo '*********Madlib test started**********'
        (source la_madlib.sh ${data_set} ${features_cat_out} ${data_op} \
            ${DFDB_SH_DUMP}  ${dump_madlib} &> ${log_madlib})
        echo '*********Madlib test finished**********'
        compare_precisions "${dump_madlib}" "${comp_madlib}" "${dump_lmfao}" $data_op
        update_times "$time_madlib" "$log_madlib" $data_op $data_set $data_set_idx
    }

    [[ $DFDB_SH_EIGEN  == true ]] && {
        echo '*********Eigen test started**********'
        (source la_eigen.sh ${data_set} ${data_op} ${features_out} ${features_cat_out} \
                "${DFDB_SH_DUMP}" "${dump_eigen}" "${DFDB_SH_NUM_REP}" &> ${log_eigen})
        echo '*********Eigen test finished**********'
        compare_precisions "${dump_eigen}" "${comp_eigen}" "${dump_lmfao}" $data_op
        update_times "$time_eigen" "$log_eigen" $data_op $data_set $data_set_idx
    }

    [[ $DFDB_SH_R  == true ]] && {
        echo '*********R test started**********'
        eval ${DFDB_TIME} Rscript "${DFDB_SH_LA_SCRIPT}/la.R ${DFDB_SH_JOIN_RES_PATH} \
                ${data_op} ${#DFDB_SH_FEATURES[@]} ${#DFDB_SH_FEATURES_CAT[@]}        \
                ${DFDB_SH_NUM_REP}  ${DFDB_SH_DUMP}  ${dump_r} ${DFDB_SH_SIN_VALS}    \
                ${DFDB_SH_FEATURES[@]} ${DFDB_SH_FEATURES_CAT[@]}" &> ${log_r}
        echo '*********R test finished**********'
        compare_precisions "${dump_r}" "${comp_r}" "${dump_lmfao}" $data_op
        update_times "$time_r" "$log_r" $data_op $data_set $data_set_idx
    }

    [[ $DFDB_SH_NUMPY  == true ]] && {
        echo '*********numpy test started**********'
        eval ${DFDB_TIME} python3 "${DFDB_SH_LA_SCRIPT}/la_python.py" \
                      -f ${features_out} -c ${features_cat_out}       \
                          -d "${DFDB_SH_JOIN_RES_PATH}" -o "$data_op" \
                          -n "${DFDB_SH_NUM_REP}"  "${dump_opt}"      \
                          --dump_file "${dump_numpy}" -s "numpy"      \
                          --sin_vals "${DFDB_SH_SIN_VALS}"            \
                           &> ${log_numpy}
        echo '*********numpy test finished**********'
        compare_precisions "${dump_numpy}" "${comp_numpy}" "${dump_lmfao}" $data_op
        update_times "$time_numpy" "$log_numpy" $data_op $data_set $data_set_idx

    }
    [[ $DFDB_SH_SCIPY  == true ]] && {
        echo '*********scipy test started**********'
        eval ${DFDB_TIME} python3 "${DFDB_SH_LA_SCRIPT}/la_python.py" \
                  -f ${features_out} -c ${features_cat_out}  \
                  -d "${DFDB_SH_JOIN_RES_PATH}" -o "$data_op"\
                  -n "${DFDB_SH_NUM_REP}"  "${dump_opt}"     \
                  --dump_file "${dump_scipy}" -s "scipy"     \
                  --sin_vals "${DFDB_SH_SIN_VALS}"           \
                   &> ${log_scipy}
        echo '*********scipy test finished**********'
        compare_precisions "${dump_scipy}" "${comp_scipy}" "${dump_lmfao}" $data_op
        update_times "$time_scipy" "$log_scipy" $data_op $data_set $data_set_idx
    }
}

# TODO: Change ** with nice dash style
function main() {
    init_global_paths
    init_global_vars

    get_str_args "$@"

    [[ $DFDB_SH_CONF_ERROR == true ]] && return
    [[ $DFDB_SH_HELP_SHOW == true ]] && return

    cd "${DFDB_SH_LA_BUILD}"
    cmake .. -DLMFAO_LIB:BOOL=ON -DLMFAO_RUN:BOOL=OFF -DLMFAO_TEST:BOOL=OFF
    make -j8
    cd "${DFDB_SH_LMFAO_BUILD}"
    cmake ..
    make -j8
    mv multifaq ..
    cd $DFDB_SH_LA_SCRIPT
    for data_set in ${DFDB_SH_DATA_SETS[@]}; do
        data_set_idx=$(( ${DFDB_SH_DATA_SETS_FULL[$data_set]} ))
        if [[ $data_set_idx == 0 ]]; then
            echo "Data set" $data_set
            continue
        fi
        DFDB_SH_DB=${data_set}
        echo ""
        echo tests for data set "$data_set" are starting;
        data_path=$DFDB_SH_DATA"/"$data_set
        get_features $data_path

        # Necessary in the case if we break script using ctrl + c
        dropdb $DFDB_SH_DB -U $DFDB_SH_USERNAME -p $DFDB_SH_PORT
        createdb $DFDB_SH_DB -U $DFDB_SH_USERNAME -p $DFDB_SH_PORT
        local log_psql=${DFDB_SH_LOG_PATH}/psql/log"${data_set}".txt
        local time_psql=${DFDB_SH_TIME_PATH}/timepsql".xlsx"
        [[ $DFDB_SH_JOIN  == true ]] && {
            echo '*********Join started**********'
            (source generate_join.sh ${data_set}  &> ${log_psql})
            echo '*********Join finished**********'
            update_times "$time_psql" "$log_psql" "$data_op" $data_set $data_set_idx
        }
        dropdb $DFDB_SH_DB -U $DFDB_SH_USERNAME -p $DFDB_SH_PORT
        # Needed for MADlib
        # Even if it remains, it will be empty.
        createdb $DFDB_SH_DB -U $DFDB_SH_USERNAME -p $DFDB_SH_PORT

        for data_op in ${DFDB_SH_OPS[@]}; do
            echo "*********${data_op} decomposition**********"
            build_and_run_tests $data_set $data_op $data_set_idx
            echo "*********${data_op} decomposition**********"
        done
        dropdb $DFDB_SH_DB -U $DFDB_SH_USERNAME -p $DFDB_SH_PORT



        # Normal removal of database.

    done
}
# Example:  ./runTests.sh --build=n -o=svd -d=usretailer_35f_1 -r=/home/max/LMFAO -f
main $@
