//--------------------------------------------------------------------
//
// AggregateRegister.cpp
//
// Created on: 9 May 2019
// Author: Max
//
//--------------------------------------------------------------------

#include <boost/dynamic_bitset.hpp>

#include <AggregateRegister.h>
#include <Logging.hpp>

AggregateRegister::AggregateRegister(
    std::shared_ptr<Database> db,
    std::shared_ptr<TreeDecomposition> td,
    std::shared_ptr<QueryCompiler> qc) :_db(db), _td(td), _qc(qc)
{  }

AggregateRegister::~AggregateRegister()
{  }


AttributeOrder& AggregateRegister::getAttributeOrder(size_t group)
{
    return attributeOrders[group];
}

void AggregateRegister::compile()
{
    /* Stores for each variable v, all variables that v depends on */
    variableDependency.resize(multifaq::params::NUM_OF_VARIABLES);
    
    /* Populate variableDepedecy */ 
    for (size_t rel = 0; rel < _db->numberOfRelations(); ++rel)
    {
        var_bitset relSchema = _db->getRelation(rel)->_schemaMask;

        for (size_t var = 0; var < multifaq::params::NUM_OF_VARIABLES; ++var)
        {
            if (relSchema[var])
                variableDependency[var] |= relSchema;
        }
    }

    /* AttributesOrders stores one order for each view group. */
    attributeOrders.resize(_qc->numberOfViewGroups());
    
    for (size_t group = 0; group < _qc->numberOfViewGroups(); ++group)
    {
        ViewGroup& viewGroup = _qc->getViewGroup(group);
        Relation* rel = viewGroup._tdNode->_relation;
        
        /* The first node in the order is a dummy node without an attribute */
        attributeOrders[group].push_back({multifaq::params::NUM_OF_ATTRIBUTES});
        
        /* Create attribute order. */
        createAttributeOrder(attributeOrders[group], &viewGroup);        
        
        /* Next we register views and aggregates to the order */
        for (const size_t& viewID : viewGroup._views)
        {
            View* view = _qc->getView(viewID);
            var_bitset nonVarOrder = view->_fVars & ~attOrderMask;    

            /* Compute set of variables that depend on the free variables in
             * head of view */
            var_bitset dependentVariables;
            for (size_t var = 0; var < multifaq::params::NUM_OF_VARIABLES; ++var)
            {
                /* We compute all dependent variables for all non-join variables. */
                if (nonVarOrder[var])
                {
                    /* check if the relation contains this variable */
                    if (rel->_schemaMask[var])
                        dependentVariables |= rel->_schemaMask & ~attOrderMask;
                    else
                    {
                        /* Check incoming views */
                        for (const size_t& incViewID :view->_incomingViews)
                        {
                            View* incView = _qc->getView(incViewID);
                            if (incView->_fVars[var])
                                dependentVariables |= incView->_fVars & ~attOrderMask;
                        }
                    }
                }
            }

            size_t outputLevel = registerOutgoingView(
                viewID,attributeOrders[group],attOrderMask);

            /* Find AttributeOrderNode that corresponds to outputLevel for the view. */
            AttributeOrderNode &outputNode = attributeOrders[group][outputLevel];

            /* Add this view as an outgoing view to this node. */
            // outputNode._outgoingViews.push_back({viewID});

            std::vector<LoopRegister*> outgoingLoopPointers;
            createLoopsForOutgoingView(view, outputNode, rel, outgoingLoopPointers);

            /* Add this view as an outgoing view to this node. */
            outgoingLoopPointers.back()->outgoingViews.push_back({viewID});

            /* Iterate over each aggregate and register it to the AttributeOrder. */
            for (size_t aggNo = 0; aggNo < view->_aggregates.size(); ++aggNo)
            {
                /* We use a copy of the actual aggregate */
                Aggregate aggregate = *(view->_aggregates[aggNo]);
                
                std::vector<RegisteredProduct*> localAggReg(
                    aggregate._sum.size(), nullptr);
                
                std::vector<RunningSumAggregate*> runSumReg(
                    aggregate._sum.size(), nullptr);

                std::vector<RegisteredProduct*> dependentProducts(
                    aggregate._sum.size(), new RegisteredProduct());

                /**************************************************/

                registerAggregates(
                    &aggregate,view,group,0,outputLevel,dependentVariables,
                    localAggReg,runSumReg,dependentProducts);

                std::cout << "++++++++" << aggNo << "  " <<
                    aggregate._sum.size() << "  "<< localAggReg[0] << "  " <<
                    runSumReg[0]  << "  " << runSumReg[0]->local << std::endl;
                
                /**************************************************/

                registerOutputAggregatesToLoops(
                    outgoingLoopPointers.back()->outgoingViews.back(),
                    dependentProducts,runSumReg,
                    outgoingLoopPointers, rel);
            }
        }


        /* Register empty product which computes the count for Relation! */
        /* Register the products to Loops */ 
        for (size_t depth = 0; depth < attributeOrders[group].size(); ++depth)
        {
            AttributeOrderNode& orderNode = attributeOrders[group][depth];

            if (orderNode._loopProduct== nullptr)
                orderNode._loopProduct = new LoopRegister(_qc->numberOfViews()+1);

            if (depth+1 == attributeOrders[group].size())
                orderNode._loopProduct->localProducts.push_back(prod_bitset());

            registerLocalAggregatesWithLoops(rel, orderNode);
        }
    }
}


