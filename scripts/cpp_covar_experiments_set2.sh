#!/bin/bash

DATASET=$(basename "$1")
LOG_FILE=experiments_"$DATASET"_covar_exp2.log

######### SINGLE NODE - NO MO 
cp src/codegen/CppGenerator_noMO.cpp src/codegen/CppGenerator.cpp

make 

cp "$1"/features_singleroot.conf "$1"/features.conf

./multifaq --path "$1" --model covar

echo "$DATASET - cpp - covar - single node / no MO " >> runtime/"$LOG_FILE"
cat compiler-data.out >> runtime/"$LOG_FILE"

cd runtime/cpp/

rm times.txt

make -j precomp

# make LMFAO 
time make -j

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

######### SINGLE NODE - MO 

cp src/codegen/CppGenerator_withMO.cpp src/codegen/CppGenerator.cpp

make

./multifaq --path "$1" --model covar

echo "$DATASET - cpp - covar - single node / MO " >> runtime/"$LOG_FILE"
cat compiler-data.out >> runtime/"$LOG_FILE"

cd runtime/cpp/

rm times.txt

make -j precomp

# make LMFAO 
time make -j

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


########### BEST ROOT - MO 

cp "$1"/features_bestroot.conf "$1"/features.conf

./multifaq --path "$1" --model covar

echo "$DATASET - cpp - covar - best root - MO" >> runtime/"$LOG_FILE"
cat compiler-data.out >> runtime/"$LOG_FILE"

cd runtime/cpp/

rm times.txt

make -j precomp

# make LMFAO 
time make -j

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


########### BEST ROOT - MO - PARALELL

./multifaq --path "$1" --model covar --parallel both

echo "$DATASET - cpp - covar - best root - MO - parallel" >> runtime/"$LOG_FILE"
cat compiler-data.out >> runtime/"$LOG_FILE"

cd runtime/cpp/

rm times.txt

make -j precomp

# make LMFAO 
time make -j

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
