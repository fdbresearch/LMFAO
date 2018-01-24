//--------------------------------------------------------------------
//
// TreeDecomposition.h
//
// Created on: 12/09/2017
// Author: Max
//
//--------------------------------------------------------------------

#ifndef INCLUDE_TREEDECOMPOSITION_H_
#define INCLUDE_TREEDECOMPOSITION_H_

#include <string>
#include <unordered_map>
#include <vector>

#include <TDNode.hpp>

typedef std::bitset<multifaq::params::NUM_OF_VARIABLES> var_bitset;
typedef std::bitset<multifaq::params::NUM_OF_FUNCTIONS> prod_bitset;


enum Type
{
    Integer,
    Double,
    Short,
    U_Integer
};    

struct Attribute
{
    int _id;
    std::string _name;
    Type _type;

    Attribute(std::string n, int i) :
        _id(i), _name(n)
    {
    }

    Attribute(std::string n, int i, Type t) :
        _id(i), _name(n), _type(t)
    {    
    }
};

/**
 * Class that represents a Factorisation Tree. 
 */
class TreeDecomposition
{

public:

    TreeDecomposition(std::string filename);

    ~TreeDecomposition();

    //! Pointer to root of the tree decomposition.
    TDNode* _root;

    /**
     * Returns the number of attributes.
     */
    size_t numberOfAttributes();

    /**
     * Returns the number of relations.
     */
    size_t numberOfRelations();
    
    //! Vector storing IDs of leaves in the TD.    
    std::vector<size_t> _leafNodes;

    /**
     * Builds the DTree from a configuration file.
     */
    void buildFromFile(std::string fileName);

    /**
     * Get pointer to node for given node ID. 
     */
    TDNode* getRelation(int relationID);

    Attribute* getAttribute(int attID);

    size_t getAttributeIndex(const std::string& name);

    size_t getRelationIndex(const std::string& name);

//    const std::vector<size_t>& getLeafNodes();
    
private:

    //! Number of attributes in the database.
    size_t _numOfAttributes;

    //! Number of relations in the tree decomposition.
    size_t _numOfRelations;

    //! Number of edges in the tree decomposition. 
    size_t _numOfEdges;

    //! List of attributes in the Database
    std::vector<Attribute*> _attributes;

    //! List of relations in the Tree Decomposition 
    std::vector<TDNode*> _relations;

    //! Mapping from Attribute Name to Attribute ID
    std::unordered_map<std::string, size_t> _attributeMap;

    //! Mapping from Node Name to Node ID
    std::unordered_map<std::string, size_t> _relationsMap;

    //! Vector storing the IDs for each attribute in each table.
    std::vector<var_bitset> _attributesInRelation;
    //TODO: This could be taken from the bag bitsets - thus redundant 
    
    void neighborSchema(TDNode* node, size_t originID, var_bitset& schema);

    void parentNeighborSchema(TDNode* node, size_t originID, var_bitset schema);
};

#endif /* INCLUDE_TREEDECOMPOSITION_H_ */
