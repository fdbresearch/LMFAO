//--------------------------------------------------------------------
//
// RegressionTree.cpp
//
// Created on: Nov 27, 2017
// Author: Max
//
//--------------------------------------------------------------------

#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <fstream>

#include <Launcher.h>
#include <RegressionTree.h>

static const std::string FEATURE_CONF = "/features.conf";

static const char COMMENT_CHAR = '#';
static const char NUMBER_SEPARATOR_CHAR = ',';
// static const char PARAMETER_SEPARATOR_CHAR = ' ';
// static const char ATTRIBUTE_SEPARATOR_CHAR = ',';
static const char ATTRIBUTE_NAME_CHAR = ':';

using namespace std;
using namespace multifaq::params;
namespace phoenix = boost::phoenix;
using namespace boost::spirit;

RegressionTree::RegressionTree(const string& pathToFiles,
                                   shared_ptr<Launcher> launcher) :
    _pathToFiles(pathToFiles)
{
    _compiler = launcher->getCompiler();
    _td = launcher->getTreeDecomposition();
}

RegressionTree::~RegressionTree()
{
}

void RegressionTree::run()
{
    cout << "Starting Regression Tree - Run() \n";

    loadFeatures();

    cout << "Loaded Feautres \n";

    computeCandidates();

    cout << "Computed Candidates \n";

    /* Create EmptyNode? */
    RegTreeNode* root = new RegTreeNode();
    root->_conditions.set(5);
    root->_conditions.set(9);
    root->_conditions.set(15);
    
    cout << "here \n";

    splitNodeQueries(root);
    _compiler->compile();


    cout << "here \n";
    

    // 1. define queries for new splits
    // 2. compile views for these queries
    // 3. compute these views / queries and opt. split
    // 4. repeat from 1
    
    /* The regression tree is iterative */
    while (!_activeNodes.empty()) 
    {
        RegTreeNode* node = _activeNodes.front();
        _activeNodes.pop();
        
        splitNodeQueries(node);
        _compiler->compile();

        /* Compute the queries */
        
        /* For each of them find minimum candidates */
        /* Then the optimal split  */
        // TODO: how do we do this?!?!

        /* split node and add them to active nodes if they satisfy the minimum
         * threshold condition */
    }
}

void RegressionTree::computeCandidates()
{
    //TODO: Hardcode Thresholds.    
    DINFO(_pathToFiles + "\n");
    DINFO(_features << "\n");

    if (_pathToFiles.compare("data/example") == 0 || _pathToFiles.compare("data/example/") == 0) 
    {
       _thresholds =
            {
                {3,4,5},
                {3,4,5},
                {3,4,5},
                {3,4,5},
                {3,4,5},
                {3,4,5}
            };
    }
    
    _numOfCandidates = new size_t[NUM_OF_VARIABLES];
    
    /* We can have a candidate mask for each feature. */
    vector<prod_bitset> candidateMask(NUM_OF_VARIABLES);
    
    // Create functions that are linear sums for y and y^2 first 
     _compiler->addFunction(
         new Function({static_cast<size_t>(_labelID)}, Operation::linear_sum));
     _compiler->addFunction(
         new Function({static_cast<size_t>(_labelID)}, Operation::quadratic_sum));

     /* CANDIDATES ARE FUNCTIONS --- so we define the functions for all
      * candidates and set them accordingly  */

     // Push them on the functionList in the order they are accessed 
    size_t idx = 2;
    for (size_t var=0; var < NUM_OF_VARIABLES; ++var)
    {
        cout << _pathToFiles + "\n";
        cout << _features[var] << " " << var << "here \n";
                
        if (_features[var])
        {
            for (size_t t = 0; t < _thresholds[var].size(); ++t)
            {                
                double* parameter = &_thresholds[var][t]; 
            
                /* Create function and add to functionList */
                //- IF VAR = CONT : indicator_lt (threshold)
                //- IF VAR = CAT : indicator_eq (threshold)
                /* single free var == var */            

                if (_categoricalFeatures[var])
                {
                    _compiler->addFunction(
                        new Function({var}, Operation::indicator_eq, parameter));
                }
                else if (_features[var])
                {
                    _compiler->addFunction(
                        new Function({var}, Operation::indicator_lt, parameter));
                }

                cout << idx << endl;

                candidateMask[var].set(idx);
                ++idx;
            }
            
            _numOfCandidates[var] = _thresholds[var].size();
            _totalNumOfCandidates +=  _numOfCandidates[var];
        }
    }

    /* Now we also add the complement functions! */
    for (size_t var=0; var < NUM_OF_VARIABLES; ++var)
    {
        if (_features[var])
        {
        
            for (size_t t = 0; t < _thresholds[var].size(); ++t)
            {
                double* parameter = &_thresholds[var][t];
            
                if (_categoricalFeatures[var])
                {
                    _compiler->addFunction(
                        new Function({var}, Operation::indicator_neq, parameter));
                }
                else if (_features[var])
                {
                    _compiler->addFunction(
                        new Function({var}, Operation::indicator_gt, parameter));
                }

                cout << idx << endl;
            
                candidateMask[var].set(idx);
                ++idx;
            }
        }
    }
}

