//--------------------------------------------------------------------
//
// MutualInformation.cpp
//
// Created on: Oct 15, 2018
// Author: Max
//
//--------------------------------------------------------------------


#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <fstream>

#include <Launcher.h>
#include <MutualInformation.h>

static const char COMMENT_CHAR = '#';
static const char NUMBER_SEPARATOR_CHAR = ',';
static const char ATTRIBUTE_NAME_CHAR = ':';

using namespace std;

using namespace multifaq::params;
using namespace multifaq::config;
using namespace multifaq::dir;

namespace phoenix = boost::phoenix;
using namespace boost::spirit;

using namespace std;

MutualInformation::MutualInformation(shared_ptr<Launcher> launcher)
{
    _compiler = launcher->getCompiler();
    _td = launcher->getTreeDecomposition();
    _db = launcher->getDatabase();
}

MutualInformation::~MutualInformation()
{
}

void MutualInformation::run()
{
    loadFeatures();
    modelToQueries();
    _compiler->compile();
}

void MutualInformation::generateCode()
{}


void MutualInformation::modelToQueries()
{
    Aggregate* countAgg = new Aggregate();
    countAgg->_sum.push_back(prod_bitset());

    Query* countQuery = new Query();
    countQuery->_aggregates.push_back(countAgg);
    countQuery->_root = _td->_root;

    _compiler->addQuery(countQuery);

                 
    for (size_t var = 0; var < NUM_OF_VARIABLES; ++var) 
    {
        if (_isCategoricalFeature[var])
        {
            for (size_t var2 = var+1; var2 < NUM_OF_VARIABLES; ++var2) 
            {
                if (_isCategoricalFeature[var2])
                {
                    Aggregate* agg = new Aggregate();
                    agg->_sum.push_back(prod_bitset());

                    // Create a query & Aggregate
                    Query* query = new Query();
                    query->_fVars.set(var);
                    query->_fVars.set(var2);
                    query->_aggregates.push_back(agg);
                    _compiler->addQuery(query);

                    // if (_queryRootIndex[var] <= _queryRootIndex[var2])
                    //     query->_rootID = _queryRootIndex[var];
                    // else
                    query->_root = _td->getTDNode(_queryRootIndex[var2]);
                }
            }

            Aggregate* agg = new Aggregate();
            agg->_sum.push_back(prod_bitset());

            // Create a query & Aggregate
            Query* query = new Query();
            query->_aggregates.push_back(agg);
            query->_root = _td->getTDNode(_queryRootIndex[var]);
            query->_fVars.set(var);

            _compiler->addQuery(query);
        }
    }
    
}


void MutualInformation::loadFeatures()
{
    _queryRootIndex = new size_t[NUM_OF_VARIABLES]();
    
    /* Load the two-pass variables config file into an input stream. */
    ifstream input(FEATURE_CONF);

    if (!input)
    {
        ERROR(FEATURE_CONF+" does not exist. \n");
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

        int attributeID = _db->getAttributeIndex(attrName);
        int categorical = stoi(typeOfFeature); 
        int rootID = _db->getRelationIndex(rootName);

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
        if (featureNo == 0 && categorical == 1)
        {
            ERROR("The label needs to be continuous! ");
            exit(1);
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
