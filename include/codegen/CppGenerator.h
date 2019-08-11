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

class CppGenerator: public CodeGenerator
{
public:

    CppGenerator(std::shared_ptr<Launcher> launcher);

    ~CppGenerator();

    void generateCode(bool hasApplicationHandler, bool hasDynamicFunctions);
    
private:

    std::shared_ptr<Database> _db;

    std::shared_ptr<TreeDecomposition> _td;

    std::shared_ptr<QueryCompiler> _qc;

    std::string* viewName;
    
    void generateMainFile(bool hasApplicationHandler);
    
    void generateMakeFile(bool hasApplicationHandler, bool hasDynamicFunctions);

    void generateDataHandler();

    void generateViewGroupFiles(size_t group_id, bool hasDynamicFunctions);

    /**** Used in generateMainFunction ****/
    
    std::string genLoadRelationFunction();

    std::string genSortFunction(const size_t& rel_id);

    std::string genRunFunction(bool parallelize, bool hasApplicationHandler);

    std::string genDumpFunction();

    std::string genTestFunction();
    
    /**** Used in generateDataHandler ****/ 

    std::string genTupleStructs();

    std::string genTupleStructConstructors();
    
    std::string genCaseIdentifiers();

    /**** Used in generateViewGroupFiles ****/ 

    std::string genComputeGroupFunction(size_t group_id);

    std::string genHashStructAndFunction(const ViewGroup& viewGroup);   

    // std::string genPointerArrays(const std::string& rel,std::string& numOfJoinVars,
    //                              bool parallelizeRelation);
    
    std::string genJoinAggregatesCode(
        size_t group_id, const AttributeOrder& varOrder,
        const TDNode& node, size_t depth);

    std::string genOutputLoopAggregateStringCompressed(
        const LoopRegister* loop, const TDNode& node, size_t depth,
        const std::vector<size_t>& contributingViews,const size_t numOfViewLoops,
        std::string& resetString);

    std::string genAggLoopStringCompressed(
        const LoopRegister* loop, const TDNode& node, size_t depth,
        const std::vector<size_t>& contributingViews, const size_t numOfOutViewLoops,
        std::string& resetString);

    std::string genProductString(
        const TDNode& node, const std::vector<size_t>& contributingViews,
        const prod_bitset& product);

    
};

#endif /* INCLUDE_CODEGEN_CPPGENERATOR_HPP_ */
