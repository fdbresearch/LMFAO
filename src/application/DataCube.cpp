//--------------------------------------------------------------------
//
// DataCube.cpp
//
// Created on: Feb 1, 2018
// Author: Max
//
//--------------------------------------------------------------------


#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <fstream>

#include <Launcher.h>
#include <DataCube.h>

static const std::string FEATURE_CONF = "/features.conf";

static const char COMMENT_CHAR = '#';
static const char NUMBER_SEPARATOR_CHAR = ',';
static const char ATTRIBUTE_NAME_CHAR = ':';

using namespace std;
using namespace multifaq::params;
namespace phoenix = boost::phoenix;
using namespace boost::spirit;

using namespace std;

DataCube::DataCube(
    const string& pathToFiles, shared_ptr<Launcher> launcher) :
    _pathToFiles(pathToFiles)
{
    _compiler = launcher->getCompiler();
    _td = launcher->getTreeDecomposition();
}

DataCube::~DataCube()
{
    if (_queryRootIndex != nullptr)
        delete[] _queryRootIndex;
}

void DataCube::run()
{
    loadFeatures();
    modelToQueries();
    _compiler->compile();
}


void DataCube::modelToQueries()
{
    _compiler->addFunction(
        new Function({_labelID}, Operation::sum));

    Aggregate* agg = new Aggregate();
    prod_bitset product;
    product.set(0);
    agg->_agg.push_back(product);
    
    Query* query = new Query();
    query->_rootID = _td->_root->_id;
    
    query->_aggregates.push_back(agg);
    _compiler->addQuery(query);

    std::vector<var_bitset> pathNodes(_td->numberOfRelations());

    for (size_t rel = 0; rel < _td->numberOfRelations(); ++rel)
    {
        TDNode* node = _td->getRelation(rel);
        pathNodes[rel].set(node->_id);

        while (node->_parent != nullptr)
        {
            node = node->_parent;
            pathNodes[rel].set(node->_id);
        }
    }
    
    std::vector<size_t> listOfCatFeatures;
    for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
    {
        if (_isCategoricalFeature[var])
            listOfCatFeatures.push_back(var);
    }

    size_t pow_set_size = pow(2,listOfCatFeatures.size());

    // std::vector<size_t> variables(_td->numberOfRelations());
    
    size_t counter, j;
    for(counter = 0; counter < pow_set_size; counter++) 
    {
        Aggregate* agg = new Aggregate();
        prod_bitset product;
        product.set(0);
        agg->_agg.push_back(product);

        Query* query = new Query();
        query->_aggregates.push_back(agg);
        query->_rootID = _td->_root->_id;

        for(j = 0; j < listOfCatFeatures.size(); j++) 
        {
            
            if(counter & (1<<j))
            {
                query->_fVars.set(listOfCatFeatures[j]);
                // variables[_queryRootIndex[listOfCatFeatures[j]]]++;
                // query->_rootID *= _queryRootIndex[listOfCatFeatures[j]];
                printf("%lu ", listOfCatFeatures[j]);
            }
        }
        printf("\n");

        // size_t maxFvarsInBag = 0;
        // std::vector<size_t> rootNode;
        // for (size_t rel = 0; rel < _td->numberOfRelations(); ++rel)
        // {
        //     if (variables[rel] > maxFvarsInBag)
        //     {
        //         rootNode.clear();
        //         rootNode.push_back(rel);
        //         maxFvarsInBag = variables[rel];
        //     }
        //     if (variables[rel] > 0 &&  variables[rel] == maxFvarsInBag)
        //     {
        //         rootNode.push_back(rel);
        //     }
        // }
        
        // if (rootNode.size() > 1)
        // {
        //     var_bitset common_path = pathNodes[rootNode[0]];
        //     for (size_t rel : rootNode)
        //     {
        //         common_path |= pathNodes[rootNode[rel]];
        //     }
            
        //     for (size_t rel = 0; rel < _qc->numberOfRelations(); ++rel)
        //     {
        //         if (common_path[rel])
        //         {
        //             rootNode[0] = rel;
        //             break;
        //         }
        //     }
        // }
        // query->_rootID = rootNode[0];
        
        _compiler->addQuery(query);
        // variables.clear();
    }
}


void DataCube::loadFeatures()
{
    _queryRootIndex = new size_t[NUM_OF_VARIABLES]();
    
    /* Load the two-pass variables config file into an input stream. */
    ifstream input(_pathToFiles + FEATURE_CONF);

    if (!input)
    {
        ERROR(_pathToFiles + FEATURE_CONF+" does not exist. \n");
        exit(1);
    }

    /* String and associated stream to receive lines from the file. */
    string line;
    stringstream ssLine;

    int numOfFeatures = 0;
    int degreeOfInteractions = 0;
    
    /* Ignore comment and empty lines at the top */
    while (getline(input, line))
    {
        if (line[0] == COMMENT_CHAR || line == "")
            continue;

        break;
    }
    
    /* 
     * Extract number of labels, features and interactions from the config. 
     * Parse the line with the three numbers; ignore spaces. 
     */
    bool parsingSuccess =
        qi::phrase_parse(line.begin(), line.end(),
                         /* Begin Boost Spirit grammar. */
                         ((qi::int_[phoenix::ref(numOfFeatures) = qi::_1])
                          >> NUMBER_SEPARATOR_CHAR
                          >> (qi::int_[phoenix::ref(degreeOfInteractions) =
                                       qi::_1])),
                         /*  End grammar. */
                         ascii::space);

    assert(parsingSuccess && "The parsing of the features file has failed.");
    
    /* Read in the features. */
    for (int featureNo = 0; featureNo < numOfFeatures; ++featureNo)
    {
        getline(input, line);
        if (line[0] == COMMENT_CHAR || line == "")
        {
            --featureNo;
            continue;
        }

        ssLine << line;
 
        string attrName;
        /* Extract the name of the attribute in the current line. */
        getline(ssLine, attrName, ATTRIBUTE_NAME_CHAR);

        string typeOfFeature;
        /* Extract the dimension of the current attribute. */
        getline(ssLine, typeOfFeature, ATTRIBUTE_NAME_CHAR);

        string rootName;
        /* Extract the dimension of the current attribute. */
        getline(ssLine, rootName, ATTRIBUTE_NAME_CHAR);

        int attributeID = _td->getAttributeIndex(attrName);
        int categorical = stoi(typeOfFeature); 
        int rootID = _td->getRelationIndex(rootName);

        if (attributeID == -1)
        {
            ERROR("Attribute |"+attrName+"| does not exist.");
            exit(1);
        }
        if (rootID == -1)
        {
            ERROR("Relation |"+rootName+"| does not exist.");
            exit(1);
        }
        if (featureNo == 0)
        {
            _labelID = attributeID;

            if (categorical)
            {
                ERROR("The Measure of a DataCube should be continuous!\n");
                exit(1);
            }
        }

        _isFeature.set(attributeID);
        _features.set(attributeID);

        if (categorical)
        {
            _queryRootIndex[attributeID] = rootID;
            _isCategoricalFeature.set(attributeID);
        }
        else
        {
            _queryRootIndex[attributeID] = _td->_root->_id;
        }

        /* Clear string stream. */
        ssLine.clear();
    }
}
