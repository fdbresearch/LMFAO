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
#include <CodegenUtils.hpp>

static const std::string FEATURE_CONF = "/features.conf";

static const char COMMENT_CHAR = '#';
static const char NUMBER_SEPARATOR_CHAR = ',';
static const char ATTRIBUTE_NAME_CHAR = ':';

using namespace std;
using namespace multifaq::params;
namespace phoenix = boost::phoenix;
using namespace boost::spirit;


struct QueryThresholdPair
{
    Query* query;
    size_t varID;
    Function* function;
};

std::vector<QueryThresholdPair> queryToThreshold;

RegressionTree::RegressionTree(const string& pathToFiles,
                                   shared_ptr<Launcher> launcher) :
    _pathToFiles(pathToFiles)
{
    _compiler = launcher->getCompiler();
    _td = launcher->getTreeDecomposition();
    
    if (_pathToFiles.back() == '/')
        _pathToFiles.pop_back();
}

RegressionTree::~RegressionTree()
{
    delete[] _queryRootIndex;
    delete[] _numOfCandidates;
    delete[] _complementFunction;
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

    for (size_t f = 0; f < NUM_OF_FUNCTIONS; ++f)
        if (_candidateMask[NUM_OF_VARIABLES+1][f])
            root->_conditions.set(f);
    
    genDynamicFunctions();
    
    // root->_conditions.set(5);
    // root->_conditions.set(9);
    // root->_conditions.set(15);
    
    splitNodeQueries(root);
    _compiler->compile();
    
    // 1. define queries for new splits
    // 2. compile views for these queries
    // 3. compute these views / queries and opt. split
    // 4. repeat from 1
    
    /* The regression tree is iterative */
    // while (!_activeNodes.empty()) 
    // {
    //     RegTreeNode* node = _activeNodes.front();
    //     _activeNodes.pop();
    
    //     splitNodeQueries(node);
    //     _compiler->compile();

    //     /* Compute the queries */
        
    //     /* For each of them find minimum candidates */
    //     /* Then the optimal split  */
    //     // TODO: how do we do this?!?!

    //     /* split node and add them to active nodes if they satisfy the minimum
    //      * threshold condition */
    // }

    generateCode();
}

void RegressionTree::computeCandidates()
{
    initializeThresholds();
    
    _numOfCandidates = new size_t[NUM_OF_VARIABLES];
    
    _complementFunction = new size_t[NUM_OF_VARIABLES];

    _varToQueryMap = new Query*[NUM_OF_VARIABLES];
    
    /* We can have a candidate mask for each feature. */
    _candidateMask.resize(NUM_OF_VARIABLES + 1);
    
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
                else
                {
                    _compiler->addFunction(
                        new Function({var}, Operation::indicator_lt, parameter));
                }

                _candidateMask[var].set(idx);
                ++idx;
            }
            
            _numOfCandidates[var] = _thresholds[var].size();
            _totalNumOfCandidates +=  _numOfCandidates[var];
        }
    }

    // size_t firstComplementFunction = idx;

    size_t origIdx = 2;
    
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
                // _candidateMask[var].set(idx);
                _complementFunction[origIdx] = idx;
                ++origIdx;
                ++idx;
            }
        }
    }

    // Adding dynamic functions for each relation, which can be modified
    // without recompiling the entire codebase. 
    for (size_t rel = 0; rel < _td->numberOfRelations(); ++rel)
    {
        TDNode* relation = _td->getRelation(rel);
        var_bitset& relationBag = relation->_bag;

        std::set<size_t> relationVariables;
        for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
            if (relationBag[var])
                relationVariables.insert(var);

        std::string functionName = "f_"+relation->_name;
        
        _compiler->addFunction(
            new Function(
                relationVariables, Operation::dynamic, true, functionName));
        _candidateMask[NUM_OF_VARIABLES+1].set(idx);
        ++idx;
    }
}

