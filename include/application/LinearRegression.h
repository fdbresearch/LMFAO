//--------------------------------------------------------------------
//
// LinearRegression.h
//
// Created on: Nov 27, 2017
// Author: Max
//
//--------------------------------------------------------------------


#ifndef INCLUDE_APP_REGRESSION_H_
#define INCLUDE_APP_REGRESSION_H_

#include <bitset>
#include <string>
#include <vector>

#include <Application.hpp>
// #include <DataTypes.hpp>

/** Forward declarations to allow pointer to launcher and Linear Regressionas
 * without cyclic includes. */
class Launcher;

/**
 * Class that launches regression model on the data, using d-trees.
 */
class LinearRegression: public Application
{
public:

    LinearRegression(const std::string& pathToFiles,
                     std::shared_ptr<Launcher> launcher);

    ~LinearRegression();

    void run();
    
private:
    
    //! Physical path to the schema and table files.
    std::string _pathToFiles;

    // std::shared_ptr<Launcher> _launcher;

    std::shared_ptr<QueryCompiler> _compiler;
    
    std::shared_ptr<TreeDecomposition> _td;

    //! Array containing the features of the model.
    var_bitset _features;

    var_bitset _categoricalFeatures;

    size_t* rootIndex = nullptr;
    
    void modelToQueries();

    void loadFeatures();

    void computeGradient();
    
    
};

#endif /* INCLUDE_APP_REGRESSION_H_ */
