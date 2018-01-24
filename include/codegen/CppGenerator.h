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

#include <CodeGenerator.hpp>
#include <Launcher.h>
#include <QueryCompiler.h>
#include <TreeDecomposition.h>

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
    
    /* TODO: Technically there is no need to pre-materialise this! */
    void createVariableOrder();
    void createGroupVariableOrder();
    void createRelationSortingOrder(TDNode* node, const size_t& parent_id);

    void createTopologicalOrder();
    
    inline std::string offset(size_t off);

    std::string typeToStr(Type t);

    std::string typeToStringConverter(Type t);

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
    
    std::string genPointerArrays(const std::string& rel, std::string& numOfJoinVars);

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
                              const std::string& attr_name);
    
    std::string updateMaxCase(size_t depth, const std::string& rel_name,
                              const std::string& attr_name);
    
    std::string getUpperPointer(const std::string rel_name, size_t depth);
    
    std::string getLowerPointer(const std::string rel_name, size_t depth);
    
    std::string updateRanges(size_t depth, const std::string& rel_name,
                             const std::string& attr_name);
    
    std::string getFunctionString(Function* f, std::string& fvars);

    std::string genSortFunction(const size_t& rel_id);
    
    std::string genRunFunction();
    std::string genRunMultithreadedFunction();

    bool resortRelation(const size_t& rel, const size_t& view);

    bool resortView(const size_t& incView, const size_t& view);

    bool requireHash(const size_t& rel, const size_t& view);
    

};


#endif /* INCLUDE_CODEGEN_CPPGENERATOR_HPP_ */
