#!/bin/bash

DATASET=$(basename "$1")
LOG_FILE=experiments_"$DATASET"_covar.log

./multifaq --path "$1" --model covar

echo "$DATASET - cpp - covar" >> runtime/"$LOG_FILE"
cat compiler-data.out >> runtime/"$LOG_FILE"

cd runtime/cpp/

rm times.txt

# make LMFAO 
/usr/bin/time -f "%e " -o "time_to_compile.txt" make -j

cat time_to_compile.txt >> ../"$LOG_FILE"

for r in {1..5}
do
    ./lmfao
done

# awk -F$'\t' '{OFS = FS} {for(i=1; i<=NF; i++) {a[i]+=$i; if($i!="")
# b[i]++}}; END {for(i=1; i<=NF; i++) printf "%s%s", a[i]/b[i],
# (i==NF?ORS:OFS)}' times.txt

cat times.txt >> ../"$LOG_FILE"

awk -f ../../scripts/awk_average.awk times.txt >> ../"$LOG_FILE"

rm times.txt

cd - 