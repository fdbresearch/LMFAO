//--------------------------------------------------------------------
//
// CovarianceMatrix.h
//
// Created on: Feb 1, 2018
// Author: Max
//
//--------------------------------------------------------------------


#ifndef INCLUDE_APP_COVARMATRIX_H_
#define INCLUDE_APP_COVARMATRIX_H_

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
class CovarianceMatrix: public Application
{
public:

    CovarianceMatrix(const std::string& pathToFiles,
                     std::shared_ptr<Launcher> launcher);

    ~CovarianceMatrix();

    void run();
    
//     var_bitset getCategoricalFeatures();
    
private:
    
    //! Physical path to the schema and table files.
    std::string _pathToFiles;
    
    std::shared_ptr<QueryCompiler> _compiler;
    
    std::shared_ptr<TreeDecomposition> _td;
    
    var_bitset _categoricalFeatures;

    size_t* _queryRootIndex = nullptr;
    
    void modelToQueries();

    void loadFeatures();    
};

#endif /* INCLUDE_APP_COVARMATRIX_H_ */
