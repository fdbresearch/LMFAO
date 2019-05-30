#!/bin/bash
DFDB_SH_DATA_SET=$1
local data_operation=$2
local features=$3
local features_cat=$4
DFDB_SH_DUMP=$5
local dump_eigen=$6
local DFDB_SH_NUM_REP=$7

echo "Eigen" $dump_eigen
echo "Dump" $DFDB_SH_DUMP
g++ -std=c++14 -O3 -pthread -mtune=native -ftree-vectorize \
-fopenmp  -DEIGEN_USE_BLAS -DEIGEN_USE_LAPACKE             \
lmfao_la_eigen.cpp -lboost_program_options                 \
-lopenblas -llapacke  -o lmfao_la_eigen
eval ${DFDB_TIME} ./lmfao_la_eigen -p $DFDB_SH_JOIN_RES_PATH -o "$data_operation"        \
-f "${features}" -c "${features_cat}" --dump "${DFDB_SH_DUMP}" --dump_path "$dump_eigen" \
--num_it "${DFDB_SH_NUM_REP}"
