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
#include <cstring>

#include <CodeGenerator.hpp>
#include <Launcher.h>
#include <QueryCompiler.h>
#include <TreeDecomposition.h>

// #define PREVIOUS

const size_t LOOPIFY_THRESHOLD = 5;

namespace std
{
/**
 * Custom hash function for vector of pairs.
 */
    template<> struct hash<vector<pair<size_t,size_t>>>
    {
        HOT inline size_t operator()(const vector<pair<size_t,size_t>>& p) const
	{
            size_t seed = 0;
            hash<size_t> h;
            for (const pair<size_t,size_t>& d : p)
            {
                seed ^= h(d.first) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
                seed ^= h(d.second) + 0x9e3779b9 + (seed << 6) + (seed >> 2);    
            }
            return seed;
	}
    };
}


struct ProductAggregate
{
    prod_bitset product;
    std::vector<std::pair<size_t,size_t>> viewAggregate;
    std::pair<size_t,size_t> previous;
    std::pair<size_t,size_t> correspondingLoopAgg;

    bool multiplyByCount = false;
    
    bool operator==(const ProductAggregate &other) const
    {
        return this->product == other.product &&
            this->viewAggregate == other.viewAggregate &&
            this->previous == other.previous &&
            this->multiplyByCount == other.multiplyByCount;
    }
};

struct ProductAggregate_hash
{
    size_t operator()(const ProductAggregate &key ) const
    {
        size_t h = 0;
        h ^= std::hash<prod_bitset>()(key.product)+ 0x9e3779b9 + (h<<6) + (h>>2);
        h ^= std::hash<size_t>()(key.previous.first)+ 0x9e3779b9 + (h<<6) + (h>>2);
        h ^= std::hash<size_t>()(key.previous.second)+ 0x9e3779b9 + (h<<6) + (h>>2);
        h ^= std::hash<std::vector<std::pair<size_t,size_t>>>()(key.viewAggregate)+
            0x9e3779b9 + (h<<6) + (h>>2);
        h ^= std::hash<bool>()(key.multiplyByCount)+0x9e3779b9 + (h<<6) + (h>>2);
        return h;
    }
};

struct PostLoopAgg
{
    std::pair<size_t,size_t> local;
    std::pair<size_t,size_t> post;

    bool operator==(const PostLoopAgg &other) const
    {
        return this->local == other.local &&
            this->post == other.post;
    }
};

struct PostLoopAgg_hash
{
    size_t operator()(const PostLoopAgg &key ) const
    {
        size_t h = 0;
        h ^= std::hash<size_t>()(key.local.first)+ 0x9e3779b9 + (h<<6) + (h>>2);
        h ^= std::hash<size_t>()(key.local.second)+ 0x9e3779b9 + (h<<6) + (h>>2);
        h ^= std::hash<size_t>()(key.post.first)+ 0x9e3779b9 + (h<<6) + (h>>2);
        h ^= std::hash<size_t>()(key.post.second)+ 0x9e3779b9 + (h<<6) + (h>>2);
        return h;
    }
};

struct DependentComputation
{
    prod_bitset product;
    std::vector<std::pair<size_t,size_t>> view;    
};


struct AggRegTuple
{
    // bool hasPreviousComputation = false;
    std::pair<size_t,size_t> previous;
    std::pair<bool,size_t> product;
    std::pair<bool,size_t> view;
    std::pair<size_t,size_t> postLoop;

    bool postLoopAgg = false;
    bool preLoopAgg = false;

    size_t prevDepth = multifaq::params::NUM_OF_VARIABLES;

    bool newViewProduct = false;
    bool singleViewAgg = false;
    std::pair<size_t,size_t> viewAgg;

    bool multiplyByCount = false;
    // size_t view;
    // size_t loopID;
    // std::pair<size_t,size_t> localAgg;

    bool operator==(const AggRegTuple &other) const
    {
        return this->product == other.product &&
            this->view == other.view &&
            this->viewAgg == other.viewAgg &&
            this->previous == other.previous &&
            // this->localAgg == other.localAgg &&
            this->postLoop == other.postLoop &&
            this->preLoopAgg == other.preLoopAgg &&
            this->postLoopAgg == other.postLoopAgg;
        
    }
};

