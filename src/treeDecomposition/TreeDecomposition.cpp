//--------------------------------------------------------------------
//
// TreeDecomposition.cpp
//
// Created on: 12/09/2017
// Author: Max
//
//--------------------------------------------------------------------

#include <fstream>
#include <sstream>

#include <Logging.hpp>
#include <TreeDecomposition.h>

#define BUF_SIZE 1024
// #define PRINT_TD

static const char NAME_SEPARATOR_CHAR = ':';
static const char ATTRIBUTE_SEPARATOR_CHAR = ',';
static const char COMMENT_CHAR = '#';
static const char PARAMETER_SEPARATOR_CHAR = ' ';
static const char EDGE_SEPARATOR_CHAR = '-';

TreeDecomposition::TreeDecomposition()
{
    buildFromFile();
}

TreeDecomposition::~TreeDecomposition()
{
    /* TODO: Determine what needs to be deleted. */
}

size_t TreeDecomposition::numberOfAttributes()
{
    return _numOfAttributes;
}

size_t TreeDecomposition::numberOfRelations()
{
    return _numOfRelations;
}

TDNode* TreeDecomposition::getRelation(size_t relationID)
{
    return _relations[relationID];
}

Attribute* TreeDecomposition::getAttribute(size_t attID)
{
    if (attID > _attributes.size())
    {
        ERROR(attID << " is not a valid Attribute ID!\n");
        exit(1);
    }
    
    return _attributes[attID];
}

size_t TreeDecomposition::getAttributeIndex(const std::string& name)
{
    auto it = _attributeMap.find(name);
    if (it != _attributeMap.end())
        return it->second;

    return -1;
}

size_t TreeDecomposition::getRelationIndex(const std::string& name)
{
    auto it = _relationsMap.find(name);
    if (it != _relationsMap.end())
        return it->second;

    return -1;
}

void TreeDecomposition::addAttribute(
    const std::string name, const Type t,
    std::pair<bool,double> constant, const size_t relID)
{
    size_t attID = _attributes.size();

    if (constant.first) 
        _attributes.push_back(new Attribute(name, attID, t, constant.second));
    else
        _attributes.push_back(new Attribute(name, attID, t));
    
    _attributeMap[name] = attID;

    ++_numOfAttributes;

    TDNode* rel = getRelation(relID);
    
    rel->_bag.set(attID);
    
    updateNeighborSchema(rel, relID, attID);
}

