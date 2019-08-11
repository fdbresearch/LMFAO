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

#include <GlobalParams.hpp>
#include <Logging.hpp>
#include <TreeDecomposition.h>

#define BUF_SIZE 1024
#define PRINT_TD

// static const char NAME_SEPARATOR_CHAR = ':';
static const char ATTRIBUTE_SEPARATOR_CHAR = ',';
static const char COMMENT_CHAR = '#';
static const char PARAMETER_SEPARATOR_CHAR = ' ';
static const char EDGE_SEPARATOR_CHAR = '-';

TreeDecomposition::TreeDecomposition(std::shared_ptr<Database> db) : _db(db)
{
}

TreeDecomposition::~TreeDecomposition()
{
    /* TODO: Determine what needs to be deleted. */
    delete[] _relationToNodeMapping;
}


TDNode* TreeDecomposition::getTDNode(size_t relID)
{
    return _nodes[_relationToNodeMapping[relID]];
}

std::vector<size_t>& TreeDecomposition::getJoinKeyOrder()
{
    return _joinKeyOrder;
}


void TreeDecomposition::buildTreeDecompositionFromFile()
{
    _relationToNodeMapping = new size_t[_db->numberOfRelations()];

    /* Load the TreeDecomposition config file into an input stream. */
    std::ifstream input(multifaq::config::TREEDECOMP_CONF);

    if (!input)
    {
        ERROR(multifaq::config::TREEDECOMP_CONF+" does not exist. \n");
        exit(1);
    }

    DINFO("INFO: Building the TD from file: " <<
          multifaq::config::TREEDECOMP_CONF << " ... ");

    /* String and associated stream to receive lines from the file. */
    std::string line;
    std::stringstream ssLine;

    getline(input, line);
    
    ssLine << line;

    std::string attribute;
    size_t attID;
    while (getline(ssLine, attribute, ATTRIBUTE_SEPARATOR_CHAR))
    {
        try
        {
            attID = _db->getAttributeIndex(attribute);
        }
        catch (int e)
        {
            std::cout << "An exception occurred. Exception Nr. " << e << '\n';
            exit(1);
        }
        
        _joinKeyOrder.push_back(attID);
    }
    
    ssLine.clear();

    
    _nodes.resize(_db->numberOfRelations());
    for (size_t i = 0; i < _db->numberOfRelations(); ++i)
    {
        _nodes[i] = new TDNode(_db->getRelation(i));
        _relationToNodeMapping[i] = i;
    }

    std::string node1, node2;
    
    /* Read all the node names, their parents and attributes. */
    /* Extract node information for each relation. */
    while (getline(input, line))
    {
        if (line[0] == COMMENT_CHAR || line == "")
            continue;

        ssLine << line;

        getline(ssLine, node1, EDGE_SEPARATOR_CHAR);
        getline(ssLine, node2, PARAMETER_SEPARATOR_CHAR);

#ifdef PRINT_TD
        DINFO("\n" << node1 << " - " << node2);
#endif

        size_t n1 = _db->getRelation(_db->getRelationIndex(node1))->_id;
        size_t n2 = _db->getRelation(_db->getRelationIndex(node2))->_id;

        _nodes[n1]->_neighbors.push_back(n2);
        _nodes[n2]->_neighbors.push_back(n1);

        ssLine.clear();
    }

    for (TDNode* tdNode : _nodes)
    {    
        tdNode->_neighborSchema.resize(tdNode->_neighbors.size());

        if (tdNode->_neighbors.size() == 1)
            _leafNodes.push_back(tdNode->_relation->_id);
    }
    
    
    // We are computing the schema!    
    this->_root = _nodes[0];
    var_bitset rootSchemaBitset = _root->_relation->_schemaMask;
    
    /* Setting neighborSchema for root node! */
    for (size_t n = 0; n < _root->_neighbors.size(); n++)
    {
        neighborSchema(getTDNode(_root->_neighbors[n]), _root->_id,
                       _root->_neighborSchema[n]);

        rootSchemaBitset |= _root->_neighborSchema[n];
    }

    /* Propagating schema down */
    for (size_t n = 0; n < _root->_neighbors.size(); n++)
    {
        parentNeighborSchema(
            getTDNode(_root->_neighbors[n]), _root->_id,
            (rootSchemaBitset & ~_root->_neighborSchema[n]) |
            _root->_relation->_schemaMask);
    }
 
    // for (size_t i = 0; i < _db->numberOfRelations(); i++)
    // {
    //     printf("node: %s : ", _db->getRelation(i)->_name.c_str());
    //     std::cout << _db->getRelation(i)->_schemaMask << "\n";
    //     for (size_t c = 0; c < getTDNode(i)->_neighbors.size(); c++)
    //         std::cout << getTDNode(i)->_neighborSchema[c] << " | ";
    //     std::cout << "\n";
    // }

    DINFO("... Built the TreeDecomposition.\n");

//     /* Number of attributes and tables. */
//     size_t n, m, e;
//     /* String to receive the attributes from the relation. */
//     std::string attribute;

//     /* Read all the attributes - name and value - and parent and key. */
//     std::string name;
//     std::string type;
//     std::string attrs;
//     std::string index;
    
//     /* Skmaip any comments at the top */
//     while (getline(input, line))
//     {
//         if (line[0] == COMMENT_CHAR || line == "")
//             continue;
    
//         break;
//     }

//     /* Extract number of attributes and tables. */
//     ssLine << line;

//     std::string nString, mString, eString;
//     getline(ssLine, nString, PARAMETER_SEPARATOR_CHAR);
//     getline(ssLine, mString, PARAMETER_SEPARATOR_CHAR);
//     getline(ssLine, eString, PARAMETER_SEPARATOR_CHAR);
   
//     ssLine.clear();

//     /* Convert the strings to integers. */
//     n = stoul(nString);
//     m = stoul(mString);
//     e = stoul(eString);

//     /* Set numOfAttributes and _numOfRelations. */

//     if (_db->numberOfRelations() != m)
//         ERROR("The number of relations in TD.conf and DB doesn't match.\n")
//     if (_db->numberOfAttributes() != n)
//         ERROR("The number of attributes in TD.conf and DB doesn't match.\n")

//     // this->_numOfAttributes = n;
//     // this->_numOfRelations = m;
//     this->_numOfEdges = e;

//     this->_nodes.resize(m);
//     // this->_attributesInRelation.resize(m);
//     // this->_attributes.resize(n);
   
//     for (size_t i = 0; i < n; ++i)
//     {
//         /* Extract node information for each attribute. */
//         while (getline(input, line))
//         {
//             if (line[0] == COMMENT_CHAR || line == "")
//                 continue;

//             break;
//         }

//         ssLine << line;

//         /* Get the six parameters specified for each attribute. */
//         getline(ssLine, index, PARAMETER_SEPARATOR_CHAR);
//         getline(ssLine, name, PARAMETER_SEPARATOR_CHAR);
//         getline(ssLine, type, PARAMETER_SEPARATOR_CHAR);

//         /* Clear string stream. */
//         ssLine.clear();

//         if (i != stoul(index))
//             ERROR("Inconsistent index specified in your DTree config file.\n");

//         Type t = Type::Integer;        
//         if (type.compare("double")==0) t = Type::Double;
//         if (type.compare("short")==0) t = Type::Short;
//         if (type.compare("uint")==0) t = Type::UInteger;
        
//         // this->_attributes[i] = new Attribute(name, i, t);
//         // this->_attributeMap[name] = i;

//         assert(_db->getAttributeIndex(name));
//     }
  
//     /* Read all the node names, their parents and attributes. */
//     for (size_t i = 0; i < m; ++i)
//     {
//         /* Extract node information for each relation. */
//         while (getline(input, line))
//         {
//            if (line[0] == COMMENT_CHAR || line == "")
//               continue;

//            break;
//         }

//         ssLine << line;

//         /* Get the three parameters specified for each relation. */
//         // getline(ssLine, name, PARAMETER_SEPARATOR_CHAR);
//         // getline(ssLine, parent, PARAMETER_SEPARATOR_CHAR);

//         getline(ssLine, index, PARAMETER_SEPARATOR_CHAR);
//         getline(ssLine, name, NAME_SEPARATOR_CHAR);
//         getline(ssLine, attrs, PARAMETER_SEPARATOR_CHAR);

//         if (i != stoul(index))
//             ERROR("Inconsistent index specified in your DTree config file.\n");

// #ifdef PRINT_TD
//         DINFO( index << " " << name << " " << attrs << "\n");
// #endif
//         // this->_relations[i] = new TDNode(name, i);
//         // this->_relationsMap[name] = i;

//         // assert(_db->getRelationIndex(name));

//         /* Clear string stream. */
//         ssLine.clear();
//         ssLine << attrs;

//         // while (getline(ssLine, attribute, ATTRIBUTE_SEPARATOR_CHAR))
//         //     this->_relations[i]->_bag.set(_attributeMap[attribute]);
        
         
//         /* Clear string stream. */
//         ssLine.clear();
//     }


}