void AggregateRegister::createAttributeOrder(
    AttributeOrder& attributeOrder, const ViewGroup* viewGroup)
{
    var_bitset joinAttributes;

    TDNode* baseNode = viewGroup->_tdNode;
    Relation* baseRelation = baseNode->_relation;

    // Find join attributes for all views in the Group 
    for (const size_t& viewID : viewGroup->_views)
    {
        View* view = _qc->getView(viewID);
        Relation* destRelation = _db->getRelation(view->_destination);

        // Add the intersection between the origin and destination to the joinAttributes
        if (view->_origin != view->_destination)
            joinAttributes |= destRelation->_schemaMask & baseRelation->_schemaMask;

        // TODO: there should never be more join attributes than the
        // intersection between the origin and destination relations
        
        for (const Aggregate* aggregate : view->_aggregates)
        {
            // First find the all views that contribute to this Aggregate
            for (const Product &p : aggregate->_sum)
            {
                for (const auto& incAggregate : p._incoming)
                {
                    View* incView = _qc->getView(incAggregate.first);
                        
                    // Add the intersection of the view and the base
                    // relation to joinAttributes
                    joinAttributes |= (incView->_fVars & baseRelation->_schemaMask);
                }
            }
        }
    }

    /* Add the join variables to the variable order */
    for (const size_t& att : _td->getJoinKeyOrder())
    {        
        if (joinAttributes[att])
        {
            attributeOrder.push_back(att);
            joinAttributes.reset(att);
        }
    }

    /* Verify that all join attributes were captured. */
    if (joinAttributes.any())
    {
        ERROR("The joinKeyOrder for "<< baseRelation->_name
              << "does not contain all join attributes.");
        ERROR("The following attributes are not in the order: \n");

        for (size_t att = 0; att < multifaq::params::NUM_OF_ATTRIBUTES; ++att)
        {
            if (joinAttributes[att])
                ERROR(_db->getAttribute(att)->_name << " -- " << att << "\n");
        }
        exit(1);
    }

    /* Compute mask of the variables in the order */
    attOrderMask.reset();
    for (const AttributeOrderNode& node : attributeOrder)
        attOrderMask.set(node._attribute);

    var_bitset coveredVariableOrder;
    
    /* We are setting the join views for each attribute in the order */
    for (AttributeOrderNode& orderNode : attributeOrder)
    {
        const size_t attribute = orderNode._attribute;
        
        var_bitset& coveredVariables = orderNode._coveredVariables;
        view_bitset& coveredIncViews = orderNode._coveredIncomingViews;
        
        coveredVariableOrder.set(attribute);
        coveredVariables.set(attribute);

        /* If the relation contains the variable add it to the order */
        if (baseRelation->_schemaMask[orderNode._attribute])
            orderNode._registerdRelation = viewGroup->_tdNode->_relation;

        /* If all join attributes of the relation are covered then add the
         * non-join attributes to the covered variables. */
        var_bitset relationJoinAttributes = baseRelation->_schemaMask & attOrderMask;
        if ((relationJoinAttributes & coveredVariableOrder) == relationJoinAttributes)
        {
            coveredVariables |= baseRelation->_schemaMask;
        }
        
        for (const size_t& incViewID : viewGroup->_incomingViews) 
        {
            // Check if view is covered !
            View* incView = _qc->getView(incViewID);
                
            var_bitset viewJoinVars = incView->_fVars & attOrderMask;
            if ((viewJoinVars & coveredVariableOrder) == viewJoinVars) 
            {
                coveredIncViews.set(incViewID);
                coveredVariables |= incView->_fVars;
            }
        }
        
        for (const size_t& incViewID : viewGroup->_incomingViews)
        {
            const var_bitset& viewFVars = _qc->getView(incViewID)->_fVars;

            if (viewFVars[orderNode._attribute])
                orderNode._joinViews.push_back(incViewID);

            var_bitset viewJoinVars = viewFVars & attOrderMask;
            if ((viewJoinVars & coveredVariableOrder) == viewJoinVars) 
            {
                coveredIncViews.set(incViewID);
                coveredVariables |= viewFVars;
            }
        }
    }

    DINFO("***** Relation: " << viewGroup->_tdNode->_relation->_name << " Order: ");
    for (AttributeOrderNode& orderNode : attributeOrder)
        DINFO(orderNode._attribute << ", ");
    DINFO("\n");
}