struct AggRegTuple_hash
{
    size_t operator()(const AggRegTuple &key ) const
    {
        size_t h = 0;
        h ^= std::hash<size_t>()(key.previous.first)+ 0x9e3779b9 + (h<<6) + (h>>2);
        h ^= std::hash<size_t>()(key.previous.second)+ 0x9e3779b9 + (h<<6) + (h>>2);
        h ^= std::hash<bool>()(key.product.first)+ 0x9e3779b9 + (h<<6) + (h>>2);
        h ^= std::hash<size_t>()(key.product.second)+ 0x9e3779b9 + (h<<6) + (h>>2);
        h ^= std::hash<bool>()(key.view.first)+ 0x9e3779b9 + (h<<6) + (h>>2);
        h ^= std::hash<size_t>()(key.view.second)+ 0x9e3779b9 + (h<<6) + (h>>2);
        h ^= std::hash<size_t>()(key.viewAgg.first)+ 0x9e3779b9 + (h<<6) + (h>>2);
        h ^= std::hash<size_t>()(key.viewAgg.second)+ 0x9e3779b9 + (h<<6) + (h>>2);
        // h ^= std::hash<size_t>()(key.localAgg.first)+ 0x9e3779b9 + (h<<6) + (h>>2);
        // h ^= std::hash<size_t>()(key.localAgg.second)+ 0x9e3779b9 + (h<<6) + (h>>2);
        h ^= std::hash<size_t>()(key.postLoop.first)+ 0x9e3779b9 + (h<<6) + (h>>2);
        h ^= std::hash<size_t>()(key.postLoop.second)+ 0x9e3779b9 + (h<<6) + (h>>2);
        return h;
    }
};

struct PostAggRegTuple
{
    std::pair<size_t,size_t> local = {0,0};
    std::pair<size_t,size_t> post  = {0,0};

    bool operator==(const PostAggRegTuple &other) const
    {
        return this->local == other.local &&
            this->post == other.post;
    }
};

struct PostAggRegTuple_hash
{
    size_t operator()(const PostAggRegTuple &key ) const
    {
        size_t h = 0;
        h ^= std::hash<size_t>()(key.local.first)+ 0x9e3779b9 + (h<<6) + (h>>2);
        h ^= std::hash<size_t>()(key.local.second)+ 0x9e3779b9 + (h<<6) + (h>>2);
        h ^= std::hash<size_t>()(key.post.first)+ 0x9e3779b9 + (h<<6) + (h>>2);
        h ^= std::hash<size_t>()(key.post.second)+ 0x9e3779b9 + (h<<6) + (h>>2);
        return h;
    }
};

struct AggregateTuple
{
    size_t viewID;
    size_t aggID;
    std::pair<size_t,size_t> local;
    std::pair<size_t,size_t> post;
    std::pair<bool, size_t> dependentProduct;
    std::pair<bool, size_t> dependentView;
    size_t loopID;

    DependentComputation dependentComputation;

    bool hasDependentComputation = false;
    ProductAggregate dependentProdAgg;
};

struct DependentLoop
{
    std::vector<std::pair<size_t,prod_bitset>> functionMask;
    std::vector<size_t> next;
    
    boost::dynamic_bitset<> outView;
    boost::dynamic_bitset<> loopFactors;

    size_t loopVariable;
    prod_bitset branchFunctions; // TODO: These need to be specific for the
                                 // actual view!! 
    
    DependentLoop(size_t numOfViews)
    {
        outView.resize(numOfViews + 1);
        outView.reset();
    }
};


struct AggregateIndexes
{
    std::bitset<7> present;
    size_t indexes[8];

    void reset()
    {
        present.reset();
        memset(&indexes[0], 0, 8*sizeof(size_t));
    }

    bool isIncrement(const AggregateIndexes& bench, const size_t& offset,
                     std::bitset<7>& increasing)
    {
        if (present != bench.present)
            return false;
        
        // If offset is zero we didn't compare any two tuples yet
        if (offset==0)
        {
            // compare this and bench
            increasing.reset();
            for (size_t i = 0; i < 7; i++)
            {
                if (present[i])
                {
                    if (indexes[i] == bench.indexes[i] + 1)
                    {
                        increasing.set(i);
                    }
                    else if (indexes[i] != bench.indexes[i])
                    {
                        return false;
                    }
                }
            }
            return true;
        }
        
        for (size_t i = 0; i < 7; i++)
        {
            if (present[i])
            {
                if (increasing[i])
                {
                    if (indexes[i] != bench.indexes[i] + offset + 1)
                        return false;
                }
                else
                {
                    if (indexes[i] != bench.indexes[i])
                        return false;
                }
            }
        }
        return true;
    }
};


class CppGenerator: public CodeGenerator
{
public:

    CppGenerator(const std::string path, const std::string outputDirectory,
                 const bool multioutput_flag, const bool resort_flag,
                 const bool microbench_flag, const bool compression_flag,
                 const bool column_flag, const bool linear_flag, const bool trie_flag,
                 std::shared_ptr<Launcher> launcher);

