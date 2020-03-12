//--------------------------------------------------------------------
//
// QueryCompiler.h
//
// Created on: 07/10/2017
// Author: Max
//
//--------------------------------------------------------------------

#ifndef INCLUDE_QUERYCOMPILER_H_
#define INCLUDE_QUERYCOMPILER_H_

#include <map>
#include <unordered_map>
#include <unordered_set>
#include <string>

#include <CompilerStructures.hpp>

class AggregateRegister;
class ViewGroupCompiler;
class DataStructureLayer;

typedef std::tuple<int,int,std::vector<Product>,var_bitset> cache_tuple;
typedef std::tuple<int,int,var_bitset> view_tuple;


class QueryCompiler : public std::enable_shared_from_this<QueryCompiler>
{

public:
    QueryCompiler(std::shared_ptr<Database> db, std::shared_ptr<TreeDecomposition> td);

    ~QueryCompiler();

    void compile();

    void addFunction(Function* f);

    void addQuery(Query* q);

    size_t numberOfViews() const;

    size_t numberOfQueries() const;

    size_t numberOfFunctions() const;

    size_t numberOfViewGroups() const;
    
    View* getView(size_t v_id);

    Query* getQuery(size_t q_id);

    Function* getFunction(size_t f_id);

    ViewGroup& getViewGroup(size_t group_id);

    AttributeOrder& getAttributeOrder(size_t group_id);

    bool requireHashing(size_t view_id) const;
    
private:
    
    /* Pointer to the database which stores the schema and catalog*/
    std::shared_ptr<Database> _db;

    /* Pointer to the tree decomposition*/
    std::shared_ptr<TreeDecomposition> _td;

    std::shared_ptr<AggregateRegister> _aggregateRegistrationLayer;
    
    std::shared_ptr<ViewGroupCompiler> _viewGroupLayer;

    std::shared_ptr<DataStructureLayer> _dataStructureLayer;
    
    /* List of Querys that we want to compute. */
    std::vector<Query*> _queryList;

    /*  List of all views. The index indicates the ID of the view */
    std::vector<View*> _viewList;
    
    /*  List of all functions. The index indicates the ID of the function */
    std::vector<Function*> _functionList;
    
    /* Method that turns Querys into messages. */
    std::pair<size_t,size_t> compileViews(TDNode* node, size_t targetID,
        std::vector<Product> aggregate,var_bitset freeVars);
    
    std::unordered_map<cache_tuple, std::pair<int,int>> _cache;
    
    std::unordered_map<view_tuple, int> _viewCache;
    
    void computeIncomingViews();
    
    void test(); // TODO: this should be removed - but helpful for testing

    void printQueries();

    void printViews();

    void printViewGraph();
    
};

#endif /* INCLUDE_QUERYCOMPILER_H_ */
