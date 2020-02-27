#!/bin/bash

PARALLEL=both

for dataset in retailer favorita; # yelp; 
do 
	for k in 5 10;
	do 
		LOG_FILE=aistats20_experiments_"$dataset"_"$model".log
		model = kmeans 

		./multifaq --path ../benchmarking/datasets/"$dataset" --model kmeans --parallel "$PARALLEL" --files config_nips19  --k "$k" >> log_multifaq_aistats20_experiments_"$dataset"_"$model".log

		echo "$dataset - cpp - $model" >> runtime/"$LOG_FILE"
		cat compiler-data.out >> runtime/"$LOG_FILE"

		cd runtime/cpp/
		rm times.txt

		make -j precomp

		# make LMFAO 
		/usr/bin/time -f "%e " -o "time_to_compile.txt" make -j

		cat time_to_compile.txt >> ../"$LOG_FILE"

		for r in {1..5}
		do
		    ./lmfao >> ../../log_multifaq_aistats20_experiments_"$dataset"_"$model".log

		    mv output/assignments.csv output/assignments_"$dataset"_"$k"_"$r".csv
		done

		cat times.txt >> ../"$LOG_FILE"

		# awk -F$'\t' '{OFS = FS} {for(i=1; i<=NF; i++) {a[i]+=$i; if($i!="")
		# b[i]++}}; END {for(i=1; i<=NF; i++) printf "%s%s", a[i]/b[i],
		# (i==NF?ORS:OFS)}' times.txt

		awk -f ~/LMFAO/scripts/awk_average.awk times.txt >> ../"$LOG_FILE"

		rm times.txt
		cd - 
	done
done