size_t AggregateRegister::registerOutgoingView(
    size_t viewID, AttributeOrder& order, const var_bitset& attOrderMask)
{
    View* view = _qc->getView(viewID);
    
    var_bitset varsToCover = view->_fVars;

    var_bitset dependentViewVars;
    for (size_t var=0; var< multifaq::params::NUM_OF_VARIABLES;++var)
    {
        if (view->_fVars[var])
            dependentViewVars |= variableDependency[var];
    }
    dependentViewVars &= ~attOrderMask;

    // We removed this condition: & ~rel->_schemaMask;
    var_bitset nonJoinVars = view->_fVars & ~attOrderMask;
    
    if (nonJoinVars.any())
    {
        //  if this is the case then look for overlapping functions
        for (size_t aggNo = 0; aggNo < view->_aggregates.size(); ++aggNo)
        {
            Aggregate* aggregate = view->_aggregates[aggNo];

            for (size_t i = 0; i < aggregate->_sum.size(); ++i)
            {
                const prod_bitset& product = aggregate->_sum[i]._prod;
                prod_bitset considered;
                        
                for (size_t f = 0; f < multifaq::params::NUM_OF_FUNCTIONS; ++f)
                {
                    if (product.test(f) && !considered[f])
                    {
                        Function* function = _qc->getFunction(f);
                                
                        // check if this function is covered
                        if ((function->_fVars & dependentViewVars).any())
                        {
                            var_bitset dependentFunctionVars;
                            for (size_t att=0;
                                 att < multifaq::params::NUM_OF_ATTRIBUTES;++att)
                            {
                                if (function->_fVars[att])
                                    dependentFunctionVars |= variableDependency[att];
                            }
                            dependentFunctionVars &= ~attOrderMask;

                            // if it is - find functions that overlap
                            bool overlaps;
                            var_bitset overlapVars = function->_fVars;
                            prod_bitset overlapFunc;
                            overlapFunc.set(f);

                            do
                            {
                                overlaps = false;
                                overlapVars |= function->_fVars;

                                for (size_t f2 = 0;
                                     f2 < multifaq::params::NUM_OF_FUNCTIONS; ++f2)
                                {
                                    if (product[f2] && !considered[f2] &&
                                        !overlapFunc[f2])
                                    {
                                        Function* otherFunction = _qc->getFunction(f);
                                        // check if functions overlap
                                        if ((otherFunction->_fVars &
                                             dependentFunctionVars).any())
                                        {
                                            considered.set(f2);
                                            overlapFunc.set(f2);
                                            overlaps = true;
                                            overlapVars |= otherFunction->_fVars;
                                        }
                                    }
                                }
                            } while(overlaps);

                            varsToCover |= overlapVars;
                        }
                    }
                }                        
            }    
        }
    }
        
    // check when the freeVars of the view are covered - this determines
    // when we need to add the declaration of aggregate array and push to
    // view vector !
    for (size_t level = 0; level < order.size(); ++level)
    {
        std::cout << level << " : " << order.size() << " - "<<
            order[level]._coveredVariables << std::endl;
        
        if ((varsToCover & order[level]._coveredVariables) == varsToCover)
        {
            DINFO("View: " << viewID << " Registered at: " << level << "\n");
            DINFO(view->_fVars << "\n");
            
            // Found level where we output tuples to the outgoing view 
            return level;
        }
    }

    ERROR("An Outgoing View was not assigned to a node in the order.\n");
    exit(1);
    return -1;
}







