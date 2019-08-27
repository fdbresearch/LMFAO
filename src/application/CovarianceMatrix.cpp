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

static const char COMMENT_CHAR = '#';
static const char NUMBER_SEPARATOR_CHAR = ',';
static const char ATTRIBUTE_NAME_CHAR = ':';

using namespace std;

using namespace multifaq::params;
using namespace multifaq::dir;
using namespace multifaq::config;

namespace phoenix = boost::phoenix;
using namespace boost::spirit;

CovarianceMatrix::CovarianceMatrix(shared_ptr<Launcher> launcher)
{
    _db = launcher->getDatabase();
    _td = launcher->getTreeDecomposition();
    _compiler = launcher->getCompiler();
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
        quadFeatureToFunction[NUM_OF_VARIABLES];
    
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
            quadFeatureToFunction[var] = numberOfFunctions;
            numberOfFunctions++;
        }
    }

    Aggregate* agg_count = new Aggregate();
    agg_count->_sum.push_back(prod_bitset());
            
    Query* count = new Query();
    count->_aggregates.push_back(agg_count);
    count->_root = _td->_root;
    _compiler->addQuery(count);
    listOfQueries.push_back(count);    
    
    size_t numberOfQueries = 0;
    TDNode* cont_var_root = _td->_root;

    for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
    {
        if (!_features[var])
            continue;
        
        prod_bitset prod_linear_v1;
        prod_bitset prod_quad_v1;

        if (!_categoricalFeatures[var])
        {
            // Linear function per Feature 
            prod_linear_v1.set(featureToFunction[var]);
 
            Aggregate* agg_linear = new Aggregate();
            agg_linear->_sum.push_back(prod_linear_v1);
            
            Query* linear = new Query();
            linear->_aggregates.push_back(agg_linear);
            linear->_root = cont_var_root;

            _compiler->addQuery(linear);
            
            listOfQueries.push_back(linear);            
            ++numberOfQueries;
            
            // TODO: What is the queryID ?
            // TODO: what is the variable of the intercept? NUM_OF_VARS + 1?
            // TODO: We should also add this to the NUM_OF_VARS + 1 gradient for
            // the intercept multiplied by param for this var!
            
            // Quadratic function for each feature
            prod_quad_v1.set(quadFeatureToFunction[var]);
            
            Aggregate* agg_quad = new Aggregate();
            agg_quad->_sum.push_back(prod_quad_v1);
            
            Query* quad = new Query();
            quad->_aggregates.push_back(agg_quad);
            quad->_root = _td->_root; // cont_var_root;

            _compiler->addQuery(quad);
            
            listOfQueries.push_back(quad);
            ++numberOfQueries;
        }
        else
        {
            Aggregate* agg_linear = new Aggregate();
            agg_linear->_sum.push_back(prod_linear_v1);
            
            Query* linear = new Query();
            linear->_aggregates.push_back(agg_linear);
            linear->_fVars.set(var);
            linear->_root = _td->getTDNode(_queryRootIndex[var]);
            _compiler->addQuery(linear);

            listOfQueries.push_back(linear);
            ++numberOfQueries;
        }
        

        for (size_t var2 = var+1; var2 < NUM_OF_VARIABLES; ++var2)
        {
            if (_features[var2])
            {
                //*****************************************//
                prod_bitset prod_quad_v2 = prod_linear_v1;
                
                Aggregate* agg_quad_v2 = new Aggregate();
                
                Query* quad_v2 = new Query();
                quad_v2->_root = _td->_root;

                // if (_categoricalFeatures[var])
                // {
                //     quad_v2->_rootID = _queryRootIndex[var];
                //     quad_v2->_fVars.set(var);
                //     if (_categoricalFeatures[var2])
                //     {
                //         quad_v2->_fVars.set(var2);
                //         // If both varaibles are categoricalVars - we choose the
                //         // var2 as the root
                //         // TODO: We should use the root that is highest to the
                //         // original root
                //         if (_queryRootIndex[var2] <= _queryRootIndex[var])
                //             quad_v2->_rootID = _queryRootIndex[var2];
                //     }
                //     else
                //     {
                //         prod_quad_v2.set(featureToFunction[var2]);
                //     }
                // }
                // else if (_categoricalFeatures[var2])
                // {
                //     quad_v2->_fVars.set(var2);
                //     quad_v2->_rootID = _queryRootIndex[var2];
                // }
                // else
                // {
                //     // create prod_quad_v2 of both var and var2
                //     prod_quad_v2.set(featureToFunction[var2]);                    
                // }
                if (_categoricalFeatures[var])
                {
                    quad_v2->_root = _td->getTDNode(_queryRootIndex[var]);
                    quad_v2->_fVars.set(var);                    
                }
                
                if (_categoricalFeatures[var2])
                {
                    quad_v2->_fVars.set(var2);
                    // If both varaibles are categoricalVars - we choose the
                    // var2 as the root
                    quad_v2->_root = _td->getTDNode(_queryRootIndex[var2]);
                }
                else
                {
                    // create prod_quad_v2 of both var and var2
                    prod_quad_v2.set(featureToFunction[var2]);                    
                }
                
                agg_quad_v2->_sum.push_back(prod_quad_v2);
                
                quad_v2->_aggregates.push_back(agg_quad_v2);
                _compiler->addQuery(quad_v2);
                
                // TODO: Here we add both sides! multiply var times param of
                // var2 -- add that to gradient of var. And then multiply var2
                // times param of var and add to gradient of var2! -- this can
                // be done togther in one pass over the vector!

                listOfQueries.push_back(quad_v2);                
                ++numberOfQueries;
                // TODO: once we have identified the queries and how they are
                // multiplied, we then need to identify the actual views that
                // are computed over and use the information group computation
                // over this view!
            }
        }
    }
}



