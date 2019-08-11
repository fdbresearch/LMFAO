//--------------------------------------------------------------------
//
// AggregateRegister.h
//
// Created on: 07/05/2019
// Author: Max
//
//--------------------------------------------------------------------

#ifndef INCLUDE_AGGREGATEREGISTER_H_
#define INCLUDE_AGGREGATEREGISTER_H_

#include <bitset>
#include <boost/dynamic_bitset.hpp>
#include <string>

#include <GlobalParams.hpp>
#include <QueryCompiler.h>
#include <ViewGroupCompiler.h>

class AggregateRegister 
{
public:

    AggregateRegister(std::shared_ptr<Database> db,
                      std::shared_ptr<TreeDecomposition> td,
                      std::shared_ptr<QueryCompiler> qc);

    ~AggregateRegister();
    
    void compile();

    AttributeOrder& getAttributeOrder(size_t group_id);
    
private:
    std::shared_ptr<Database> _db;
    
    std::shared_ptr<TreeDecomposition> _td;
    
    std::shared_ptr<QueryCompiler> _qc;
    
    std::vector<AttributeOrder> attributeOrders;

    std::vector<var_bitset> variableDependency;
    
    var_bitset attOrderMask;

    void createAttributeOrder(
        AttributeOrder& attributeOrder, const ViewGroup* viewGroup);
    
    size_t registerOutgoingView(
        size_t viewID, AttributeOrder& order, const var_bitset& attOrderMask);
    
    void registerAggregates(
        Aggregate* aggregate, const View* view, const size_t group_id,
        const size_t depth, const size_t outputLevel,
        const var_bitset depedentVariables,
        std::vector<RegisteredProduct*>& localAggReg,
        std::vector<RunningSumAggregate*>& runSumReg,
        std::vector<RegisteredProduct*> dependentProducts);
    
    void registerLocalAggregatesWithLoops(
        Relation* relation, AttributeOrderNode &orderNode);
    
    FinalLoopAggregate* registerProductToLoop(    
        LoopRegister* loopRegister, const ViewAggregateProduct& viewAggregateProduct,
        FinalLoopAggregate* previousAggregate,
        std::map<dyn_bitset,prod_bitset>& loopFunctionMask,
        dyn_bitset consideredLoops, bool multiplyByCount);
    
    void createLoopsForOutgoingView(
        const View* view, AttributeOrderNode& orderNode, const Relation* relation,
        std::vector<LoopRegister*>& outgoingLoopPointers);
    
    void registerOutputAggregatesToLoops(
        OutgoingView &outView, const std::vector<RegisteredProduct*>& dependentProducts,
        const std::vector<RunningSumAggregate*>& runSumReg,
        std::vector<LoopRegister*>& outgoingLoopPointers,const Relation* relation);
};


#endif /* INCLUDE_AGGREGATEREGISTER_H_ */
