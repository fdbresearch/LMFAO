//--------------------------------------------------------------------
//
// Database.cpp
//
// Created on: 2 May 2019
// Author: Max
//
//--------------------------------------------------------------------

#include <fstream>
#include <sstream>

#include <GlobalParams.hpp>
#include <Logging.hpp>
#include <Database.h>

#define BUF_SIZE 1024
#define PRINT_SCHEMA

static const char TABLE_SEPARATOR_CHAR = ':';
static const char ATTRIBUTE_SEPARATOR_CHAR = ',';
static const char COMMENT_CHAR = '#';
static const char PARAMETER_SEPARATOR_CHAR = ' ';

Database::Database()
{
    loadSchema();
    loadCatalog();
}

Database::~Database()
{
    /* TODO: Determine what needs to be deleted. */
}

size_t Database::numberOfAttributes()
{
    return _attributes.size();
}

size_t Database::numberOfRelations()
{
    return _relations.size();
}

Relation* Database::getRelation(size_t relationID)
{
    return _relations[relationID];
}

Attribute* Database::getAttribute(size_t attID)
{
    if (attID > _attributes.size())
    {
        ERROR(attID << " is not a valid Attribute ID!\n");
        exit(1);
    }
    
    return _attributes[attID];
}

size_t Database::getAttributeIndex(const std::string& name)
{
    auto it = _attributeMap.find(name);
    if (it != _attributeMap.end())
        return it->second;

    return -1;
}

size_t Database::getRelationIndex(const std::string& name)
{
    auto it = _relationsMap.find(name);
    if (it != _relationsMap.end())
        return it->second;

    return -1;
}

void Database::addAttribute(
    const std::string name, const Type t,
    std::pair<bool,double> constant, const size_t relID)
{
    size_t attID = _attributes.size();

    Attribute *attr;
    
    if (constant.first) 
        attr = new Attribute(name, attID, t, constant.second);
    else
        attr = new Attribute(name, attID, t);
    
    _attributes.push_back(attr);
    _attributeMap[name] = attID;
    
    _relations[relID]->_schema.push_back(attr);
    _relations[relID]->_schemaMask.set(attID);
}

void Database::loadSchema()
{
    /* Load the TreeDecomposition config file into an input stream. */
    std::ifstream input(multifaq::config::SCHEMA_CONF);

    if (!input)
    {
        ERROR(multifaq::config::SCHEMA_CONF+" does not exist. \n");
        exit(1);
    }

    DINFO("Loading schema from file: " << multifaq::config::SCHEMA_CONF << " ... ");

    /* String and associated stream to receive lines from the file. */
    std::string line;
    std::stringstream ssLine;
    
    /* Scan through the input stream line by line. */
    while (getline(input, line))
    {        
        if (line[0] == COMMENT_CHAR || line.empty())
            continue;
        
        ssLine << line;

        std::string relationName;
        size_t relationID = _relations.size();
        
        getline(ssLine, relationName, TABLE_SEPARATOR_CHAR);

        _relations.push_back(new Relation(relationName, relationID));
        _relationsMap[relationName] = relationID;

        std::string attributes, attrName;
        getline(ssLine, attributes, PARAMETER_SEPARATOR_CHAR);

        /* Clear string stream. */
        ssLine.clear();

        ssLine << attributes;

        while (getline(ssLine, attrName, ATTRIBUTE_SEPARATOR_CHAR))
        {
            Attribute* attribute = nullptr;

            auto it = _attributeMap.find(attrName);
            if (it != _attributeMap.end())
            {
                attribute = _attributes[it->second];
                _joinAttributeMask.set(it->second);
            }
            else
            {
                size_t attrID = _attributes.size();
                attribute = new Attribute(attrName, attrID);

                _attributes.push_back(attribute);
                _attributeMap[attrName] = attrID;           
            }
            
            _relations[relationID]->_schema.push_back(attribute);            
        }
        
        /* Clear string stream. */
        ssLine.clear();
        
#ifdef PRINT_SCHEMA        
        DINFO(relationID << " : " << relationName << " ( " << attributes << " )\n");
#endif
    }

    for (size_t a = 0; a < numberOfAttributes(); ++a)
        std::cout << _attributes[a]->_name << std::endl;

    DINFO("Loading schema from file: " << multifaq::config::SCHEMA_CONF << " ... ");

}

void Database::loadCatalog()
{
        /* Load the TreeDecomposition config file into an input stream. */
    std::ifstream input(multifaq::config::CATALOG_CONF);

    if (!input)
    {
        ERROR(multifaq::config::CATALOG_CONF+" does not exist. "<<
              "No metadata is loaded.\n");
        return;
    }

    DINFO("Loading catalog from file: " << multifaq::config::CATALOG_CONF << " ... ");

    /* String and associated stream to receive lines from the file. */
    std::string line;
    std::stringstream ssLine;
    
    /* Scan through the input stream line by line. */
    while (getline(input, line))
    {        
        if (line[0] == COMMENT_CHAR || line.empty())
            continue;
        
        ssLine << line;

        std::string relationName;
        size_t relationID = _relations.size();
        
        getline(ssLine, relationName, TABLE_SEPARATOR_CHAR);

        _relations.push_back(new Relation(relationName, relationID));
        _relationsMap[relationName] = relationID;

        std::string attributes, attrName;
        getline(ssLine, attributes, PARAMETER_SEPARATOR_CHAR);

        /* Clear string stream. */
        ssLine.clear();

        ssLine << attributes;

        while (getline(ssLine, attrName, ATTRIBUTE_SEPARATOR_CHAR))
        {
            Attribute* attribute = nullptr;

            auto it = _attributeMap.find(attrName);
            if (it != _attributeMap.end())
            {
                attribute = _attributes[it->second];
            }
            else
            {
                size_t attrID = _attributes.size();
                attribute = new Attribute(attrName, attrID);

                _attributes.push_back(attribute);
                _attributeMap[attrName] = attrID;           
            }
            
            _relations[relationID]->_schema.push_back(attribute);
            _relations[relationID]->_schemaMask.set(attribute->_id);
        }
        
        /* Clear string stream. */
        ssLine.clear();
        
#ifdef PRINT_SCHEMA        
        DINFO(relationID << " : " << relationName << " ( " << attributes << " )\n");
#endif
    }

    for (size_t a = 0; a < numberOfAttributes(); ++a)
        std::cout << _attributes[a]->_name << std::endl;
    
}