void AggregateRegister::registerAggregates(
    Aggregate* aggregate, const View* view, const size_t group_id,
    const size_t depth, const size_t outputLevel,
    const var_bitset dependentVariables,
    std::vector<RegisteredProduct*>& localAggReg,
    std::vector<RunningSumAggregate*>& runSumReg,
    std::vector<RegisteredProduct*> dependentProducts)
{
    AttributeOrder& order = _qc->getAttributeOrder(group_id);
    AttributeOrderNode& orderNode = order[depth];
    
    const var_bitset& relationBag = _db->getRelation(view->_origin)->_schemaMask;
    const size_t numOfProducts = aggregate->_sum.size();
    
    std::vector<RegisteredProduct*> localComputation(numOfProducts, nullptr);
    std::vector<std::pair<size_t,size_t>> viewReg;

    prod_bitset localFunctions, considered;
    
    var_bitset nonVarOrder = view->_fVars & ~attOrderMask;

    // Go over each product and check if parts of it can be computed at current depth
    for(size_t prodID = 0; prodID < numOfProducts; ++prodID)
    {
        Product& product = aggregate->_sum[prodID];
        prod_bitset &prodMask = product._prod;
        
        // std::cout << "depth: " << depth << " ProdID: " << prodID << " Product: " <<
        //     prodMask << " outputLevel:  "<< outputLevel <<  " Relation: " <<
        //     _db->getRelation(view->_origin)->_name << "\n";

        considered.reset();
        localFunctions.reset();

        for (size_t f = 0; f < multifaq::params::NUM_OF_FUNCTIONS; ++f)
        {
            if (prodMask.test(f) && !considered[f])
            {
                Function* function = _qc->getFunction(f);
                considered.set(f);
                
                // check if this function is covered
                if ((function->_fVars & orderNode._coveredVariables) == function->_fVars)
                {   
                    // get all variables that this function depends on. 
                    var_bitset dependentFunctionVars;
                    for (size_t var=0; var< multifaq::params::NUM_OF_VARIABLES;++var)
                    {
                        if (function->_fVars[var])
                            dependentFunctionVars |= variableDependency[var];
                    }
                    dependentFunctionVars &= ~attOrderMask;

                    // Find all functions that overlap / depend on this function
                    bool overlaps, computeHere;
                    var_bitset overlapVars;
                    prod_bitset overlapFunc = prod_bitset().set(f);

                    do
                    {
                        overlaps = false;
                        computeHere = true;
                        overlapVars |= function->_fVars;

                        for (size_t f2 = 0; f2 < multifaq::params::NUM_OF_FUNCTIONS; ++f2)
                        {
                            if (prodMask[f2] && !considered[f2] && !overlapFunc[f2])
                            {
                                Function* otherFunction = _qc->getFunction(f);
                                const var_bitset& otherFVars = otherFunction->_fVars;
                                // check if functions depend on each other
                                // if ((otherFunction->_fVars & nonVarOrder).any()) 
                                if ((otherFVars & dependentFunctionVars).any())
                                {
                                    overlaps = true;
                                    considered.set(f2);
                                    overlapFunc.set(f2);
                                    overlapVars |= otherFunction->_fVars;

                                    // check if otherFunction is covered
                                    if ((otherFVars & orderNode._coveredVariables) !=
                                        otherFVars)
                                    {
                                        // if it is not covered then compute entire
                                        // group at later stage
                                        computeHere = false;
                                    }
                                }
                            }
                        }
                    } while(overlaps && !computeHere);
                    
                    if (computeHere)
                    {
                        // check if the variables are dependent on vars in head
                        // of outcoming view
                        if (((overlapVars & ~attOrderMask) & dependentVariables).any())
                        {
                            if (outputLevel != depth)
                                ERROR("ERROR: This view is not registered at this node: "
                                      <<" outputLevel != depth\n");
                            
                            dependentProducts[prodID]->product |= overlapFunc;

                            // TODO: in theory we could have view aggregates
                            // which contribute to the dependent product, but
                            // are not directly dependent on the outgoing view

                            // Then this view aggregate should also be added to
                            // the dependentViewAggs, but could it be in the non
                            // dependent views are the same time?

                            // --- I don't think so, because if one of them
                            // --- depends on the rest, all of them should 
                        }
                        else // add all functions that overlap together at this level
                            localFunctions |= overlapFunc;

                        // remove overlapping functions from product
                        prodMask &= ~overlapFunc;
                    }
                }
            }
        }

        /*
         * Go over each incoming views and check if their incoming products can
         * be computed at this level. 
         */
        viewReg.clear();
        
        for (std::pair<size_t,size_t>& incoming : product._incoming)
        {
            size_t& incViewID = incoming.first;

            if (orderNode._coveredIncomingViews[incViewID])
            {
                View* incView = _qc->getView(incViewID);
                
                if (((incView->_fVars & ~attOrderMask) & incView->_fVars).none())
                {
                    /* This incoming aggregate does not have any dependent
                     * variables and is covered by the attribute order */
                    viewReg.push_back(incoming);
                    
                    /* This incoming view will not be considered at next depth */
                    incoming.first = _qc->numberOfViews();
                }
                else if (((incView->_fVars & ~attOrderMask) & dependentVariables).none())
                {
                    /* In this case, the incoming view has non-join attributes,
                     * but does not share them with the outgoing view. So the
                     * attribute must be used in a function. */

                    /* We verify that there is such a function registered at
                     * this depth at this level. */ 
                    bool viewCoversFunctions = false;
                    for (size_t f = 0; f < multifaq::params::NUM_OF_FUNCTIONS; ++f)
                    {
                        if (localFunctions.test(f))
                        {
                            Function* func = _qc->getFunction(f);

                            if ((func->_fVars & ~attOrderMask & incView->_fVars).any())
                            {
                                viewCoversFunctions = true;
                                break;
                            }
                        }
                    }

                    /* As a sanity check, we check that this views does not
                     * cover any of the remaining functions in this product. */ 
                    bool viewCoversRemainingFunction = false;
                    for (size_t f = 0; f < multifaq::params::NUM_OF_FUNCTIONS; ++f)
                    {
                        if (prodMask.test(f))
                        {
                            Function* func = _qc->getFunction(f);

                            if ((func->_fVars & ~attOrderMask & incView->_fVars).any())
                            {
                                viewCoversRemainingFunction = true;
                                break;
                            }
                        }
                    }

                    if (viewCoversFunctions)
                    {
                        if (viewCoversRemainingFunction)
                        {
                            ERROR("ERROR: this view should not cover both the current "<<
                                  "and the remaining functions in a single product.\n");
                            exit(1);
                        }
                        
                        viewReg.push_back(incoming);
                        
                        /* This incoming view will not be considered at next depth */
                        incoming.first = _qc->numberOfViews();
                    }
                    else
                    {
                        if (!viewCoversRemainingFunction)
                        {
                            ERROR("ERROR: We would have expected to have a function "<< 
                                  "which requires additional variable in the view.\n");
                            exit(1);
                        }
                    }
                }
                else
                {
                    /* This incoming view has variables that depend of the
                     * variables on the outgoing view, so we add it to the
                     * dependent computation. */
                    dependentProducts[prodID]->viewAggregateProduct.push_back(incoming);
                    
                    /* This incoming view will not be considered at next depth */
                    incoming.first = _qc->numberOfViews();
                }
            }
        }

        bool multiplyByCount = depth+1 == order.size() &&
            (dependentVariables & relationBag & nonVarOrder).none();

        // Check if this product has already been computed, if not we add it
        if (localFunctions.any() || !viewReg.empty() || multiplyByCount)
        {
            // Register this product to the Attribute Order Node 
            localComputation[prodID] = orderNode.registerProduct(
                localFunctions, viewReg, localAggReg[prodID], multiplyByCount);

            // Update the local Aggregate Pointer
            localAggReg[prodID] = localComputation[prodID];

            if (group_id == 4)
            {
                std::cout << "WE REGISTERED A LOCAL PRODUCT " << depth << "  "
                          << prodID << " Func: " << localFunctions.count() <<
                    " ViewReg: " << viewReg.size() <<  " ID: " << localComputation[prodID] << std::endl;
            }
        }
        else if (depth == outputLevel)
        {
            // if we didn't set a new computation - check if this level is the view Level:
            // Add the previous aggregate to local computation
            localComputation[prodID] = localAggReg[prodID];
        }
    }

    /* Recurse to next depth ! */
    if (depth+1 != order.size())
        registerAggregates(
            aggregate,view,group_id,depth+1,outputLevel,dependentVariables,
            localAggReg,runSumReg,dependentProducts);

    if (depth > outputLevel)
    {
        // Now adding aggregates coming from below to running sum
        for (size_t prodID = 0; prodID < numOfProducts; ++prodID)
        {
            if (localComputation[prodID] != nullptr || runSumReg[prodID] != nullptr)
            {
                RunningSumAggregate* postTuple =
                    new RunningSumAggregate(localComputation[prodID], runSumReg[prodID]);

                // auto postit = orderNode._registeredRunningSumMap.find(*postTuple);
                // if (postit !=  orderNode._registeredRunningSumMap.end())
                // {
                //     runSumReg[prodID] = orderNode._registeredRunningSum[postit->second];
                //     delete postTuple;
                // }
                // else
                // {
                    size_t postProdID = orderNode._registeredRunningSum.size();

                    orderNode._registeredRunningSumMap[*postTuple] = postProdID;
                    orderNode._registeredRunningSum.push_back(postTuple);

                    runSumReg[prodID] = orderNode._registeredRunningSum[postProdID];
                // }

                if (group_id == 2)
                    std::cout << " ************************************ " <<
                        localComputation[prodID] << "   " <<
                        runSumReg[prodID] << std::endl;
                if (group_id == 2)
                    std::cout << " ************************************ " <<
                        postTuple->local << "   " <<
                        postTuple->post << "   " << postTuple << std::endl;
            }

            if (localAggReg[prodID] == nullptr && depth+1 != order.size())
            {
                ERROR("ERROR: No runningSumAggregate found - ");
                ERROR(depth << " : " << order.size()<< "  " << prodID << "\n");
                exit(1);
            }
        }
    }

    /* Adding product and running sum pointers to the OutputAggregate. */
    if (depth == outputLevel)
    {
        // localAggReg = localComputation;
        
        // Now adding aggregates coming from below to running sum
        for (size_t prodID = 0; prodID < numOfProducts; ++prodID)
        {
            /*
             if (group_id == 2)
                std::cout << " ************************************ " <<
                    runSumReg[prodID] << "  " << runSumReg[prodID]->local
                          << "  " << runSumReg[prodID]->post << std::endl;
             */
            RunningSumAggregate* runSum = new RunningSumAggregate(
                localComputation[prodID], runSumReg[prodID]);

            runSumReg[prodID] = runSum;
             
            // if (runSumReg[prodID] == nullptr)
            // {
            //    runSumReg[prodID] = new RunningSumAggregate(
            //            localComputation[prodID], nullptr);
            // }
            // else
            // {
            //    runSumReg[prodID]->post = runSumReg[prodID];
            //    runSumReg[prodID]->local = localComputation[prodID];
            // }
        }            
            
        //     if (localComputation[prodID] != nullptr || localAggReg[prodID] != nullptr)
        //     {
        //        /* Register postTuple to registeredOutgoingRunningSum*/
        //        RunningSumAggregate* postTuple = nullptr;

        //        for (RunningSumAggregate* runSumAgg :
        //                 orderNode._registeredOutgoingRunningSum) 
        //        {
        //            if (runSumAgg->local == localComputation[prodID] &&
        //                runSumAgg->post  == runSumReg[prodID])
        //            {
        //                postTuple = runSumAgg;
        //                break;
        //            }
        //        }

        //        if (postTuple == nullptr)
        //        {
        //            postTuple = new RunningSumAggregate(
        //                localComputation[prodID], runSumReg[prodID]);

        //            orderNode._registeredOutgoingRunningSum.push_back(postTuple);
        //        }
        //        runSumReg[prodID] = postTuple;
        //     }
        //     else
        //     {
        //       ERROR("We are adding RunningSumAgg for an output, but localComp as "<<
        //               "well as runSumAgg are empty!?\nDepth:" << depth <<
        //               " OrderSize: " << order.size()<< "  " << prodID << "\n");
        //     }

        //     if (localAggReg[prodID] == nullptr && depth+1 != order.size())
        //     {
        //         ERROR("ERROR: No (output) runningSumAggregate found - ");
        //         ERROR(depth << " : " << order.size()<< "  " << prodID << "\n");
        //         exit(1);
        //     }
        // } 
    }
}








