#include <bitset>
#include <boost/dynamic_bitset.hpp>
#include <set>
#include <vector>

#include <CompilerUtils.hpp>
#include <GlobalParams.hpp>
#include <Logging.hpp>
#include <TreeDecomposition.h>

typedef std::bitset<multifaq::params::NUM_OF_PRODUCTS> agg_bitset;
typedef std::bitset<multifaq::params::NUM_OF_VIEWS+1> view_bitset;

typedef std::pair<size_t, size_t> ViewAggregateIndex;
typedef std::vector<ViewAggregateIndex> ViewAggregateProduct;
typedef boost::dynamic_bitset<> dyn_bitset;

struct ViewGroup;
struct RegisteredProduct;
struct RunningSumAggregate;

struct OutputAggregate;
struct OutputView;

struct LoopRegister;
struct LoopAggregate;
struct FinalLoopAggregate;

struct AttributeOrderNode;

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
        if (_parameter != nullptr)
            delete[] _parameter;
    }    
};


struct Product
{
    /* Bitset denotes a product of functions. */ 
    prod_bitset _prod;

    /* Each product can have potentially different incoming Views */
    std::vector<std::pair<size_t,size_t>> _incoming;

    Product(prod_bitset p) : _prod(p) { }

    bool operator==(const Product &other) const
    {          
        return this->_prod == other._prod &&
            this->_incoming == other._incoming;
    }  
};


struct Aggregate
{
    // Each aggregate is a sum of products of functions
    std::vector<Product> _sum;
    
    // Each aggregate is a sum of products of functions
    // std::vector<prod_bitset> _agg;
    // Each product can have potentially different incoming Views 
    // std::vector<std::pair<size_t,size_t>> _incoming;
    
    Aggregate() {}
};


struct Query
{    
    TDNode* _root;
    var_bitset _fVars;

    std::vector<Aggregate*> _aggregates;

    std::vector<ViewAggregateIndex> _incoming;

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

