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
#include <vector>

#include <Application.hpp>

/** Forward declarations to allow pointer to launcher and Linear Regressionas
 * without cyclic includes. */
class Launcher;

// struct RegTreeNode
// {
//     size_t _functionID;

//     /* Set of parent conditions, stored as a product of functions. */
//     prod_bitset _conditions;

//     size_t count;
//     double prediction;
//     double variance;

//     RegTreeNode(size_t f) : _functionID(f) {}
//     RegTreeNode() {}
// };

class RegressionTree: public Application
{
public:

    RegressionTree(std::shared_ptr<Launcher> launcher,
                   bool classification);

    ~RegressionTree();

    void run();

    void generateCode();

    
private:

    bool _classification;
    
    //! Physical path to the schema and table files.
    std::string _pathToFiles;

    //! QueryCompiler that is called to turn queries into views.
    std::shared_ptr<QueryCompiler> _compiler;

    //! TreeDecomposition of the the underlying join query.
    std::shared_ptr<TreeDecomposition> _td;
    
    //! Array containing the features of the model.
    var_bitset _features;

    var_bitset _categoricalFeatures;

    int _labelID;

    size_t _totalNumOfCandidates = 0;

    size_t* _numOfCandidates = nullptr;

    size_t* _complementFunction = nullptr;

    Query** _varToQueryMap = nullptr;

    std::vector<std::vector<double>> _thresholds;
    
    std::vector<Function*> _functionOfAggregate;
    std::vector<size_t> _varOfFunction;
    
    size_t* _queryRootIndex = nullptr;

    /* For each feature the candidate mask indicates which functions correspond
     * to that feature. */ 
    std::vector<prod_bitset> _candidateMask;
    
    void loadFeatures();

    void computeCandidates();
    
    void regressionTreeQueries();

    void classificationTreeQueries();
    
    void genDynamicFunctions();

    std::string genVarianceComputation();

    std::string genGiniComputation();
    
    void initializeThresholds();

    std::string dynamicFunctionsGenerator();
    
};

#endif /* INCLUDE_APP_REGRESSION_TREE_H_ */
