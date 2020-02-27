#!/bin/bash

PARALLEL=both

for dataset in retailer favorita yelp tpc-ds; 
do 
    LOG_FILE=sigmod19_experiments_compilation_"$dataset".log

    for model in covar count rtree mi cube;
    do 
	./multifaq --path ../benchmarking/datasets/"$dataset" --model "$model" --parallel "$PARALLEL" --feat config_sigmod19/features_"$model".conf >> log_sigmod19_experiments_compilation_"$dataset".log

	echo "$dataset - compilation - $model" >> runtime/"$LOG_FILE"
	cat compiler-data.out >> runtime/"$LOG_FILE"

	cd runtime/cpp/
	rm times.txt

	for r in {1..5}
	do
	    make -j precomp
	    /usr/bin/time -f "%e " -o "time_to_compile.txt" make -j8
	    cat time_to_compile.txt >> times.txt
	    make clean
	done 

	cat times.txt >> ../"$LOG_FILE"

	awk -f ~/LMFAO/scripts/awk_average.awk times.txt >> ../"$LOG_FILE"

	rm times.txt
	cd - 
	done
done
