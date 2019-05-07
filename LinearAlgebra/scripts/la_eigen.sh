#!/bin/bash
DFDB_SH_DATA_SET=$1
DFDB_SH_DATA_OP=$2
g++ -std=c++14 -O3 -pthread -mtune=native -ftree-vectorize -fopenmp lmfao_la_eigen.cpp  ../../libs/libboost_program_options.a -o lmfao_la_eigen
eval ${DFDB_TIME} ./lmfao_la_eigen -p $DFDB_SH_JOIN_RES_PATH -d "$DFDB_SH_DATA_OP"
