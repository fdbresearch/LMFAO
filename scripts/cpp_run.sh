#!/bin/bash

cd runtime/cpp/

version=$1

if [[ -n "$version" ]]; then
if [ "$version" == "make" ]; then
    make
else
    make "$1"
if [ "$version" != "clean" ]; then
    ./lmfao
fi
fi
else 
    make
    ./lmfao
fi

cd ../../
