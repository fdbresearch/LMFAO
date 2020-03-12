//--------------------------------------------------------------------
//
// QueryCompiler.cpp
//
// Created on: 07/10/2017
// Author: Max
//
//--------------------------------------------------------------------

#include <QueryCompiler.h>

#include <AggregateRegister.h>
#include <DataStructureLayer.h>
#include <Logging.hpp>
#include <ViewGroupCompiler.h>

#include <boost/dynamic_bitset.hpp>

using namespace std;
using namespace multifaq::params;

#define PRINT_OUT_COMPILER
// #define PRINT_OUT_COMPILER_EXTENDED

QueryCompiler::QueryCompiler(
    std::shared_ptr<Database> db, shared_ptr<TreeDecomposition> td) :
    _db(db), _td(td)
{
}


QueryCompiler::~QueryCompiler()
{
    for (Function* f : _functionList)
        delete f;
}

void QueryCompiler::compile()
{
    _aggregateRegistrationLayer.reset(
        new AggregateRegister(_db, _td, shared_from_this()));
    
    _viewGroupLayer.reset(new ViewGroupCompiler(_db, _td, shared_from_this()));

    _dataStructureLayer.reset(new DataStructureLayer(_db, _td, shared_from_this()));

    
    DINFO("Compiling the queries into views - number of queries: " + 
          to_string(_queryList.size())+"\n");

#ifdef PRINT_OUT_COMPILER_EXTENDED
    printQueries();
#endif
    
    for (Query* q : _queryList)
    {
        for (size_t agg = 0; agg < q->_aggregates.size(); ++agg)
        {
            Aggregate* aggregate = q->_aggregates[agg];
            
            q->_incoming.push_back(
                compileViews(q->_root, q->_root->_relation->_id,
                             aggregate->_sum, q->_fVars)
                );
        }
    }

    // TODO:TODO: Do this automatically in compile Views?! 
    computeIncomingViews();

#ifdef PRINT_OUT_COMPILER  /* Printout */
    printViews();
    printViewGraph();

    exit(0);
    
#endif /* Printout */

    _viewGroupLayer->compile();

    _aggregateRegistrationLayer->compile();

    _dataStructureLayer->compile();
}

void QueryCompiler::addFunction(Function* f)
{
    _functionList.push_back(f);
}

void QueryCompiler::addQuery(Query* q)
{
    _queryList.push_back(q);
}

size_t QueryCompiler::numberOfViews() const
{
    return _viewList.size();
}

size_t QueryCompiler::numberOfQueries() const
{
    return _queryList.size();
}

size_t QueryCompiler::numberOfFunctions() const
{
    return _functionList.size();
}

