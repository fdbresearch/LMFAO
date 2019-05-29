#!/bin/bash

PARALLEL=both

for dataset in tpc-ds; # retailer favorita yelp tpc-ds; 
do 
	for model in covar; #covar count rtree mi cube;
	do 
		LOG_FILE=sigmod19_experiments_set2_"$dataset"_"$model".log

		echo "$dataset - cpp - single root - no MO - $model" >> runtime/"$LOG_FILE"
	       	./multifaq --path ../benchmarking/datasets/"$dataset" --model "$model" --parallel none --mo 0 --feat config_sigmod19/features_singleroot.conf >> log_multifaq_sigmod19_experiments_set2_"$dataset"_"$model".log
		cat compiler-data.out >> runtime/"$LOG_FILE"

		cd runtime/cpp/
		rm times.txt

		make -j precomp

		# make LMFAO 
		/usr/bin/time -f "%e " -o "time_to_compile.txt" make 

		cat time_to_compile.txt >> ../"$LOG_FILE"

		for r in {1..5}
		do
		    ./lmfao >> ../../log_multifaq_sigmod19_experiments_set2_"$dataset"_"$model".log
		done

		cat times.txt >> ../"$LOG_FILE"

		# awk -F$'\t' '{OFS = FS} {for(i=1; i<=NF; i++) {a[i]+=$i; if($i!="")
		# b[i]++}}; END {for(i=1; i<=NF; i++) printf "%s%s", a[i]/b[i],
		# (i==NF?ORS:OFS)}' times.txt

		awk -f ~/LMFAO/scripts/awk_average.awk times.txt >> ../"$LOG_FILE"

		rm times.txt
		cd - 

		echo "$dataset - cpp - single root mo - $model" >> runtime/"$LOG_FILE"
	       	./multifaq --path ../benchmarking/datasets/"$dataset" --model "$model" --parallel none --feat config_sigmod19/features_singleroot.conf >> log_multifaq_sigmod19_experiments_set2_"$dataset"_"$model".log
		cat compiler-data.out >> runtime/"$LOG_FILE"

		cd runtime/cpp/
		rm times.txt

		make -j precomp

		# make LMFAO 
		/usr/bin/time -f "%e " -o "time_to_compile.txt" make 

		cat time_to_compile.txt >> ../"$LOG_FILE"

		for r in {1..5}
		do
		    ./lmfao >> ../../log_multifaq_sigmod19_experiments_set2_"$dataset"_"$model".log
		done

		cat times.txt >> ../"$LOG_FILE"

		# awk -F$'\t' '{OFS = FS} {for(i=1; i<=NF; i++) {a[i]+=$i; if($i!="")
		# b[i]++}}; END {for(i=1; i<=NF; i++) printf "%s%s", a[i]/b[i],
		# (i==NF?ORS:OFS)}' times.txt

		awk -f ~/LMFAO/scripts/awk_average.awk times.txt >> ../"$LOG_FILE"

		rm times.txt
		cd - 


		echo "$dataset - cpp - best root - mo - $model" >> runtime/"$LOG_FILE"
	       	./multifaq --path ../benchmarking/datasets/"$dataset" --model "$model" --parallel none --feat config_sigmod19/features_"$model".conf >> log_multifaq_sigmod19_experiments_set2_"$dataset"_"$model".log
		cat compiler-data.out >> runtime/"$LOG_FILE"

		cd runtime/cpp/
		rm times.txt

		make -j precomp

		# make LMFAO 
		/usr/bin/time -f "%e " -o "time_to_compile.txt" make -j

		cat time_to_compile.txt >> ../"$LOG_FILE"

		for r in {1..5}
		do
		    ./lmfao >> ../../log_multifaq_sigmod19_experiments_set2_"$dataset"_"$model".log
		done

		cat times.txt >> ../"$LOG_FILE"

		# awk -F$'\t' '{OFS = FS} {for(i=1; i<=NF; i++) {a[i]+=$i; if($i!="")
		# b[i]++}}; END {for(i=1; i<=NF; i++) printf "%s%s", a[i]/b[i],
		# (i==NF?ORS:OFS)}' times.txt

		awk -f ~/LMFAO/scripts/awk_average.awk times.txt >> ../"$LOG_FILE"

		rm times.txt
		cd - 


		echo "$dataset - cpp - best root - mo - parallel - $model" >> runtime/"$LOG_FILE"
	       	./multifaq --path ../benchmarking/datasets/"$dataset" --model "$model" --parallel both --feat config_sigmod19/features_"$model".conf >> log_multifaq_sigmod19_experiments_set2_"$dataset"_"$model".log
		cat compiler-data.out >> runtime/"$LOG_FILE"

		cd runtime/cpp/
		rm times.txt

		make -j precomp

		# make LMFAO 
		/usr/bin/time -f "%e " -o "time_to_compile.txt" make -j

		cat time_to_compile.txt >> ../"$LOG_FILE"

		for r in {1..5}
		do
		    ./lmfao >> ../../log_multifaq_sigmod19_experiments_set2_"$dataset"_"$model".log
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