void AggregateRegister::registerLocalAggregatesWithLoops(
    Relation* relation, AttributeOrderNode &orderNode)
{
    var_bitset relationBag = relation->_schemaMask;
 
    std::map<dyn_bitset, prod_bitset> loopFunctionMask;
    dyn_bitset loopFactors(_qc->numberOfViews()+1);
    
    for (RegisteredProduct* p : orderNode._registeredProducts)
    {
        prod_bitset& productFuns = p->product;
        ViewAggregateProduct& productViewProd = p->viewAggregateProduct;        

        loopFunctionMask.clear();
        // loopFunctionMask[loopFactors] = prod_bitset();
        
        bool loopsOverRelation = false;

        for (size_t fun = 0; fun < multifaq::params::NUM_OF_FUNCTIONS; ++fun)
        {
            if (productFuns[fun])
            {
                var_bitset nonJoinFunVars =
                    _qc->getFunction(fun)->_fVars & ~attOrderMask;
                
                loopFactors.reset();

                for (size_t a = 0; a < multifaq::params::NUM_OF_ATTRIBUTES; ++a)
                {
                    if (nonJoinFunVars[a])
                    {
                        if (relationBag[a])
                        {
                            loopFactors.set(_qc->numberOfViews());
                            loopsOverRelation = true;
                        }
                        else
                        {
                            bool foundLoop = false;
                            for (const ViewAggregateIndex& incViewID : productViewProd)
                            {
                                if (_qc->getView(incViewID.first)->_fVars[a])
                                {
                                    loopFactors.set(incViewID.first);
                                    foundLoop = true;
                                }
                            }
                            
                            if (!foundLoop)
                                ERROR("THERE IS AN ERROR IN THE LOOP GEN!\n");
                        }
                    }
                }
                
                loopFunctionMask[loopFactors].set(fun);
            }
        }

        std::cout << "--BEFORE: registerProductToLoop: "<<orderNode._attribute<<"\n";

        FinalLoopAggregate* prevProduct =
            (p->previous != nullptr ? p->previous->correspondingLoop.second : nullptr);
        
        // Then register loops as nodes attached to the attribute order
        FinalLoopAggregate* correspondingLoopAgg =
            registerProductToLoop(
                orderNode._loopProduct, p->viewAggregateProduct, prevProduct,
                loopFunctionMask, dyn_bitset(_qc->numberOfViews()+1),
                (p->multiplyByCount && !loopsOverRelation));

        p->correspondingLoop = {orderNode._loopProduct, correspondingLoopAgg};
    }

    // TODO: Figure out how we can reuse all this for the dependent computation!!
}