void RegressionTree::splitNodeQueries(RegTreeNode* node)
{
    for (size_t var=0; var < NUM_OF_VARIABLES; ++var)
    {
        if (_features[var])
        {
            if (_categoricalFeatures[var])
            {
                Aggregate* aggC = new Aggregate();
                Aggregate* aggL = new Aggregate();
                Aggregate* aggQ = new Aggregate();
                
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

                query->_rootID = _queryRootIndex[var];  
                query->_aggregates = {aggC,aggL,aggQ};
                query->_fVars.set(var);
                
                _compiler->addQuery(query);

                QueryThresholdPair qtPair;
                qtPair.query = query;
                qtPair.varID = var;
                qtPair.function = nullptr;
                
                queryToThreshold.push_back(qtPair);

                _varToQueryMap[var] = query;
            }
            else
            {
                for (size_t f = 0; f < NUM_OF_FUNCTIONS; ++f)
                {
                    if (!_candidateMask[var][f])
                        continue;
                    
                    Aggregate* aggC = new Aggregate();
                    Aggregate* aggL = new Aggregate();
                    Aggregate* aggQ = new Aggregate();
        
                    /* Q_C */
                    aggC->_agg.push_back(node->_conditions);
                    aggC->_agg[0].set(f);

                    /* Q_L */
                    aggL->_agg.push_back(node->_conditions);
                    aggL->_agg[0].set(0);
                    aggL->_agg[0].set(f);
        
                    /* Q_Q */
                    aggQ->_agg.push_back(node->_conditions);
                    aggQ->_agg[0].set(1);
                    aggQ->_agg[0].set(f);
                                        
                    Aggregate* aggC_complement = new Aggregate();
                    Aggregate* aggL_complement = new Aggregate();
                    Aggregate* aggQ_complement = new Aggregate();
        
                    /* Q_C */
                    aggC_complement->_agg.push_back(node->_conditions);
                    aggC_complement->_agg[0].set(_complementFunction[f]);

                    /* Q_L */
                    aggL_complement->_agg.push_back(node->_conditions);
                    aggL_complement->_agg[0].set(0);
                    aggL_complement->_agg[0].set(_complementFunction[f]);
        
                    /* Q_Q */
                    aggQ_complement->_agg.push_back(node->_conditions);
                    aggQ_complement->_agg[0].set(1);
                    aggQ_complement->_agg[0].set(_complementFunction[f]);
                    

                    Query* query = new Query();
                    query->_rootID = _queryRootIndex[var];
                    query->_aggregates = {aggC,aggL,aggQ,
                         aggC_complement,aggL_complement,aggQ_complement};
                    
                    _compiler->addQuery(query);
                    _varToQueryMap[var] = query;
                    
                    QueryThresholdPair qtPair;
                    qtPair.query = query;
                    qtPair.varID = var;
                    qtPair.function = _compiler->getFunction(f);
                    
                    queryToThreshold.push_back(qtPair);
                }
            }
        }
    }
}


void RegressionTree::loadFeatures()
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
        getline(ssLine, typeOfFeature, ATTRIBUTE_NAME_CHAR);

        string rootName;
        /* Extract the dimension of the current attribute. */
        getline(ssLine, rootName);
        
        int attributeID = _td->getAttributeIndex(attrName);
        bool categorical = stoi(typeOfFeature);
        int rootID = _td->getRelationIndex(rootName);

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

        if (rootID == -1)
        {
            ERROR("Relation |"+rootName+"| does not exist.");
            exit(1);
        }

        _features.set(attributeID);

        if (categorical)
            _categoricalFeatures.set(attributeID);

        _queryRootIndex[attributeID] = rootID;
        
        /* Clear string stream. */
        ssLine.clear();
    }

    // cout << _features << "\n";
    // cout << _categoricalFeatures << "\n";
}


