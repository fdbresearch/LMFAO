#!/bin/bash
DFDB_SH_DATA_SET=$1
g++ -std=c++14 -O3 -pthread -mtune=native -ftree-vectorize -fopenmp lmfao_qr_eigen.cpp  /usr/local/boost_1_68_0/bin.v2/libs/program_options/build/gcc-7.3.0/release/link-static/threading-multi/libboost_program_options.a -o lmfao_qr_eigen
eval ${DFDB_TIME} ./lmfao_qr_eigen -p $DFDB_SH_JOIN_RES_PATH -d qr
#eval ${DFDB_TIME} ./lmfao_qr_eigen -p DFDB_SH_JOIN_RES_PATH -d svd