    ~CppGenerator();

    void generateCode(const ParallelizationType parallelization_type,
                      bool hasApplicationHandler, bool hasDynamicFunctions);

    size_t numberOfGroups()
    {
        return viewGroups.size();
    };

    const std::vector<size_t>& getGroup(size_t group_id)
    {
        return viewGroups[group_id];
    }
    
private:
    std::string _pathToData;

    std::string _outputDirectory;

    std::string _datasetName;

    std::shared_ptr<TreeDecomposition> _td;

    std::shared_ptr<QueryCompiler> _qc;
    
    std::vector<size_t>* variableOrder = nullptr;
    
    std::vector<var_bitset> variableOrderBitset;

    //TODO: this could be part of the view
    std::vector<size_t>* incomingViews = nullptr;
    
    // keeps track of where we can output tuples to the view
    size_t* viewLevelRegister;

    std::vector<size_t>* groupVariableOrder = nullptr;
    std::vector<size_t>* groupIncomingViews = nullptr;
    std::vector<size_t>* groupViewsPerVarInfo = nullptr;
    std::vector<var_bitset> groupVariableOrderBitset;
    bool* _requireHashing = nullptr;

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

    bool _hasApplicationHandler;
    bool _hasDynamicFunctions;
    
    // std::unordered_map<std::string, std::pair<size_t,size_t>> deferredAggregateMap;
    // std::vector<std::vector<std::string>> deferredAggregateRegister;
    // std::vector<std::vector<boost::dynamic_bitset<>>> deferredLoopRegister;
    
/**************************************************************************/
/**************************************************************************/
    std::vector<std::vector<AggRegTuple>> newAggregateRegister;
    std::vector<std::unordered_map<
                    AggRegTuple, size_t, AggRegTuple_hash>> aggregateRegisterMap;

    std::vector<std::vector<size_t>> aggregateRemapping;
    std::vector<std::vector<std::vector<size_t>>> newAggregateRemapping;
    size_t aggregateCounter;

    std::vector<std::unordered_map<ProductAggregate,size_t,ProductAggregate_hash>>
    productToVariableMap;
    
    std::vector<std::vector<ProductAggregate>> productToVariableRegister;
    
    std::vector<std::unordered_map<prod_bitset,size_t>> localProductMap;
    std::vector<std::vector<prod_bitset>> localProductList;
    std::vector<std::vector<size_t>> localProductRemapping;
    // std::vector<std::vector<size_t>> newLocalProductRemapping;
    size_t productCounter;
    
    std::vector<std::map<std::vector<std::pair<size_t,size_t>>,size_t>> viewProductMap;
    std::vector<std::vector<std::vector<std::pair<size_t,size_t>>>> viewProductList;
    std::vector<std::vector<size_t>> viewProductRemapping;
    
    // std::vector<std::vector<size_t>> newViewProductRemapping;
    size_t viewCounter;

    std::vector<std::vector<size_t>> postRemapping;
    size_t postCounter;
    
    std::vector<std::unordered_map<
                    PostAggRegTuple,size_t,PostAggRegTuple_hash>>postRegisterMap;

    std::vector<std::vector<PostAggRegTuple>> postRegisterList;

    std::vector<std::vector<boost::dynamic_bitset<>>> contributingViewList;
    // std::vector<std::vector<dyn_bitset>> productLoopFactors;

    std::vector<std::map<boost::dynamic_bitset<>, size_t>> totalLoopFactors;
    std::vector<std::vector<boost::dynamic_bitset<>>> totalLoopFactorList;

    std::vector<std::vector<size_t>> productLoopID;
    std::vector<std::vector<size_t>> incViewLoopID;
    std::vector<size_t> outViewLoopID;
    std::vector<std::vector<size_t>> aggRegLoopID;

    std::vector<std::vector<AggregateTuple>> aggregateComputation;
    std::vector<DependentComputation> dependentComputation;

    std::vector<boost::dynamic_bitset<>> listOfLoops;
    std::vector<prod_bitset> functionsPerLoop;
    std::vector<prod_bitset> functionsPerLoopBranch;
    std::vector<boost::dynamic_bitset<>> viewsPerLoop;
    std::vector<std::vector<size_t>> nextLoopsPerLoop;

    std::vector<DependentLoop> depListOfLoops;

    std::vector<boost::dynamic_bitset<>> outViewLoop;
    
