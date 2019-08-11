//--------------------------------------------------------------------
//
// DataStructureLayer.h
//
// Created on: 13/05/2019
// Author: Max
//
//--------------------------------------------------------------------

#ifndef INCLUDE_DATASTRUCTURELAYER_H_
#define INCLUDE_DATASTRUCTURELAYER_H_

#include <bitset>
#include <boost/dynamic_bitset.hpp>
#include <string>

#include <GlobalParams.hpp>
#include <QueryCompiler.h>

class DataStructureLayer 
{
public:

    DataStructureLayer(std::shared_ptr<Database> db,
                      std::shared_ptr<TreeDecomposition> td,
                      std::shared_ptr<QueryCompiler> qc);

    ~DataStructureLayer();
    
    void compile();

    bool requireHashing(size_t view_id);
    
private:

    std::shared_ptr<Database> _db;
    
    std::shared_ptr<TreeDecomposition> _td;
    
    std::shared_ptr<QueryCompiler> _qc;
    
    int localAggregateIndex;
    int loopAggregateIndex;
    int runSumAggregateIndex;

    bool* _requireHashing = nullptr;

    void requireHashing();
    
    // TODO: this should decide which groups to parallelize 
    void parallelize();

    // [TODO] We should also offer the option to sort the views each time
    
    // TODO: give IDs to aggregates ...
    // ... this would also need to handle composite functions
    // void generateAggregateIndexes();

    void setAggregateIndexesForAttributeOrder(
        AttributeOrder& attributeOrder, size_t depth);

    void setLoopAggregateIndexes(LoopRegister* loopRegister);
    
    void setOutputLoopAggregateIndexes(LoopRegister* loopRegister);
};

#endif /* INCLUDE_DATASTRUCTURELAYER_H_ */
