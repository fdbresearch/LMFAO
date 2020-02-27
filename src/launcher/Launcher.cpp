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
#include <DataCube.h>
#include <KMeans.h>
#include <Launcher.h>
#include <LinearRegression.h>
#include <MutualInformation.h>
#include <RegressionTree.h>
#include <Percentile.h>
#include <SqlGenerator.h>

#include <bitset>
#include <fstream>

std::string multifaq::config::FEATURE_CONF = "";
std::string multifaq::config::TREEDECOMP_CONF = "";
std::string multifaq::config::SCHEMA_CONF = "";

bool multifaq::cppgen::MULTI_OUTPUT;
bool multifaq::cppgen::RESORT_RELATIONS;
bool multifaq::cppgen::MICRO_BENCH;
bool multifaq::cppgen::COMPRESS_AGGREGATES;
bool multifaq::cppgen::BENCH_INDIVIDUAL;

multifaq::cppgen::PARALLELIZATION_TYPE multifaq::cppgen::PARALLEL_TYPE;


using namespace multifaq::params;
using namespace multifaq::config;
using namespace std;
using namespace std::chrono;

Launcher::Launcher() 
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

shared_ptr<CodeGenerator> Launcher::getCodeGenerator()
{
    return _codeGenerator;
}
// Model Launcher::getModel()
// {
//     return _model;
// }

// int Launcher::launch(const string& model, const string& codeGenerator,
//                      const string& featureFile,
//                      const string& tdFile, const string& outDirectory,
//                      const bool multioutput_flag, const bool resort_flag,
//                      const bool microbench_flag, const bool compression_flag,
//                      const int k
//     )
int Launcher::launch(boost::program_options::variables_map& vm)
{
    /* Define the Feature Conf File */
    FEATURE_CONF = multifaq::dir::PATH_TO_FILES+"/"+vm["feat"].as<std::string>(); 

    /* Define the TreeDecomposition Conf File */
    TREEDECOMP_CONF = multifaq::dir::PATH_TO_FILES+"/"+vm["td"].as<std::string>();

    const string model = vm["model"].as<std::string>();
    
    const string codeGenerator = vm["codegen"].as<std::string>();
    
    multifaq::cppgen::PARALLELIZATION_TYPE parallelization_type =
        multifaq::cppgen::NO_PARALLELIZATION;

    std::string parallel = vm["parallel"].as<std::string>();
   
    if (parallel.compare("task") == 0)
        parallelization_type =  multifaq::cppgen::TASK_PARALLELIZATION;
    else if (parallel.compare("domain") == 0)
        parallelization_type =  multifaq::cppgen::DOMAIN_PARALLELIZATION;
    else if (parallel.compare("both") == 0)
        parallelization_type = multifaq::cppgen::BOTH_PARALLELIZATION;
    else if (parallel.compare("none") != 0)
        ERROR("ERROR - We only support task and/or domain parallelism. "<<
              "We continue single threaded.\n\n");

    multifaq::cppgen::RESORT_RELATIONS = vm.count("resort");

    multifaq::cppgen::MULTI_OUTPUT = (vm["mo"].as<bool>())
        && !multifaq::cppgen::RESORT_RELATIONS;

    multifaq::cppgen::MICRO_BENCH = vm.count("microbench");

    multifaq::cppgen::COMPRESS_AGGREGATES = vm["compress"].as<bool>();
    
    multifaq::cppgen::BENCH_INDIVIDUAL = vm.count("bench_individual");

    multifaq::cppgen::PARALLEL_TYPE = parallelization_type;

    /* TODO: when this gets fixed, degree needs to be passed to relevant models. */
    if (vm["degree"].as<int>() > 1)
        ERROR("A degree > 1 is currenlty not supported.\n");

    /* Build tree decompostion. */
    _treeDecomposition.reset(new TreeDecomposition());

    DINFO("INFO: Built the TreeDecomposition.\n");
    
    int64_t start = duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count();

    _compiler.reset(new QueryCompiler(_treeDecomposition));
    
    bool hasApplicationHandler = false;
    bool hasDynamicFunctions = false;
    
    if (model.compare("reg") == 0)
    {
        _application.reset(
            new LinearRegression(shared_from_this()));
        hasApplicationHandler = true;
    }
    else if (model.compare("rtree") == 0)
    {
        _application.reset(
            new RegressionTree(shared_from_this(), false));
        hasApplicationHandler = true;
        hasDynamicFunctions = true;
    }
    else if (model.compare("ctree") == 0)
    {
        _application.reset(
            new RegressionTree(shared_from_this(), true));
        hasApplicationHandler = true;
        hasDynamicFunctions = true;
    }
    else if (model.compare("covar") == 0)
    {
        _application.reset(
            new CovarianceMatrix(shared_from_this()));
        hasApplicationHandler = true;
    }
    else if (model.compare("count") == 0)
    {
        _application.reset(
            new Count(shared_from_this()));
        hasApplicationHandler = true;
    }
    else if (model.compare("cube") == 0)
    {
        _application.reset(
            new DataCube(shared_from_this()));
    }
    else if (model.compare("mi") == 0)
    {
        _application.reset(
            new MutualInformation(shared_from_this()));
    }
    else if (model.compare("perc") == 0)
    {
        _application.reset(
            new Percentile(shared_from_this()));
        hasApplicationHandler = true;
    }
    else if (model.compare("kmeans") == 0)
    {
            
        size_t kappa = vm["clusters"].as<size_t>();
        if (vm.count("kappa"))
            kappa = vm["kappa"].as<size_t>();

        _application.reset(
            new KMeans(shared_from_this(), vm["clusters"].as<size_t>(), kappa));
        hasApplicationHandler = true;
    }
    else
    {
        ERROR("The model "+model+" is not supported. \n");
        exit(1);
    }
    
    _application->run();
    
    if (codeGenerator.compare("cpp") == 0)
        _codeGenerator.reset(
            new CppGenerator(shared_from_this())
            );
    else if (codeGenerator.compare("sql") == 0)
        _codeGenerator.reset(
            new SqlGenerator(shared_from_this()));
    else
    {
        ERROR("The code generator "+codeGenerator+" is not supported. \n");
        exit(1);
    }
    
    _codeGenerator->generateCode(hasApplicationHandler, hasDynamicFunctions);

    if (hasApplicationHandler && codeGenerator.compare("sql") != 0)
        _application->generateCode();
    
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
