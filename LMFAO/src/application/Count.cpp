//--------------------------------------------------------------------
//
// Count.cpp
//
// Created on: Feb 1, 2018
// Author: Max
//
//--------------------------------------------------------------------

#include <fstream>

#include <Launcher.h>
#include <Count.h>

using namespace std;

Count::Count(const string& pathToFiles, shared_ptr<Launcher> launcher) :
    _pathToFiles(pathToFiles)
{
    _compiler = launcher->getCompiler();
    _td = launcher->getTreeDecomposition();
}

Count::~Count()
{
}

void Count::run()
{
    modelToQueries();
    _compiler->compile();
}

void Count::generateCode(const std::string& outputString)
{}

void Count::modelToQueries()
{
    // Create a query & Aggregate
    Query* query = new Query();
    query->_rootID = _td->_root->_id;
    
    Aggregate* agg = new Aggregate();

    prod_bitset product;
    agg->_agg.push_back(product);
    
    query->_aggregates.push_back(agg);
    _compiler->addQuery(query);
}
