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
#include <QRDecompositionApplication.h>
#include <CppGenerator.h>
#include <DataCube.h>
#include <Launcher.h>
#include <LinearRegression.h>
#include <MutualInformation.h>
#include <RegressionTree.h>
#include <Percentile.h>
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

int Launcher::launch(const string& model, const string& codeGenerator,
                     const string& parallel)
{   
    /* Build tree decompostion. */
    _treeDecomposition.reset(new TreeDecomposition(_pathToFiles + TREEDECOMP_CONF));

    DINFO("Built the TD. \n");
    
    int64_t start = duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count();

    _compiler.reset(new QueryCompiler(_treeDecomposition));

    bool hasApplicationHandler = false;
    bool hasDynamicFunctions = false;
    
    if (model.compare("reg") == 0)
    {
        _model = LinearRegressionModel;
        _application.reset(
            new LinearRegression(_pathToFiles, shared_from_this()));
    }
    else if (model.compare("rtree") == 0)
    {
        _model = RegressionTreeModel;
        _application.reset(
            new RegressionTree(_pathToFiles, shared_from_this(), false));
        hasApplicationHandler = true;
        hasDynamicFunctions = true;
    }
    else if (model.compare("ctree") == 0)
    {
        _model = RegressionTreeModel;
        _application.reset(
            new RegressionTree(_pathToFiles, shared_from_this(), true));
        hasApplicationHandler = true;
        hasDynamicFunctions = true;
    }
    else if (model.compare("covar") == 0)
    {
        _model = CovarianceMatrixModel;
        _application.reset(
            new CovarianceMatrix(_pathToFiles, shared_from_this()));
        hasApplicationHandler = true;
    }
    else if (model.compare("qrdecomp") == 0)
    {
        _model = CovarianceMatrixModel;
        _application.reset(
            new QRDecompApp(_pathToFiles, shared_from_this()));
        hasApplicationHandler = true;
    }
    else if (model.compare("count") == 0)
    {
        // _model = CountModel;
        _application.reset(
            new Count(_pathToFiles, shared_from_this()));
    }
    else if (model.compare("cube") == 0)
    {
        // _model = CountModel;
        _application.reset(
            new DataCube(_pathToFiles, shared_from_this()));
    }
    else if (model.compare("mi") == 0)
    {
        // _model = CountModel;
        _application.reset(
            new MutualInformation(_pathToFiles, shared_from_this()));
    }
    else if (model.compare("perc") == 0)
    {
        // _model = CountModel;
        _application.reset(
            new Percentile(_pathToFiles, shared_from_this()));
        hasApplicationHandler = true;
    }
    else
    {
        ERROR("The model "+model+" is not supported. \n");
        exit(1);
    }
    _application->run();

    ParallelizationType parallelization_type = NO_PARALLELIZATION;
    if (parallel.compare("task") == 0)
        parallelization_type = TASK_PARALLELIZATION;
    else if (parallel.compare("domain") == 0)
        parallelization_type = DOMAIN_PARALLELIZATION;
    else if (parallel.compare("both") == 0)
        parallelization_type = BOTH_PARALLELIZATION;
    else if (parallel.compare("none") != 0)
        ERROR("ERROR - We only support task and/or domain parallelism. "<<
              "We continue single threaded.\n\n");
    
    if (codeGenerator.compare("cpp") == 0)
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
    
    _codeGenerator->generateCode(
        parallelization_type, hasApplicationHandler, hasDynamicFunctions);
    
    int64_t processingTime = duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count() - start;

    size_t numOfViews = _compiler->numberOfViews();    
    size_t numOfQueries = _compiler->numberOfQueries();
    size_t numOfGroups = _codeGenerator->numberOfGroups();

    size_t finalNumberOfAggregates = 0;
    size_t totalNumberOfAggregates = 0;
    for (size_t v = 0; v < numOfViews; ++v)
    {
        View* view = _compiler->getView(v);
        
        if (view->_origin == view->_destination)
            finalNumberOfAggregates += view->_aggregates.size();
        
        totalNumberOfAggregates += view->_aggregates.size();
    }
    
    /* Write loading times times to times file */
    ofstream ofs("compiler-data.out", std::ofstream::out);// | std::ofstream::app);
    
    if (ofs.is_open())
    {   
        ofs << "aggs\tfinAgg\tqueries\tviews\tgroups\ttime" << std::endl;
        ofs << totalNumberOfAggregates << "\t"
            << finalNumberOfAggregates << "\t"
            << numOfQueries <<"\t"
            << numOfViews <<"\t"<< numOfGroups <<"\t"
            << processingTime << std::endl;
    }
    else
        cout << "Unable to open compiler-data.out \n";
    
    ofs.close();
    
    BINFO(
	"Time for Compiler: " + to_string(processingTime) + "ms.\n");
    
    return EXIT_SUCCESS;
}
