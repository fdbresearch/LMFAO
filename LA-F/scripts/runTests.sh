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
    # Reset
    DFDB_SH_COLOR_OFF='\033[0m'       # Text Reset

    # Regular Colors
    Black='\033[0;30m'        # Black
    Red='\033[0;31m'          # Red
    DFDB_SH_GREEN='\033[0;32m' # DFDB_SH_GREEN
    Yellow='\033[0;33m'       # Yellow
    Blue='\033[0;34m'         # Blue
    Purple='\033[0;35m'       # Purple
    Cyan='\033[0;36m'         # Cyan
    White='\033[0;37m'

    DFDB_SH_IN_STR="-- "
    DFDB_SH_INFO_STR="[------] "
    DFDB_SH_RUN_STR="[RUN    ] "
    DFDB_SH_END_STR="[    END] "
    DFDB_TIME="/usr/bin/time -f \"%e\""
    DFDB_SH_DB="usretailer"
    DFDB_SH_USERNAME=$(whoami)
    DFDB_SH_PASSWORD=""
    DFDB_SH_PORT=5432
    declare -gA DFDB_SH_DATA_SETS_FULL
    DFDB_SH_DATA_SETS_FULL=( [usretailer_35f_1]=1 [usretailer_35f_10]=2 [usretailer_35f_100]=3 [usretailer_35f_1000]=4 [usretailer_35f]=5 [favorita_7f_30]=6 [favorita_7f_300]=7 [favorita_7f_1000]=8)
    DFDB_SH_DATA_SETS=("${!DFDB_SH_DATA_SETS_FULL[@]}")
    #TODO: Create one array that will represent decomposition (qr/svd)
    # Create another array that will represent flavor (chol, ...).
    DFDB_SH_OPS=("qr" "qr_chol" "qr_mul_t" "svd" "svd_qr" "svd_qr_chol" "svd_eig_dec" "svd_alt_min")
    DFDB_SH_DEC_OPS=()
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
    DFDB_SH_RUN=false

    DFDB_SH_DUMP=false
    DFDB_SH_NUM_REP=1
    DFDB_SH_CLEAN=false
    DFDB_SH_HELP_SHOW=false
    DFDB_SH_HELP_TXT=$"
Usage: runTests [-h --help] [-b|--build =<OPTS>]
[-r|--root=<PATH>] [-u|--user=<NAME>] [-p|--port=<NUMBER>]
[-o|--operation=<NAME1>, <NAME2>,...>]
[-d|--data_sets=<NAME1>,<NAME2>,...>][-f|--full_exps]

Run experiments for matrix factorizations.

Mandatory arguments to long options are mandatory for short options too.
    -b, --build=<OPTS>           Implementations to be run in the experiments stated in opts:
                                 j - join tables using PSQL and export the joined result
                                 l - factorizations using LA-F (linear algebra on top of LMFAO)
                                 r - factorizations implemented in r
                                 n - factorizations implemented in numpy
                                 s - factorizations implemented in scipy
                                 e - factorizations implemented using eigen and C++
                                 m - factorizations implemented using MADlib
    -r, --root=<PATH>            set the root path to PATH of LMFAO project
    -u, --user=<NAME>            connect to PSQL datatabase with the username NAME.
    -p, --port=<NUMBER>          connect to PSQL database on the port NUMBER.
    -o, --operation=<NAME1>,...> Decompositons to be run in experiments factorization NAME1, NAME2,...
                                 Possible operations are qr and svd.
    -d, --data_sets=<NAME1>,...> run experiments for data sets NAME1, NAME2,...
                                 located on root of LMFAO project/data.
    -f, --full_exps              Run multiple times the same experiment to reduce cache
                                 locality and statistics problems. All times
                                 will be outputed.
    -t, --time                   Create an xlsxs with times for corresponding operations,
                                 builds and data_sets. Note: --ful_exps
                                 has to be enabled for 5 time repeatition.
    --run                        Run experiments for corresponding operations,
                                 builds and data_sets.
    --dump                       If we should dump R in QR decomposition/singular values to a file.
    --precision                  Compare R/singular values computed by LMFAOs algorithm with ones computed
                                 by other implementatiuons (python/r/...) and create corresponding xlsxs and
                                 .txt files with comparisons. Note: dump files have to exist.
    -s, --sin_vals               Some algorithms from competitors run faster if only singular values
                                 are needed to be calculated. This flag, enables that option.
    --clean                      WARNING: Removes all dumps/txts/xlsx produced by this script.
    -h, --help                   show help.
