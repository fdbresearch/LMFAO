#!/bin/bash

data_sets=(usretailer_1 usretailer_10 usretailer_100 usretailer_1000 usretailer)
for data_set in ${data_sets[@]}; do
    echo data_set_name: "$data_set"; 
    log_name=log"${data_set}".txt
    sh runTest.sh ${data_set} > ${log_name}
done
