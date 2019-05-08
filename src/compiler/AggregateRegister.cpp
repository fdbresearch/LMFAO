#include <AggregateRegister.h>
#include <Logging.hpp>

AggregateRegister::AggregateRegister(
    std::shared_ptr<Database> db, std::shared_ptr<TreeDecomposition> td,
    std::shared_ptr<QueryCompiler> qc) :_db(db), _td(td), _qc(qc)
{
}

AggregateRegister::~AggregateRegister()
{
}

void AggregateRegister::compile()
{    
    variableDependency.resize(multifaq::params::NUM_OF_VARIABLES);
    
    // Find all the dependencies amongst the variables
    for (size_t rel = 0; rel < _db->numberOfRelations(); ++rel)
    {
        var_bitset relSchema = _db->getRelation(rel)->_schemaMask;

        for (size_t var = 0; var < multifaq::params::NUM_OF_VARIABLES; ++var)
        {
            if (relSchema[var])
                variableDependency[var] |= relSchema;
        }
    }

    // Call createAttributeOrder();
    attributeOrders.resize(_qc->getNumberOfViewGroups());
    
    for (size_t group = 0; group < _qc->getNumberOfViewGroups(); ++group)
    {
        attributeOrders[group].push_back({multifaq::params::NUM_OF_ATTRIBUTES});
        createAttributeOrder(attributeOrders[group], &_qc->getViewGroup(group));

        ViewGroup& viewGroup = _qc->getViewGroup(group);
        Relation* rel = viewGroup._tdNode->_relation;
        
        /* Compute Mask of the variables in the order */
        var_bitset attOrderMask;
        attOrderMask.reset();
        for (size_t node=1; node<attributeOrders[group].size(); ++node)
            attOrderMask.set(attributeOrders[group][node]._attribute);
    
        for (const size_t& viewID : viewGroup._views)
        {
            View* view = _qc->getView(viewID);

            var_bitset nonVarOrder = view->_fVars & ~attOrderMask;    

            // Compute dependent variables on the variables in the head of view:
            dependentVariables.reset();
            
            for (size_t var = 0; var < multifaq::params::NUM_OF_VARIABLES; ++var)
            {
                if (nonVarOrder[var])
                {
                    // find bag that contains this variable
                    if (rel->_schemaMask[var])
                        dependentVariables |= rel->_schemaMask & ~attOrderMask;
                    else
                    {
                        // check incoming views
                        for (const size_t& incViewID :view->_incomingViews)
                        {
                            View* incView = _qc->getView(incViewID);
                            if (incView->_fVars[var])
                                dependentVariables |= incView->_fVars & ~attOrderMask;
                        }
                    }
                }
            }

            // TODO: COMPUTE THE OUTPUT LEVEL --> This used to be called viewLevelRegister
            size_t outputLevel = 0;
            
            for (size_t aggNo = 0; aggNo < view->_aggregates.size(); ++aggNo)
            {
                // We use a copy of the actual aggregate
                Aggregate aggregate = *(view->_aggregates[aggNo]);

                std::vector<RegisteredProduct*> localAggReg(
                    aggregate._sum.size(), nullptr);
                std::vector<RunningSumAggregate*> runSumReg(
                    aggregate._sum.size(), nullptr);
            
                dependentComputation.clear();
                dependentComputation.resize(aggregate._sum.size());

                registerAggregates(&aggregate,group,viewID,aggNo,0,
                                   outputLevel,localAggReg,runSumReg);
            }
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
        joinAttributes |= destRelation->_schemaMask & baseRelation->_schemaMask;

        // if there are not inc views we use the fVars as free variable
        if (view->_origin == view->_destination)
            joinAttributes |= (view->_fVars & destRelation->_schemaMask);
        else
        {
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
    }

    const std::vector<size_t>& joinKeyOrder = _td->getJoinKeyOrder(baseRelation->_id);

    /* Add the join variables to the variable order */
    for (const size_t& att : joinKeyOrder)
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
        ERROR("The joinKeyOrder does not contain all join attributes.");
        ERROR("The following attributes are not in the order: \n");
        for (size_t att = 0; att < multifaq::params::NUM_OF_ATTRIBUTES; ++att)
        {
            if (joinAttributes[att])
                ERROR(_db->getAttribute(att) << "\n");
        }
        exit(1);
    }

    /* We are setting the join views for each attribute in the order */
    for (AttributeOrderNode& node : attributeOrder)
    {
        /* If the relation contains the varible add it to the order */
        if (baseRelation->_schemaMask[node._attribute])
            node._registerdRelation = viewGroup->_tdNode->_relation;

        for (const size_t& incViewID : viewGroup->_incomingViews)
        {
            const var_bitset& viewFVars = _qc->getView(incViewID)->_fVars;

            if (viewFVars[node._attribute])
                node._joinViews.push_back(incViewID);
        }
    }

    // TODO: TODO: add a method to print out the attribute order --> this is for
    // sanity checks.
}



void AggregateRegister::registerViews(size_t group_id)
{
    const ViewGroup& group = _qc->getViewGroup(group_id);
    AttributeOrder& order = _qc->getAttributeOrder(group_id);

    TDNode* node = group._tdNode;
    Relation* rel = node->_relation;

    // TODO: need to figure out what to do with _incomingViewRegister
    _incomingViewRegister.reset();
    _incomingViewRegister.resize(order.size() * (_qc->numberOfViews()+1));

    for (const size_t& viewID : group._views)
    {
        View* view = _qc->getView(viewID);
        bool viewRegistered = false;
        
        /* If this view has no free variables it should be outputted after the join */
        if (view->_fVars.none())
        {
            // TODO: TODO: TODO: FIX THIS FIX THIS FIX THIS 
            // order._viewsWithoutGroupBy.push_back(viewID);
            viewRegistered = true;
        }
        
        var_bitset varsToCover = view->_fVars;

        var_bitset dependentViewVars;
        for (size_t var=0; var< multifaq::params::NUM_OF_VARIABLES;++var)
        {
            if (view->_fVars[var])
                dependentViewVars |= variableDependency[var];
        }
        dependentViewVars &= ~attOrderMask;

        var_bitset nonJoinVars = view->_fVars & ~attOrderMask & ~rel->_schemaMask;
        
        if (nonJoinVars.any())
        {
            //  if this is the case then look for overlapping functions
            for (size_t aggNo = 0; aggNo < view->_aggregates.size(); ++aggNo)
            {
                Aggregate* aggregate = view->_aggregates[aggNo];

                for (size_t i = 0; i < aggregate->_agg.size(); ++i)
                {
                    const prod_bitset& product = aggregate->_agg[i];
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
                                        if (product[f2] && !considered[f2] && !overlapFunc[f2])
                                        {
                                            Function* otherFunction = _qc->getFunction(f);
                                            // check if functions overlap
                                            if ((otherFunction->_fVars & dependentFunctionVars).any())
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
        size_t depth = 0;
        var_bitset coveredVariableOrder;

        for (AttributeOrderNode& orderNode : order)
        {
            const size_t var = orderNode._attribute;
            var_bitset& coveredVariables = orderNode._coveredVariables;
            
            coveredVariableOrder.set(var);
            coveredVariables.set(var);

            // TOOD: This should really only be the case at the last layer?!
            var_bitset intersection = rel->_schemaMask & attOrderMask;
            if ((intersection & coveredVariableOrder) == intersection) 
                orderNode._coveredVariables |= rel->_schemaMask;


            // TODO: why do we check incoming views here ?!
            // we have all incoming views for one viewGroup!!
            
            for (const size_t& incViewID : view->_incomingViews) 
            {
                // Check if view is covered !
                View* incView = _qc->getView(incViewID);
                
                var_bitset viewJoinVars = incView->_fVars & attOrderMask;
                if ((viewJoinVars & coveredVariableOrder) == viewJoinVars) 
                {
                    _incomingViewRegister.set(incViewID + (depth * (_qc->numberOfViews()+1)));
                    coveredVariables |= incView->_fVars;
                }
            }

            if (!viewRegistered && (varsToCover & coveredVariables) == varsToCover)
            {
                // this is the depth where we add the declaration of aggregate
                // array and push to the vector !
                orderNode._registeredViews.push_back(viewID);
                viewRegistered = true;
            }
            ++depth;
        }
    }
}



void AggregateRegister::registerAggregates(
    Aggregate* aggregate, const size_t group_id,
    const size_t view_id, const size_t agg_id, const size_t depth,
    const size_t outputLevel,
    std::vector<RegisteredProduct*>& localAggReg,
    std::vector<RunningSumAggregate*>& runSumReg)
{
    AttributeOrder& order = _qc->getAttributeOrder(group_id);
    AttributeOrderNode& orderNode = order[depth];
    
    const ViewGroup& group = _qc->getViewGroup(group_id);
    
    TDNode* node = group._tdNode;
    
    Relation* rel = node->_relation;
    
    View* view = _qc->getView(view_id);
    
    const var_bitset& relationBag = node->_relation->_schemaMask;
    
    const size_t numLocalProducts = aggregate->_sum.size();
             
    std::vector<RegisteredProduct*> localComputation(numLocalProducts);

    std::vector<std::pair<size_t,size_t>> viewReg;

    prod_bitset localFunctions, considered;

    
    var_bitset nonVarOrder = view->_fVars & ~attOrderMask;    

    
    // Go over each product and check if parts of it can be computed at current depth
    for(size_t prodID = 0; prodID < numLocalProducts; ++prodID)
    {
        Product& product = aggregate->_sum[prodID];
        prod_bitset &prodMask = product._prod; //aggregate->_agg[prodID];

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
                                    considered.set(f2);
                                    overlapFunc.set(f2);
                                    overlaps = true;
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
                            
                            dependentComputation[prodID].product |= overlapFunc;
                        }
                        else // add all functions that overlap together at this level
                            localFunctions |= overlapFunc;

                        // remove overlapping functions from product
                        prodMask &= ~overlapFunc;
                    }
                }
            }
        }
        
        // go over each incoming views and check if their incoming products can
        // be computed at this level. 
        viewReg.clear();

        for (std::pair<size_t,size_t>& incoming : product._incoming)
        {
            size_t& incViewID =  incoming.first;

            if (_incomingViewRegister[incViewID + (depth * (_qc->numberOfViews()+1))])
            {
                View* incView = _qc->getView(incViewID);

                // TODO: I think the below statement is fine but should we use
                // dependent variables instead?! - here we just check if any of
                // the vars from incView are present in the vars for view
                if (((incView->_fVars & ~attOrderMask) & view->_fVars).none())
                    viewReg.push_back(incoming);
                else
                    dependentComputation[prodID].viewAggregateProduct.push_back(incoming);

                // This incoming view will no longer be considered 
                incoming.first = _qc->numberOfViews();
            }
        }
        
        // Check if this product has already been computed, if not we add it
        // to the list
        if (localFunctions.any() || !viewReg.empty() ||
            (depth+1 == order.size() &&     // TODO: make sure order.size is correct
             (dependentVariables & relationBag & nonVarOrder).none()))
        {
            RegisteredProduct prodAgg;
            prodAgg.product = localFunctions;
            prodAgg.viewAggregateProduct = viewReg;
            
            //  && viewLevelRegister[view_id] != order.size()) // assuming
            //  new attribute order
            if (depth <= outputLevel)
                prodAgg.previous = localAggReg[prodID];

            // else -- not needed assuming pointers 
            //     prodAgg.previous = {varOrder.size(), 0};

            if (depth+1 == order.size() &&
                (dependentVariables & relationBag & nonVarOrder).none())
                prodAgg.multiplyByCount = true;
            else
                prodAgg.multiplyByCount = false;
            
            auto prod_it = orderNode._registeredProductMap.find(prodAgg);
            if (prod_it != orderNode._registeredProductMap.end())
            {
                localComputation[prodID] = &orderNode._registeredProducts[prod_it->second];
            }
            else
            {
                // If so, add it to the aggregate register
                size_t newProdID = orderNode._registeredProducts.size();
                orderNode._registeredProductMap[prodAgg] = newProdID;
                orderNode._registeredProducts.push_back(prodAgg);

                localComputation[prodID] = &orderNode._registeredProducts[newProdID];
            }
            
            localAggReg[prodID] = localComputation[prodID];

            /******** PRINT OUT **********/
            // dyn_bitset contribViews(_qc->numberOfViews()+1);
            // for (auto& p : viewReg)
            //     contribViews.set(p.first);
            // std::cout << "group_id " << group_id << " depth: " << depth << std::endl;
            // std::cout << genProductString(*node, contribViews, localFunctions)
            //           << std::endl;
            /******** PRINT OUT **********/
        }
        // if we didn't set a new computation - check if this level is the view Level:
        else if (depth == outputLevel)
        {
            // Add the previous aggregate to local computation
            localComputation[prodID] = localAggReg[prodID];
            // TODO: (CONSIDER) think about keeping this separate as above
        }
        else if (depth+1 == order.size())
        {
            ERROR("WE HAVE A PRODUCT THAT DOES NOT OCCUR AT THE LOWEST LEVEL!?");
            exit(1);
        }
    }
    
    // recurse to next depth ! 
    if (depth != order.size()-1)
    {
        registerAggregates(aggregate,group_id,view_id,agg_id,depth+1,outputLevel,localAggReg,runSumReg);
    }
    // else
    // {
    //     // resetting aggRegisters !        
    //     for (size_t i = 0; i < numLocalProducts; ++i)
    //         localAggReg[i] = nullptr;
    // }

    // || viewLevelRegister[view_id] == varOrder.size()) TODO: THIS SHOULD BE FINE NOW
    if (depth > outputLevel)
    {
        // now adding aggregates coming from below to running sum
        for (size_t prodID = 0; prodID < numLocalProducts; ++prodID)
        {
            if (localComputation[prodID] != nullptr || localAggReg[prodID] != nullptr)
            {
                RunningSumAggregate postTuple;                
                postTuple.local = localComputation[prodID];
                postTuple.post = runSumReg[prodID];

                auto postit = orderNode._registeredRunningSumMap.find(postTuple);
                if (postit !=  orderNode._registeredRunningSumMap.end())
                {
                    runSumReg[prodID] = &orderNode._registeredRunningSum[postit->second];
                }
                else
                {
                    size_t postProdID = orderNode._registeredRunningSum.size();

                    orderNode._registeredRunningSumMap[postTuple] = postProdID;
                    orderNode._registeredRunningSum.push_back(postTuple);

                    runSumReg[prodID] = &orderNode._registeredRunningSum[postProdID];
                }
            }
            else if (depth+1 != order.size())
            {
                ERROR("ERROR: No runningSumAggregate found - ");
                ERROR(depth << " : " << order.size()<< "  " << prodID << "\n");
                exit(1);
            }
        }
    }

/*
    // (depth == 0 && viewLevelRegister[view_id] == varOrder.size()))
    if (depth == outputLevel)
    {
        // We now add both components to the respecitve registers
        // std::pair<bool,size_t> regTupleProduct;
        // std::pair<bool,size_t> regTupleView;

        // now adding aggregates to final computation 
        for (size_t prodID = 0; prodID < numLocalProducts; ++prodID)
        {
            // TODO: Why not avoid this loop and put the vectors into the AggregateTuple
            // This is then one aggregateTuple for one specific Aggregate for
            // this view then add this aggregate to the specific view and not to depth
            
            const prod_bitset&  dependentFunctions = dependentComputation[prodID].product;
            const std::vector<std::pair<size_t,size_t>>& dependentViewReg =
                dependentComputation[prodID].viewAggregateProduct;

            ProductAggregate prodAgg;
            prodAgg.product = dependentFunctions;
            prodAgg.viewAggregate = dependentViewReg;
            prodAgg.previous = {order.size(), 0};
            
            AggregateTuple aggComputation;
            aggComputation.viewID = view_id;
            aggComputation.aggID = agg_id;

            // TODO: avoid this check --- but make sure it is correct
            // The local aggregate should be nullptr!! 
            if (outputLevel != varOrder.size()) 
                aggComputation.local = localComputation[prodID];
            else
                aggComputation.local = nullptr;
            
            aggComputation.post = runSumReg[prodID];

            aggComputation.dependentComputation = dependentComputation[prodID];

            aggComputation.dependentProdAgg = prodAgg;

            if (dependentFunctions.any() || !dependentViewReg.empty())
                aggComputation.hasDependentComputation = true;
            
            // if (dependentFunctions.any() || !dependentViewReg.empty())
            // {                
            //     ProductAggregate prodAgg;
            //     prodAgg.product = dependentFunctions;
            //     prodAgg.viewAggregate = dependentViewReg;
            //     prodAgg.previous = {varOrder.size(), 0};
            //     auto prod_it = productToVariableMap[depDepth].find(prodAgg);
            //     if (prod_it != productToVariableMap[depDepth].end())
            //     {
            //         // Then update the localAggReg for this product
            //         aggComputation.dependentProduct = {depDepth,prod_it->second};
            //     }
            //     else
            //     {
            //         // If so, add it to the aggregate register
            //         size_t newProdID = productToVariableRegister[depDepth].size();
            //         productToVariableMap[depDepth][prodAgg] = newProdID;
            //         aggComputation.dependentProduct = {depDepth,newProdID};
            //         productToVariableRegister[depDepth].push_back(prodAgg);
            //     }
            // }
            // else
            // {
            //     aggComputation.dependentProduct = {varOrder.size(), 0};
            // }
            
            aggregateComputation[view_id].push_back(aggComputation);
        }
    }
*/
}
