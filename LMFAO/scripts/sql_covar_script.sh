#!/bin/bash

DATASET=$(basename "$1")
LOG_FILE=experiments_"$DATASET"_sql.log

echo "$DATASET"
echo "$LOG_FILE"

echo "$DATASET -- covar -- sql " >> runtime/"$LOG_FILE"

./multifaq --path "$1" --model covar --codegen sql
cat compiler-data.out >> runtime/"$LOG_FILE"

cd runtime/sql/

psql -d vldb18 -f load_data.sql

## RUN JOIN EXPERIMENTS

rm times.txt 

echo "Run join.sql - $DATASET - covar " >> ../"$LOG_FILE"

for i in {1..3}
do
      echo "Run $i lmfao.sql - $DATASET - covar "
      /usr/bin/time -f "%e %P %I %O" -o "times.txt" -a psql -d vldb18 -f join.sql
      psql -d vldb18 -f lmfao_cleanup.sql
done

cat times.txt >> ../"$LOG_FILE"
    
awk -f ../../scripts/awk_average.awk times.txt >> ../"$LOG_FILE"

rm times.txt 

echo "Run lmfao.sql - $DATASET - covar " >> ../"$LOG_FILE"

for i in {1..3}
do
      echo "Run $i lmfao.sql - $DATASET - covar "
      /usr/bin/time -f "%e %P %I %O" -o "times.txt" -a psql -d vldb18 -f lmfao.sql
      psql -d vldb18 -f lmfao_cleanup.sql
done

cat times.txt >> ../"$LOG_FILE"
    
awk -f ../../scripts/awk_average.awk times.txt >> ../"$LOG_FILE"

rm times.txt 

echo "Run aggregates.sql - $DATASET - covar " >> ../"$LOG_FILE"

for i in {1..3}
do
      echo "Run $i aggregates.sql - $DATASET - covar"
      /usr/bin/time -f "%e %P %I %O" -o "times.txt" -a psql -d vldb18 -f aggregates.sql > /dev/null
done

cat times.txt >> ../"$LOG_FILE"
    
awk -f ../../scripts/awk_average.awk times.txt >> ../"$LOG_FILE"

rm times.txt 

psql -d vldb18 -f drop_data.sql 

cd -