"
}

function indent()
{
    DFDB_SH_IN_STR="${DFDB_SH_IN_STR}  "
}
function unindent()
{
    DFDB_SH_IN_STR="${DFDB_SH_IN_STR:0:-2}"
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

function get_decomp_ops()
{
    for data_op in ${DFDB_SH_OPS[@]}; do
        local decomp_op=""
        [[ $data_op == svd* ]] && decomp_op="svd"
        [[ $data_op == qr* ]] && decomp_op="qr"
        cont=$(array_contains $decomp_op ${DFDB_SH_DEC_OPS[@]})
        if [[ $cont != true ]]; then
            DFDB_SH_DEC_OPS+=($decomp_op)
        fi
    done
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
    get_decomp_ops
    echo ${DFDB_SH_DEC_OPS[@]}

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
        --run)
        DFDB_SH_RUN=true
        ;;
        --dump)
        DFDB_SH_DUMP=true
        ;;
        --precision)
        DFDB_SH_PRECISION=true
        ;;
        -t|--time)
        DFDB_SH_PERF=true
        ;;
        -f|--full_exps)
        DFDB_SH_NUM_REP=5
        ;;
        -s|--sin_vals)
        DFDB_SH_SIN_VALS=true
        ;;
        --clean)
        DFDB_SH_CLEAN=true
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
    echo "${DFDB_SH_INFO_STR}Features path: $file_name"

    local regex_num="^([0-9]+)(, ?)?([0-9]+)(, ?)?([0-9]+)(, ?)?$"
    local regex_features="^([a-zA-Z_]+):[0-9]+:[a-zA-Z_]+$"
    local regex_cat_features="^([a-zA-Z_]+):([1-9][0-9]*):[a-zA-Z_]+$"
    local features=()
    features_cat=()
    while IFS='' read -r line || [[ -n "$line" ]]; do
        if [[ $line =~ $regex_num ]]; then
            NUM_FEATURES=${BASH_REMATCH[1]}
            echo "${DFDB_SH_INFO_STR}Number of features: ${line}"
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


function build_lmfao_and_la()
{
    cd "${DFDB_SH_LA_BUILD}"
    cmake .. -DLMFAO_LIB:BOOL=ON -DLMFAO_RUN:BOOL=OFF -DLMFAO_TEST:BOOL=OFF
    make -j8
    cd "${DFDB_SH_LMFAO_BUILD}"
    cmake ..
    make -j8
    mv multifaq ..
}

function clean_intermediate()
{
    find $DFDB_SH_TIME_PATH -name "*.xlsx" -type f -delete
    find $DFDB_SH_COMP_PATH -name "*.xlsx" -type f -delete
    find $DFDB_SH_COMP_PATH -name "*.txt" -type f -delete
    find $DFDB_SH_DUMP_PATH -name "*.txt" -type f -delete
    find $DFDB_SH_LOG_PATH -name "*.txt" -type f -delete
}

function compare_precisions()
{
    local test_dump="$1"
    local dump_lmfao="$2"
    local test_path_comp="$3"
    local decomp_op_alg="$4"
    local test_comp_precs="$5"
    local precs_txt="${test_path_comp}.txt"
    if [[ $DFDB_SH_PRECISION  == true ]]; then
        echo "${DFDB_SH_IN_STR}${DFDB_SH_RUN_STR}comparison test"
        indent
        echo "${DFDB_SH_IN_STR}$test_dump"
        echo "${DFDB_SH_IN_STR}$test_path_comp"
        echo "${DFDB_SH_IN_STR}$dump_lmfao"

        python3 "${DFDB_SH_LA_SCRIPT}/precision_comparison.py" \
              -lp "${dump_lmfao}" -cp "${test_dump}"           \
              -pr 1e-10 --output_file "${test_path_comp}"      \
              --operation "$decomp_op_alg"
        echo "${DFDB_SH_IN_STR}$precs_txt"
        echo "${DFDB_SH_IN_STR}$test_comp_precs"
        python3 "${DFDB_SH_LA_SCRIPT}/precision_formating.py"                    \
                --prec_file_for "${test_comp_precs}" --precs_dump "${precs_txt}" \
                -op "$decomp_op_alg" -ds "$data_set" -s "$data_set_idx"          \
                --rename False -cp $DFDB_SH_COMP_PATH

        unindent
        echo "${DFDB_SH_IN_STR}${DFDB_SH_END_STR}comparison test"
    fi
}
# TODO: Refactor this properly.