/* We return a list of pointer to the loops for this view */
void AggregateRegister::createLoopsForOutgoingView(
    const View* view, AttributeOrderNode& orderNode, const Relation* relation,
    std::vector<LoopRegister*>& outgoingLoopPointers)
{
    var_bitset relationBag = relation->_schemaMask;
    
    dyn_bitset outViewLoopFactor(_qc->numberOfViews()+1);

    var_bitset nonVarOrder = view->_fVars & ~attOrderMask;
        
    for (size_t var = 0; var < multifaq::params::NUM_OF_ATTRIBUTES; ++var)
    {
        if (nonVarOrder[var])
        {
            // if var is in relation - use relation 
            if (relationBag[var])
            {                                   
                outViewLoopFactor.set(_qc->numberOfViews());
            }
            else // else find corresponding view 
            {
                for (const size_t& incViewID : view->_incomingViews)
                {
                    if (_qc->getView(incViewID)->_fVars[var])
                        outViewLoopFactor.set(incViewID);
                }
            }
        }
    }
    
    if (orderNode._outgoingLoopRegister == nullptr)
    {
        orderNode._outgoingLoopRegister =
            new LoopRegister(_qc->numberOfViews()+1);
    }

    outgoingLoopPointers.push_back(orderNode._outgoingLoopRegister);

    LoopRegister* outProd = orderNode._outgoingLoopRegister;
    LoopRegister* nextLoop = nullptr;
    
    if (outViewLoopFactor[_qc->numberOfViews()])
    {
        for (LoopRegister* innerLoop : outProd->innerLoops)
        {
            if (innerLoop->loopID == _qc->numberOfViews())
            {
                nextLoop = innerLoop;
                break;
            }
        }

        if (nextLoop == nullptr)
        {
            nextLoop = new LoopRegister(_qc->numberOfViews());
            outProd->innerLoops.push_back(nextLoop);
        }

        outProd = nextLoop;
        outViewLoopFactor.reset(_qc->numberOfViews());

        outgoingLoopPointers.push_back(outProd);
    }

    if (outViewLoopFactor.any())
    {
        size_t nextLoopID =  0;
        dyn_bitset existingLoops(_qc->numberOfViews()+1);

        do
        {
            existingLoops.reset();
            
            for (LoopRegister* innerLoop : outProd->innerLoops)
                existingLoops.set(innerLoop->loopID);

            dyn_bitset overlapLoops = outViewLoopFactor & existingLoops;
         
            if (overlapLoops.any())
            {
                nextLoopID = overlapLoops.find_first();

                for (LoopRegister* innerLoop : outProd->innerLoops)
                {
                    if (innerLoop->loopID == nextLoopID)
                    {
                        nextLoop = innerLoop;
                        break;
                    }
                }
            }
            else
            {
                nextLoopID = outViewLoopFactor.find_first();

                nextLoop = new LoopRegister(nextLoopID);
                outProd->innerLoops.push_back(nextLoop);
            }
            outProd = nextLoop;
            outViewLoopFactor.reset(nextLoopID);
            
            outgoingLoopPointers.push_back(outProd);
        
        } while (outViewLoopFactor.any());
    }

    // TODO: Print loops for this view for santiy checks!
}







