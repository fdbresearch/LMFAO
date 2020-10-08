# Source Code for LMFAO

The source code of LMFAO is divided into the following directories:

* `launcher`
  
  Contains the launcher for the LMFAO process. The entire process is initiated
  by this launcher.
  
* `database`
  
  Contains the code that loads and represents the database schema. 

* `treeDecomposition`

  Contains the code that loads and represents the tree decomposition that the
  aggregate batch is computed over.


* `application` 
  
  Encodes the applications of LMFAO. Each application is turned into a batch of
  aggregate queries, and assigns each query to one node in the tree
  decomposition. If required, we generate C++ code specific for this application
  (captured by the ApplicationHandler).


* `compiler`

	Contains the code that decomposes a batch of aggregate into views over the
    tree decomposition.

* `codegen`

	Contains the code that groups the generated views into view groups, and then
    generates C++ code for each view group that is specialized to the given
    application and dataset.
	
