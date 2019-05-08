//--------------------------------------------------------------------
//
// DataHandler.h
//
// Created on: 2 May 2019
// Author: Max
//
//--------------------------------------------------------------------

#ifndef INCLUDE_DATABASE_H_
#define INCLUDE_DATABASE_H_

#include <bitset>
#include <string>
#include <unordered_map>
#include <vector>

#include <GlobalParams.hpp>

typedef std::bitset<multifaq::params::NUM_OF_VARIABLES> var_bitset;
typedef std::bitset<multifaq::params::NUM_OF_FUNCTIONS> prod_bitset;

enum class Type
{
    Integer,
    Double,
    Short,
    U_Integer
};

struct Attribute
{
    //! ID of the attirbute    
    int _id;

    //! Name of the attribute
    std::string _name;

    //! Type of the attribute 
    Type _type;

    //! Default value for this attribute
    double _default;

    //! Flag to determine if this attribute is a constant
    bool _isConstant;

    size_t _domainSize;
    
    //! Flag to determine if this attribute is a constant
    // std::pair<bool,double> _constant = {0, 0.0}; // TODO: REMOVE

    Attribute(std::string n, int i, Type t=Type::Double, double d=0.0, bool c=false) :
        _id(i), _name(n), _type(t), _default(d), _isConstant(c)
    {    
    }
};

struct Relation
{
    //! Name of the relation (relation name).
    std::string _name = "";

    //! ID of the relation.
    size_t _id;

    //! Number of tuples for this relations 
    size_t _numOfTuples = 0;

    //! Vector of attributes in this relation 
    std::vector<Attribute*> _schema;

    //! Vector of attributes in this relation 
    var_bitset _schemaMask;

    //! Number of threads for this relations 
    size_t _threads = 1;
    
    
    Relation(std::string n, size_t i) :
        _name(n), _id(i)
    {
    }
};


/**
 * Class that represents a Factorisation Tree. 
 */
class Database
{

public:

    Database();

    ~Database();
    
    /**
     * Returns the number of attributes.
     */
    size_t numberOfAttributes();

    /**
     * Returns the number of relations.
     */
    size_t numberOfRelations();
    
    /**
     * Get pointer to the relation for given relation ID. 
     */ 
    Relation* getRelation(size_t relationID);

    /**
     * Get relation index for a given relation name.
     */ 
    size_t getRelationIndex(const std::string& name);

    /**
     * Get pointer to the attribute for given attribute ID. 
     */ 
    Attribute* getAttribute(size_t attID);

    /**
     * Get attribute index for a given attribute name.
     */ 
    size_t getAttributeIndex(const std::string& name);
    
    /**
     * Add an attribute to a given relation. 
     */ 
    void addAttribute(const std::string name, const Type t,
                      std::pair<bool,double> constantValue, const size_t relID);

private:

    //! List of attributes in the Database
    std::vector<Attribute*> _attributes;

    //! List of relations in the Tree Decomposition 
    std::vector<Relation*> _relations;

    //! Mapping from Attribute Name to Attribute ID
    std::unordered_map<std::string, size_t> _attributeMap;

    //! Mapping from Relation Name to Relation ID
    std::unordered_map<std::string, size_t> _relationsMap;
    
    //! Mask indicating join attributes - this assumes natural join
    var_bitset _joinAttributeMask;

    /**
     * Loads the Schema from a configuration file.
     */
    void loadSchema();

    /**
     * Loads the Catalog from a configuration file.
     */
    void loadCatalog();

};

#endif /* INCLUDE_DATABASE_H_ */
