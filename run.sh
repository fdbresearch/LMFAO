#!/bin/bash

cd runtime/inmemory/

version=$1

if [[ -n "$version" ]]; then
    make "$1"
if [ "$version" != "clean" ]; then
    ./lmfao
fi
else 
    make
    ./lmfao
fi

cd ../../