void RegressionTree::splitNodeQueries(RegTreeNode* node)
{   
    for (size_t var=0; var < NUM_OF_VARIABLES; ++var)
    {
        Aggregate* aggC = new Aggregate(1);
        Aggregate* aggL = new Aggregate(1);
        Aggregate* aggQ = new Aggregate(1);
        
        /* Q_C */
        aggC->_agg.push_back(node->_conditions);
        
        /* Q_L */
        aggL->_agg.push_back(node->_conditions);
        aggL->_agg[0].set(0);
        
        /* Q_Q */
        aggQ->_agg.push_back(node->_conditions);
        aggQ->_agg[0].set(1);

        /*  One each for Q_c, Q_y, Q_q  -- probably should be in one */
        Query* query = new Query();

        // TODO: SET ROOT OF QUERY - should be any node that contains the var.
        query->_rootID = _td->_root->_id;
        query->_aggregates = {aggC,aggL,aggQ};
        query->_rootID = _td->_root->_id;
        
        _compiler->addQuery(query);
    }
}

void RegressionTree::loadFeatures()
{
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

    
    /* Ignore comment and empty lines at the top */
    while (getline(input, line))
    {
        if (line[0] == COMMENT_CHAR || line == "")
            continue;

        break;
    }
    
    ssLine << line;


    string labelName;
    /* Extract the name of the attribute in the current line. */
    getline(ssLine, labelName, ATTRIBUTE_NAME_CHAR);

    string typeOfLabel;
    /* Extract the dimension of the current attribute. */
    getline(ssLine, typeOfLabel);

    if (stoi(typeOfLabel) != 0)
    {
        ERROR("The label needs to be continuous! ");
        exit(1);
    }    

    _labelID = _td->getAttributeIndex(labelName);

    /* Clear string stream. */
    ssLine.clear();
    
    /* Read in the features. */
    for (int featureNo = 1; featureNo < numOfFeatures; ++featureNo)
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
        getline(ssLine, typeOfFeature);

        int attributeID = _td->getAttributeIndex(attrName);
        bool categorical = stoi(typeOfFeature);

        if (attributeID == -1)
        {
            ERROR("Attribute |"+attrName+"| does not exist.");
            exit(1);
        }

        if (featureNo == 0 && categorical == 1)
        {
            ERROR("The label needs to be continuous! ");
            exit(1);
        }

        _features.set(attributeID);

        if (categorical)
        {
            _categoricalFeatures.set(attributeID);
        }
        
        /* Clear string stream. */
        ssLine.clear();
    }

    cout << _features << "\n";
    cout << _categoricalFeatures << "\n";
}
