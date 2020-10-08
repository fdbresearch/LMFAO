
//--------------------------------------------------------------------
//
// QueryCompiler.cpp
//
// Created on: 07/10/2017
// Author: Max
//
// This file provides the compileViews functions that decomposes a batch of
// aggregate queries into directed views along the edges of a given
// tree decoposition.
//
//--------------------------------------------------------------------

#include <boost/dynamic_bitset.hpp>

#include <QueryCompiler.h>

using namespace std;
using namespace multifaq::params;

/* Enable options below to print additional information on the Queries and Views */
// #define PRINT_OUT_COMPILER
// #define PRINT_OUT_COMPILER_EXTENDED

QueryCompiler::QueryCompiler(shared_ptr<TreeDecomposition> td) : _td(td)
{
}

QueryCompiler::~QueryCompiler()
{
    for (Function* f : _functionList)
        delete f;
}


/*
 * This function takes the queries defined for a given application and
 * decomposes each aggregate in the query into directed views over the given
 * tree decomposition.  
 *
 * The main procedure is encoded in the compileView() function below. 
 */
void QueryCompiler::compile()
{
    DINFO("Compiling the queries into views - number of queries: " + 
          to_string(_queryList.size())+"\n");

#ifdef PRINT_OUT_COMPILER_EXTENDE /* PRINT OUT */
    printf("Print list of queries to be computed: \n");
    int qID = 0;
    for (Query* q : _queryList)
    {
        printf("%d (%s): ", qID++, _td->getRelation(q->_rootID)->_name.c_str());
        for (size_t i = 0; i < NUM_OF_VARIABLES; i++)
            if(q->_fVars.test(i)) printf(" %s ",_td->getAttribute(i)->_name.c_str());
        printf(" || ");
        
        for (size_t aggNum = 0; aggNum < q->_aggregates.size(); ++aggNum)
        {
            Aggregate* agg = q->_aggregates[aggNum];
            size_t aggIdx = 0;
            for (size_t i = 0; i < agg->_agg.size(); i++)
            {
                const auto &prod = agg->_agg[aggIdx];
                for (size_t f = 0; f < NUM_OF_FUNCTIONS; f++)
                    if (prod.test(f))
                    {
                        Function* func = getFunction(f);
                        printf(" f_%lu(", f);
                        for (size_t i = 0; i < NUM_OF_VARIABLES; i++)
                            if (func->_fVars.test(i))
                                printf(" %s ", _td->getAttribute(i)->_name.c_str());
                        printf(" )");
                    }
                    ++aggIdx;
                printf(" - ");
            }
        }
        printf("\n");
    }
#endif /* PRINT OUT */


    /* Iterate over each query and compile it into views.  */
    for (Query* q : _queryList)
    {
        for (size_t agg = 0; agg < q->_aggregates.size(); ++agg)
        {
            Aggregate* aggregate = q->_aggregates[agg];

            aggregate->_incoming.push_back(
                compileViews(_td->getRelation(q->_rootID), q->_rootID,
                             aggregate->_agg, q->_fVars)
                );
        }
    }

#ifdef PRINT_OUT_COMPILER  /* PRINT OUT */
    printf("Print list of views to be computed: \n");
    int viewID = 0;
    for (View* v : _viewList)
    {
        printf("%d (%s, %s): ", viewID++, _td->getRelation(v->_origin)->_name.c_str(),
               _td->getRelation(v->_destination)->_name.c_str());
        for (size_t i = 0; i < NUM_OF_VARIABLES; i++)
            if (v->_fVars.test(i)) printf(" %s ", _td->getAttribute(i)->_name.c_str());
        printf(" || ");

        std::string aggString = "";
        // for (size_t aggNum = 0; aggNum < v->_aggregates.size(); ++aggNum)
        // {
        //     aggString += "\n("+to_string(aggNum)+") : ";
        //     Aggregate* agg = v->_aggregates[aggNum];
        //     size_t aggIdx = 0;
        //     for (size_t i = 0; i < agg->_agg.size(); i++)
        //     {
        //         const auto &prod = agg->_agg[aggIdx];
        //         for (size_t f = 0; f < NUM_OF_FUNCTIONS; f++) {
        //             if (prod.test(f)) {
        //                 Function* func = getFunction(f);
        //                 aggString += "f_"+to_string(f)+"(";
        //                 for (size_t i = 0; i < NUM_OF_VARIABLES; i++)
        //                     if (func->_fVars.test(i))
        //                         aggString += _td->getAttribute(i)->_name;
        //                 aggString += ")*";
        //             }
        //         }
        //         aggString.pop_back();
        //         aggString += "+";
        //         ++aggIdx;
        //     }
        //     aggString.pop_back();
        //     aggString += "   ||   ";
        // }
        printf("%s -- %lu \n", aggString.c_str(),v->_aggregates.size());
    }
#endif /* PRINT OUT */

}

