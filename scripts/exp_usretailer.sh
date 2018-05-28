#!/bin/bash

for v in 2 3 4 5 6
do
    cp data/usretailer/features"$v".conf data/usretailer/features.conf
    ./multifaq --path data/usretailer/ --model covar

    echo "usretailer - features$v - covar" >> runtime/experiments.log
    cat compile-data.out >> runtime/experiments.log
    
    ## TODO: Output the information we need from multifaq to experiments 
    
    cd runtime/cpp/

    # make LMFAO 
    /usr/bin/time -f "%e " -o "time_to_compile.txt" make -j

    cat time_to_compile.txt >> ../experiments.log
    
    for r in {1..5}
    do
        ./lmfao
    done

    # awk -F$'\t' '{OFS = FS} {for(i=1; i<=NF; i++) {a[i]+=$i; if($i!="")
    # b[i]++}}; END {for(i=1; i<=NF; i++) printf "%s%s", a[i]/b[i],
    # (i==NF?ORS:OFS)}' times.txt

    cat times.txt >> ../experiments.log
    
    awk -f ../../scripts/awk_average.awk times.txt >> ../experiments.log

    rm times.txt
    
    cd - 
done
