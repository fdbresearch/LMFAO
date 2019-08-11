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

#include <bitset>
#include <string>
#include <unordered_map>
#include <vector>

#include <Database.h>
#include <GlobalParams.hpp>

struct TDNode 
{  
    TDNode(Relation* rel) : _relation(rel), _id(rel->_id)
    {     }
    
    ~TDNode()
    {    }
    
    //! Pointer to relation that this node represents
    Relation* _relation = nullptr;

    size_t _id; // TODO: this should be removed!!
    
    //! Array of neighbors.
    std::vector<size_t> _neighbors;
    
    //! Bitset that indicates schema of subtree root at neighbors 
    std::vector<var_bitset> _neighborSchema;
};


/**
 * Class that represents a Factorisation Tree. 
 */
class TreeDecomposition
{

public:

    TreeDecomposition(std::shared_ptr<Database> db);

    ~TreeDecomposition();

    //! Pointer to root of the tree decomposition.
    TDNode* _root;

    // /**
    //  * Returns the number of attributes.
    //  */
    // size_t numberOfAttributes();
    // /**
    //  * Returns the number of relations.
    //  */
    // size_t numberOfRelations();
    
    //! Vector storing IDs of leaves in the TD.    
    std::vector<size_t> _leafNodes;

    /**
     * Builds the DTree from a configuration file.
     */
    void buildTreeDecompositionFromFile();

    /**
     * Get pointer to treedecomposition node from relation id.
     */
    TDNode* getTDNode(size_t relID);

    
    /**
     * Get pointer to treedecomposition node from relation id.
     */
    std::vector<size_t>& getJoinKeyOrder();

    // Attribute* getAttribute(size_t attID);
    // size_t getAttributeIndex(const std::string& name);
    // size_t getRelationIndex(const std::string& name);
    // void addAttribute(const std::string name, const Type t,
    //                   std::pair<bool,double> constantValue, const size_t relID);
    
private:

    /* Pointer to the database which stores the schema and catalog*/
    std::shared_ptr<Database> _db;

    // //! Number of attributes in the database.
    // size_t _numOfAttributes;

    // //! Number of relations in the tree decomposition.
    // size_t _numOfRelations;

    std::vector<TDNode*> _nodes;
    
    size_t* _relationToNodeMapping = nullptr;

    //! Number of edges in the tree decomposition. 
    size_t _numOfEdges;
    
    std::vector<size_t> _joinKeyOrder;

    // //! List of attributes in the Database
    // std::vector<Attribute*> _attributes;

    // //! List of relations in the Tree Decomposition 
    // std::vector<TDNode*> _relations;

    // //! Mapping from Attribute Name to Attribute ID
    // std::unordered_map<std::string, size_t> _attributeMap;

    // //! Mapping from Node Name to Node ID
    // std::unordered_map<std::string, size_t> _relationsMap;

    //! Vector storing the IDs for each attribute in each table.
    // std::vector<var_bitset> _attributesInRelation;
    //TODO: This could be taken from the bag bitsets - thus redundant 
    
    void neighborSchema(TDNode* node, size_t originID, var_bitset& schema);

    void parentNeighborSchema(TDNode* node, size_t originID, var_bitset schema);

    void updateNeighborSchema(TDNode* node, size_t originID, size_t newAttrID);
    
    void computeVariableOrder();
};

#endif /* INCLUDE_TREEDECOMPOSITION_H_ */
