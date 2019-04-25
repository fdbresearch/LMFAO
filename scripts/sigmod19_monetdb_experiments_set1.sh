#!/bin/bash

## IMPORTANT: Make sure that MonetDB is installed, and dataabse sigmod19 is running! 

SQL_SCRIPT=aggregates.sql
CLEANUP_SCRIPT=aggregate_cleanup.sql

echo "$SQL_SCRIPT"

for dataset in retailer favorita tpc-ds yelp; 
do 
	mclient -d sigmod19 < ~/benchmarking/datasets/"$dataset"/monetdb/load_data.sql 

	for model in covar count rtree mi cube;
	do 
		LOG_FILE=sigmod19_experiments_set1_"$dataset"_"$model"_monetdb.log

		echo " MonetDB -- sql -- $dataset -- $model" >> runtime/"$LOG_FILE"

		./multifaq --path ../benchmarking/datasets/"$dataset" --model "$model" --codegen sql --feat config_sigmod19/features_"$model".conf >> log_multifaq_sigmod19_experiments_set1_"$dataset"_"$model"_monetdb.log

		cat compiler-data.out >> runtime/"$LOG_FILE"

		cd runtime/sql/

		## RUN MONETDB EXPERIMENTS
		rm times.txt

		echo "Run $SQL_SCRIPT - MonetDB - $dataset - $mdel " >> ../"$LOG_FILE"

		for i in {1..3}
		do
		      echo "Run $i $SQL_SCRIPT  - MonetDB - $dataset - $model" >> ../../log_multifaq_sigmod19_experiments_set1_"$dataset"_"$model"_monetdb.log
		      /usr/bin/time -f "%e %P %I %O" -o "times.txt" -a mclient -d sigmod19 < $SQL_SCRIPT > /dev/null
		      mclient -d sigmod19 < $CLEANUP_SCRIPT > /dev/null
		done

		cat times.txt >> ../"$LOG_FILE"
    
		awk -f ~/LMFAO/scripts/awk_average.awk times.txt >> ../"$LOG_FILE"

		rm times.txt
		cd ../../
	done

	mclient -d sigmod19 < ~/benchmarking/datasets/"$dataset"/monetdb/drop_data.sql
done 
