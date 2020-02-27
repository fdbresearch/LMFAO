#!/bin/bash

## To run this script give {path_to_data} and {model} as input parameters

echo "Redshift experiments"

SQL_SCRIPT=aggregates.sql
CLEANUP_SCRIPT=aggregate_cleanup.sql
HOST=sigmod19.caftjmyud2pr.eu-west-2.redshift.amazonaws.com
REDSHIFT_USER=max

for dataset in favorita retailer;
do 
    for model in covar rtree; #covar count rtree mi cube;
    do 
	LOG_FILE=sigmod19_experiments_set1_"$dataset"_"$model"_redshift.log

	echo "REDSHIFT -- sql -- $dataset -- $model" >> runtime/"$LOG_FILE"

	./multifaq --path ../benchmarking/datasets/"$dataset" --model "$model" --codegen sql --feat config_sigmod19/features_"$model".conf >> log_"$LOG_FILE"

	cat compiler-data.out >> runtime/"$LOG_FILE"

	cd runtime/sql/

	## RUN REDSHIFT EXPERIMENTS
	rm redshift_times.txt 

	echo "Run aggregates.sql - $dataset - $model"

	for i in {1..3}
	do
	    PGPASSWORD=Sigmod!2019. /usr/bin/time -f "%e %P %I %O" -o "redshift_times.txt" -a psql -h $HOST -U $REDSHIFT_USER -d sigmod -p 5439 -f $SQL_SCRIPT
	    PGPASSWORD=Sigmod!2019. psql -h $HOST -U $REDSHIFT_USER -d sigmod -p 5439 -f $CLEANUP_SCRIPT 
	done

	cat redshift_times.txt >> ../"$LOG_FILE"
	awk -f ~/LMFAO/scripts/awk_average.awk redshift_times.txt >> ../"$LOG_FILE" 

	rm redshift_times.txt 
	cd ../../
    done 

done 