    std::vector<boost::dynamic_bitset<>> outputLoops;
    std::vector<std::vector<size_t>> outputNextLoops;
    std::vector<boost::dynamic_bitset<>> outputViewsPerLoop;

    
    std::vector<std::vector<ProductAggregate>> outProductToVariableRegister;

    std::vector<std::vector<prod_bitset>> outLocalProductRegister;
    std::vector<std::vector<std::vector<std::pair<size_t,size_t>>>>
    outViewProductRegister;
    std::vector<std::vector<AggRegTuple>> outAggregateRegister;

    std::vector<std::vector<size_t>> depAggregateRemapping;
    std::vector<std::vector<size_t>> depLocalProductRemapping;

/**************************************************************************/
/**************************************************************************/
    
    /* TODO: Technically there is no need to pre-materialise this! */
    void createGroupVariableOrder();
    void createRelationSortingOrder(TDNode* node, const size_t& parent_id);

    void computeViewOrdering();

    void computeParallelizeGroup(bool paralleize_groups);

    void genDataHandler();
    void genComputeGroupFiles(size_t group);
    void genMakeFile();
    void genMainFunction(bool parallelize);
    
    prod_bitset computeLoopMasks(
        prod_bitset presentFunctions, boost::dynamic_bitset<> consideredLoops,
        const var_bitset& varOrderBitset, const var_bitset& relationBag,
        const boost::dynamic_bitset<>& contributingViews,
        boost::dynamic_bitset<>& nextVariable,
        const boost::dynamic_bitset<>& prefixLoops);

    void registerAggregatesToLoops(size_t depth, size_t group_id);

    void registerDependentComputationToLoops(size_t view_id, size_t group_id);

    std::pair<size_t,size_t> addProductToLoop(
        ProductAggregate& prodAgg, size_t& currentLoop, bool& loopsOverRelation,
        const size_t& maxDepth);
    
    inline std::string offset(size_t off);

    std::string typeToStr(Type t);
    
    std::string genHeader();

    std::string genTupleStructs();

    std::string genTupleStructConstructors();
    
    std::string genCaseIdentifiers();
    
    std::string genLoadRelationFunction();
    
    // std::string genComputeViewFunction(size_t view_id);

    std::string genComputeGroupFunction(size_t view_id);

    std::string genMaxMinValues(const std::vector<size_t>& view_id);
    
    std::string genPointerArrays(const std::string& rel, std::string& numOfJoinVars,
                                 bool parallelize);

    std::string genGroupRelationOrdering(const std::string& rel_name,
                                         const size_t& depth,
                                         const size_t& group_id);
    
    std::string genGroupLeapfrogJoinCode(size_t group_id, const TDNode& node,
                                         size_t depth);

    std::string genGroupGenericJoinCode(size_t group_id, const TDNode& node,
                                        size_t depth);

    // One Generic Case for Seek Value 
    std::string seekValueCase(size_t depth, const std::string& rel_name,
                              const std::string& attr_name, bool parallel);

    // One Generic Case for Seek Value 
    std::string seekValueGenericJoin(size_t depth, const std::string& rel_name,
                                     const std::string& attr_name, bool parallel, bool first);
    
    std::string updateMaxCase(size_t depth, const std::string& rel_name,
                              const std::string& attr_name, bool parallelize);
    
    std::string getUpperPointer(
        const std::string rel_name, size_t depth, bool parallel);
    
    std::string getLowerPointer(const std::string rel_name, size_t depth);

    size_t varDepthInView(const std::string& rel_name,
                                       const std::string& attr_name, size_t depth);
    
    std::string updateRanges(size_t depth, const std::string& rel_name,
                             const std::string& attr_name, bool parallel);
    
    std::string genProductString(
        const TDNode& node, const boost::dynamic_bitset<>& contributingViews,
        const prod_bitset& product);

    std::string getFunctionString(Function* f, std::string& fvars);

    std::string genSortFunction(const size_t& rel_id);
 
    std::string genFindUpperBound(const std::string& rel_name,
                                  const std::string& attrName,
                                  size_t depth, bool parallel); 
    
    std::string genRunFunction(bool parallelize);
    
    // std::string genRunMultithreadedFunction();

    std::string genTestFunction();
    std::string genDumpFunction();
    
    bool resortRelation(const size_t& rel, const size_t& view);

    bool resortView(const size_t& incView, const size_t& view);

    // This can be removed ...
    bool requireHash(const size_t& rel, const size_t& view);
    
    void aggregateGenerator(size_t group_id, const TDNode& node);

    void computeViewLevel(size_t group_id, const TDNode& node);

