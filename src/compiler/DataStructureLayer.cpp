//--------------------------------------------------------------------
//
// DataStructureLayer.cpp
//
// Created on: 9 May 2019
// Author: Max
//
//--------------------------------------------------------------------

#include <boost/dynamic_bitset.hpp>

#include <thread>

#include <DataStructureLayer.h>
#include <Logging.hpp>
#include <ViewGroupCompiler.h>

typedef boost::dynamic_bitset<> dyn_bitset;

DataStructureLayer::DataStructureLayer(
    std::shared_ptr<Database> db, std::shared_ptr<TreeDecomposition> td,
    std::shared_ptr<QueryCompiler> qc) : _db(db), _td(td), _qc(qc)
{
}

DataStructureLayer::~DataStructureLayer()
{
    delete[] _requireHashing;
}

void DataStructureLayer::compile()
{
    /* Set aggregate Indedex for the aggregate register of each view group */ 
    for (size_t group = 0; group < _qc->numberOfViewGroups(); ++group)
    {
        localAggregateIndex = 0;
        loopAggregateIndex = 0;
        runSumAggregateIndex = 0;
        
        setAggregateIndexesForAttributeOrder(_qc->getAttributeOrder(group), 0);

        ViewGroup& viewGroup = _qc->getViewGroup(group);
        
        viewGroup.numberOfLocalAggregates = localAggregateIndex;
        viewGroup.numberOfLoopAggregates = loopAggregateIndex;
        viewGroup.numberOfRunningSums = runSumAggregateIndex;
    }

    requireHashing();
    parallelize();
}

bool DataStructureLayer::requireHashing(size_t view_id)
{
    return _requireHashing[view_id];
}
    

void DataStructureLayer::setLoopAggregateIndexes(LoopRegister* loopRegister)
{   
    // assign IDs to the local Function Aggregates
    for (LocalFunctionProduct& p : loopRegister->localProducts)
    {
        // TODO: check if this function is composite function! 
        p.aggregateIndex = localAggregateIndex;
        
        // std::cout << "\tlocal: " << localAggregateIndex << std::endl;

        ++localAggregateIndex;
    }

    // assign IDs to the local View Aggregates
    for (LocalViewAggregateProduct& p :
             loopRegister->localViewAggregateProducts)
    {
        // View Aggregate Products with only one view aggregate are inlined! 
        if (p.product.size() == 1) continue;
        
        // TODO: check if we can inline this product
        p.aggregateIndex = localAggregateIndex;

        // std::cout << "\tview: " << localAggregateIndex << std::endl;

        ++localAggregateIndex;
    }

    // assign IDs to partial Aggregates
    for (LoopAggregate* p : loopRegister->loopAggregateList)
    {
        // TODO: check if this product, or previous products, contain
        // composite functions?!
        
        p->aggregateIndex = loopAggregateIndex;

        // std::cout << "\tloopAgg: " << loopAggregateIndex << std::endl;

        ++loopAggregateIndex;
    }

    for (LoopRegister* innerLoop : loopRegister->innerLoops)
        setLoopAggregateIndexes(innerLoop);
    
    // assign IDs to partial Aggregates
    for (FinalLoopAggregate* p : loopRegister->loopRunningSums)
    {
        // TODO: check if this product, or previous products, contain
        // composite functions?!
        p->aggregateIndex = loopAggregateIndex;

        // std::cout << "\tloopRunSum: " << loopAggregateIndex << std::endl;

        ++loopAggregateIndex;
    }

        
    // assign IDs to partial Aggregates
    for (FinalLoopAggregate* p : loopRegister->finalLoopAggregates)
    {
        // TODO: check if this product, or previous products, contain
        // composite functions?!
        p->aggregateIndex = loopAggregateIndex;

        // std::cout << "\tfinalLoopAgg: " << loopAggregateIndex << std::endl;

        ++loopAggregateIndex;
    }

}


