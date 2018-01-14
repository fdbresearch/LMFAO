//--------------------------------------------------------------------
//
// Launcher.cpp
//
// Created on: 30 Nov 2017
// Author: Max
//
//--------------------------------------------------------------------

#include <Launcher.h>
#include <CppGenerator.h>
#include <SqlGenerator.h>

#include <bitset>
#include <fstream>

static const std::string TREEDECOMP_CONF = "/treedecomposition.conf";

using namespace multifaq::params;
using namespace std;
using namespace std::chrono;

Launcher::Launcher(const string& pathToFiles) :
    _pathToFiles(pathToFiles)
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

shared_ptr<Application> Launcher::getModel()
{
    return _model;
}

int Launcher::launch(const string& model, const string& codeGenerator)
{
#ifdef BENCH
    int64_t start = duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count();
#endif
    
    /* Build d-tree. */
    _treeDecomposition.reset(new TreeDecomposition(_pathToFiles + TREEDECOMP_CONF));

    std::cout << "Built the TD. \n";
    std::cout << _treeDecomposition->_root->_name + "\n";
    std::cout << _treeDecomposition->_root->_numOfNeighbors << "\n";

    for (size_t i = 0; i < _treeDecomposition->_root->_neighbors.size(); i++)
    {
        std::cout << _treeDecomposition->_root->_neighbors[i] << " " <<
            _treeDecomposition->getRelation(
                _treeDecomposition->_root->_neighbors[i])->_name << "\n";
    }

    for (size_t i = 0; i < _treeDecomposition->_leafNodes.size(); i++)
    {
        std::cout << _treeDecomposition->_leafNodes[i] << " " <<
            _treeDecomposition->getRelation(
                _treeDecomposition->_leafNodes[i])->_name << "\n";
    }
    
    _compiler.reset(new QueryCompiler(_treeDecomposition));

    if (model.compare("reg") == 0)
        _model.reset(
            new LinearRegression(_pathToFiles, shared_from_this()));
    if (model.compare("regtree") == 0)
        _model.reset(
            new RegressionTree(_pathToFiles, shared_from_this()));
    
    _model->run();
    
#ifdef BENCH
    int64_t startLoading = duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count();
#endif
    
    if (codeGenerator.compare("mem") == 0)
        _codeGenerator.reset(
            new CppGenerator(_pathToFiles, shared_from_this()));
    if (codeGenerator.compare("sql") == 0)
        _codeGenerator.reset(
            new SqlGenerator(_pathToFiles, shared_from_this()));
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

