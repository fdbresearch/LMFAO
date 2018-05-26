#!/bin/bash

for v in 2,3,4,5,6
do
    cp data/usretailer/features"$v".conf data/usretailer/features.conf
    ./multifaq --path data/usretailer/ --model covar
    cd runtime/cpp/
    make -j

    echo "usretailer - features$v" >> times.txt
   
    for r in {1..5}
    do
        .lmfao
    done
done