void TreeDecomposition::neighborSchema(TDNode* node, size_t originID,
                                       var_bitset& schema) 
{
    schema |= node->_relation->_schemaMask;
    
    for (size_t c = 0; c < node->_neighbors.size(); ++c)
    {
        if (node->_neighbors[c] != originID)
        {    
            neighborSchema(getTDNode(node->_neighbors[c]), node->_id,
                           node->_neighborSchema[c]);

            schema |= node->_neighborSchema[c];
        }
    }
}

void TreeDecomposition::parentNeighborSchema(TDNode* node, size_t originID,
                                             var_bitset schema)
{
    for (size_t c = 0; c < node->_neighbors.size(); ++c)
    {
        if (node->_neighbors[c] == originID)
            node->_neighborSchema[c] = schema;
        else
            parentNeighborSchema(getTDNode(node->_neighbors[c]), node->_id,
                                 schema | node->_relation->_schemaMask);
    }
}

// This method is used to for adding new Attributes to the TD
void TreeDecomposition::updateNeighborSchema(TDNode* node, size_t originID,
                                             size_t newAttrID)
{
    for (size_t c = 0; c < node->_neighbors.size(); ++c)
    {        
        if (node->_neighbors[c] == originID)
            node->_neighborSchema[c].set(newAttrID);
        else
            updateNeighborSchema(getTDNode(node->_neighbors[c]), node->_id, newAttrID);
    }
}

// void TreeDecomposition::addAttribute(
//     const std::string name, const Type t,
//     std::pair<bool,double> constant, const size_t relID)
// {
//     size_t attID = _attributes.size();
//     if (constant.first) 
//         _attributes.push_back(new Attribute(name, attID, t, constant.second));
//     else
//         _attributes.push_back(new Attribute(name, attID, t));
    
//     _attributeMap[name] = attID;
//     ++_numOfAttributes;

//     TDNode* rel = getRelation(relID);
//     rel->_bag.set(attID);
    
//     updateNeighborSchema(rel, relID, attID);
// }
