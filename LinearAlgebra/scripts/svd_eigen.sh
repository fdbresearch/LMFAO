#!/bin/bash
DFDB_SH_DATA_SET=$1
g++ -std=c++14 -O3 -pthread -mtune=native -ftree-vectorize -fopenmp lmfao_qr_eigen.cpp  ../../libs/libboost_program_options.a -o lmfao_qr_eigen
eval ${DFDB_TIME} ./lmfao_qr_eigen -p $DFDB_SH_JOIN_RES_PATH -d qr
#eval ${DFDB_TIME} ./lmfao_qr_eigen -p DFDB_SH_JOIN_RES_PATH -d svd