void CovarianceMatrix::loadFeatures()
{
    _queryRootIndex = new size_t[NUM_OF_VARIABLES]();
    
    /* Load the two-pass variables config file into an input stream. */
    ifstream input(FEATURE_CONF);

    if (!input)
    {
        ERROR(FEATURE_CONF +" does not exist. \n");
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
            ERROR("Relation |"+rootName+"| for "+attrName+" does not exist.");
            exit(1);
        }

        if (featureNo == 0 && categorical == 1)
        {
            ERROR("The label needs to be continuous! ");
            exit(1);
        }

        _isFeature.set(attributeID);
        _features.set(attributeID);

        Feature f;

        if (categorical)
        {
            _categoricalFeatures.set(attributeID);
            _queryRootIndex[attributeID] = rootID;

            f.head.set(attributeID);
            _isCategoricalFeature.set(attributeID);
        }
        else
        {
            _queryRootIndex[attributeID] = _td->_root->_id;
            ++f.body[attributeID];
        }

        _listOfFeatures.push_back(f);

        if (featureNo == 0)
            labelID = attributeID;
        
        /* Clear string stream. */
        ssLine.clear();
    }
}

void CovarianceMatrix::generateCode()
{
    std::string dumpListOfQueries = "";

    for (Query* query : listOfQueries)
    {
        const std::pair<size_t,size_t>& viewAggPair = query->_incoming[0];
        
        dumpListOfQueries += std::to_string(viewAggPair.first)+","+
            std::to_string(viewAggPair.second)+"\\n";
    }
    
    std::string runFunction = offset(1)+"void runApplication()\n"+offset(1)+"{\n"+
        offset(2)+"std::ofstream ofs(\"times.txt\",std::ofstream::out | std::ofstream::app);\n"+
        offset(2)+"ofs << \"\\n\";\n"+
        offset(2)+"ofs.close();\n\n"+
        "#ifdef DUMP_OUTPUT\n"+
        offset(2)+"ofs.open(\"output/covarianceMatrix.out\");\n"+
        offset(2)+"ofs << \""+dumpListOfQueries+"\";\n"+
        offset(2)+"ofs.close();\n"+
        "#endif /* DUMP_OUTPUT */ \n"+
        offset(1)+"}\n";
    
    std::ofstream ofs(multifaq::dir::OUTPUT_DIRECTORY+"ApplicationHandler.h", std::ofstream::out);
    ofs << "#ifndef INCLUDE_APPLICATIONHANDLER_HPP_\n"<<
        "#define INCLUDE_APPLICATIONHANDLER_HPP_\n\n"<<
        "#include \"DataHandler.h\"\n\n"<<
        "namespace lmfao\n{\n"<<
        offset(1)+"void runApplication();\n"<<
        "}\n\n#endif /* INCLUDE_APPLICATIONHANDLER_HPP_*/\n";    
    ofs.close();

    ofs.open(multifaq::dir::OUTPUT_DIRECTORY+"ApplicationHandler.cpp", std::ofstream::out);
    ofs << "#include \"ApplicationHandler.h\"\n"
        << "namespace lmfao\n{\n";
    ofs << runFunction << std::endl;
    ofs << "}\n";
    ofs.close();
}
