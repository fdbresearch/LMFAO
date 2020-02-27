#!/bin/bash

PARALLEL=both

for dataset in retailer favorita; 
do 
	for model in rtree;
	do 
		for r in {1..3}
		do
	            LOG_FILE=sigmod19_experiments_ml_"$dataset"_"$model".log

		    ./multifaq --path ../benchmarking/datasets/"$dataset" --model "$model" --parallel "$PARALLEL" --feat config_sigmod19/features_"$model".conf >> log_multifaq_sigmod19_experiments_ml_"$dataset"_"$model".log

		    echo "$dataset - cpp - ml - $model" >> runtime/"$LOG_FILE"
		    cat compiler-data.out >> runtime/"$LOG_FILE"

		    cd runtime/cpp/

		    make -j precomp

		    # make LMFAO 
		    /usr/bin/time -f "%e " -o "time_to_compile.txt" make -j


		    g++ -std=c++11 -o treeLearner RegressionTreeLearner.cpp

		    cat time_to_compile.txt >> ../"$LOG_FILE"

		    rm times.txt
		    ./treeLearner >> ../../log_multifaq_sigmod19_experiments_ml_"$dataset"_"$model".log

		    cat times.txt >> ../"$LOG_FILE"

		    # awk -F$'\t' '{OFS = FS} {for(i=1; i<=NF; i++) {a[i]+=$i; if($i!="")
		    # b[i]++}}; END {for(i=1; i<=NF; i++) printf "%s%s", a[i]/b[i],
		    # (i==NF?ORS:OFS)}' times.txt

		    awk -f ~/LMFAO/scripts/awk_average.awk times.txt >> ../"$LOG_FILE"

		    rm times.txt
		    cd - 
		done
	done
done


for dataset in tpc-ds; 
do 
	for model in ctree;
	do 
		for r in {1..3}
		do
	            LOG_FILE=sigmod19_experiments_ml_"$dataset"_"$model".log

		    ./multifaq --path ../benchmarking/datasets/"$dataset" --model "$model" --parallel "$PARALLEL" --feat config_sigmod19/features_"$model".conf >> log_multifaq_sigmod19_experiments_ml_"$dataset"_"$model".log

		    echo "$dataset - cpp - ml - $model" >> runtime/"$LOG_FILE"
		    cat compiler-data.out >> runtime/"$LOG_FILE"

		    cd runtime/cpp/
		    
		    cp RegressionTreeHelper_tpcds.hpp  RegressionTreeHelper.hpp
		    
		    make -j precomp

		    # make LMFAO 
		    /usr/bin/time -f "%e " -o "time_to_compile.txt" make -j

		    g++ -std=c++11 -o treeLearner RegressionTreeLearner.cpp

		    cat time_to_compile.txt >> ../"$LOG_FILE"

		    rm times.txt
		    ./treeLearner >> ../../log_multifaq_sigmod19_experiments_ml_"$dataset"_"$model".log

		    cat times.txt >> ../"$LOG_FILE"

		    # awk -F$'\t' '{OFS = FS} {for(i=1; i<=NF; i++) {a[i]+=$i; if($i!="")
		    # b[i]++}}; END {for(i=1; i<=NF; i++) printf "%s%s", a[i]/b[i],
		    # (i==NF?ORS:OFS)}' times.txt

		    awk -f ~/LMFAO/scripts/awk_average.awk times.txt >> ../"$LOG_FILE"

		    rm times.txt
		    cd - 
		done
	done
done