    // void computeAggregateRegister(
    //     const size_t group_id, const size_t view_id, const size_t agg_id,
    //     std::vector<prod_bitset>& productSum, std::vector<size_t>& incoming,
    //     std::vector<std::pair<size_t, size_t>>& localAggReg,
    //     //  std::vector<std::pair<size_t, size_t>>& viewAggReg,
    //     bool splitViewAggSummation, size_t depth);
    // void computeAggregateRegister( Aggregate* aggregate,
    //     const size_t group_id, const size_t view_id,const size_t agg_id,
    //     const size_t depth, std::vector<std::pair<size_t, size_t>>& localAggReg);

    void registerAggregatesToVariables(Aggregate* aggregate,
        const size_t group_id, const size_t view_id,const size_t agg_id,
        const size_t depth, std::vector<std::pair<size_t, size_t>>& localAggReg);

    // TODO: This can be savely removed! 
    // void updateAggregateRegister(
    //     std::pair<size_t,size_t>& outputIndex, const std::string& aggString,
    //     size_t depth, const boost::dynamic_bitset<>& loopFactor, bool pre);

    // std::string newgenAggregateString(
    //     const TDNode& node, boost::dynamic_bitset<> consideredLoops,
    //     size_t depth, size_t group_id);

    // std::string newgenFinalAggregateString(
    //     const TDNode* node, boost::dynamic_bitset<> consideredLoops,
    //     size_t depth, size_t group_id);


    // template<typename T>
    // std::string resetRegisterArray(
    //     const size_t& depth, std::vector<std::vector<T>>& registerList,
    //     std::string registerName);

    prod_bitset computeDependentLoops(
        size_t view_id, prod_bitset presentFunctions, var_bitset relationBag,
        boost::dynamic_bitset<> contributingViews, var_bitset varOrderBitset,
        boost::dynamic_bitset<> addedOutLoops, size_t thisLoopID);

    std::pair<size_t,size_t> addDependentProductToLoop(
        ProductAggregate& prodAgg, const size_t view_id, size_t& currentLoop,
        const size_t& maxDepth, std::pair<size_t,size_t>& prevAgg);

    std::string genDependentAggLoopString(
        const TDNode& node, const size_t currentLoop, size_t depth,
        const boost::dynamic_bitset<>& contributingViews, const size_t maxDepth,
        std::string& resetString);

    
    void mapAggregateToIndexes(AggregateIndexes& index, const AggRegTuple& aggregate,
                               const size_t& depth, const size_t& loopID);

    void mapAggregateToIndexes(AggregateIndexes& index,const PostAggRegTuple& aggregate,
                               const size_t& depth, const size_t& loopID);

    void mapAggregateToIndexes(AggregateIndexes& index, const AggregateTuple& aggregate,
                               const size_t& viewID, const size_t& loopID);
    
    std::string outputAggRegTupleString(
        AggregateIndexes& first, size_t offset,const std::bitset<7>& increasing,
        size_t stringOffset, bool postLoop);

    std::string outputPostRegTupleString(
        AggregateIndexes& first, size_t offset,const std::bitset<7>& increasing,
        size_t stringOffset);

    std::string outputFinalRegTupleString(
        AggregateIndexes& first, size_t offset,const std::bitset<7>& increasing,
        size_t stringOffset);

    // // TODO: We should really only keep one 
    // std::string genAggLoopString(
    //     const TDNode& node, const size_t loop, size_t depth,
    //     const boost::dynamic_bitset<>& contributingViews,
    //     const size_t numOfOutViewLoops);
    
    std::string genAggLoopStringCompressed(
        const TDNode& node, const size_t loop, size_t depth,
        const boost::dynamic_bitset<>& contributingViews,
        const size_t numOfOutViewLoops, std::string& resetString);

    bool isRelation(std::string relName);

    std::string relationFieldAccess(std::string relName, std::string field, std::string index);

    std::string relationSize(std::string relName);

#ifdef OLD
    // TODO: RENAME 
    std::vector<size_t>* viewsPerVarInfo = nullptr;

    // For topological order 
    std::vector<size_t>* viewsPerNode = nullptr;

    template<typename T>
    std::string resetRegisterArray(
        const size_t& depth, std::vector<std::vector<T>>& registerList,
        std::string registerName);

        
    std::string genComputeViewFunction(size_t view_id);
    

    std::string genRelationOrdering(const std::string& rel_name,
                                    const size_t& depth,
                                    const size_t& view_id);

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

    std::string genLeapfrogJoinCode(size_t view_id, size_t depth);

    void createVariableOrder();

#endif
};

#endif /* INCLUDE_CODEGEN_CPPGENERATOR_HPP_ */
