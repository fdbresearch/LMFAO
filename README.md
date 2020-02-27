# LMFAO

The provided package can be used to compute the covariance matrix needed to compute a linear regression model. 
It does not compute the parameters. The code to compute the final model is currenlty under construction, but will
be added once completed. The convergence step, however, does not take a lot of time, and therefore the provided code 
is a good indicator of the over all runtime.

## Installation

In order to compile the LMFAO project, you need to have `cmake` installed.
Then, you need to run the following commands:

```
cmake .
make
```

## Scripts To Run:

The following script runs LMFAO 5 times and reports the performance in the runtime directory.

./scripts/cpp_covar_script.sh PATH_TO_DATASET

You can also run the tool manually using the following commands: 

1) ./multifaq --path PATH_TO_DATASET --model covar --parallel {both,none}

   This command will generate C++ code in the runtime/cpp directory. 

2) cd runtime/cpp/

3) make -j 

4) ./lmfao 


LMFAO can also generate the SQL queries for the required aggregates by running the following command: 

./multifaq --path PATH_TO_DATASET --model covar --codegen sql 

The SQL queries can be found in runtime/sql/


##XCode (good for profiling):

cmake -G Xcode -H. -B_build

