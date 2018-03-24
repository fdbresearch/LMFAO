//--------------------------------------------------------------------
//
// Launcher.cpp
//
// Created on: 30 Nov 2017
// Author: Max
//
//--------------------------------------------------------------------

#include <Count.h>
#include <CovarianceMatrix.h>
#include <CppGenerator.h>
#include <Launcher.h>
#include <LinearRegression.h>
#include <RegressionTree.h>
#include <SqlGenerator.h>

#include <bitset>
#include <fstream>

static const std::string TREEDECOMP_CONF = "/treedecomposition.conf";

using namespace multifaq::params;
using namespace std;
using namespace std::chrono;

Launcher::Launcher(const string& pathToFiles) : _pathToFiles(pathToFiles)
{
}

Launcher::~Launcher()
{
}

shared_ptr<QueryCompiler> Launcher::getCompiler()
{
    return _compiler;
}

shared_ptr<TreeDecomposition> Launcher::getTreeDecomposition()
{
    return _treeDecomposition;
}

shared_ptr<Application> Launcher::getApplication()
{
    return _application;
}

Model Launcher::getModel()
{
    return _model;
}

int Launcher::launch(const string& model, const string& codeGenerator)
{
#ifdef BENCH
    int64_t start = duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count();
#endif
    
    /* Build tree decompostion. */
    _treeDecomposition.reset(new TreeDecomposition(_pathToFiles + TREEDECOMP_CONF));

    DINFO("Built the TD. \n");
    
    _compiler.reset(new QueryCompiler(_treeDecomposition));

    if (model.compare("reg") == 0)
    {
        _model = LinearRegressionModel;
        _application.reset(
            new LinearRegression(_pathToFiles, shared_from_this()));
    }
    else if (model.compare("regtree") == 0)
    {
        _model = RegressionTreeModel;
        _application.reset(
            new RegressionTree(_pathToFiles, shared_from_this()));
    }
    else if (model.compare("covar") == 0)
    {
        _model = CovarianceMatrixModel;
        _application.reset(
            new CovarianceMatrix(_pathToFiles, shared_from_this()));
    }
    else if (model.compare("count") == 0)
    {
        // _model = CountModel;
        _application.reset(
            new Count(_pathToFiles, shared_from_this()));
    }
    else
    {
        ERROR("The model "+model+" is not supported. \n");
        exit(1);
    }
    _application->run();
    
#ifdef BENCH
    int64_t startLoading = duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count();
#endif
    
    if (codeGenerator.compare("mem") == 0)
        _codeGenerator.reset(
            new CppGenerator(_pathToFiles, shared_from_this()));
    else if (codeGenerator.compare("sql") == 0)
        _codeGenerator.reset(
            new SqlGenerator(_pathToFiles, shared_from_this()));
    else
    {
        ERROR("The code generator "+codeGenerator+" is not supported. \n");
        exit(1);
    }
    
    _codeGenerator->generateCode();
    
#ifdef BENCH
    int64_t endLoading = duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count() - startLoading;
    
    /* Write loading times times to times file */
    ofstream ofs("times.txt", std::ofstream::out | std::ofstream::app);
    
    if (ofs.is_open())
        ofs << "loading: \t"+to_string(endLoading) + "\n";
    else
        cout << "Unable to open times.txt \n";
    
    ofs.close();
#endif
    
    BINFO(
	"WORKER - data loading: " + to_string(endLoading) + "ms.\n");

    // if(NUM_OF_ATTRIBUTES != _dataHandler->getNamesMapping().size()) {
    // ERROR("NUM_OF_ATTRIBUTES and number of attributes specified in
    // schema.conf inconsistent. Aborting.\n") exit(1);}
    
    BINFO(
        "MAIN - initialisation of DFDB: " + to_string(
            duration_cast<milliseconds>(
                system_clock::now().time_since_epoch()).count() - start) + "ms.\n");

    return EXIT_SUCCESS;
}

