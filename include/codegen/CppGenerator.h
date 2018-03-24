//--------------------------------------------------------------------
//
// CppGenerator.h
//
// Created on: 16 Dec 2017
// Author: Max
//
//--------------------------------------------------------------------

#ifndef INCLUDE_CODEGEN_CPPGENERATOR_H_
#define INCLUDE_CODEGEN_CPPGENERATOR_H_

#include <boost/dynamic_bitset.hpp>

#include <CodeGenerator.hpp>
#include <Launcher.h>
#include <QueryCompiler.h>
#include <TreeDecomposition.h>

// #define PREVIOUS

class CppGenerator: public CodeGenerator
{
public:

    CppGenerator(const std::string path,
                 std::shared_ptr<Launcher> launcher);

    ~CppGenerator();

    void generateCode();
    
private:
    
    std::string _pathToData;

    std::string _datasetName;

    std::shared_ptr<TreeDecomposition> _td;

    std::shared_ptr<QueryCompiler> _qc;
    
    std::vector<size_t>* variableOrder = nullptr;
    std::vector<var_bitset> variableOrderBitset;

    // TODO: RENAME 
    std::vector<size_t>* viewsPerVarInfo = nullptr;
    
    //TODO: this could be part of the view
    std::vector<size_t>* incomingViews = nullptr;


    // keeps track of where we can output tuples to the view
    size_t* viewLevelRegister;

    std::vector<size_t>* groupVariableOrder = nullptr;
    std::vector<size_t>* groupIncomingViews = nullptr;
    std::vector<size_t>* groupViewsPerVarInfo = nullptr;
    std::vector<var_bitset> groupVariableOrderBitset;
    bool* _requireHashing = nullptr;

    // For topological order 
    std::vector<size_t>* viewsPerNode = nullptr;

    // For groups of views that can be computed together
    std::vector<std::vector<size_t>> viewGroups;
    
    // For sorting
    size_t** sortOrders = nullptr;
    std::string* viewName = nullptr;
    size_t* viewToGroupMapping = nullptr;
    bool* _parallelizeGroup = nullptr;
    size_t* _threadsPerGroup = nullptr;

    // TODO: MAKE SURE WE SET THESE TO CORRECT SIZE; 
    std::unordered_map<std::string, std::pair<size_t,size_t>> aggregateMap;
    std::vector<std::vector<std::string>> aggregateRegister;
    std::vector<std::vector<boost::dynamic_bitset<>>> loopRegister;

    std::vector<boost::dynamic_bitset<>> preAggregateIndexes;
    std::vector<boost::dynamic_bitset<>> postAggregateIndexes;
    std::vector<boost::dynamic_bitset<>> dependentAggregateIndexes;

    std::vector<std::vector<size_t>> runningSumIndexesComplete;
    // std::vector<boost::dynamic_bitset<>> runningSumIndexesPartial;

    std::vector<std::vector<size_t>> finalAggregateIndexes;
    std::vector<std::vector<size_t>> newFinalAggregates;
    
    std::vector<std::string> finalAggregateString;
    std::vector<std::string> localAggregateString;
    std::string aggregateHeader;
    
    std::vector<boost::dynamic_bitset<>> viewLoopFactors;
    
    // std::vector<std::vector<size_t>> postAggregateIndexes;
    size_t* numAggregateIndexes = nullptr;

    std::vector<var_bitset> variableDependency;
    std::vector<var_bitset> coveredVariables;
    boost::dynamic_bitset<> addableViews;

    // std::unordered_map<std::string, std::pair<size_t,size_t>> deferredAggregateMap;
    // std::vector<std::vector<std::string>> deferredAggregateRegister;
    // std::vector<std::vector<boost::dynamic_bitset<>>> deferredLoopRegister;

    
    /* TODO: Technically there is no need to pre-materialise this! */
    void createVariableOrder();
    void createGroupVariableOrder();
    void createRelationSortingOrder(TDNode* node, const size_t& parent_id);

    void createTopologicalOrder();
    
    inline std::string offset(size_t off);

    std::string typeToStr(Type t);

//    std::string typeToStringConverter(Type t);