function update_times()
{
    local time_test="$1"
    local log_test="$2"
    local data_op="$3"
    local data_set="$4"
    local data_set_idx="$5"
    if [[ $DFDB_SH_PERF  == true ]]; then
        echo "${DFDB_SH_IN_STR}${DFDB_SH_RUN_STR}perf test"
        indent
        echo "${DFDB_SH_IN_STR}$time_test"
        echo "${DFDB_SH_IN_STR}$log_test"
        python3 "${DFDB_SH_LA_SCRIPT}/time_comparison.py" \
        -t "${time_test}" -i "${log_test}"      \
        -op "$data_op" -ds "$data_set" -s "$data_set_idx"
        unindent
        echo "${DFDB_SH_IN_STR}${DFDB_SH_END_STR}perf test"
    fi
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

function eval_numpy()
{
    local test_log=$1
    local decomp_op=$2
    local features_out=$3
    local features_cat_out=$4
    local test_dump=$5
    local dump_opt=""
    [[ $DFDB_SH_DUMP == true ]] && dump_opt="--dump"
    echo "$test_log" >>/dev/stderr
    eval ${DFDB_TIME} python3 "${DFDB_SH_LA_SCRIPT}/la_python.py" \
          -f ${features_out} -c ${features_cat_out}               \
          -d "${DFDB_SH_JOIN_RES_PATH}" -o "$decomp_op"           \
          -n "${DFDB_SH_NUM_REP}"  "${dump_opt}"                  \
          --dump_file "${test_dump}" -s "numpy"                  \
          --sin_vals "${DFDB_SH_SIN_VALS}" &> ${test_log}
}

function eval_scipy()
{
    local test_log=$1
    local decomp_op=$2
    local features_out=$3
    local features_cat_out=$4
    local test_dump=$5
    local data_set=$6
    local dump_opt=""
    [[ $DFDB_SH_DUMP == true ]] && dump_opt="--dump"
     eval ${DFDB_TIME} python3 "${DFDB_SH_LA_SCRIPT}/la_python.py" \
                  -f ${features_out} -c ${features_cat_out}        \
                  -d "${DFDB_SH_JOIN_RES_PATH}" -o "$decomp_op"    \
                  -n "${DFDB_SH_NUM_REP}"  "${dump_opt}"           \
                  --dump_file "${test_dump}" -s "scipy"            \
                  --sin_vals "${DFDB_SH_SIN_VALS}" &> ${test_log}
}

function eval_r()
{
    local test_log=$1
    local decomp_op=$2
    local features_out=$3
    local features_cat_out=$4
    local test_dump=$5
    local data_set=$6
    eval ${DFDB_TIME} Rscript "${DFDB_SH_LA_SCRIPT}/la.R ${DFDB_SH_JOIN_RES_PATH} \
            ${decomp_op} ${#DFDB_SH_FEATURES[@]} ${#DFDB_SH_FEATURES_CAT[@]}      \
            ${DFDB_SH_NUM_REP}  ${DFDB_SH_DUMP}  ${test_dump} ${DFDB_SH_SIN_VALS} \
            ${DFDB_SH_FEATURES[@]} ${DFDB_SH_FEATURES_CAT[@]}" &> ${test_log}
}

function eval_eigen()
{
    local test_log=$1
    local decomp_op=$2
    local features_out=$3
    local features_cat_out=$4
    local test_dump=$5
    local data_set=$6
    (source la_eigen.sh ${data_set} ${decomp_op} ${features_out} ${features_cat_out} \
                "${DFDB_SH_DUMP}" "${test_dump}" "${DFDB_SH_NUM_REP}" &> ${test_log})
}

function eval_madlib()
{
    local test_log=$1
    local decomp_op=$2
    local features_out=$3
    local features_cat_out=$4
    local test_dump=$5
    local data_set=$6
    (source la_madlib.sh ${data_set} ${features_cat_out} ${decomp_op} \
            ${DFDB_SH_DUMP}  ${test_dump} &> ${test_log})
}

function eval_lmfao()
{
    local test_log=$1
    local decomp_op=$2
    local features_out=$3
    local features_cat_out=$4
    local test_dump=$5
    local data_set=$6
    (source test_lmfaola.sh ${data_set} ${decomp_op} ${DFDB_SH_DUMP} \
                "${test_dump}" &> ${test_log})
}

function eval_run()
{
    local fun_name=$1
    local test_log=$2
    local decomp_op=$3
    local features_out=$4
    local features_cat_out=$5
    local test_dump=$6
    local data_set=$7
    if [[ $DFDB_SH_RUN == true ]]; then
        eval $fun_name $test_log $decomp_op $features_out \
            $features_cat_out $test_dump $data_set
    fi
}

# In comparisons directory for specific test and specific algorithm
# an xlsx file be created with comparsion and .txt with additional information
# In times directory timetest xlsx will keep information about running times of specific
# test, more precisely sheets will contain times of specific test.
# Parameter decomp_op_alg is only imporant in the case of comparison of precisions
function eval_test()
{
    local test_name="$1"
    local data_set="$2"
    local data_sed_idx="$3"
    local decomp_op="$4"
    local decomp_op_alg="$5"
    local features_out="$6"
    local features_cat_out="$7"
    local test_compute_b="$8"
    local test_compare_b="$9"
    local test_time_b="${10}"
    local test_run_name="DFDB_SH_${test_name^^}"
    local test_run=${!test_run_name}

    local test_dump=${DFDB_SH_DUMP_PATH}/${test_name}/dump-"${data_set}-${decomp_op}".txt
    local test_log=${DFDB_SH_LOG_PATH}/${test_name}/log"-${data_set}-${decomp_op}".txt
    local test_comp=${DFDB_SH_COMP_PATH}/${test_name}/comp-"${data_set}-${decomp_op_alg}"
    local dump_lmfao=${DFDB_SH_DUMP_PATH}/lmfao/dump-"${data_set}-${decomp_op_alg}".txt
    local test_prec=${DFDB_SH_COMP_PATH}/comp-${test_name}".xlsx"
    local test_time=${DFDB_SH_TIME_PATH}/time-${test_name}".xlsx"
    local fun_name=eval_${test_name}


    if [[ $test_run  == true ]]; then
        if [[ $test_compute_b == true ]]; then
            echo "${DFDB_SH_IN_STR}${DFDB_SH_RUN_STR}$test_name test started"
            eval_run $fun_name $test_log $decomp_op $features_out \
                 $features_cat_out $test_dump $data_set
            echo "${DFDB_SH_IN_STR}${DFDB_SH_END_STR}$test_name test finished"
        fi
        if [[ $test_compare_b == true ]]; then
            compare_precisions "${test_dump}" "${dump_lmfao}" "${test_comp}" \
                                $decomp_op_alg "${test_prec}"
        fi
        #if [[ $test_name != "lmfao" ]]; then
        #fi
        if [[ $test_time_b == true ]]; then
            update_times "$test_time" "$test_log" $decomp_op $data_set $data_set_idx
        fi
    fi
}

#decomp_op_alg is only important in the case of comparison of precisions
function build_and_run_tests() {
    local data_set=$1
    local decomp_op_alg=$2
    local data_set_idx=$3
    local features_out=$4
    local features_cat_out=$5
    local test_compute=$6
    local test_comp=$7
    local test_time=$8
    local decomp_op=""
    # TODO: Refactor this such that regex extracts.
    [[ $decomp_op_alg == svd* ]] && decomp_op="svd"
    [[ $decomp_op_alg == qr* ]] && decomp_op="qr"
    # DecompOp for LMFAO is the same decomp_op_alg because there are different implementations
    #test_compute 'lmfao' $data_set $data_set_idx $decomp_op_alg $decomp_op_alg  \
    #                  $features_out $features_cat_out
    eval_test 'numpy' $data_set $data_set_idx $decomp_op $decomp_op_alg      \
                      $features_out $features_cat_out $test_compute $test_comp $test_time
    eval_test 'scipy' $data_set $data_set_idx $decomp_op $decomp_op_alg      \
                      $features_out $features_cat_out $test_compute $test_comp $test_time
    eval_test 'r' $data_set $data_set_idx $decomp_op $decomp_op_alg          \
                  $features_out $features_cat_out $test_compute $test_comp $test_time
    eval_test 'eigen' $data_set $data_set_idx $decomp_op  $decomp_op_alg     \
                     $features_out $features_cat_out $test_compute $test_comp $test_time
    eval_test 'madlib' $data_set $data_set_idx $decomp_op  $decomp_op_alg    \
                     $features_out $features_cat_out $test_compute $test_comp $test_time
}


# TODO: Change ** with nice dash style
function main() {
    init_global_paths
    init_global_vars

    get_str_args "$@"

    [[ $DFDB_SH_CONF_ERROR == true ]] && return
    [[ $DFDB_SH_HELP_SHOW == true ]] && return
    build_lmfao_and_la
    [[ $DFDB_SH_CLEAN == true ]] && clean_intermediate

    cd $DFDB_SH_LA_SCRIPT

    for data_set in ${DFDB_SH_DATA_SETS[@]}; do
        data_set_idx=$(( ${DFDB_SH_DATA_SETS_FULL[$data_set]} ))
        if [[ $data_set_idx == 0 ]]; then
            echo "Data set" $data_set
            continue
        fi
        DFDB_SH_DB=${data_set}
        echo -e "${DFDB_SH_GREEN}\c"
        echo "[======] Tests for data set $data_set are starting"
        echo -e "${DFDB_SH_COLOR_OFF}\c"
        data_path=$DFDB_SH_DATA"/"$data_set
        get_features $data_path

        # Feature datasets creation
        echo "${DFDB_SH_INFO_STR}Data set id: $data_set_idx"
        local features_out=$(printf "%s," ${DFDB_SH_FEATURES[@]})
        features_out=${features_out::-1}
        echo "${DFDB_SH_INFO_STR}Features:  ${features_out}"
        echo "${DFDB_SH_INFO_STR}Number of feats: ${#DFDB_SH_FEATURES[@]}"

        local features_cat_out=$(printf "%s," ${DFDB_SH_FEATURES_CAT[@]})
        features_cat_out=${features_cat_out::-1}
        echo "${DFDB_SH_INFO_STR}Categorical features:  ${features_cat_out}"
        echo "${DFDB_SH_INFO_STR}Number of categorical feats: ${#DFDB_SH_FEATURES_CAT[@]}"


        # Necessary in the case if we break script using ctrl + c
        # Join using corresponding SQL engine.
        dropdb $DFDB_SH_DB -U $DFDB_SH_USERNAME -p $DFDB_SH_PORT
        createdb $DFDB_SH_DB -U $DFDB_SH_USERNAME -p $DFDB_SH_PORT
        local log_psql=${DFDB_SH_LOG_PATH}/psql/log"${data_set}".txt
        local time_psql=${DFDB_SH_TIME_PATH}/timepsql".xlsx"
        if [[ $DFDB_SH_JOIN  == true ]]; then
            echo "${DFDB_SH_IN_STR}${DFDB_SH_RUN_STR}Joinning"
            (source generate_join.sh ${data_set}  &> ${log_psql})
            echo "${DFDB_SH_IN_STR}${DFDB_SH_END_STR}Joinning"
            update_times "$time_psql" "$log_psql" "$data_op" $data_set $data_set_idx
        fi
        dropdb $DFDB_SH_DB -U $DFDB_SH_USERNAME -p $DFDB_SH_PORT
        # Needed for MADlib
        # Even if it remains, it will be empty.
        createdb $DFDB_SH_DB -U $DFDB_SH_USERNAME -p $DFDB_SH_PORT

        # TODO: Create only array of deocmp ops to iterate over
        echo ""; echo "${DFDB_SH_IN_STR}Runtime LMFAO experiments started"
        indent
        for data_op in ${DFDB_SH_OPS[@]}; do
            eval_test 'lmfao' $data_set $data_set_idx $data_op $data_op  \
                              $features_out $features_cat_out true false true
        done
        unindent
        echo "${DFDB_SH_IN_STR}Runtime LMFAO experiments finished";echo ""
        echo "${DFDB_SH_IN_STR}Runtime experiments of competitors started"
        indent
        for decomp_op in ${DFDB_SH_DEC_OPS[@]}; do
            echo "${DFDB_SH_IN_STR}${DFDB_SH_RUN_STR}${decomp_op} decomposition"
            indent
            build_and_run_tests $data_set $decomp_op $data_set_idx $features_out $features_cat_out true false true
            unindent
            echo "${DFDB_SH_IN_STR}${DFDB_SH_END_STR}${decomp_op} decomposition"
        done
        unindent
        echo "${DFDB_SH_IN_STR}Runtime experiments of competitors finished";echo ""

        # Only used for comparison
        echo "${DFDB_SH_IN_STR}Precision testing"
        indent
        for data_op in ${DFDB_SH_OPS[@]}; do
            build_and_run_tests $data_set $data_op $data_set_idx $features_out $features_cat_out false true false
        done
        unindent
        echo "${DFDB_SH_IN_STR}Precision testing "

        dropdb $DFDB_SH_DB -U $DFDB_SH_USERNAME -p $DFDB_SH_PORT



        # Normal removal of database.

    done
}

# Example:  ./runTests.sh --build=n -o=svd -d=usretailer_35f_1 -r=/home/popina/LMFAO -f
main $@