void AggregateRegister::registerOutputAggregatesToLoops(
    OutgoingView &outgoingView,
    const std::vector<RegisteredProduct*>& dependentProducts,
    const std::vector<RunningSumAggregate*>& runSumReg,
    std::vector<LoopRegister*>& outgoingLoopPointers,
    const Relation* relation)
{
    for (size_t prodID = 0; prodID < dependentProducts.size(); ++prodID)
    {
        /* Get product that needs to be broken up over loops */
        const RegisteredProduct* depAgg = dependentProducts[prodID];
        prod_bitset product = depAgg->product;
        
        var_bitset coveredVariables = attOrderMask;

        LoopAggregate* previousProduct = nullptr;
        
        /* The first loop is just the empty loop, so we can skip it */
        for (size_t loop = 0; loop < outgoingLoopPointers.size(); ++loop)
        {
            LoopRegister* loopProd = outgoingLoopPointers[loop];

            size_t loopID = loopProd->loopID;
            if (loopID == _qc->numberOfViews())
                coveredVariables |= relation->_schemaMask;
            else if (loopID < _qc->numberOfViews())
                coveredVariables |= _qc->getView(loopID)->_fVars;
            else
                assert(loopID == _qc->numberOfViews()+1);        

            prod_bitset localFunctions;
            
            for (size_t fun = 0; fun < multifaq::params::NUM_OF_FUNCTIONS; ++fun)
            {
                if (product[fun])
                {
                    const var_bitset& funVars = _qc->getFunction(fun)->_fVars;
                    
                    if ((funVars & coveredVariables) == funVars)
                        localFunctions.set(fun);
                }
            }
            
            bool regPostProd = false;
    
            int functionProduct = -1;
            int viewProduct = -1;

            if (localFunctions.any())
            {
                // Register localFunctions at this layer
                functionProduct =
                    loopProd->registerLocalProduct(localFunctions);

                // Remove localFunctions from product
                product &= ~localFunctions;
                regPostProd = true;
            }
            
            if (loopID < _qc->numberOfViews())
            {
                // Find the loopAggregate that corresponds to this loop
                ViewAggregateProduct viewProd;

                for (const ViewAggregateIndex& viewAgg : depAgg->viewAggregateProduct)
                {
                    if (viewAgg.first == loopProd->loopID)
                    {
                        viewProd.push_back(viewAgg);
                        break;
                    }
                }

                if (viewProd.empty()) 
                {
                    ERROR("We did not find the correct, .\n");
                    exit(1);
                }
        
                viewProduct = loopProd->registerViewProduct(viewProd);
                regPostProd = true;
            }

            if (localFunctions.any() || viewProduct != -1)
            {   
                // Register the postProduct to the loop
                LoopAggregate* newLoopAgg = nullptr;
                
                // Check if this loopAggregateuct already exists 
                for (LoopAggregate* loopAgg : loopProd->loopAggregateList)
                {
                    if (loopAgg->functionProduct == functionProduct &&
                        loopAgg->viewProduct == viewProduct &&
                        loopAgg->prevProduct == previousProduct)
                    {
                        newLoopAgg = loopAgg;
                        break;
                    }   
                }

                // Otherwise create it 
                if (newLoopAgg == nullptr)
                {
                    newLoopAgg = new LoopAggregate();
                    newLoopAgg->functionProduct = functionProduct;
                    newLoopAgg->viewProduct = viewProduct;
                    newLoopAgg->prevProduct = previousProduct;
                    
                    loopProd->loopAggregateList.push_back(newLoopAgg);
                }
                
                // AND THEN KEEP TRACK OF PRODUCT
                previousProduct = newLoopAgg;
            }
        }

        FinalLoopAggregate* innerProd = nullptr;
        
        if (product.any())
        {
            // TODO: TODO: TODO: TODO: 
            // THIS PRODUCT CONTAINS ADDITIONAL FUNCTIONS
            // THEY REQUIRE EXTRA LOOPS
            // Use Existing methods to register these loops

            // create extra loop and then set inner Product 
        }

        // FinalLoopAggregate --> register this
        // It will map to previousProduct, innerProduct and prevRunSum?
        FinalLoopAggregate* outputAggregate = new FinalLoopAggregate();
        outputAggregate->loopAggregate = previousProduct;
        outputAggregate->innerProduct = innerProd;
        outputAggregate->previousRunSum = runSumReg[prodID];
        outputAggregate->aggregateIndex = outgoingView.aggregates.size();

        // std::cout << "    ----- ******* ---- ****** ------" <<
        //     outputAggregate->previousProduct << std::endl;
        // std::cout << runSumReg[prodID]->local << std::endl;
        
        outgoingView.aggregates.push_back(outputAggregate);
    }
}