void RegressionTree::genDynamicFunctions()
{
    std::string functionHeaders = "";
    std::string functionSource = "";
    for(size_t f = 0; f < NUM_OF_FUNCTIONS; ++f)
    {   
        if (_candidateMask[NUM_OF_VARIABLES + 1].test(f))
        {
            Function* func = _compiler->getFunction(f);
            const var_bitset& fVars = func->_fVars;
            std::string fvarString = "";
            
            for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
            {
                if (fVars.test(var))
                {
                    Attribute* att = _td->getAttribute(var);
                    fvarString += typeToStr(att->_type)+" "+att->_name+",";
                }
            }
            fvarString.pop_back();
            
            functionHeaders += offset(1)+"double "+func->_name+"("+fvarString+");\n\n";
            functionSource += offset(1)+"double "+func->_name+"("+fvarString+")\n"+
                offset(1)+"{\n"+offset(2)+"return 1.0;\n"+offset(1)+"}\n";
        }
    }

    std::ofstream ofs("runtime/cpp/DynamicFunctions.h", std::ofstream::out);
    ofs << "#ifndef INCLUDE_DYNAMICFUNCTIONS_H_\n"<<
        "#define INCLUDE_DYNAMICFUNCTIONS_H_\n\n"<<
        "namespace lmfao\n{\n"<< functionHeaders <<
        "}\n\n#endif /* INCLUDE_DYNAMICFUNCTIONS_H_*/\n";    
    ofs.close();
    
    ofs.open("runtime/cpp/DynamicFunctions.cpp", std::ofstream::out);
    ofs << "#include \"DynamicFunctions.h\"\nnamespace lmfao\n{\n"+functionSource+"}\n";
    ofs.close();
}



