//--------------------------------------------------------------------
//
// DataCube.h
//
// Created on: Oct 18, 2019
// Author: Max
//
//--------------------------------------------------------------------


#ifndef INCLUDE_APP_DATACUBE_H_
#define INCLUDE_APP_DATACUBE_H_

#include <bitset>
#include <string>
#include <vector>

#include <Application.hpp>

/** Forward declarations to allow pointer to launcher and Linear Regressionas
 * without cyclic includes. */
class Launcher;

/**
 * Class that launches regression model on the data, using d-trees.
 */
class DataCube: public Application
{
public:

    DataCube(const std::string& pathToFiles,
          std::shared_ptr<Launcher> launcher);

    ~DataCube();

    void run();
    
private:
    
    //! Physical path to the schema and table files.
    std::string _pathToFiles;
    
    std::shared_ptr<QueryCompiler> _compiler;
    
    std::shared_ptr<TreeDecomposition> _td;

    size_t* _queryRootIndex = nullptr;

    size_t _labelID;
    
    void modelToQueries();

    void loadFeatures();

    // The two bitsets are defined in ApplicationHandler
    // var_bitset _isFeature;
    // var_bitset _isCategoricalFeature
};

#endif /* INCLUDE_APP_DATACUBE_H_ */