    View (int o, int d) : _origin(o) , _destination(d) {}
};

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

    template<> struct hash<vector<Product>>
    {
        HOT inline size_t operator()(const vector<Product>& v) const
	{
            size_t seed = 0;
            hash<prod_bitset> h;
            for (Product p : v)
            {    
                seed ^= h(p._prod) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            }
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

    /**
     * Custom hash function for cache data stucture.
     */
    template<> struct hash<tuple<int,int,vector<Product>,var_bitset>>
    {
        HOT inline size_t operator()(const tuple<int,int,
                                     vector<Product>,var_bitset>& p) const
	{
            size_t seed = 0;
            hash<int> h1;
            seed ^= h1(std::get<0>(p)) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            hash<int> h2;
            seed ^= h2(std::get<1>(p)) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            hash<vector<Product>> h3;
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

/****************************** View Group *************************/

struct ViewGroup
{
    std::vector<size_t> _views;

    TDNode* _tdNode;
    bool _parallelize = false;
    size_t _threads = 1;

    size_t numberOfLocalAggregates = 0;
    size_t numberOfLoopAggregates = 0;
    size_t numberOfRunningSums = 0;
    
    
    std::vector<size_t> _incomingViews;

    ViewGroup(size_t viewID, TDNode* node)
    {
        _views.push_back(viewID);
        _tdNode = node;
    }
};

/************************ Prodcut Registration *********************/

struct RegisteredProduct
{
    prod_bitset product;
    ViewAggregateProduct viewAggregateProduct;
    bool multiplyByCount = false;
    RegisteredProduct* previous = nullptr;
    
    std::pair<LoopRegister*, FinalLoopAggregate*> correspondingLoop;

    // bool multiplyByCount = false;

    RegisteredProduct(prod_bitset prod,
                      ViewAggregateProduct viewProd,
                      RegisteredProduct* prev,
                      bool count ) :
        product(prod) , viewAggregateProduct(viewProd), multiplyByCount(count),
        previous(prev)
    {  }

    // RegisteredProduct(bool count,
    //                   RegisteredProduct* prev ) :
    //     multiplyByCount(count), previous(prev)
    // {  }

    RegisteredProduct() {  }
    
    
    bool operator==(const RegisteredProduct &other) const
    {          
        return this->product == other.product &&
            this->viewAggregateProduct == other.viewAggregateProduct &&
            this->previous == other.previous;
            //&&    this->multiplyByCount == other.multiplyByCount;
    }
};

struct RunningSumAggregate
{
    RegisteredProduct* local = nullptr;
    RunningSumAggregate* post = nullptr;

    int aggregateIndex = -1;

    RunningSumAggregate() {   };
    
    RunningSumAggregate(RegisteredProduct* l, RunningSumAggregate* p) :
        local(l), post(p) {   };
    
    bool operator==(const RunningSumAggregate &other) const
    {
        return local == other.local && post == other.post;
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


struct OutgoingView
{
    size_t viewID;
    std::vector<FinalLoopAggregate*> aggregates;
    
    OutgoingView(size_t v) : viewID(v) { }
};

/* TODO: PERHAPS WE SHOULD ADD THESE TWO ALSO TO std above*/
// struct RegisteredProduct_hash
// {
//     size_t operator()(const RegisteredProduct &key ) const
//     {
//         size_t h = 0;
//         h ^= std::hash<prod_bitset>()(key.product)+ 0x9e3779b9 + (h<<6) + (h>>2);
//         h ^= std::hash<RegisteredProduct*>()(key.previous)+ 0x9e3779b9 + (h<<6) + (h>>2);
//         h ^= std::hash<std::vector<std::pair<size_t,size_t>>>()(key.viewAggregateProduct)+
//             0x9e3779b9 + (h<<6) + (h>>2);
//         // h ^= std::hash<bool>()(key.multiplyByCount)+0x9e3779b9 + (h<<6) + (h>>2);
//         return h;
//     }
// };

/************************ Loop Registration *********************/


struct LocalFunctionProduct
{
    prod_bitset product;
    int aggregateIndex = -1;

    LocalFunctionProduct(prod_bitset p) : product(p) 
    {   }
    
    bool operator==(const LocalFunctionProduct &other) const
    {
        return product == other.product;
    }
};


struct LocalViewAggregateProduct
{
    ViewAggregateProduct product;
    int aggregateIndex = -1;

    LocalViewAggregateProduct(ViewAggregateProduct p) : product(p) 
    {   }

    bool operator==(const LocalViewAggregateProduct &other) const
    {
        return product == other.product;
    }
};


struct LoopAggregate 
{
    int functionProduct = -1;
    int viewProduct = -1;

    bool multiplyByCount = false;

    LoopAggregate* prevProduct = nullptr;

    int aggregateIndex = -1;

    bool operator==(const LoopAggregate &other) const
    {
        return functionProduct == other.functionProduct &&
            viewProduct == other.viewProduct &&
            prevProduct == other.prevProduct &&
            multiplyByCount == other.multiplyByCount;
    }
};


struct FinalLoopAggregate
{
    /* Loop Aggregate from this Loop */
    LoopAggregate* loopAggregate = nullptr;

    /* Aggregate from previous attributeOrderNode */
    FinalLoopAggregate* previousProduct = nullptr;

    /* Aggregate from previous attributeOrderNode */
    FinalLoopAggregate* innerProduct = nullptr;

    /* Aggregate from previous Running Sum */
    RunningSumAggregate* previousRunSum = nullptr;

    int aggregateIndex = -1;
    
    bool operator==(const FinalLoopAggregate &other) const
    {
        return loopAggregate == other.loopAggregate &&
            innerProduct == other.innerProduct &&
            previousProduct == other.previousProduct &&
            previousRunSum == other.previousRunSum;
    }
};


struct LoopRegister
{
    size_t loopID;

    std::vector<LocalFunctionProduct> localProducts;
    std::vector<LocalViewAggregateProduct> localViewAggregateProducts;
    std::vector<LoopAggregate*> loopAggregateList;
    
    std::vector<LoopRegister*> innerLoops;
    
    std::vector<FinalLoopAggregate*> finalLoopAggregates;
    std::vector<FinalLoopAggregate*> loopRunningSums;
    
    std::vector<OutgoingView> outgoingViews;

    LoopRegister(size_t id) : loopID(id) {  }

    /* Auxihilary Functions to Register Products */    
    size_t registerLocalProduct(prod_bitset& prod)
    {
        for (size_t prodID = 0; prodID < localProducts.size(); ++prodID)
        {
            if (localProducts[prodID] == prod)
                return prodID;
        }

        localProducts.push_back(prod);
        return localProducts.size()-1;
    }

    size_t registerViewProduct(ViewAggregateProduct& prod)
    {
        for (size_t prodID = 0; prodID < localViewAggregateProducts.size(); ++prodID)
        {
            if (localViewAggregateProducts[prodID] == prod)
                return prodID;
        }

        localViewAggregateProducts.push_back(prod);
        return localViewAggregateProducts.size()-1;
    }

    LoopAggregate* registerLoopAggregate(
        int functionProd, int viewProd, bool count, LoopAggregate* prev = nullptr)
    {
        for (LoopAggregate* loopAgg : loopAggregateList)
        {
            if (loopAgg->functionProduct == functionProd &&
                loopAgg->viewProduct == viewProd &&
                loopAgg->prevProduct == prev &&
                loopAgg->multiplyByCount == count) 
                return loopAgg;
        }

        LoopAggregate* loopAgg = new LoopAggregate();
        loopAgg->functionProduct = functionProd;
        loopAgg->viewProduct = viewProd;
        loopAgg->prevProduct = prev;
        loopAgg->multiplyByCount = count;

        loopAggregateList.push_back(loopAgg);
        return loopAgg;
    }


    FinalLoopAggregate* registerFinalLoopAggregate(
        LoopAggregate* local, FinalLoopAggregate* inner, FinalLoopAggregate* prev)
    {
        for (FinalLoopAggregate* loopAgg : finalLoopAggregates)
        {
            if (loopAgg->loopAggregate == local &&
                loopAgg->previousProduct == prev &&
                loopAgg->innerProduct == inner) 
                return loopAgg;
        }

        FinalLoopAggregate* loopAgg = new FinalLoopAggregate();
        loopAgg->loopAggregate = local;
        loopAgg->previousProduct = prev;
        loopAgg->innerProduct = inner;

        finalLoopAggregates.push_back(loopAgg);
        return loopAgg;
    }

    
    FinalLoopAggregate* registerLoopRunningSums(
        LoopAggregate* local, FinalLoopAggregate* inner, FinalLoopAggregate* prev)
    {
        for (FinalLoopAggregate* loopAgg : loopRunningSums)
        {
            if (loopAgg->loopAggregate == local &&
                loopAgg->previousProduct == prev &&
                loopAgg->innerProduct == inner)
                return loopAgg;
        }

        FinalLoopAggregate* loopAgg = new FinalLoopAggregate();
        loopAgg->loopAggregate = local;
        loopAgg->previousProduct = prev;
        loopAgg->innerProduct = inner;

        loopRunningSums.push_back(loopAgg);
        return loopAgg;
    }


    size_t getProduct(prod_bitset& prod) const 
    {
        for (size_t prodID = 0; prodID < localProducts.size(); ++prodID)
        {
            if (localProducts[prodID] == prod)
                return prodID;
        }

        return localProducts.size();
    }
    
};


/************************ Attribute Order *********************/

// TODO: AttributeOrderNode should be a Class with private and public fields

struct AttributeOrderNode
{
    const size_t _attribute;

    // TODO: could use this instead of sending relation pointers to different functions
    Relation* _registerdRelation;
    
    // TODO: this is incoming views that are joined on - find better name
    std::vector<size_t> _joinViews;

    std::vector<RegisteredProduct*> _registeredProducts;
    // std::unordered_map<RegisteredProduct, size_t, RegisteredProduct_hash>
    // _registeredProductMap;

    std::vector<RunningSumAggregate*> _registeredRunningSum;
    std::unordered_map<RunningSumAggregate, size_t, RunningSumAggregate_hash>
    _registeredRunningSumMap;

    LoopRegister* _loopProduct = nullptr;

    /* Outgoing Loops */
    LoopRegister* _outgoingLoopRegister = nullptr;
    std::vector<RunningSumAggregate*> _registeredOutgoingRunningSum;
    
    // TODO: Perhaps we can store this separately (locally)
    var_bitset _coveredVariables;
    view_bitset _coveredIncomingViews;
    
    AttributeOrderNode(size_t attr) : _attribute(attr)
    {    }

    RegisteredProduct* registerProduct(
        prod_bitset& prodFunctions, ViewAggregateProduct& viewAgg,
        RegisteredProduct* prevProduct, bool multiplyByCount)
    {
        // TODO [CONSIDER]: we could use the map for registration! 

        // Check if this product already exists
        for (RegisteredProduct* p : _registeredProducts)
        {
            if (p->product == prodFunctions &&
                p->viewAggregateProduct == viewAgg &&
                p->previous == prevProduct &&
                p->multiplyByCount == multiplyByCount)
            {
                return p;
            }
        }

        RegisteredProduct* newProduct =
            new RegisteredProduct(prodFunctions,viewAgg,prevProduct,multiplyByCount);
        
        _registeredProducts.push_back(newProduct);

        return newProduct;
    }

    // RegisteredProduct* registerCount(RegisteredProduct* prevProduct)
    // {
    //     for (RegisteredProduct* p : _registeredProducts)
    //     {
    //         if (p->product.none() &&
    //             p->previous == prevProduct)
    //         {
    //             return p;
    //         }
    //     }
    //     RegisteredProduct* newProduct =
    //         new RegisteredProduct(true, prevProduct);
    //     _registeredProducts.push_back(newProduct);
    //     return newProduct;
    // };
};

typedef std::vector<AttributeOrderNode> AttributeOrder;