void TreeDecomposition::buildFromFile()
{
    /* Load the TreeDecomposition config file into an input stream. */
    std::ifstream input(multifaq::config::TREEDECOMP_CONF);

    if (!input)
    {
        ERROR(multifaq::config::TREEDECOMP_CONF+" does not exist. \n");
        exit(1);
    }

    DINFO("Building the TD from file: " << multifaq::config::TREEDECOMP_CONF << " ... ");
   
    /* Number of attributes and tables. */
    size_t n, m, e;

    /* String and associated stream to receive lines from the file. */
    std::string line;
    std::stringstream ssLine;

    /* String to receive the attributes from the relation. */
    std::string attribute;

    /* Read all the attributes - name and value - and parent and key. */
    std::string name;
    std::string type;
    std::string attrs;
    std::string index;
    
    /* Skmaip any comments at the top */
    while (getline(input, line))
    {
        if (line[0] == COMMENT_CHAR || line == "")
            continue;
    
        break;
    }

    /* Extract number of attributes and tables. */
    ssLine << line;

    std::string nString, mString, eString;
    getline(ssLine, nString, PARAMETER_SEPARATOR_CHAR);
    getline(ssLine, mString, PARAMETER_SEPARATOR_CHAR);
    getline(ssLine, eString, PARAMETER_SEPARATOR_CHAR);
   
    ssLine.clear();

    /* Convert the strings to integers. */
    n = stoul(nString);
    m = stoul(mString);
    e = stoul(eString);

    /* Set numOfAttributes and _numOfRelations. */
    this->_numOfAttributes = n;
    this->_numOfRelations = m;
    this->_numOfEdges = e;

    this->_relations.resize(m);
    // this->_attributesInRelation.resize(m);
    this->_attributes.resize(n);
   
    for (size_t i = 0; i < n; ++i)
    {
        /* Extract node information for each attribute. */
        while (getline(input, line))
        {
            if (line[0] == COMMENT_CHAR || line == "")
                continue;

            break;
        }

        ssLine << line;

        /* Get the six parameters specified for each attribute. */
        getline(ssLine, index, PARAMETER_SEPARATOR_CHAR);
        getline(ssLine, name, PARAMETER_SEPARATOR_CHAR);
        getline(ssLine, type, PARAMETER_SEPARATOR_CHAR);

        /* Clear string stream. */
        ssLine.clear();

        if (i != stoul(index))
            ERROR("Inconsistent index specified in your DTree config file.\n");

        Type t = Type::Integer;        
        if (type.compare("double")==0) t = Type::Double;
        if (type.compare("short")==0) t = Type::Short;
        if (type.compare("uint")==0) t = Type::U_Integer;
        
        this->_attributes[i] = new Attribute(name, i, t);
        this->_attributeMap[name] = i;
    }
  
    /* Read all the node names, their parents and attributes. */
    for (size_t i = 0; i < m; ++i)
    {
        /* Extract node information for each relation. */
        while (getline(input, line))
        {
           if (line[0] == COMMENT_CHAR || line == "")
              continue;

           break;
        }

        ssLine << line;

        /* Get the three parameters specified for each relation. */
        // getline(ssLine, name, PARAMETER_SEPARATOR_CHAR);
        // getline(ssLine, parent, PARAMETER_SEPARATOR_CHAR);

        getline(ssLine, index, PARAMETER_SEPARATOR_CHAR);
        getline(ssLine, name, NAME_SEPARATOR_CHAR);
        getline(ssLine, attrs, PARAMETER_SEPARATOR_CHAR);

        if (i != stoul(index))
            ERROR("Inconsistent index specified in your DTree config file.\n");

#ifdef PRINT_TD
        DINFO( index << " " << name << " " << attrs << "\n");
#endif
        this->_relations[i] = new TDNode(name, i);
        this->_relationsMap[name] = i;

        /* Clear string stream. */
        ssLine.clear();

        ssLine << attrs;

        while (getline(ssLine, attribute, ATTRIBUTE_SEPARATOR_CHAR))
            this->_relations[i]->_bag.set(_attributeMap[attribute]);
        
        /* Clear string stream. */
        ssLine.clear();
    }

    std::string node1, node2;
    
    /* Read all the node names, their parents and attributes. */
    for (size_t i = 0; i < e; ++i)
    {
        /* Extract node information for each relation. */
        while (getline(input, line))
        {
           if (line[0] == COMMENT_CHAR || line == "")
              continue;

           break;
        }

        ssLine << line;

        getline(ssLine, node1, EDGE_SEPARATOR_CHAR);
        getline(ssLine, node2, PARAMETER_SEPARATOR_CHAR);

#ifdef PRINT_TD
        DINFO( node1 << " - " << node2 << "\n");
#endif

        int n1 = _relationsMap[node1];
        int n2 = _relationsMap[node2];

        _relations[n1]->_neighbors.push_back(n2);
        _relations[n2]->_neighbors.push_back(n1);

        ssLine.clear();
    }
    
    for (size_t i = 0; i < _numOfRelations; i++)
    {
        _relations[i]->_numOfNeighbors = _relations[i]->_neighbors.size();
        _relations[i]->_neighborSchema = new var_bitset[_relations[i]->_numOfNeighbors];
        
        if (_relations[i]->_numOfNeighbors == 1)
            _leafNodes.push_back(_relations[i]->_id);


        /* Extract node information for each relation. */
        while (getline(input, line))
        {
            if (line[0] == COMMENT_CHAR || line == "")
                continue;

            break;
        }

        ssLine << line;

        getline(ssLine, name, PARAMETER_SEPARATOR_CHAR);
        getline(ssLine, attrs, PARAMETER_SEPARATOR_CHAR);

#ifdef PRINT_TD        
        DINFO( name << " - " << attrs << "\n");
#endif
        int relID = _relationsMap[name];
        size_t threads = std::stoi(attrs);

        if (relID == -1)
            ERROR("The relation "+name+" was not found\n");
        
        if (threads > 1)
        {
            this->_relations[relID]->_threads = threads;
        }
        
        ssLine.clear();
    }
    
    // We are computing the schema!    
    this->_root = _relations[0];
    var_bitset rootSchemaBitset = _root->_bag;
    
    /* Setting neighborSchema for root node! */
    for (size_t n = 0; n < _root->_numOfNeighbors; n++)
    {
        neighborSchema(getRelation(_root->_neighbors[n]), _root->_id,
                       _root->_neighborSchema[n]);

        rootSchemaBitset |= _root->_neighborSchema[n];
    }

    /* Propagating schema down */
    for (size_t n = 0; n < _root->_numOfNeighbors; n++)
    {
        parentNeighborSchema(
            getRelation(_root->_neighbors[n]), _root->_id,
            (rootSchemaBitset & ~_root->_neighborSchema[n]) | _root->_bag);
    }
 
    // for (size_t i = 0; i < _numOfRelations; i++)
    // {
    //     printf("node: %s : ", getRelation(i)->_name.c_str());
    //     std::cout << getRelation(i)->_bag << "\n";
    //     for (size_t c = 0; c < getRelation(i)->_numOfNeighbors; c++)
    //         std::cout <<getRelation(i)->_neighborSchema[c] << " | ";
    //     std::cout << "\n";
    // }
}


void TreeDecomposition::neighborSchema(TDNode* node, size_t originID,
                                       var_bitset& schema) 
{
    schema |= node->_bag;
    
    for (size_t c = 0; c < node->_numOfNeighbors; ++c)
    {
        if (node->_neighbors[c] != originID)
        {
             
            neighborSchema(getRelation(node->_neighbors[c]), node->_id,
                           node->_neighborSchema[c]);

            schema |= node->_neighborSchema[c];
        }
    }
}

void TreeDecomposition::parentNeighborSchema(TDNode* node, size_t originID,
                                             var_bitset schema)
{
    for (size_t c = 0; c < node->_numOfNeighbors; ++c)
    {
        if (node->_neighbors[c] == originID)
            node->_neighborSchema[c] = schema;
        else
            parentNeighborSchema(getRelation(node->_neighbors[c]), node->_id,
                                 schema | node->_bag);
    }
}

// This method is used to for adding new Attributes to the TD
void TreeDecomposition::updateNeighborSchema(TDNode* node, size_t originID,
                                             size_t newAttrID)
{
    for (size_t c = 0; c < node->_numOfNeighbors; ++c)
    {        
        if (node->_neighbors[c] == originID)
            node->_neighborSchema[c].set(newAttrID);
        else
            updateNeighborSchema(getRelation(node->_neighbors[c]), node->_id, newAttrID);
    }
}

