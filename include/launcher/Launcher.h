//--------------------------------------------------------------------
//
// Launcher.h
//
// Created on: 30 Nov 2017
// Author: Max
//
//--------------------------------------------------------------------

#ifndef INCLUDE_RUN_LAUNCHER_H_
#define INCLUDE_RUN_LAUNCHER_H_

//#include <DataHandler.hpp>

#include <Application.hpp>
#include <LinearRegression.h>
#include <Logging.hpp>
#include <QueryCompiler.h>
#include <RegressionTree.h>
#include <TreeDecomposition.h>

// #include <Engine.h>
// #include <MachineLearningModel.hpp>
// #include <ResultHandler.hpp>
// #include <Shuffler.hpp>

/**
 * Class that takes care of assembling the different components of the database
 * and launching the tasks.
 */
class Launcher: public std::enable_shared_from_this<Launcher>
{
public:

    /**
     * Constructor; need to provide path to the schema, table and other
     * configuration files.
     */
    Launcher(const std::string& pathToFiles);

    ~Launcher();

    /**
     * Launches the database operations.
     */
    int launch(std::string model);

    // /**
    //  * Returns a pointer to the DataHandler module.
    //  */
    // std::shared_ptr<DataHandler> getDataHandler();

    /**
     * Returns a pointer to the tree decomposition.
     */
    std::shared_ptr<TreeDecomposition> getTreeDecomposition();
    
    /**
     * Returns a pointer to the model.
     */
    std::shared_ptr<Application> getModel();

    /**
     * Returns a pointer to the Query Compiler.
     */
    std::shared_ptr<QueryCompiler> getCompiler();
    
private:

    // //! DataHandler module of the database.
    // std::shared_ptr<DataHandler> _dataHandler;

    //! Query Compiler that turns queries into views. 
    std::shared_ptr<QueryCompiler> _compiler;

    //! Model to be computed of the database.
    std::shared_ptr<Application> _model;

    //! Engine module of the database.
    std::shared_ptr<TreeDecomposition> _treeDecomposition;
    
    //! Path to the files used by the database.
    std::string _pathToFiles;
};

#endif // INCLUDE_RUN_LAUNCHER_H_
