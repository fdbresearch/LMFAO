//--------------------------------------------------------------------
//
// RegressionTree.h
//
// Created on: Dec 7, 2017
// Author: Max
//
//--------------------------------------------------------------------


#ifndef INCLUDE_APP_REGRESSION_TREE_H_
#define INCLUDE_APP_REGRESSION_TREE_H_

#include <bitset>
#include <string>
#include <queue>
#include <vector>

#include <Application.hpp>
// #include <CompilerUtils.hpp>
// #include <GlobalParams.hpp>
// #include <Logging.hpp>
// #include <QueryCompiler.h>
// #include <TDNode.hpp>
// #include <TreeDecomposition.h>
// #include <DataTypes.hpp>

/** Forward declarations to allow pointer to launcher and Linear Regressionas
 * without cyclic includes. */
class Launcher;

struct RegTreeNode
{
    /* Condition is a pointer to a function ? - better id of a function  */
    /* Parent conditions - bitset */
    size_t _functionID;
    prod_bitset _conditions;

    RegTreeNode(size_t f) : _functionID(f) {}
    RegTreeNode() {}

};  

class RegressionTree: public Application
{
public:

    RegressionTree(const std::string& pathToFiles,
                     std::shared_ptr<Launcher> launcher);

    ~RegressionTree();

    void run();
    
private:
    
    //! Physical path to the schema and table files.
    std::string _pathToFiles;

    //! QueryCompiler that is called to turn queries into views.
    std::shared_ptr<QueryCompiler> _compiler;

    //! TreeDecomposition of the the underlying join query.
    std::shared_ptr<TreeDecomposition> _td;
    
    //! Queue of active RegTreeNodes to be considered for splitting.
    std::queue<RegTreeNode*> _activeNodes;
    
    //! Array containing the features of the model.
    var_bitset _features;

    var_bitset _categoricalFeatures;

    int _labelID;
    
    void loadFeatures();

    void computeCandidates();

    //  void modelToQueries();

    void splitNodeQueries(RegTreeNode* node);

    size_t _totalNumOfCandidates = 0;

    size_t* _numOfCandidates = nullptr;

    std::vector<std::vector<double>> _thresholds;

    size_t* _queryRootIndex = nullptr;

    /* We can have a candidate mask for each feature. */
    std::vector<prod_bitset> _candidateMask;

};

#endif /* INCLUDE_APP_REGRESSION_TREE_H_ */