// TODO: TODO: TODO: we should be able to remove this function!! 
void DataStructureLayer::setOutputLoopAggregateIndexes(LoopRegister* loopRegister)
{
    // assign IDs to the local Function Aggregates
    for (LocalFunctionProduct& p : loopRegister->localProducts)
    {
        // TODO: check if this function is composite function!
        
        p.aggregateIndex = localAggregateIndex;
        
        // std::cout << "-- local: " << localAggregateIndex << std::endl;
        
        ++localAggregateIndex;
    }

    // assign IDs to the local View Aggregates
    for (LocalViewAggregateProduct& p :
             loopRegister->localViewAggregateProducts)
    {
        // TODO: check if we can inline this product
        p.aggregateIndex = localAggregateIndex;

        // std::cout << "-- view: " << localAggregateIndex << std::endl;
        ++localAggregateIndex;
    }

    // assign IDs to partial Aggregates
    for (LoopAggregate* p : loopRegister->loopAggregateList)
    {
        // TODO: check if this product, or previous products contain
        // composite functions?!

        // std::cout << "-- loopAgg: " << loopAggregateIndex << std::endl;

        p->aggregateIndex = loopAggregateIndex;
        ++loopAggregateIndex;
        // TODO: Create new datastructure that keeps track of the indexes 
    }

    for (LoopRegister* innerLoop : loopRegister->innerLoops)
        setOutputLoopAggregateIndexes(innerLoop);

    
    // assign IDs to partial Aggregates
    for (FinalLoopAggregate* p : loopRegister->loopRunningSums)
    {
        // TODO: check if this product, or previous products, contain
        // composite functions?!
        p->aggregateIndex = loopAggregateIndex;

        // std::cout << "\tloopRunSum: " << loopAggregateIndex << std::endl;

        ++loopAggregateIndex;
    }

        
    // assign IDs to partial Aggregates
    for (FinalLoopAggregate* p : loopRegister->finalLoopAggregates)
    {
        // TODO: check if this product, or previous products, contain
        // composite functions?!
        p->aggregateIndex = loopAggregateIndex;

        // std::cout << "\tfinalLoopAgg: " << loopAggregateIndex << std::endl;

        ++loopAggregateIndex;
    }
}


void DataStructureLayer::setAggregateIndexesForAttributeOrder(
    AttributeOrder& attributeOrder, size_t depth)
{
    AttributeOrderNode& orderNode = attributeOrder[depth];

    setLoopAggregateIndexes(orderNode._loopProduct);
    
    if (depth < attributeOrder.size()-1) 
        setAggregateIndexesForAttributeOrder(attributeOrder, depth+1);

    for (RunningSumAggregate* runSum : orderNode._registeredOutgoingRunningSum) 
    {
        runSum->aggregateIndex = runSumAggregateIndex;
        ++runSumAggregateIndex;
    }

    for (RunningSumAggregate* runSum : orderNode._registeredRunningSum) 
    {
        runSum->aggregateIndex = runSumAggregateIndex;
        ++runSumAggregateIndex;
    }

    if (orderNode._outgoingLoopRegister != nullptr)
        setOutputLoopAggregateIndexes(orderNode._outgoingLoopRegister);
}


void DataStructureLayer::requireHashing()
{
    _requireHashing = new bool[_qc->numberOfViews()]();
    
    for (size_t viewID = 0; viewID < _qc->numberOfViews(); ++viewID)
    {
        View* view = _qc->getView(viewID);
        
        const std::vector<size_t>& joinKeyOrder = _td->getJoinKeyOrder();
        const var_bitset& relSchema = _db->getRelation(view->_origin)->_schemaMask;

        bool hash = false;
        var_bitset viewVars = view->_fVars;

        // std::cout << viewID<<" fvars: " << view->_fVars.any()  << hash <<
        // std::endl;
        
        if (viewVars.any())
        {
            for (const size_t& att : joinKeyOrder)
            {        
                if (relSchema[att])
                {
                    if (!viewVars[att])
                    {
                        hash = true;
                        break;
                    }
                    else
                    {
                        viewVars.reset(att);

                        if (viewVars.none())
                            break;
                    }
                }
            }
        }
        
        // std::cout << viewID<< " fvars: " << view->_fVars.any()  << hash << std::endl;
        _requireHashing[viewID] = hash || viewVars.any();
    }

    DINFO("Hashing Info for each view: \n");
    for (size_t viewID = 0; viewID < _qc->numberOfViews(); ++viewID)
    {
        View* view = _qc->getView(viewID);
        
        DINFO("View: " << viewID << " (" << _db->getRelation(view->_origin)->_name
              << ", " << _db->getRelation(view->_destination)->_name << ") "
              << " FVars: " << view->_fVars.any()
              << " Hashing: " <<_requireHashing[viewID] << "\n");
    }
    
}


void DataStructureLayer::parallelize()
{   
    for (size_t gid = 0; gid < _qc->numberOfViewGroups(); ++gid)
    {
        ViewGroup& group = _qc->getViewGroup(gid);
        Relation* node = group._tdNode->_relation;

        if (node->getThreads() > 1)
        {
            group._parallelize = true;            
            group._threads =
                std::min(node->getThreads(), (size_t)std::thread::hardware_concurrency());
        }
    }
}

