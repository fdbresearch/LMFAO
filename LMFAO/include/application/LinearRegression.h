//--------------------------------------------------------------------
//
// LinearRegression.h
//
// Created on: Feb 1, 2018
// Author: Max
//
//--------------------------------------------------------------------


#ifndef INCLUDE_APP_LINEARREGRESSION_H_
#define INCLUDE_APP_LINEARREGRESSION_H_

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
class LinearRegression: public Application
{
public:

    LinearRegression(const std::string& pathToFiles,
                     std::shared_ptr<Launcher> launcher);

    ~LinearRegression();

    void run();
    
    var_bitset getCategoricalFeatures();

    void generateCode(const std::string& ouputDirectory);

private:
    
    //! Physical path to the schema and table files.
    std::string _pathToFiles;
    
    std::shared_ptr<QueryCompiler> _compiler;
    
    std::shared_ptr<TreeDecomposition> _td;
    
    var_bitset _categoricalFeatures;

    size_t labelID;

    size_t* _queryRootIndex = nullptr;

    std::vector<size_t> _parameterIndex;
    
    void modelToQueries();

    void loadFeatures();

    Query** parameterQueries;
    std::string generateParameters();
    std::string generateGradients();
    std::string generateConvergenceLoop();
    std::string generatePrintFunction();
    std::string getAttributeName(size_t attID);
    std::string offset(size_t off);
    std::string typeToStr(Type t);
    
    std::string generateTestDataEvaluation();
    
};

#endif /* INCLUDE_APP_LINEARREGRESSION_H_ */