void QueryCompiler::addFunction(Function* f)
{
    _functionList.push_back(f);
}

void QueryCompiler::addQuery(Query* q)
{
    _queryList.push_back(q);
}

size_t QueryCompiler::numberOfViews()
{
    return _viewList.size();
}

size_t QueryCompiler::numberOfQueries()
{
    return _queryList.size();
}

size_t QueryCompiler::numberOfFunctions()
{
    return _functionList.size();
}

View* QueryCompiler::getView(size_t v_id)
{
    return _viewList[v_id];
}

Query* QueryCompiler::getQuery(size_t q_id)
{
    return _queryList[q_id];
}

Function* QueryCompiler::getFunction(size_t f_id)
{
    return _functionList[f_id];
}


/*
 * This function takes as input an aggregate (sum of products), a set of free
 * variables, and node in the tree decomposition, and then recursively
 * decomposes the aggregate into views over the subtree that is rooted by the
 * given node.
 * 
 * The procedure is merges views which share common aggregates and/or free
 * variables.
 */
pair<size_t,size_t> QueryCompiler::compileViews(TDNode* node, size_t destinationID,
                                   vector<prod_bitset> aggregate, var_bitset freeVars)
{
    bool print = false;
    
    /* Check if the required view has already been declared - if so reuse this one */
    cache_tuple t = make_tuple(node->_id, destinationID, aggregate, freeVars);
    auto it = _cache.find(t);
    if (it != _cache.end())
    {
        if (print) 
            DINFO("We reused an existing view. \n");      
        return it->second;
    }

    
    if (print)
    {
        DINFO("\nBeginning compileViews at node " + to_string(node->_id) + " destination "+
              to_string(destinationID) + " freeVars: " + freeVars.to_string() +
              " number of products: " + to_string(aggregate.size()) +"\n");

        std::string aggString = " ";
        size_t aggIdx = 0;
        for (size_t i = 0; i < aggregate.size(); i++) {
            const auto &prod = aggregate[aggIdx];
            for (size_t f = 0; f < NUM_OF_FUNCTIONS; f++) {
                if (prod.test(f)) {
                    Function* func = getFunction(f);
                    aggString += "f_"+to_string(f)+"(";
                    for (size_t i = 0; i < NUM_OF_VARIABLES; i++)
                        if (func->_fVars.test(i))
                            aggString += _td->getAttribute(i)->_name;
                    aggString += ")*";
                }
            }
            aggString.pop_back();
            aggString += " + ";
            ++aggIdx;
        }
        aggString.pop_back();
        printf("%s\n", aggString.c_str());
    }
    
    vector<prod_bitset> pushDownMask(node->_numOfNeighbors);
    prod_bitset localMask, forcedLocalFunctions, remainderMask, presentFunctions;

    // Find all functions in the aggregate 
    for (size_t prod = 0; prod < aggregate.size(); prod++)
        presentFunctions |= aggregate[prod];

    for (size_t i = 0; i < NUM_OF_FUNCTIONS; i++)
    {
        if (presentFunctions[i])
        {            
            const var_bitset& fVars = _functionList[i]->_fVars;
            bool pushedDown = false;

            /*
             * Check if that function is included in the local bag
             * - if so, then we do not push it down 
             */
            if ((node->_bag & fVars) == fVars)
            {
                /* Leave it here as local computation */
                localMask.set(i);
            }
            else
            {
                /* For each child of current node */
                for (size_t c = 0; c < node->_numOfNeighbors; c++)
                {
                    /* chekc that this neighbor is not the destination */
                    if (node->_neighbors[c] != destinationID)
                    {
                        /* childSchema contains the function then push down */
                        if ((node->_neighborSchema[c] & fVars) == fVars)
                        {
                            /* push it to this child */
                            pushDownMask[c].set(i);
                            remainderMask.set(i);
                            pushedDown = true;
                            break;
                        }
                    }
                }

                if (!pushedDown)
                {
                    /* Leave it here as local computation */
                    localMask.set(i);
                    forcedLocalFunctions.set(i);
                }
            }
        }
    }
    
    vector<var_bitset> pushDownFreeVars(aggregate.size(), freeVars);
    
    std::unordered_map<prod_bitset, agg_bitset> localAggMap;


    /* 
     * For each product - split the functions into the ones that are pushed down
     * and the local functions 
     */
    for (size_t prod = 0; prod < aggregate.size(); prod++)
    {
        // Add freeVars that do not exist in local bag to pushDownFreeVars
        prod_bitset forcedLocal = aggregate[prod] & forcedLocalFunctions;
        
        for (size_t f = 0; f < NUM_OF_FUNCTIONS; ++f)
        {
            if (forcedLocal[f])
            {
                pushDownFreeVars[prod] |=
                    _functionList[f]->_fVars & ~node->_bag;
            }
        }
        
        prod_bitset localAggregate = aggregate[prod] & localMask;
        
        // add this product to localAggregates
        auto it = localAggMap.find(localAggregate);
        if (it != localAggMap.end())
        {
            it->second.set(prod);
        }
        else
        {
            localAggMap[localAggregate] = agg_bitset().set(prod);
        }
    }
  
    if (print)
    {
        size_t localAggIndex = 0;
        for (pair<const prod_bitset, agg_bitset>& localAgg : localAggMap)
            std::cout << localAggIndex++ << " : " << localAgg.first 
                      << " -> " << localAgg.second << std::endl;
    }


    Aggregate* agg = new Aggregate();

    /* 
     * We now computed the local aggregates and the groups of products they
     * correspond to - now we check if we can merge several products before the
     * recursion 
     */
    size_t localAggIndex = 0;
    for (pair<const prod_bitset, agg_bitset>& localAgg : localAggMap)
    {
        if (print)
            std::cout << localAggIndex++ << " : " << localAgg.first 
                      << " -> " << localAgg.second << std::endl;
        
        for (size_t prod = 0; prod < NUM_OF_PRODUCTS; ++prod)
        {
            if (localAgg.second[prod])
            {
                if (print)
                    std::cout << prod << std::endl;
                
                // Push local aggregate on aggregate
                agg->_agg.push_back(localAgg.first);
                
                size_t neigh = 0;
                bool mergable = false;

                // Check if we can merge some of the products that need to be
                // computed below
                agg_bitset merges, candidate = localAgg.second;

                if (localAgg.second.count() > 1)
                {
                    for (neigh = 0; neigh < node->_numOfNeighbors; ++neigh)
                    {
                        for (size_t other_prod = prod+1; other_prod < NUM_OF_PRODUCTS;
                             ++other_prod)
                        {
                            if (candidate[other_prod])
                            {
                                if ((aggregate[prod] & pushDownMask[neigh]) !=
                                    (aggregate[other_prod] & pushDownMask[neigh]))
                                {
                                    if ((aggregate[prod] & ~pushDownMask[neigh]) ==
                                        (aggregate[other_prod] & ~pushDownMask[neigh]))
                                    {
                                        merges.set(other_prod);
                                        mergable = true;
                                    }

                                    candidate.reset(other_prod);
                                }
                            }
                        }

                        if (mergable)
                            break;                        
                    }
                }

                // For each neighbor compile the (merged) views and push to incoming
                for (size_t n = 0; n < node->_numOfNeighbors; ++n)
                {
                    if (node->_neighbors[n] != destinationID)
                    {
                        vector<prod_bitset> childAgg =
                            {aggregate[prod] & pushDownMask[n]};
                        
                        if (mergable && n == neigh)
                        {  
                            for (size_t other = 0; other < NUM_OF_PRODUCTS; ++other)
                            {
                                if (merges[other])
                                {     
                                    /* Add mergable products to this array */
                                    childAgg.push_back(
                                        aggregate[other] & pushDownMask[n]);
                                    
                                    /* And unset for the flag in localAgg.second */
                                    localAgg.second.reset(other);
                                }
                            }
                        }

                        TDNode* neighbor = _td->getRelation(node->_neighbors[n]);

                        var_bitset neighborFreeVars =
                            ((freeVars | pushDownFreeVars[prod]) &
                             node->_neighborSchema[n]) |
                            (neighbor->_bag & node->_bag);

                        /* Recursive call for the next child */ 
                        agg->_incoming.push_back(
                             compileViews(neighbor, node->_id, childAgg, neighborFreeVars));
                    }
                }
            }
        }
    }

    /* 
     * Check here if a View with the same node, destination and freeVars exists 
     * if so then we want to append this aggregate to the exisiting view
     */
    View* view;
    view_tuple vt = make_tuple(node->_id, destinationID, freeVars);
    pair<size_t, size_t> viewAggregatePair;
    
    auto view_it = _viewCache.find(vt);
    if (view_it != _viewCache.end())
    {
        /* If the required view has already been declared, we append the
         * aggregate to this view 
         */

        view = _viewList[view_it->second];
        
        view->_aggregates.push_back(agg);

        /* Used for caching entire view and aggregate */
        viewAggregatePair =  make_pair(view_it->second, view->_aggregates.size()-1);
    }
    else
    {
        /* 
         * If the required view has not already been declared, we construct a
         * new view and the aggregate to it
         */
        view = new View(node->_id, destinationID);
        
        view->_fVars = freeVars;
        view->_aggregates.push_back(agg);

        /* Adding local view to the list of views */
        _viewList.push_back(view);

        /* Cache this view structure */
        _viewCache[vt] = _viewList.size()-1;

        /* Used for caching entire view and aggregate */
        viewAggregatePair =  make_pair(_viewList.size()-1, 0);
    }
    
    /* Adding the  view for the entire subtree to the cache */ 
    _cache[t] = viewAggregatePair;
    
    /* Return ID of this view */  
    return viewAggregatePair;    
}

