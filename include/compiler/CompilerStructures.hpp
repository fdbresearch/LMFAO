#include <bitset>
#include <set>
#include <vector>

#include <CompilerUtils.hpp>
#include <GlobalParams.hpp>
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

    template<> struct hash<vector<pair<size_t,size_t>>>
    {
        HOT inline size_t operator()(const vector<pair<size_t,size_t>>& p) const
	{
            size_t seed = 0;
            hash<size_t> h;
            for (const pair<size_t,size_t>& d : p)
            {
                seed ^= h(d.first) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
                seed ^= h(d.second) + 0x9e3779b9 + (seed << 6) + (seed >> 2);    
            }
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
        // if (_parameter != nullptr)
        //     delete[] _parameter;
    }    
};


struct Product
{
    /* Bitset denotes a product of functions. */ 
    prod_bitset _prod;

    /* Each product can have potentially different incoming Views */
    std::vector<std::pair<size_t,size_t>> _incoming;

    Product(prod_bitset p) : _prod(p) { }
};


struct Aggregate
{
    // Each aggregate is a sum of products of functions
    std::vector<Product> _sum;
    
    // Each aggregate is a sum of products of functions
    std::vector<prod_bitset> _agg;

    // Each product can have potentially different incoming Views 
    std::vector<std::pair<size_t,size_t>> _incoming;
    
    Aggregate() {}
};


struct Query
{    
    TDNode* _root;
    var_bitset _fVars;

    std::vector<Aggregate*> _aggregates;

    Query(std::set<size_t> v, TDNode* root) :
        _root(root)
    {
        for (size_t i : v) _fVars.set(i);
    }
    
    Query() {}
};

struct View
{
    var_bitset _fVars;
    unsigned int _origin;
    unsigned int _destination;

    std::vector<Aggregate*> _aggregates;

    /* Helper list to quickly determine ids of incoming views */
    std::vector<size_t> _incomingViews;

    // unsigned int _usageCount; 
    // unsigned int _numberIncomingViews;
    
    View (int o, int d) : _origin(o) , _destination(d) {}
};


// typedef std::vector<size_t> ViewGroup;
struct ViewGroup
{
    std::vector<size_t> _views;

    TDNode* _tdNode;
    bool _parallelize;

    std::vector<size_t> _incomingViews;

    ViewGroup(size_t viewID, TDNode* node)
    {
        _views.push_back(viewID);
        _tdNode = node;
    }
};


struct RegisteredProduct
{
    size_t id;
    prod_bitset product;
    std::vector<std::pair<size_t,size_t>> viewAggregateProduct;
    RegisteredProduct* previous = nullptr;
    // std::pair<size_t,size_t> correspondingLoopAgg;

    bool multiplyByCount = false;
    
    bool operator==(const RegisteredProduct &other) const
    {          
        return this->product == other.product &&
            this->viewAggregateProduct == other.viewAggregateProduct &&
            this->previous == other.previous &&
            this->multiplyByCount == other.multiplyByCount;
    }
};



struct RegisteredProduct_hash
{
    size_t operator()(const RegisteredProduct &key ) const
    {
        size_t h = 0;
        h ^= std::hash<prod_bitset>()(key.product)+ 0x9e3779b9 + (h<<6) + (h>>2);
        h ^= std::hash<RegisteredProduct*>()(key.previous)+ 0x9e3779b9 + (h<<6) + (h>>2);
        h ^= std::hash<std::vector<std::pair<size_t,size_t>>>()(key.viewAggregateProduct)+
            0x9e3779b9 + (h<<6) + (h>>2);
        h ^= std::hash<bool>()(key.multiplyByCount)+0x9e3779b9 + (h<<6) + (h>>2);
        return h;
    }
};

struct RunningSumAggregate
{
    RegisteredProduct* local = nullptr;
    RunningSumAggregate* post = nullptr;

    bool operator==(const RunningSumAggregate &other) const
    {
        return this->local == other.local && this->post == other.post;
    }
};

struct RunningSumAggregate_hash
{
    size_t operator()(const RunningSumAggregate &key ) const
    {
        size_t h = 0;
        h ^= std::hash<RegisteredProduct*>()(key.local)+ 0x9e3779b9 + (h<<6) + (h>>2);
        h ^= std::hash<RunningSumAggregate*>()(key.post)+ 0x9e3779b9 + (h<<6) + (h>>2);
        return h;
    }
};


struct AttributeOrderNode
{
    size_t _attribute;
    
    Relation* _registerdRelation;

    // TODO: this is incoming views that are joined on - find better name
    std::vector<size_t> _joinViews;

    // TODO: This should probably be renamed to outgoingViews 
    std::vector<size_t> _registeredViews;

    std::vector<RegisteredProduct> _registeredProducts;
    std::unordered_map<RegisteredProduct, size_t, RegisteredProduct_hash>
    _registeredProductMap;

    std::vector<RunningSumAggregate> _registeredRunningSum;
    std::unordered_map<RunningSumAggregate, size_t, RunningSumAggregate_hash>
    _registeredRunningSumMap;

    // TODO: Perhaps we can store this separately (locally)
    var_bitset _coveredVariables;
    
    AttributeOrderNode(size_t attr) : _attribute(attr)  {   }
};

typedef std::vector<AttributeOrderNode> AttributeOrder;
