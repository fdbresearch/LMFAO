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

#include <Application.hpp>
#include <CodeGenerator.hpp>
#include <Logging.hpp>
#include <QueryCompiler.h>
#include <TreeDecomposition.h>

enum Model 
{
    LinearRegressionModel,
    RegressionTreeModel
};

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
    int launch(const std::string& model, const std::string& codeGenerator);

    // /**
    //  * Returns a pointer to the DataHandler module.
    //  */
    // std::shared_ptr<DataHandler> getDataHandler();

    /**
     * Returns a pointer to the tree decomposition.
     */
    std::shared_ptr<TreeDecomposition> getTreeDecomposition();
    
    /**
     * Returns a pointer to the application.
     */
    std::shared_ptr<Application> getApplication();

    /**
     * Returns a model identifier.
     */
    Model getModel();

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
    std::shared_ptr<Application> _application;

    //! Engine module of the database.
    std::shared_ptr<TreeDecomposition> _treeDecomposition;

    //! Engine module of the database.
    std::shared_ptr<CodeGenerator> _codeGenerator;
    
    //! Path to the files used by the database.
    std::string _pathToFiles;

    Model _model;
};

#endif // INCLUDE_RUN_LAUNCHER_H_