std::string RegressionTree::genVarianceComputation()
{
    std::vector<std::vector<std::string>> variancePerView(_compiler->numberOfViews());
    std::vector<std::vector<std::string>> functionPerView(_compiler->numberOfViews());
    std::vector<size_t> firstThreshold(NUM_OF_VARIABLES);
    
    for (QueryThresholdPair& qtPair : queryToThreshold)
    {

        if (_categoricalFeatures[qtPair.varID])
        {
            // Find the view that corresponds to the query
            Query* query = qtPair.query;
            size_t& viewID = query->_aggregates[0]->_incoming[0].first;
            size_t& viewID2 = query->_aggregates[1]->_incoming[0].first;
            size_t& viewID3 = query->_aggregates[2]->_incoming[0].first;

            if (viewID != viewID2 || viewID != viewID3)
                std::cout << "THERE IS AN ERROR IN genVarianceComputation" << std::endl;
        
            const size_t& count = query->_aggregates[0]->_incoming[0].second;
            const size_t& linear = query->_aggregates[1]->_incoming[0].second;
            const size_t& quad = query->_aggregates[2]->_incoming[0].second;
        
            std::string viewTup = "V"+std::to_string(viewID)+"tuple";
        
            variancePerView[viewID].push_back(
                "("+viewTup+".aggregates["+std::to_string(count)+"] > 0 ? "+
                viewTup+".aggregates["+std::to_string(quad)+"] - ("+
                viewTup+".aggregates["+std::to_string(linear)+"] * "+
                viewTup+".aggregates["+std::to_string(linear)+"]) / "+
                viewTup+".aggregates["+std::to_string(count)+"] : 999);\n"
                );
        
            Attribute* att = _td->getAttribute(qtPair.varID);
            
            functionPerView[viewID].push_back(
                "\"Variable: "+std::to_string(qtPair.varID)+" "+
                "Threshold: \"+std::to_string(tuple."+att->_name+")+\" "+
                "Operator: = \";\n");
        }
        else
        {   // Continuous variable
            
            // Find the view that corresponds to the query
            Query* query = qtPair.query;
            size_t& viewID = query->_aggregates[0]->_incoming[0].first;
            size_t& viewID2 = query->_aggregates[1]->_incoming[0].first;
            size_t& viewID3 = query->_aggregates[2]->_incoming[0].first;

            size_t& cviewID = query->_aggregates[3]->_incoming[0].first;
            size_t& cviewID2 = query->_aggregates[4]->_incoming[0].first;
            size_t& cviewID3 = query->_aggregates[5]->_incoming[0].first;

            if (viewID != viewID2 || viewID != viewID3 ||
                cviewID != viewID || cviewID2 != viewID || cviewID3 != viewID  )
                std::cout << "THERE IS AN ERROR IN genVarianceComputation" << std::endl;
        
            const size_t& count = query->_aggregates[0]->_incoming[0].second;
            const size_t& linear = query->_aggregates[1]->_incoming[0].second;
            const size_t& quad = query->_aggregates[2]->_incoming[0].second;

            const size_t& compcount = query->_aggregates[3]->_incoming[0].second;
            const size_t& complinear = query->_aggregates[4]->_incoming[0].second;
            const size_t& compquad = query->_aggregates[5]->_incoming[0].second;
        
            std::string viewTup = "V"+std::to_string(viewID)+"tuple";
        
            variancePerView[viewID].push_back(
                "(("+viewTup+".aggregates["+std::to_string(count)+"] > 0  && "+
                viewTup+".aggregates["+std::to_string(compcount)+"] > 0) ? "+
                viewTup+".aggregates["+std::to_string(quad)+"] - ("+
                viewTup+".aggregates["+std::to_string(linear)+"] * "+
                viewTup+".aggregates["+std::to_string(linear)+"]) / "+
                viewTup+".aggregates["+std::to_string(count)+"] + "+
                viewTup+".aggregates["+std::to_string(compquad)+"] - ("+
                viewTup+".aggregates["+std::to_string(complinear)+"] * "+
                viewTup+".aggregates["+std::to_string(complinear)+"]) / "+
                viewTup+".aggregates["+std::to_string(compcount)+"] : 999);\n"
                );
           
            functionPerView[viewID].push_back(
                "\"Variable: "+std::to_string(qtPair.varID)+" "+
                "Threshold: "+std::to_string(qtPair.function->_parameter[0])+" "+
                "Operator: < \";\n");
        }
        
    }

    std::string numOfThresholds = "numberOfThresholds = ";
    size_t numOfContThresholds = 0;

    for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
    {
        if (!_features.test(var))
            continue;
        
        if (!_categoricalFeatures.test(var))
        {
            firstThreshold[var] = numOfContThresholds;
            numOfContThresholds += _thresholds[var].size();
        }
        else
        {
            size_t& viewID = _varToQueryMap[var]->_aggregates[0]->_incoming[0].first;   
            numOfThresholds += "V"+std::to_string(viewID)+".size() + ";
        }
    }

    numOfThresholds += std::to_string(numOfContThresholds)+";\n";

    std::string initVariance = offset(2)+numOfThresholds +
        offset(2)+"variance = new double[numberOfThresholds];\n";
    
    std::string contVariance = "", categVariance = "";
    std::string contFunctionIndex = "", categFunctionIndex = "";
    
    size_t varianceCount = 0;
    for (size_t viewID = 0; viewID < _compiler->numberOfViews(); ++viewID)
    {
        if (!variancePerView[viewID].empty())
        {
            std::string viewString = "V"+std::to_string(viewID);
            
            if (_compiler->getView(viewID)->_fVars.any())
            {
                categVariance += offset(2)+"for ("+viewString+"_tuple& "+
                    viewString+"tuple : "+viewString+")\n";

                for (size_t j = 0; j < variancePerView[viewID].size(); ++j)
                {
                    categVariance += offset(3)+
                        "variance["+std::to_string(numOfContThresholds)+
                        "+categIdx++] = "+variancePerView[viewID][j];
                    
                    categFunctionIndex += 
                        offset(2)+"for (V"+std::to_string(viewID)+"_tuple& tuple : "+
                        "V"+std::to_string(viewID)+")\n"+
                        offset(3)+"thresholdMap["+std::to_string(numOfContThresholds)+
                        "+categIdx++] = "+functionPerView[viewID][j];
                }
            }
            else
            {
                contVariance += offset(2)+viewString+"_tuple& "+
                    viewString+"tuple = "+viewString+"[0];\n";
                                
                for (size_t j = 0; j < variancePerView[viewID].size(); ++j)
                {
                    contVariance += offset(2)+
                        "variance["+std::to_string(varianceCount)+"] = "+
                        variancePerView[viewID][j];

                    contFunctionIndex += offset(2)+
                        "thresholdMap["+std::to_string(varianceCount)+"] = "+
                        functionPerView[viewID][j];
                    
                    ++varianceCount;
                }
            }
        }
    }

    std::string computeVariance = contVariance;
    
    if (!contVariance.empty())
        computeVariance += offset(2)+"size_t categIdx = 0;\n"+categVariance;
    
    computeVariance += "\n"+offset(2)+"double min_variance = variance[0];\n"+
        offset(2)+"size_t threshold = 0;\n\n"+
        offset(2)+"for (size_t t=1; t < numberOfThresholds; ++t)\n"+offset(2)+"{\n"+
        offset(3)+"if (variance[t] < min_variance)\n"+offset(3)+"{\n"+
        offset(4)+"min_variance = variance[t];\n"+offset(4)+"threshold = t;\n"+
        offset(3)+"}\n"+offset(2)+"}\n"+
        offset(2)+"std::cout << \"The minimum variance is: \" << min_variance <<"+
        "\" for threshold: \" << thresholdMap[threshold] << std::endl;\n";

    // TODO:TODO: For categorical variables, when we want to compute the other half
    // of the variance, we would need to compute the total and then infer from
    // that!

    // TODO:TODO:TODO:TODO:TODO:TODO:TODO: We could compute the variance and
    // minimum variance over categorical variables right away. No need to store
    // all of them in an array. Then we only need one index in the variance[..]
    // for each categorical variable. --> if this index is chose we need to know
    // what kind of function we need to construct now.

    std::string functionIndex = contFunctionIndex;

    if (!categFunctionIndex.empty())
    {
        functionIndex += offset(2)+"size_t categIdx = 0;\n"+categFunctionIndex;
    }
    
    return offset(1)+"size_t numberOfThresholds;\n"+
        offset(1)+"double* variance = nullptr;\n"+
        offset(1)+"std::string* thresholdMap = nullptr;\n\n"+
        offset(1)+"void initVarianceArray()\n"+
        offset(1)+"{\n"+initVariance+
        offset(2)+"thresholdMap = new std::string[numberOfThresholds];\n"+functionIndex+
        offset(2)+"// TODO: NEED TO CONSTRUCT MAP FROM VARIANCE INDEX TO FUNCTION!\n"+
        offset(1)+"}\n\n"+
        offset(1)+"void computeVariance()\n"+
        offset(1)+"{\n"+computeVariance+offset(1)+"}\n";    
}


