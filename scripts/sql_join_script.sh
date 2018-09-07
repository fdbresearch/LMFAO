#!/bin/bash

DATASET=$(basename "$1")
LOG_FILE=experiments_join_export.log

echo "$DATASET"
echo "$LOG_FILE"

echo "$DATASET -- join -- sql " >> runtime/"$LOG_FILE"

./multifaq --path "$1" --model covar --codegen sql
cat compiler-data.out >> runtime/"$LOG_FILE"

cd runtime/sql/

psql -d vldb18 -f load_data.sql

## RUN JOIN EXPERIMENTS

rm times.txt 
rm export_times.txt 

echo "Run join.sql - $DATASET - join " >> ../"$LOG_FILE"

for i in {1..3}
do
      echo "Run $i lmfao.sql - $DATASET - join "
      /usr/bin/time -f "%e %P %I %O" -o "times.txt" -a psql -d vldb18 -f join.sql
      /usr/bin/time -f "%e %P %I %O" -o "export_times.txt" -a psql -d vldb18 -f export.sql
      psql -d vldb18 -f lmfao_cleanup.sql
      rm joinresult.txt
done

cat times.txt >> ../"$LOG_FILE"
    
awk -f ../../scripts/awk_average.awk times.txt >> ../"$LOG_FILE"

rm times.txt 

echo "$DATASET - export times " >> ../"$LOG_FILE"

cat export_times.txt >> ../"$LOG_FILE"
    
awk -f ../../scripts/awk_average.awk export_times.txt >> ../"$LOG_FILE"

rm export_times.txt 