size_t QueryCompiler::numberOfViewGroups() const 
{
    return _viewGroupLayer->numberOfGroups();
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

ViewGroup& QueryCompiler::getViewGroup(size_t group_id) 
{
    return _viewGroupLayer->getGroup(group_id);
}

AttributeOrder& QueryCompiler::getAttributeOrder(size_t group_id)
{
    return _aggregateRegistrationLayer->getAttributeOrder(group_id);
}

bool QueryCompiler::requireHashing(size_t view_id) const
{
    return _dataStructureLayer->requireHashing(view_id);
}

pair<size_t,size_t> QueryCompiler::compileViews(TDNode* node, size_t targetID,
                                   vector<Product> aggregate, var_bitset freeVars)
{
    bool print = 0;

    Relation* rel = node->_relation;
    
    /* Check if the required view has already been declared - if so reuse this one */
    cache_tuple t = make_tuple(rel->_id, targetID, aggregate, freeVars);
    auto it = _cache.find(t);
    if (it != _cache.end())
    {
        if (print) 
            DINFO("We effectively reused an exisitng view. \n");      
        // _viewList[it->second]->_usageCount += 1;
        return it->second;
    }

    
    if (print)
    {
        DINFO("\nBeginning compileViews at node " + to_string(node->_relation->_id) +
              " target "+to_string(targetID) + " freeVars: " + freeVars.to_string() +
              " number of products: " + to_string(aggregate.size()) +"\n");

        std::string aggString = " ";
        size_t aggIdx = 0;
        for (size_t i = 0; i < aggregate.size(); i++) {
            const auto &prod = aggregate[aggIdx];
            for (size_t f = 0; f < NUM_OF_FUNCTIONS; f++) {
                if (prod._prod.test(f)) {
                    Function* func = getFunction(f);
                    aggString += "f_"+to_string(f)+"(";
                    for (size_t i = 0; i < NUM_OF_VARIABLES; i++)
                        if (func->_fVars.test(i))
                            aggString += _db->getAttribute(i)->_name;
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
    
    vector<prod_bitset> pushDownMask(node->_neighbors.size());
    prod_bitset localMask, forcedLocalFunctions, remainderMask, presentFunctions;

    // find all functions in the aggregate 
    for (size_t prod = 0; prod < aggregate.size(); prod++)
        presentFunctions |= aggregate[prod]._prod;

    for (size_t i = 0; i < NUM_OF_FUNCTIONS; i++)
    {
        if (presentFunctions[i])
        {
            /* Get parameters of the function */
            const var_bitset& fVars = _functionList[i]->_fVars;
            bool pushedDown = false;

            /*
             * Check if function variables are included in the local bag
             * - if so, then we do not push it down 
             */
            if ((rel->_schemaMask & fVars) == fVars)
            {
                /* Leave it here as local computation */
                localMask.set(i);
            }
            else 
            {
                /* The function variables are not included in local bag
                 *  - check if they are included in any subtree. */
                
                /* For each child of current node */
                for (size_t c = 0; c < node->_neighbors.size(); c++)
                {     
                    /* ChildSchema contains the function then push down */
                    if (node->_neighbors[c] != targetID &&
                        (node->_neighborSchema[c] & fVars) == fVars)
                    {
                        /* push it to this child */
                        pushDownMask[c].set(i);
                        remainderMask.set(i);
                        pushedDown = true;
                        break;
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

    if (print) std::cout << localMask << std::endl;
    
    vector<var_bitset> pushDownFreeVars(aggregate.size(), freeVars);
    
    std::unordered_map<prod_bitset, agg_bitset> localAggMap;

    for (size_t prod = 0; prod < aggregate.size(); prod++)
    {
        // Add freeVars that do not exist in local bag to pushDownFreeVars
        prod_bitset forcedLocal = aggregate[prod]._prod & forcedLocalFunctions;
        
        for (size_t f = 0; f < NUM_OF_FUNCTIONS; ++f)
        {
            if (forcedLocal[f])
            {
                pushDownFreeVars[prod] |=
                    _functionList[f]->_fVars & ~rel->_schemaMask;
            }
        }
        
        prod_bitset localAggregate = aggregate[prod]._prod & localMask;
        
        //****
        // add this product to localAggMap
        // which for each local aggregate keeps a list of produts that require
        // the same local aggregate, this is need for merging
        //*** 
        auto it = localAggMap.find(localAggregate);
        if (it != localAggMap.end())
            it->second.set(prod);
        else
            localAggMap[localAggregate] = agg_bitset().set(prod);
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
                // Push local aggregate on aggregate
                agg->_sum.push_back(localAgg.first);
                Product& p = agg->_sum.back();
                
                size_t neigh = 0;
                bool mergable = false;

                // Check if we can merge some of the products that need to be
                // computed below
                agg_bitset merges, candidate = localAgg.second;

                if (localAgg.second.count() > 1)
                {
                    for (neigh = 0; neigh < node->_neighbors.size(); ++neigh)
                    {
                        for (size_t other_prod = prod+1; other_prod < NUM_OF_PRODUCTS;
                             ++other_prod)
                        {
                            if (candidate[other_prod])
                            {
                                if ((aggregate[prod]._prod & pushDownMask[neigh]) !=
                                    (aggregate[other_prod]._prod & pushDownMask[neigh]))
                                {
                                    if ((aggregate[prod]._prod & ~pushDownMask[neigh]) ==
                                        (aggregate[other_prod]._prod & ~pushDownMask[neigh]))
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

                if (print && mergable)
                    std::cout << "mergeable: " << merges << std::endl;
                
                // For each neighbor compile the (merged) views and push on incoming
                for (size_t n = 0; n < node->_neighbors.size(); ++n)
                {
                    if (node->_neighbors[n] != targetID)
                    {
                        vector<Product> childAgg =
                            {aggregate[prod]._prod & pushDownMask[n]};
                        
                        if (mergable && n == neigh)
                        {  
                            for (size_t other = 0; other < NUM_OF_PRODUCTS; ++other)
                            {
                                if (merges[other])
                                {     
                                    /* Add mergable products to this array */
                                    childAgg.push_back(
                                        aggregate[other]._prod & pushDownMask[n]);
                                    
                                    /* And unset for the flag in localAgg.second */
                                    localAgg.second.reset(other);
                                }
                            }
                        }

                        TDNode* neighbor = _td->getTDNode(node->_neighbors[n]);

                        var_bitset neighborFreeVars =
                            ((freeVars | pushDownFreeVars[prod]) & node->_neighborSchema[n]) |
                            (neighbor->_relation->_schemaMask & rel->_schemaMask);

                        p._incoming.push_back(
                             compileViews(neighbor, node->_relation->_id, childAgg,
                                          neighborFreeVars));             
                    }
                }
            }
        }
    }

    /* Check here if a View with the same node, target and freeVars exists 
    - if so then we want to push the aggregate on this array 
    */
    View* view;
    view_tuple vt = make_tuple(node->_id, targetID, freeVars);
    pair<size_t, size_t> viewAggregatePair;
    
    /* Check if the required view has already been declared - if so reuse this one */
    auto view_it = _viewCache.find(vt);
    if (view_it != _viewCache.end())
    {
        view = _viewList[view_it->second];
        view->_aggregates.push_back(agg);

        /* Used for caching entire view and aggregate */
        viewAggregatePair =  make_pair(view_it->second, view->_aggregates.size()-1);
    }
    else
    {
        view = new View(node->_id, targetID);
        
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

    // DINFO("End of compileViews for "+to_string(node->_id)+" to "+
    //       to_string(targetID)+"\n");
    
    /* Return ID of this view */  
    return viewAggregatePair;    
}


void QueryCompiler::computeIncomingViews()
{
    for (size_t viewID = 0; viewID < numberOfViews(); ++viewID)
    {
        View* view = getView(viewID);
        
        std::vector<bool> incViewBitset(numberOfViews());
        for (size_t aggNo = 0; aggNo < view->_aggregates.size(); ++aggNo)
        {
            Aggregate* aggregate = view->_aggregates[aggNo];

            // First find the all views that contribute to this Aggregate
            for (Product& p : aggregate->_sum)
            {
                for (const auto& viewAgg : p._incoming)
                {
                    // Indicate that this view contributes to some aggregate
                    incViewBitset[viewAgg.first] = 1;
                }
            }
        }        
        // For each incoming view, check if it contains any vars from the
        // variable order and then add it to the corresponding info
        for (size_t incViewID=0; incViewID < numberOfViews(); ++incViewID)
            if (incViewBitset[incViewID])
                view->_incomingViews.push_back(incViewID);
    }   
}



void QueryCompiler::printQueries()
{
    printf("List of queries to be computed: \n");
    int qID = 0;
    for (Query* q : _queryList)
    {
        printf("%d (%s): ", qID++, q->_root->_relation->_name.c_str());
        for (size_t i = 0; i < NUM_OF_VARIABLES; i++)
            if(q->_fVars.test(i)) printf(" %s ",_db->getAttribute(i)->_name.c_str());
        printf(" || ");
        
        for (size_t aggNum = 0; aggNum < q->_aggregates.size(); ++aggNum)
        {
            Aggregate* agg = q->_aggregates[aggNum];
            for (size_t i = 0; i < agg->_sum.size(); i++)
            {
                const auto &prod = agg->_sum[i]._prod;
                for (size_t f = 0; f < NUM_OF_FUNCTIONS; f++)
                {
                    if (prod.test(f))
                    {
                        Function* func = getFunction(f);
                        printf(" f_%lu(", f);
                        for (size_t i = 0; i < NUM_OF_VARIABLES; i++)
                            if (func->_fVars.test(i))
                                printf(" %s ", _db->getAttribute(i)->_name.c_str());
                        printf(" )");
                    }
                }
                printf(" - ");
            }
        }
        printf("\n");
    }
}


void QueryCompiler::printViews()
{
    printf("List of views to be computed: \n");
    int viewID = 0;
    for (View* v : _viewList)
    {
        printf("%d (%s, %s): ", viewID++, _db->getRelation(v->_origin)->_name.c_str(),
               _db->getRelation(v->_destination)->_name.c_str());
        for (size_t i = 0; i < NUM_OF_VARIABLES; i++)
            if (v->_fVars.test(i)) printf(" %s ", _db->getAttribute(i)->_name.c_str());
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
        //                         aggString += _db->getAttribute(i)->_name;
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
}



void QueryCompiler::printViewGraph()
{
    printf("List of views to be computed: \n");
    int viewID = 0;
    for (View* v : _viewList)
    {
        printf("V%d_{%s, %s}( ", viewID++, _db->getRelation(v->_origin)->_name.c_str(),
               _db->getRelation(v->_destination)->_name.c_str());
        for (size_t i = 0; i < NUM_OF_VARIABLES; i++)
            if (v->_fVars.test(i)) printf(" %s, ", _db->getAttribute(i)->_name.c_str());

        std::string aggString = "";
        for (size_t aggNum = 0; aggNum < v->_aggregates.size(); ++aggNum)
        {
            // aggString += "("+to_string(aggNum)+") : ";
            Aggregate* agg = v->_aggregates[aggNum];
            size_t aggIdx = 0;

            aggString += "SUM(";
            for (size_t i = 0; i < agg->_sum.size(); i++)
            {
                const auto &prod = agg->_sum[aggIdx];
                
                for (size_t f = 0; f < NUM_OF_FUNCTIONS; f++)
                {
                    if (prod._prod.test(f))
                    {
                        Function* func = getFunction(f);
                        for (size_t i = 0; i < NUM_OF_VARIABLES; i++)
                            if (func->_fVars.test(i))
                                aggString += _db->getAttribute(i)->_name;
                        aggString += "*";
                    }
                }

                if (prod._prod.none())
                    aggString += "1*";


                for (const std::pair<size_t,size_t>& p : prod._incoming)
                {
                    aggString += "V"+std::to_string(p.first)+".agg["+
                        std::to_string(p.second)+"]*";
                }
                
                
                aggString.pop_back();
                aggString += ")+";
                ++aggIdx;
            }
            aggString.pop_back();
            aggString += ",";
        }
        aggString.pop_back();
        printf("%s ) #Aggs: %lu \n\n", aggString.c_str(),v->_aggregates.size());
    }

    viewID = 0;

    std::cout << "graph { " << std::endl;
    
    for (View* v : _viewList)
    {
        std::cout << "\t" << _db->getRelation(v->_origin)->_name << " -> "
                  << "V" << viewID << ";" <<  std::endl;
        
        for (const auto incoming : v->_incomingViews)
            std::cout << "\t" << "V" << incoming << " -> "
                      << "V" << viewID  << ";" <<  std::endl;
        ++viewID;
    }

    std::cout << "}" << std::endl;
    
}


void QueryCompiler::test()
{
    // Function* f0 = new Function({0,1}, Operation::count);
    // Function* f1 = new Function({1,3}, Operation::count);
    // Function* f2 = new Function({0,4}, Operation::sum);
    // Function* f3 = new Function({4,5}, Operation::sum);

    // Function* f4 = new Function({0,2}, Operation::count);
    // Function* f5 = new Function({1,3}, Operation::sum);
    // Function* f6 = new Function({1,3}, Operation::sum);
    
    // // Function* f4 = new Function({4,5}, Operation::count);
    // // Function* f5 = new Function({1,3}, Operation::sum);
    // // Function* f6 = new Function({0,4}, Operation::sum);
    // // Function* f7 = new Function({3,4}, Operation::sum);
    // // Function* f8 = new Function({4,5}, Operation::count);
    // // _functionList = {f0,f1,f2,f3,f4,f5,f6,f7,f8};
    // _functionList = {f0,f1,f2,f3,f4,f5,f6};
    
    // vector<int> prod1 = {0,1,2,3};
    // vector<int> prod2 = {4,1,2,3};
    // vector<int> prod3 = {0,5,2,3};
    // vector<int> prod4 = {0,6,2,3};
    
    // prod_bitset p1, p2, p3, p4;
    // for (int i : prod1)
    //     p1.set(i);
    // for (int i : prod2)
    //     p2.set(i);
    // for (int i : prod3)
    //     p3.set(i);
    // for (int i : prod4)
    //     p4.set(i);
    
    // Aggregate* agg = new Aggregate();
    // agg->_agg = {p1, p2, p3, p4};
    
    // Query* faq = new Query();
    // faq->_root = _td->_root;
    // faq->_aggregates = {agg};
    
    // faq->_fVars.set(1);
    // faq->_fVars.set(3);

    // // faq->_freeVars = {1,3};
    // // faq->_aggregate.push_back(prod1);
    // // faq->_aggregate.push_back(prod2);    
    // // for (auto prod : faq->_aggregate)
    // // {
    // //     printf("Compiling next Product: \n");
    // //     faq->_incomingViews.push_back(
    // //         compileViews(rootNode, rootNode->_id, prod, faq->_freeVars)
    // //         );
    // // }

    // TDNode* rootNode = _td->getTDNode(faq->_root->_id);

    // DINFO("Compiling Views \n");

    // auto resultPair = compileViews(rootNode, rootNode->_id,
    //                                faq->_aggregates[0]->_agg, faq->_fVars);
    
    // agg->_incoming.push_back(resultPair);
    
    // DINFO("Compiled Views - number of Views: "+to_string(_viewList.size())+"\n");
    
    // int viewID = 0;
    // for (View* v : _viewList)
    // {
    //     printf("%d (%s, %s): ", viewID++, _db->getRelation(v->_origin)->_name.c_str(),
    //            _db->getRelation(v->_destination)->_name.c_str());
    //     for (size_t i = 0; i < NUM_OF_VARIABLES; i++)
    //         if (v->_fVars.test(i))
    //             printf(" %lu ", i);
    //     printf(" || \n");
    //     // string aggString = "";
    //     // Aggregate* agg = v->_aggregates[0];
    //     // size_t aggIdx = 0;
    //     // for (size_t i = 0; i < agg->_agg.size(); i++)
    //     // {
    //     //     // cout << "_m["+to_string(i)+"] : " << agg->_m[i] << endl;
    //     //     // while (aggIdx < agg->_m[i])
    //     //     // {
    //     //         auto prod = agg->_agg[aggIdx];

    //     //         // cout << agg->_agg.size() << "aggIndex : " << aggIdx << endl;
    //     //         // cout << "prod : " << prod << endl;    
              
    //     //         for (size_t f = 0; f < NUM_OF_FUNCTIONS; f++)
    //     //             if (prod.test(f))
    //     //                 aggString += " f_"+to_string(f);
    //     //         // aggString+="+";
    //     //         ++aggIdx;
    //     //     // }
    //     //     aggString += " - ";
    //     // }
    //     // printf("%s\n", aggString.c_str());
    // }
}

