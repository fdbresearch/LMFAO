//--------------------------------------------------------------------
//
// LoopRegister.h
//
// Created on: 08/05/2019
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

class LoopRegister 
{
public:

    LoopRegister(std::shared_ptr<Database> db,
                      std::shared_ptr<TreeDecomposition> td,
                      std::shared_ptr<QueryCompiler> qc);

    ~LoopRegister();
    
    void compile();
    
private:

    
void registerOutViewLoop(size_t depth, size_t group_id)
{
    // TODO: This should go into the LOOP LAYER !! it has nothing to do here?
    dyn_bitset loopFactors(_qc->numberOfViews()+1);

    // Compute the loops that are required by this view. 
    for (size_t var = 0; var < multifaq::params::NUM_OF_VARIABLES; ++var)
    {
        if (view->_fVars[var])
        {
            if (!attOrderMask[var])
            {
                // if var is in relation - use relation 
                if (node._schemaMask[var])
                {                                   
                    loopFactors.set(_qc->numberOfViews());
                }
                else // else find corresponding view 
                {
                    for (const size_t& incViewID : incomingViews[viewID])
                    {
                        if (_qc->getView(incViewID)->_fVars[var])
                            loopFactors.set(incViewID);
                    }
                }
            }
        }
    }


    outViewLoop[viewID] = loopFactors;
}



void registerAggregatesToLoops(size_t depth, size_t group_id)
{
    const var_bitset& varOrderBitset = groupVariableOrderBitset[group_id];
    const size_t maxDepth = groupVariableOrder[group_id].size();
    const var_bitset& relationBag =
        _db->getRelation(_qc->getView(viewGroups[group_id][0])->_origin)->_bag;

    listOfLoops.clear();
    functionsPerLoop.clear();
    functionsPerLoopBranch.clear();
    viewsPerLoop.clear();
    nextLoopsPerLoop.clear();

    // go through the ProductAggregates and find functions computed here 
    prod_bitset presentFunctions;    
    dyn_bitset contributingViews(_qc->numberOfViews()+1);
    for (const ProductAggregate& prodAgg : productToVariableRegister[depth])
    {
        presentFunctions |= prodAgg.product;
        for (const std::pair<size_t,size_t>& p : prodAgg.viewAggregate)
            contributingViews.set(p.first);
    }

    /*************** PRINT OUT **************/
    // const TDNode& node =
    //     *_db->getRelation(_qc->getView(viewGroups[group_id][0])->_origin);
    // std::cout << genProductString(node,contributingViews,presentFunctions)<<std::endl;
    /*************** PRINT OUT **************/
    
    dyn_bitset consideredLoops(_qc->numberOfViews()+1);
    dyn_bitset nextVariable(_qc->numberOfViews()+1);
    dyn_bitset prefixLoops(_qc->numberOfViews()+1);

    // compute the function mask for each loop 
    computeLoopMasks(presentFunctions, consideredLoops, varOrderBitset,
                     relationBag, contributingViews, nextVariable, prefixLoops);

    
    // The first register loop should all views that we do not iterate over! 
    viewsPerLoop[0] |= contributingViews;
    for (size_t j = 1; j < viewsPerLoop.size(); ++j)
        viewsPerLoop[0] &= ~viewsPerLoop[j];
    
    newAggregateRegister.clear();
    aggregateRegisterMap.clear();
    newAggregateRegister.resize(listOfLoops.size());
    aggregateRegisterMap.resize(listOfLoops.size());

    localProductMap.clear();
    localProductList.clear();
    localProductList.resize(listOfLoops.size());
    localProductMap.resize(listOfLoops.size());

    viewProductMap.clear();
    viewProductList.clear();
    viewProductList.resize(listOfLoops.size());
    viewProductMap.resize(listOfLoops.size());
    
    for(ProductAggregate& prodAgg : productToVariableRegister[depth])
    {    
        size_t currentLoop = 0;
        bool loopsOverRelation = false;
        prodAgg.correspondingLoopAgg =
            addProductToLoop(prodAgg, currentLoop, loopsOverRelation, maxDepth);
    }
}



prod_bitset computeLoopMasks(
    prod_bitset presentFunctions, dyn_bitset consideredLoops,
    const var_bitset& varOrderBitset, const var_bitset& relationBag,
    const dyn_bitset& contributingViews, dyn_bitset& nextVariable,
    const dyn_bitset& prefixLoops)
{
    dyn_bitset loopFactors(_qc->numberOfViews()+1);
    dyn_bitset nextLoops(_qc->numberOfViews()+1);
    
    prod_bitset loopFunctionMask;
    
    // then find all loops required to compute these functions and create masks
    for (size_t f = 0; f < NUM_OF_FUNCTIONS; f++)
    {
        if (presentFunctions[f])
        {
            const var_bitset& functionVars = _qc->getFunction(f)->_fVars;
            loopFactors.reset();
            
            for (size_t v = 0; v < NUM_OF_VARIABLES; ++v)
            {
                if (functionVars[v] && !varOrderBitset[v])
                {
                    if (relationBag[v])
                        loopFactors.set(_qc->numberOfViews());
                    else
                    {
                        for (size_t incID = 0; incID < _qc->numberOfViews(); ++incID)
                        {
                            if (contributingViews[incID])
                            {
                                const var_bitset& incViewBitset =
                                    _qc->getView(incID)->_fVars;

                                if (incViewBitset[v])
                                {
                                    loopFactors.set(incID);
                                    break;
                                }
                            }
                        }
                    }
                }
            }

            loopFactors &= ~prefixLoops;
            
            if (loopFactors == consideredLoops)
            {
                // Add this function to the mask for this loop
                loopFunctionMask.set(f);
            }
            else
            {
                if ((consideredLoops & loopFactors) == consideredLoops)
                {
                    dyn_bitset remainingLoops = loopFactors & ~consideredLoops;
                    if (remainingLoops.any())
                    {
                        size_t highestPosition = 0;
                        // need to continue with this the next loop
                        for (size_t view=_qc->numberOfViews(); view > 0; view--)
                        {
                            if (consideredLoops[view])
                            {
                                highestPosition = view;
                                break;
                            }
                        }

                        size_t next = remainingLoops.find_next(highestPosition);
                        if (next != boost::dynamic_bitset<>::npos)
                            nextLoops.set(next);
                        else
                        {
                            ERROR("We found a remaining Loop which is not higher " <<
                                  " than the highest considered loop?!\n");
                            exit(1);
                        }
                    }
                }
            }
        }
    }

    // Then use the registrar to split the functions to their loops
    
    size_t numOfLoops = listOfLoops.size();
    listOfLoops.push_back(consideredLoops);
    functionsPerLoop.push_back(loopFunctionMask);
    functionsPerLoopBranch.push_back(loopFunctionMask);
    viewsPerLoop.push_back(nextVariable);
    nextLoopsPerLoop.push_back(std::vector<size_t>());
        
    std::vector<size_t>& nextLoopIDs = nextLoopsPerLoop[numOfLoops];

    presentFunctions &= ~loopFunctionMask;

    // Recurse to the next loops
    for (size_t viewID=0; viewID < _qc->numberOfViews() + 1; ++viewID)
    {
        if (nextLoops[viewID])
        {
            dyn_bitset nextConsideredLoops = consideredLoops;
            nextConsideredLoops.set(viewID);

            nextVariable.reset();
            nextVariable.set(viewID);
            
            nextLoopIDs.push_back(listOfLoops.size());
            
            prod_bitset nextLoopFunctions = computeLoopMasks(
                presentFunctions,nextConsideredLoops,varOrderBitset,
                relationBag,contributingViews,nextVariable, prefixLoops);
            
            functionsPerLoopBranch[numOfLoops] |= nextLoopFunctions;
        }
    }
    
    return functionsPerLoopBranch[numOfLoops];
}





std::pair<size_t,size_t> addProductToLoop(
    ProductAggregate& prodAgg, size_t& currentLoop, bool& loopsOverRelation,
    const size_t& maxDepth)
{
    const size_t thisLoopID = currentLoop;
    const dyn_bitset& thisLoop = listOfLoops[currentLoop];

    dyn_bitset viewsForThisLoop = viewsPerLoop[currentLoop];
    loopsOverRelation = viewsForThisLoop[_qc->numberOfViews()];
    
    viewsForThisLoop.reset(_qc->numberOfViews());
 
    std::pair<bool,size_t> regTupleProduct = {false,0};  

    // check if this product requires computation over this loop loopmask
    prod_bitset loopFunctions = prodAgg.product & functionsPerLoop[currentLoop];
    if (loopFunctions.any())
    {
        // This should be add to the loop not depth 
        auto proditerator = localProductMap[currentLoop].find(loopFunctions);
        if (proditerator != localProductMap[currentLoop].end())
        {
            // set this local product to the id given by the map
            regTupleProduct = {true, proditerator->second};
        }
        else
        {
            size_t nextID = localProductList[currentLoop].size();
            localProductMap[currentLoop][loopFunctions] = nextID;
            regTupleProduct = {true, nextID};
            localProductList[currentLoop].push_back(loopFunctions);
        }
    }

    bool newViewProduct = false;
    bool singleViewAgg = false;
    
    std::pair<size_t, size_t> regTupleView = {2147483647,2147483647};

    if (!prodAgg.viewAggregate.empty())
    {
        if (viewsForThisLoop.count() > 1)
        {
            newViewProduct = true;
       
            std::vector<std::pair<size_t, size_t>> viewReg;

            size_t viewID = viewsForThisLoop.find_first();
            while (viewID != boost::dynamic_bitset<>::npos)
            {
                for (const std::pair<size_t, size_t>& viewAgg : prodAgg.viewAggregate)
                {
                    if (viewAgg.first == viewID)
                    {
                        viewReg.push_back(viewAgg);
                        break;
                    }
                }

                viewID = viewsPerLoop[currentLoop].find_next(viewID);
            }
            auto viewit = viewProductMap[currentLoop].find(viewReg);
            if (viewit != viewProductMap[currentLoop].end())
            {
                // set this local product to the id given by the map
                regTupleView = {currentLoop, viewit->second};            
            }
            else
            {
                size_t nextID =  viewProductList[currentLoop].size();
                viewProductMap[currentLoop][viewReg] = nextID;
                regTupleView = {currentLoop, nextID};
                viewProductList[currentLoop].push_back(viewReg);
            }
        }
        else if (viewsForThisLoop.count() == 1)
        {
            // Inside a loop there would only be a single view agg, so we could avoid
            // explicitly materializing the view Agg and simply multiply directly with
            // the aggregate from the view - this could be captured in the aggregate
            // computation that captures both the local aggregate and the view Agg
            singleViewAgg = true;
        
            // reuse the simple aggregate from the corresponding view
            size_t viewID = viewsForThisLoop.find_first();

            for (const std::pair<size_t, size_t>& viewAgg : prodAgg.viewAggregate)
            {
                if (viewAgg.first == viewID)
                {
                    regTupleView = viewAgg;
                    break;
                }
            }

            /*************** PRINT OUT **************/
            // if (regTupleView.first == 2)
            // {
            //     std::cout << "################## " <<
            //         regTupleView.first << "  " << regTupleView.second << std::endl;
            // }
            /*************** PRINT OUT **************/

        }
    }
    
    
    std::pair<size_t,size_t> previousComputation = {listOfLoops.size(),0};
    
    for (size_t next=0; next < nextLoopsPerLoop[thisLoopID].size(); ++next)
    {
        size_t nextLoop = nextLoopsPerLoop[thisLoopID][next];

        if (((listOfLoops[nextLoop] & thisLoop) != thisLoop))
        {
            ERROR("There is a problem with nextLoopsPerLoop\n");
            exit(1);
        }
        
        if ((functionsPerLoopBranch[nextLoop] & prodAgg.product).any())
        {
            std::pair<size_t, size_t> postComputation =
                addProductToLoop(prodAgg, nextLoop, loopsOverRelation, maxDepth);
            
            if (next + 1 != nextLoopsPerLoop[thisLoopID].size() &&
                (functionsPerLoopBranch[nextLoopsPerLoop[thisLoopID][next+1]]&
                 prodAgg.product).any())
            {
                // Add this product to this loop with the 'prev'Computation
                AggRegTuple postRegTuple;
                postRegTuple.postLoopAgg = true;
                postRegTuple.previous = previousComputation; 
                postRegTuple.postLoop = postComputation;
                
                // TODO: add the aggregate to the register!!
                auto regit = aggregateRegisterMap[nextLoop].find(postRegTuple);
                if (regit != aggregateRegisterMap[nextLoop].end())
                {
                    previousComputation = {nextLoop,regit->second};
                }
                else
                {
                    // If so, add it to the aggregate register
                    size_t newID = newAggregateRegister[nextLoop].size();
                    aggregateRegisterMap[nextLoop][postRegTuple] = newID;
                    newAggregateRegister[nextLoop].push_back(postRegTuple);
                    
                    previousComputation =  {nextLoop,newID};
                }
            }
            else
            {
                previousComputation = postComputation;
            }
            
        }
    }
    
    // while(currentLoop < listOfLoops.size()-1)
    // {
    //     currentLoop++;
    //     if (((listOfLoops[currentLoop] & thisLoop) == thisLoop))
    //     {
    //         if ((functionsPerLoopBranch[currentLoop] & prodAgg.product).any())
    //         {
    //             std::pair<size_t, size_t> postComputation =
    //                 addProductToLoop(prodAgg,currentLoop,loopsOverRelation,maxDepth);
    //             /**************** PRINT OUT ******************/
    //             // std::cout << "HERE \n";
    //             // std::cout << listOfLoops[currentLoop] <<"  "<<thisLoop<<std::endl;
    //             /**************** PRINT OUT ******************/
    //             // Add this product to this loop with the 'prev'Computation
    //             AggRegTuple postRegTuple;
    //             postRegTuple.postLoopAgg = true;
    //             postRegTuple.localAgg = previousComputation; // TODO:TODO:TODO:
    //             postRegTuple.postLoop = postComputation;
    //             // TODO: add the aggregate to the register!!
    //             auto regit = aggregateRegisterMap[currentLoop].find(postRegTuple);
    //             if (regit != aggregateRegisterMap[currentLoop].end())
    //                 previousComputation = {currentLoop,regit->second};
    //             else
    //             {
    //                 // If so, add it to the aggregate register
    //                 size_t newID = newAggregateRegister[currentLoop].size();
    //                 aggregateRegisterMap[currentLoop][postRegTuple] = newID;
    //                 newAggregateRegister[currentLoop].push_back(postRegTuple); 
    //                 previousComputation =  {currentLoop,newID};;
    //             }
    //         }
    //     }
    //     else break;
    // }

    // Check if this product has already been computed, if not we add it to the list
    AggRegTuple regTuple;
    regTuple.product = regTupleProduct;
    regTuple.viewAgg = regTupleView;

    /********* PRINT OUT ********/
    // if (regTupleView.first == 2)
    // {
    //     std::cout << "###########/>>>>>>>>> " <<
    //         regTuple.viewAgg.first << "  " << regTuple.viewAgg.second << std::endl;
    // }
    /********* PRINT OUT ********/
           
    regTuple.newViewProduct = newViewProduct;
    regTuple.singleViewAgg = singleViewAgg;
    regTuple.postLoop = previousComputation;

    regTuple.previous = {listOfLoops.size(),0};
    regTuple.multiplyByCount = false;
    
    // Multiply by previous #computation -- if needed
    if (thisLoop.none())
    {
        if (prodAgg.previous.first < maxDepth)
        {            
            const ProductAggregate& prevProdAgg =
                productToVariableRegister[prodAgg.previous.first]
                [prodAgg.previous.second];
            regTuple.previous = prevProdAgg.correspondingLoopAgg;
            regTuple.prevDepth = prodAgg.previous.first;
        }
        // Mutliply by count if needed
        if (!loopsOverRelation && prodAgg.multiplyByCount)
            regTuple.multiplyByCount = true;
    }

    auto regit = aggregateRegisterMap[thisLoopID].find(regTuple);
    if (regit != aggregateRegisterMap[thisLoopID].end())
    {
        previousComputation = {thisLoopID,regit->second};
    }
    else
    {
        // If so, add it to the aggregate register
        size_t newID = newAggregateRegister[thisLoopID].size();
        aggregateRegisterMap[thisLoopID][regTuple] = newID;
        newAggregateRegister[thisLoopID].push_back(regTuple);
        
        previousComputation =  {thisLoopID,newID};
    }

    return previousComputation;
}



#endif /* INCLUDE_AGGREGATEREGISTER_H_ */
