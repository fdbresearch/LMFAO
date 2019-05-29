//--------------------------------------------------------------------
//
// Count.h
//
// Created on: Feb 1, 2018
// Author: Max
//
//--------------------------------------------------------------------


#ifndef INCLUDE_APP_COUNT_H_
#define INCLUDE_APP_COUNT_H_

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
class Count: public Application
{
public:

    Count(const std::string& pathToFiles,
          std::shared_ptr<Launcher> launcher);

    ~Count();

    void run();
    
    void generateCode(const std::string& outputString);

private:
    
    //! Physical path to the schema and table files.
    std::string _pathToFiles;
    
    std::shared_ptr<QueryCompiler> _compiler;
    
    std::shared_ptr<TreeDecomposition> _td;
    
    void modelToQueries();
};

#endif /* INCLUDE_APP_COUNT_H_ */