    std::string genHeader();

    std::string genTupleStructs();

    // std::string genFooter();
    // std::string genRelationTupleStructs(TDNode* rel);
    // std::string genViewTupleStructs(View* view, size_t view_id);

    std::string genCaseIdentifiers();
    
    std::string genLoadRelationFunction();
    
    std::string genComputeViewFunction(size_t view_id);

    std::string genComputeGroupFunction(size_t view_id);

    std::string genMaxMinValues(const std::vector<size_t>& view_id);
    
    std::string genPointerArrays(const std::string& rel, std::string& numOfJoinVars,
                                 bool parallelize);

    /* TODO: Simplify this to avoid sort for cases with few views! */
    std::string genRelationOrdering(const std::string& rel_name,
                                    const size_t& depth,
                                    const size_t& view_id);
    /* TODO: Simplify this to avoid sort for cases with few views! */
    std::string genGroupRelationOrdering(const std::string& rel_name,
                                         const size_t& depth,
                                         const size_t& group_id);
    
    std::string genLeapfrogJoinCode(size_t view_id, size_t depth);
    std::string genGroupLeapfrogJoinCode(size_t view_id, const TDNode& node,
                                         size_t depth);
    
    // One Generic Case for Seek Value 
    std::string seekValueCase(size_t depth, const std::string& rel_name,
                              const std::string& attr_name, bool parallel);
    
    std::string updateMaxCase(size_t depth, const std::string& rel_name,
                              const std::string& attr_name, bool parallelize);
    
    std::string getUpperPointer(const std::string rel_name, size_t depth, bool parallel);
    
    std::string getLowerPointer(const std::string rel_name, size_t depth);
    
    std::string updateRanges(size_t depth, const std::string& rel_name,
                             const std::string& attr_name, bool parallel);
    
    std::string getFunctionString(Function* f, std::string& fvars);

    std::string genSortFunction(const size_t& rel_id);

    std::string genFindUpperBound(const std::string& rel_name,
                                  const std::string& attrName,
                                  size_t depth, bool parallel); 
    
    std::string genRunFunction();
    std::string genRunMultithreadedFunction();

    bool resortRelation(const size_t& rel, const size_t& view);

    bool resortView(const size_t& incView, const size_t& view);

    // This can be removed ...
    bool requireHash(const size_t& rel, const size_t& view);

    // std::string getProductAggregate(const prod_bitset& product,
    //                                 const TDNode& node,
    //                                 Aggregate* aggregate,
    //                                 const var_bitset& varOrderBitset,
    //                                 size_t incomingCounter, size_t i);

    void aggregateGenerator(size_t group_id, const TDNode& node);

    void computeViewLevel(size_t group_id, const TDNode& node);

    void computeAggregateRegister(
        const size_t group_id, const size_t view_id, const size_t agg_id,
        std::vector<prod_bitset>& productSum, std::vector<size_t>& incoming,
        std::vector<std::pair<size_t, size_t>>& localAggReg,
        //  std::vector<std::pair<size_t, size_t>>& viewAggReg,
        bool splitViewAggSummation, size_t depth);
    
    void updateAggregateRegister(
        std::pair<size_t,size_t>& outputIndex, const std::string& aggString,
        size_t depth, const boost::dynamic_bitset<>& loopFactor, bool pre);

    std::string genAggregateString(
        const std::vector<std::string>& aggRegister,
        const std::vector<boost::dynamic_bitset<>>& loopReg,
        boost::dynamic_bitset<> consideredLoops,
        boost::dynamic_bitset<>& addedAggs, size_t depth);

    std::string genFinalAggregateString(
        const std::vector<std::string>& aggRegister,
        const std::vector<boost::dynamic_bitset<>>& loopReg,
        boost::dynamic_bitset<> consideredLoops,
        boost::dynamic_bitset<>& addedAggs, size_t depth,
        std::vector<size_t>& includableViews,
        boost::dynamic_bitset<>& addedViews,size_t offDepth);
    
};

#endif /* INCLUDE_CODEGEN_CPPGENERATOR_HPP_ */
