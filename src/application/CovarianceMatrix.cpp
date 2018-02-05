//--------------------------------------------------------------------
//
// CovarianceMatrix.cpp
//
// Created on: Feb 1, 2018
// Author: Max
//
//--------------------------------------------------------------------

#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <fstream>

#include <Launcher.h>
#include <CovarianceMatrix.h>

static const std::string FEATURE_CONF = "/features.conf";

static const char COMMENT_CHAR = '#';
static const char NUMBER_SEPARATOR_CHAR = ',';
static const char ATTRIBUTE_NAME_CHAR = ':';

using namespace std;
using namespace multifaq::params;
namespace phoenix = boost::phoenix;
using namespace boost::spirit;

CovarianceMatrix::CovarianceMatrix(
    const string& pathToFiles, shared_ptr<Launcher> launcher) :
    _pathToFiles(pathToFiles)
{
    _compiler = launcher->getCompiler();
    _td = launcher->getTreeDecomposition();
}

CovarianceMatrix::~CovarianceMatrix()
{
    delete[] _queryRootIndex;
}

void CovarianceMatrix::run()
{
    loadFeatures();
    modelToQueries();
    _compiler->compile();
}

void CovarianceMatrix::modelToQueries()
{
    size_t numberOfFunctions = 0;
    size_t featureToFunction[NUM_OF_VARIABLES],
        secondaryFeatureToFunction[NUM_OF_VARIABLES];
    
    for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
    {
        if (_features.test(var) && !_categoricalFeatures.test(var))
        {
            _compiler->addFunction(
                new Function({var}, Operation::sum));
            featureToFunction[var] = numberOfFunctions;
            numberOfFunctions++;
        }
    }

    for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
    {
        if (_features.test(var) && !_categoricalFeatures.test(var))
        {
            _compiler->addFunction(
                new Function({var}, Operation::quadratic_sum));
            secondaryFeatureToFunction[var] = numberOfFunctions;
            numberOfFunctions++;
        }
    }

    for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
    {
        if (!_features[var])
            continue;
        
        if (!_categoricalFeatures[var])
        {
            // quadratic function ...
            Aggregate* agg = new Aggregate(1);

            prod_bitset product;
            product.set(secondaryFeatureToFunction[var]);

            agg->_agg.push_back(product);
            agg->_m[0] = 1;

            Query* quad = new Query();
            quad->_aggregates.push_back(agg);

            quad->_rootID = _td->_root->_id;
            // quad->_rootID = _queryRootIndex[var];

            _compiler->addQuery(quad);
        }

        for (size_t otherVar = var+1; otherVar < NUM_OF_VARIABLES; ++otherVar)
        {
            if (_features[otherVar])
            {
                // Create a query & Aggregate
                Query* query = new Query();
                Aggregate* agg = new Aggregate(1);

                prod_bitset product;
                
                if (_categoricalFeatures[var])
                {
                    query->_fVars.set(var);
                    query->_rootID = _queryRootIndex[var];
                }
                else
                {
                    product.set(featureToFunction[var]);
                    query->_rootID = _td->_root->_id;
                }
                
                if (_categoricalFeatures[otherVar])
                {
                    query->_fVars.set(otherVar);

                    // If both varaibles are categoricalVars - we choose the
                    // otherVar as the root 
                    query->_rootID = _queryRootIndex[otherVar];
                }
                else
                {
                    // create product of both var and otherVar
                    product.set(featureToFunction[otherVar]);                    
                }
                agg->_agg.push_back(product);
                agg->_m[0] = 1;
                
                query->_aggregates.push_back(agg);
                _compiler->addQuery(query);
            }
        }
    }
}

void CovarianceMatrix::loadFeatures()
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
    
    // std::vector<int> linearContinuousFeatures;
    // std::vector<int> linearCategoricalFeatures;

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

        if (featureNo == 0 && categorical == 1)
        {
            ERROR("The label needs to be continuous! ");
            exit(1);
        }

        _features.set(attributeID);

        if (categorical)
            _categoricalFeatures.set(attributeID);

        _queryRootIndex[attributeID] = rootID;
        
        /* Clear string stream. */
        ssLine.clear();
    }
}
