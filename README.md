# LMFAO - Layered Multiple Functional Ag- gregate Optimization

This package provides the code for the LMFAO engine. Please refer to our [SIGMOD 2019 paper](https://arxiv.org/abs/1906.08687) for details on the engine. 

## Installation

In order to compile the LMFAO project, you need to have `cmake` installed.
Then, you need to run the following commands:

```
cmake .
make
```

## Configuration Files 

In order to run LMFAO, you need to provide the following configuration files. The `data/` directory provides examples for these configuration files.

* `treedecomposition.conf` defines the tree decomposition used to compute the aggregates in LMFAO 
* `features.conf` enumerates the features of the desired machine learning model and defines the type of each feature (continuous or categorical)

By default, LMFAO assumes that the configuration files are provided in the directory that contains the data, but you can specify a different directory with the `--files (-f)` flag in the `multifaq` command (see below). 

## Running LMFAO: 

LMFAO is run in two stages: Then, you need to compile and run the generated code. The commands to run LMFAO end-to-end are: 

1) First, the following command generates the C++ code for a specific application and dataset.

   ```./multifaq --path PATH_TO_DATASET --model DESIRED_MODEL --parallel both```

   where
   * `--path` denotes the path to multi-relational dataset 
   * `--model` specifies the model to be computed, the current options include: 
     1) `covar` computes the Covariance Matrix 
     2) `reg` computes a linear regression model
     3) `kmeans` computes the KMeans clusters with the Rk-means algorithm (see the AISTATS 2020 paper for details)
     4) `mi` computes all pairwise mutual information 
   * `--parallel both` enables both task and domain parallelism 
   
   Please refer to `./multifaq -h` for additional information on the command line options. 
   
   By default, this command generates C++ code in the `runtime/cpp/` directory. You can change the output directory with the `--out (-o)` flag. 

   This command also generates the `compiler-data.out` file which provides information on the number aggregates, queries, views, and groups that are required for the given application. 

2) Then, run the following commands to execute the generated code: 
``` 
cd runtime/cpp/ 
make -j
./lmfao
```