void RegressionTree::generateCode()
{
    std::string runFunction = offset(1)+"void runApplication()\n"+offset(1)+"{\n"+
        offset(2)+"initVarianceArray();\n"+
        offset(2)+"computeVariance();\n"+
        offset(1)+"}\n";
        
    std::ofstream ofs("runtime/cpp/ApplicationHandler.h", std::ofstream::out);
    ofs << "#ifndef INCLUDE_APPLICATIONHANDLER_HPP_\n"<<
        "#define INCLUDE_APPLICATIONHANDLER_HPP_\n\n"<<
        "#include \"DataHandler.h\"\n\n"<<
        "namespace lmfao\n{\n"<<
        offset(1)+"void runApplication();\n"<<
        "}\n\n#endif /* INCLUDE_APPLICATIONHANDLER_HPP_*/\n";    
    ofs.close();

    ofs.open("runtime/cpp/ApplicationHandler.cpp", std::ofstream::out);
    
    ofs << "#include \"ApplicationHandler.h\"\nnamespace lmfao\n{\n";
    ofs << genVarianceComputation() << std::endl;
    ofs << runFunction << std::endl;
    ofs << "}\n";
    ofs.close();
}



void RegressionTree::initializeThresholds()
{
    if (_pathToFiles.compare("data/example") == 0)
    {
       _thresholds =
            {
                {1,2},
                {1,2},
                {1,2},
                {1,2},
                {1,2},
                {1,2}
            };
    }
    
    if (_pathToFiles.compare("data/usretailer") == 0 ||
        _pathToFiles.compare("../../data/usretailer") == 0)
    {
        _thresholds =
            {
                {}, // locn
                {}, // dateid
                {}, // ksn 
                {}, // inventory_units
                {}, // zip 
                {1,8}, // rgn_code TODO: TODO: 
                {1,1,2,3,4,5}, // clim_zn_nbr  TODO:  TODO: 
                {6.8444,8.2101,8.38184,8.418,8.5584,8.6479,8.6752,8.75316,8.94666,9.1594,9.45,9.484,9.5933,9.74834,10.2716,10.49996,10.8474,11.67102,12.53952},
                {5.14708,6.59456,6.75694,6.9867,7.1428,7.23374,7.28834,7.34834,7.40032,7.4443,7.5089,7.58436,7.83488,7.97012,8.094,8.41024,8.94032,9.22284,9.88272},
                {4.8677,5.10068,5.29006,5.42888,5.5881,5.72704,5.87592,6.07782,6.29424,6.4877,6.7022,6.7022,6.7022,6.96112,7.2848,7.71394,8.0942,8.62438,9.62584},
                {5.463098,11.910442,15.46413,15.46413,15.46413,15.46413,15.46413,15.46413,15.46413,15.46413,15.46413,15.46413,15.46413,15.46413,15.46413,15.46413,15.46413,15.46413,24.701992},
                {9.63648,15.82272,19.10509,19.10509,19.10509,19.10509,19.10509,19.10509,19.10509,19.10509,19.10509,19.10509,19.10509,19.10509,19.10509,19.10509,19.10509,19.10509,26.72928},
                {0.922112,1.409274,2.01697,2.555082,3.14414,3.800304,4.524828,5.470554,6.587774,7.77957,8.17724,8.17724,8.17724,8.17724,8.17724,8.17724,10.988578,17.148604,26.414984},
                {2.51712,3.82464,4.93344,5.76864,6.5952,7.42752,8.44704,9.52992,11.07648,11.8131,11.8131,11.8131,11.8131,11.8131,11.8131,13.2768,15.78816,20.95776,30.01248},
                {2.279188,3.826404,5.346278,7.176834,9.25843,12.77042,14.91195,14.91195,14.91195,14.91195,14.91195,14.91195,14.91195,14.91195,14.91195,15.980424,21.257112,27.055744,35.04919},
                {5.2992,7.52544,9.41472,11.74464,14.2704,17.2944,19.01726,19.01726,19.01726,19.01726,19.01726,19.01726,19.01726,19.01726,19.01726,20.51712,25.6176,30.81888,38.58336},
                {0.49834,0.9072,1.257654,1.585742,1.98839,2.355,2.72782,3.20628,3.748112,4.33096,5.091512,5.48422,5.48422,5.48422,5.49914,6.523156,8.026004,11.287834,16.754654},
                {1.56672,2.50272,3.2256,3.9456,4.7664,5.35104,5.94144,6.5808,7.3008,8.2512,8.94215,8.94215,8.94215,8.94215,9.7488,11.088,12.90816,16.51392,23.36544},
                {0.879565,1.20835,1.48906,1.71446,1.9222,2.12759,2.33187,2.5418,2.767985,2.9511,3.099045,3.31318,3.48249,3.73503,3.97215,4.17808,4.53785,5.09063,5.90789},
                {0.367075,0.68876,0.86049,1.03922,1.21985,1.37419,1.49291,1.63308,1.76269,1.89735,2.04024,2.18968,2.35554,2.53027,2.6794,2.84528,3.07802,3.36536,3.86612},
                {0.005,0.00791,0.0101,0.0137,0.0176,0.0216,0.0259,0.03094,0.03878,0.0465,0.05501,0.06576,0.07733,0.09527,0.115,0.13914,0.180325,0.24962,0.41677},
                {0.0001,0.0002,0.0003,0.0004,0.0005,0.0006,0.0007,0.0009,0.001,0.0012,0.0014,0.0017,0.0021,0.0026,0.0033,0.0043,0.0064,0.00889,0.02037},
                {0.006005,0.01102,0.0183,0.027,0.0374,0.05186,0.069005,0.09134,0.111335,0.13965,0.1758,0.2129,0.26073,0.32154,0.3914,0.48164,0.6759749,0.93456,1.38585},
                {2.92,3.15,3.2915,3.412,3.51,3.593,3.67,3.75,3.8,3.87,3.93,3.98,4.03,4.097,4.16,4.24,4.32,4.44,4.6095},
                {0.36597,0.48414,0.5933,0.68434,0.758725,0.8501,0.930775,1.00754,1.098125,1.16395,1.227335,1.29482,1.36063,1.44301,1.52965,1.61848,1.733755,1.91537,2.163435},
                {0.41592,0.55556,0.66975,0.76792,0.853425,0.94507,1.03729,1.12624,1.20527,1.28435,1.351665,1.42398,1.502865,1.5908,1.681625,1.77568,1.900845,2.07273,2.343025},
                {0.230335,0.31326,0.3778,0.44232,0.487575,0.53859,0.59594,0.64704,0.707025,0.7437,0.7858,0.83144,0.88467,0.92641,0.9951,1.06248,1.136925,1.30247,1.45904},
                {0.36597,0.48414,0.5933,0.68434,0.758725,0.8501,0.930775,1.00754,1.098125,1.16395,1.227335,1.29482,1.36063,1.44301,1.52965,1.61848,1.733755,1.91537,2.163435},
                {0.160725,0.21877,0.2686,0.31692,0.350875,0.38626,0.428035,0.46528,0.500235,0.5303,0.55991,0.5898,0.621265,0.65571,0.706025,0.7628,0.82464,0.91217,1.051925},
                {0.43066,0.58418,0.734335,0.83388,0.9356,1.03524,1.13995,1.23788,1.3482,1.42665,1.509985,1.60746,1.706875,1.82,1.927075,2.03126,2.23329,2.46377,2.875135},
                {0.44948,0.616,0.755835,0.86612,0.9811,1.08723,1.195405,1.30296,1.42244,1.51905,1.589,1.70016,1.796245,1.90427,2.029825,2.1512,2.340305,2.60259,3.01861},
                {0.04231,0.06043,0.07603,0.09026,0.101825,0.11433,0.12977,0.1395,0.15,0.16135,0.1731,0.1838,0.198465,0.21439,0.23225,0.25838,0.29004,0.3277,0.395085},
                {0.016605,0.026,0.036715,0.04934,0.064225,0.07968,0.096705,0.11908,0.1389,0.16925,0.20311,0.24116,0.290205,0.37102,0.47285,0.61374,0.812529999999999,1.40992,2.73662},
                {}, // subcategory
                {}, // category
                {}, // categoryCluster
                {4.99,6.99,8.99,9.99,11.99,12.99,14.99,14.99,16.99,19.99,19.99,21.99,24.99,26.93,29.99,34.99,39.99,49.99,69.99},
                {}, // rain
                {}, // snow
                {-1,3,6,9,12,14,16,18,20,22,23,24,26,27,28,29,31,32,34}, // maxtemp
                {-10,-6,-3,-1,1,2,4,6,7,9,11,12,14,15,17,18,20,22,23}, // mintemp
                {2,3,5,5,6,6,8,8,9,10,11,11,13,13,14,16,18,19,23}, // meanwind
                {}, // thunder
            };
    }
}
