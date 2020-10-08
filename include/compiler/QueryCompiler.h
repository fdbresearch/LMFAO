//--------------------------------------------------------------------
//
// QueryCompiler.h
//
// Created on: 07/10/2017
// Author: Max
//
// Header file for the QueryCompiler which decomposes a batch of aggregate
// queries into directed views along the edges of a given tree decoposition.
//
//--------------------------------------------------------------------

#ifndef INCLUDE_QUERYCOMPILER_H_
#define INCLUDE_QUERYCOMPILER_H_

#include <bitset>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <string>
#include <vector>

#include <CompilerUtils.hpp>
#include <GlobalParams.hpp>
#include <Logging.hpp>
#include <TDNode.hpp>
#include <TreeDecomposition.h>

typedef std::bitset<multifaq::params::NUM_OF_VARIABLES> var_bitset;
typedef std::bitset<multifaq::params::NUM_OF_FUNCTIONS> prod_bitset;
typedef std::bitset<multifaq::params::NUM_OF_PRODUCTS> agg_bitset;

typedef std::tuple<int,int,std::vector<prod_bitset>,var_bitset> cache_tuple;
typedef std::tuple<int,int,var_bitset> view_tuple;


namespace std
{
/**
 * Custom hash function for aggregate stucture.
 */
    template<> struct hash<vector<prod_bitset>>
    {
        HOT inline size_t operator()(const vector<prod_bitset>& p) const
	{
            size_t seed = 0;
            hash<prod_bitset> h;
            for (prod_bitset d : p)
                seed ^= h(d) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            return seed;
	}
    };

    /**
     * Custom hash function for cache data stucture.
     */
    template<> struct hash<tuple<int,int,var_bitset>>
    {
        HOT inline size_t operator()(const tuple<int,int,var_bitset>& p) const
	{
            size_t seed = 0;
            hash<int> h1;
            seed ^= h1(std::get<0>(p)) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= h1(std::get<1>(p)) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            hash<var_bitset> h4;
            seed ^= h4(std::get<2>(p)) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            return seed;
	}
    };


    /**
     * Custom hash function for cache data stucture.
     */
    template<> struct hash<tuple<int,int,vector<prod_bitset>,var_bitset>>
    {
        HOT inline size_t operator()(const tuple<int,int,
                                     vector<prod_bitset>,var_bitset>& p) const
	{
            size_t seed = 0;
            hash<int> h1;
            seed ^= h1(std::get<0>(p)) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            hash<int> h2;
            seed ^= h2(std::get<1>(p)) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            hash<vector<prod_bitset>> h3;
            seed ^= h3(std::get<2>(p)) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            hash<var_bitset> h4;
            seed ^= h4(std::get<3>(p)) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            return seed;
	}
    };
}

enum Operation 
{
    count,
    sum,
    linear_sum,
    quadratic_sum,
    cubic_sum,
    quartic_sum,
    exponential,
    prod,
    indicator_eq,
    indicator_neq,
    indicator_lt,
    indicator_gt,
    lr_cont_parameter,
    lr_cat_parameter,
    dynamic  // TODO: we should provide functionality to give names to dynamic functions
};

struct Function
{
    var_bitset _fVars;
    Operation _operation;
    double* _parameter = nullptr;
    bool _dynamic;
    std::string _name = "";  
    
    Function(std::set<size_t> v, Operation o) :
        _operation(o), _dynamic(false)
    {
        for (size_t i : v) _fVars.set(i);
    }

    Function(std::set<size_t> v, Operation o, double* parameter) :
        _operation(o), _parameter(parameter), _dynamic(false)
    {
        for (size_t i : v) _fVars.set(i);
    }

    Function(std::set<size_t> v, Operation o, bool dynamic,
             const std::string& name) :
        _operation(o), _dynamic(dynamic), _name(name)
    {
        for (size_t i : v) _fVars.set(i);
    }

    ~Function()
    {
    }    
};

struct Aggregate
{
    // Each aggregate is a sum of products of functions
    std::vector<prod_bitset> _agg;

    // Each product can have potentially different incoming Views 
    std::vector<std::pair<size_t,size_t>> _incoming;
    
    Aggregate() {}
};

struct Query
{    
    size_t _rootID;
    var_bitset _fVars;

    std::vector<Aggregate*> _aggregates;

    Query(std::set<size_t> v, int root) :
        _rootID(root)
    {
        for (size_t i : v) _fVars.set(i);
    }
    
    Query() {}
};

struct View
{
    unsigned int _numberIncomingViews;    

    unsigned int _origin;
    unsigned int _destination;
    unsigned int _usageCount; 
    var_bitset _fVars;
    std::vector<Aggregate*> _aggregates;
    
    View (int o, int d) : _origin(o) , _destination(d) {}
};



class QueryCompiler
{

public:
    QueryCompiler(std::shared_ptr<TreeDecomposition> td);

    ~QueryCompiler();

    void compile();
    
    void addFunction(Function* f);

    void addQuery(Query* q);

    size_t numberOfViews();

    size_t numberOfQueries();

    size_t numberOfFunctions();
    
    View* getView(size_t v_id);

    Query* getQuery(size_t q_id);

    Function* getFunction(size_t f_id);
    
private:
    /* Pointer to the tree decomposition*/
    std::shared_ptr<TreeDecomposition> _td;
     
    /* List of Querys that we want to compute. */
    std::vector<Query*> _queryList;

    /*  List of all views. The index indicates the ID of the view */
    std::vector<View*> _viewList;
    
    /*  List of all functions. The index indicates the ID of the function */
    std::vector<Function*> _functionList;
    
    /* Method that turns aggregates into views. */
    std::pair<size_t,size_t> compileViews(
        TDNode* node, size_t targetID, std::vector<prod_bitset> aggregate, var_bitset freeVars);
    
    std::unordered_map<cache_tuple, std::pair<int,int>> _cache;
    
    std::unordered_map<view_tuple, int> _viewCache;
};

#endif /* INCLUDE_QUERYCOMPILER_H_ */
