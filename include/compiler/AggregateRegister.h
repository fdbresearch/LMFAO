//--------------------------------------------------------------------
//
// AggregateRegister.h
//
// Created on: 07/10/2017
// Author: Max
//
// The view group compiler reads in a list of views and groups them 
// into view groups. 
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
    
private:
    std::shared_ptr<Database> _db;
    
    std::shared_ptr<TreeDecomposition> _td;
    
    std::shared_ptr<QueryCompiler> _qc;
    
    std::vector<AttributeOrder> attributeOrders;

    std::vector<var_bitset> variableDependency;
    
    boost::dynamic_bitset<> _incomingViewRegister;

    var_bitset dependentVariables;

    std::vector<RegisteredProduct> dependentComputation;
    
    var_bitset attOrderMask;

    void createAttributeOrder(AttributeOrder& attributeOrder,
                              const ViewGroup* viewGroup);
    
    void registerViews(size_t group_id);

    void registerAggregates(Aggregate* aggregate, const size_t group_id,
        const size_t view_id, const size_t agg_id, const size_t depth,
                            const size_t outputLevel,
         std::vector<RegisteredProduct*>& localAggReg,
         std::vector<RunningSumAggregate*>& runSumReg);
    
    // TODO: THE BELOW NEEDS TO BE FIXED --- This should come much later, it has
    // nothing to do with the aggregate registration
    // void requireHashing()
    // {
    //     // TODO: this should be separated out!
    //     // TODO: PROPERLY DEFINE THIS
    //     size_t** sortOrders;
    //     bool* _requireHashing = new bool[_qc->numberOfViews()]();
    //     for (size_t viewID = 0; viewID < _qc->numberOfViews(); ++viewID)
    //     {
    //         View* view = _qc->getView(viewID);
    //         bool hash = false;
    //         size_t orderIdx = 0;
    //         var_bitset intersection = view->_fVars & _db->getRelation(view->_origin)->_bag;
    //         for (size_t var = 0; var < multifaq::params::NUM_OF_ATTRIBUTES; ++var)
    //         {
    //             if (intersection[var])
    //             {
    //                 if (sortOrders[view->_origin][orderIdx] != var)
    //                 {
    //                     hash = true;
    //                     break;
    //                 }
    //                 ++orderIdx;
    //             }
    //         }
    //         _requireHashing[viewID] = hash || (view->_fVars & ~intersection).any();
    //     }
    // }
    
    
};



#endif /* INCLUDE_AGGREGATEREGISTER_H_ */
