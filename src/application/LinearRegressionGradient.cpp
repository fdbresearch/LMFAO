//--------------------------------------------------------------------
//
// LinearRegression.cpp
//
// Created on: Nov 27, 2017
// Author: Max
//
//--------------------------------------------------------------------

#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <fstream>

#include <Launcher.h>
#include <LinearRegression.h>

static const char COMMENT_CHAR = '#';
static const char NUMBER_SEPARATOR_CHAR = ',';
// static const char PARAMETER_SEPARATOR_CHAR = ' ';
// static const char ATTRIBUTE_SEPARATOR_CHAR = ',';
static const char ATTRIBUTE_NAME_CHAR = ':';


using namespace std;
using namespace multifaq::params;
namespace phoenix = boost::phoenix;
using namespace boost::spirit;


LinearRegression::LinearRegression(const string& pathToFiles,
                                   shared_ptr<Launcher> launcher) :
    _pathToFiles(pathToFiles)
{
    _compiler = launcher->getCompiler();
    _td = launcher->getTreeDecomposition();
}

LinearRegression::~LinearRegression()
{
    delete[] _queryRootIndex;
}

void LinearRegression::run()
{
    loadFeatures();
    modelToQueries();
    _compiler->compile();
}

// var_bitset LinearRegression::getFeatures()
// {
//     cout << _features << endl;
    
//     return _features;
// }

var_bitset LinearRegression::getCategoricalFeatures()
{
    return _categoricalFeatures;
}


void LinearRegression::modelToQueries()
{

    size_t numberOfFunctions = 0;
    size_t featureToFunction[NUM_OF_VARIABLES],
        secondaryFeatureToFunction[NUM_OF_VARIABLES];
    
    for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
    {
        if (_features.test(var))
        {
            if (_categoricalFeatures.test(var))
            {
                // TODO: TODO: TODO: Need to set the parameter here correctly!
                double* parameter = new double[1];
                parameter[0] = 0.0;
                _compiler->addFunction(
                    new Function({var}, Operation::lr_cat_parameter, parameter));
            }
            else 
            {
                double* parameter = new double[1];
                parameter[0] = 0.0;
            
                _compiler->addFunction(
                    new Function({var}, Operation::lr_cont_parameter, parameter));
            }

            featureToFunction[var] = numberOfFunctions;
            numberOfFunctions++;
        }
    }

    size_t numberOfFeatures = _features.count();
    numberOfFunctions = 0;
    
    for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
    {
        if (_features.test(var) && !_categoricalFeatures.test(var))
        {
            _compiler->addFunction(new Function({var}, Operation::sum));
            secondaryFeatureToFunction[var] = numberOfFeatures + numberOfFunctions;
            numberOfFunctions++;
        }
    }

    for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
    {
        if (_features.test(var))
        {   
            // Aggregate* agg = new Aggregate(1);
            // agg->_m[0] = 0;
            Aggregate* agg = new Aggregate();

            for (size_t otherVar = 0; otherVar < NUM_OF_VARIABLES; ++otherVar)
            {
                if (_features.test(otherVar))
                {
                    prod_bitset product;
                    product.set(featureToFunction[otherVar]);
                    
                    if (!_categoricalFeatures.test(var))
                        product.set(secondaryFeatureToFunction[var]);
                    
                    agg->_agg.push_back(product);

                    // TODO: Make sure this is correct!?!
                    // ++agg->_m[0];
                }
            }
            
            Query* query = new Query();
            query->_aggregates.push_back(agg);

            if (_categoricalFeatures[var])
            {
                query->_fVars.set(var);
                query->_rootID = _queryRootIndex[var];
            }
            else
            {
                query->_rootID = _td->_root->_id;
                // query->_rootID = 3;
            }
            
            _compiler->addQuery(query);
        }
    }    
}


void LinearRegression::loadFeatures()
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


void LinearRegression::computeGradient()
{
    _compiler->compile();
}