FinalLoopAggregate* AggregateRegister::registerProductToLoop(
    LoopRegister* loopRegister, const ViewAggregateProduct& viewAggregateProduct,
    FinalLoopAggregate* previousAggregate,
    std::map<dyn_bitset,prod_bitset>& loopFunctionMask, dyn_bitset consideredLoops,
    bool multiplyByCount)
{
    int functionProduct = -1;
    int viewProduct = -1;
    
    /******************** Register Function Product ****************************/
    
    auto mask_it = loopFunctionMask.find(consideredLoops);
    if (mask_it != loopFunctionMask.end())
    {
        prod_bitset& funProduct = loopFunctionMask[consideredLoops];

        /* Register local Function Product to the LoopRegister */
        if (funProduct.none())
            ERROR("This product doesn't contain any functions?!\n");
        
        functionProduct = loopRegister->registerLocalProduct(funProduct);
        loopFunctionMask.erase(mask_it);
    }
    
    /********************* Register View Product *****************************/
    
    if (loopRegister->loopID == _qc->numberOfViews()+1)
    {
        // This corresponds to no loops
        // Find all viewAggregates that do not require a loop
        ViewAggregateProduct viewProd;

        for (const ViewAggregateIndex& viewAgg : viewAggregateProduct)
        {
            View* view = _qc->getView(viewAgg.first);
            
            if ((view->_fVars & ~attOrderMask).none())
            {
                viewProd.push_back(viewAgg);
            }

            if (viewAgg.first == 2) 
                std::cout << viewAgg.first << " --- " << viewAgg.second << "\n";
        }

        if (!viewProd.empty())
            viewProduct = loopRegister->registerViewProduct(viewProd);
    }
    else if (loopRegister->loopID < _qc->numberOfViews())
    {
        // Find the loopAggregate that corresponds to this loop
        ViewAggregateProduct viewProd;

        for (const ViewAggregateIndex& viewAgg : viewAggregateProduct)
        {            
            if (viewAgg.first == loopRegister->loopID)
            {
                viewProd.push_back(viewAgg);
                break;
            }
        }

        if (viewProd.empty()) 
        {
            ERROR("There is an issue in the registration of view aggregates.\n");
            exit(1);
        }
        
        viewProduct = loopRegister->registerViewProduct(viewProd);
    }

    /**************************************************/

    LoopAggregate* loopAggregate = nullptr;

    if (functionProduct != -1 || viewProduct != -1 || multiplyByCount )
        loopAggregate = loopRegister->registerLoopAggregate(
            functionProduct, viewProduct, multiplyByCount);
    
    /**************************************************/

    /* Find inner loops for this product */
    dyn_bitset nextLoops(_qc->numberOfViews()+1);

    for (const auto& loop : loopFunctionMask)
    {
        const dyn_bitset& loopFactors = loop.first;

        if ((consideredLoops & loopFactors) == consideredLoops &&
            loopFactors != consideredLoops)
        {
            dyn_bitset remainingLoops = loopFactors & ~consideredLoops;
            if (remainingLoops.any())
            {       
                if (remainingLoops[_qc->numberOfViews()])
                {
                    nextLoops.set(_qc->numberOfViews());
                }
                else
                {
                    size_t next = remainingLoops.find_first();
                    if (next != boost::dynamic_bitset<>::npos)
                        nextLoops.set(next);
                }
            }
            else
            {
                ERROR("WHY IS THERE NO REMAINING LOOP?!\n");
                exit(1);
            }
        }
    }

    /**************************************************/

    FinalLoopAggregate* innerLoopAggregate = nullptr;
    
    if (nextLoops.any())
    {
        size_t nextLoopID = nextLoops.find_first();

        while (nextLoopID != dyn_bitset::npos)
        {
            LoopRegister* nextLoop = nullptr;
            
            for (LoopRegister* innerLoop : loopRegister->innerLoops)
            {
                if (innerLoop->loopID == nextLoopID)
                {
                    nextLoop = innerLoop;
                    break;
                }
            }
        
            if (nextLoop == nullptr)
            {
                nextLoop = new LoopRegister(nextLoopID);
                loopRegister->innerLoops.push_back(nextLoop);
            }
                        
            dyn_bitset nextLoopFactors = consideredLoops;
            nextLoopFactors.set(nextLoopID);
            
            FinalLoopAggregate* prevRunSum = registerProductToLoop(
                nextLoop,viewAggregateProduct,nullptr,loopFunctionMask,
                nextLoopFactors,false);

            nextLoopID = nextLoops.find_next(nextLoopID);

            if (innerLoopAggregate != nullptr || nextLoopID != dyn_bitset::npos)
                 innerLoopAggregate = nextLoop->registerFinalLoopAggregate(
                    nullptr, innerLoopAggregate, prevRunSum);
            else
                innerLoopAggregate = prevRunSum;
        }
    }

    /***************** Register Running Sum **********************/

    return loopRegister->registerLoopRunningSums(
        loopAggregate, innerLoopAggregate, previousAggregate);
}
