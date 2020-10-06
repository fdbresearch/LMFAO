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

In order to run LMFAO, you need to provide the following configuration files. 

* `treedecomposition.conf` defines the tree decomposition used to compute the aggregates in LMFAO 
* `features.conf` enumerates the features of the desired machine learning model and defines the type of each feature (continuous or categorical)

By default, LMFAO assumes that the configuration files are provided in the directory that contains the data, but you can specify a different directory with the `--output` flag in the `multifaq` command (see below). 

## Running LMFAO: 

LMFAO is run in two stages. First, you need to run `./multifaq` to generate the C++ code for a specific applkcation and dataset. Then, you need to compile and run the generated code. The commands to run LMFAO end-to-end are: 

1) `./multifaq --path PATH_TO_DATASET --model covar --parallel both`

   By default, this command generates C++ code in the runtime/cpp directory. You can change the output directory with the `--output` flag. 

2) `cd runtime/cpp/`
3) `make -j`
4) `./lmfao`

Please refer to `./multifaq -h` for additional information on the command line options. 
