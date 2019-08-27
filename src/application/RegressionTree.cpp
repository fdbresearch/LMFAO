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
// #include <CodegenUtils.hpp>

static const char COMMENT_CHAR = '#';
static const char NUMBER_SEPARATOR_CHAR = ',';
static const char ATTRIBUTE_NAME_CHAR = ':';

using namespace std;

using namespace multifaq::params;
using namespace multifaq::dir;
using namespace multifaq::config;

namespace phoenix = boost::phoenix;
using namespace boost::spirit;


struct QueryThresholdPair
{
    Query* query;
    size_t varID;
    Function* function;
    Query* complementQuery = nullptr;
};

std::vector<QueryThresholdPair> queryToThreshold;

RegressionTree::RegressionTree(shared_ptr<Launcher> launcher, bool classification) :
    _classification(classification)
{
    _db = launcher->getDatabase();
    _td = launcher->getTreeDecomposition();
    _compiler = launcher->getCompiler();
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

    // genDynamicFunctions();

    if (_classification)
        classificationTreeQueries();
    else
        regressionTreeQueries();
    
    _compiler->compile();
}

void RegressionTree::computeCandidates()
{   
    initializeThresholds();
    
    _numOfCandidates = new size_t[NUM_OF_VARIABLES];
    
    _complementFunction = new size_t[NUM_OF_FUNCTIONS];

    _varToQueryMap = new Query*[NUM_OF_VARIABLES];
    
    /* We can have a candidate mask for each feature. */
    _candidateMask.resize(NUM_OF_VARIABLES + 1);

    size_t idx = 0;
    size_t origIdx = 0;

    if (!_classification)
    {        
        // Create functions that are linear sums for y and y^2 first
        _compiler->addFunction(
            new Function({static_cast<size_t>(_labelID)}, Operation::linear_sum));
        _compiler->addFunction(
            new Function({static_cast<size_t>(_labelID)}, Operation::quadratic_sum));
        idx = 2;
        origIdx = 2;
    }
    
    /* CANDIDATES ARE FUNCTIONS --- so we define the functions for all
     * candidates and set them accordingly  */
    
    // Push them on the functionList in the order they are accessed 
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
    for (size_t rel = 0; rel < _db->numberOfRelations(); ++rel)
    {
        Relation* relation = _db->getRelation(rel);
        var_bitset& relationBag = relation->_schemaMask;
        
        std::string functionName = "f_"+relation->_name;

        std::set<size_t> relationVariables;
        for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
        {
            if (relationBag[var])
                relationVariables.insert(var);
        }

        _compiler->addFunction(
            new Function(
                relationVariables, Operation::dynamic, true, functionName));

        /* _candidateMask[NUM_OF_VARIABLES] keeps track of all dynamic functions */
        _candidateMask[NUM_OF_VARIABLES].set(idx);
        ++idx;
    }   
}

void RegressionTree::regressionTreeQueries()
{

    Aggregate* countC = new Aggregate();
    Aggregate* countL = new Aggregate();
    Aggregate* countQ = new Aggregate();
                
    /* Q_C */
    countC->_sum.push_back(_candidateMask[NUM_OF_VARIABLES]);
                            
    /* Q_L */
    countL->_sum.push_back(_candidateMask[NUM_OF_VARIABLES]);
    countL->_sum[0].set(0);
        
    /* Q_Q */
    countQ->_sum.push_back(_candidateMask[NUM_OF_VARIABLES]);
    countQ->_sum[0].set(1);

    // We add an additional query without free vars to compute the
    // complement of each threshold 
    Query* countQuery = new Query();
    countQuery->_root = _td->_root;
    countQuery->_aggregates = {countC,countL,countQ};

    _compiler->addQuery(countQuery);

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
                aggC->_sum.push_back(_candidateMask[NUM_OF_VARIABLES]);
                            
                /* Q_L */
                aggL->_sum.push_back(_candidateMask[NUM_OF_VARIABLES]);
                aggL->_sum[0].set(0);
        
                /* Q_Q */
                aggQ->_sum.push_back(_candidateMask[NUM_OF_VARIABLES]);
                aggQ->_sum[0].set(1);

                
                Query* query = new Query();
                query->_root = _td->getTDNode(_queryRootIndex[var]);
                query->_aggregates = {aggC,aggL,aggQ};
                query->_fVars.set(var);
                
                _compiler->addQuery(query);


                QueryThresholdPair qtPair;
                qtPair.query = query;
                qtPair.varID = var;
                qtPair.function = nullptr;
                qtPair.complementQuery = countQuery;
                
                _varToQueryMap[var] = query;

                queryToThreshold.push_back(qtPair);
            }
            else /* Continuous Variable */
            {
                /* We create a query for each threshold for this variable */ 
                for (size_t f = 0; f < NUM_OF_FUNCTIONS; ++f)
                {
                    if (!_candidateMask[var][f])
                        continue;
                    
                    Aggregate* aggC = new Aggregate();
                    Aggregate* aggL = new Aggregate();
                    Aggregate* aggQ = new Aggregate();
        
                    /* Q_C */
                    aggC->_sum.push_back(_candidateMask[NUM_OF_VARIABLES]);
                    aggC->_sum[0].set(f);

                    /* Q_L */
                    aggL->_sum.push_back(_candidateMask[NUM_OF_VARIABLES]);
                    aggL->_sum[0].set(0);
                    aggL->_sum[0].set(f);
        
                    /* Q_Q */
                    aggQ->_sum.push_back(_candidateMask[NUM_OF_VARIABLES]);
                    aggQ->_sum[0].set(1);
                    aggQ->_sum[0].set(f);
                                        
                    Aggregate* aggC_complement = new Aggregate();
                    Aggregate* aggL_complement = new Aggregate();
                    Aggregate* aggQ_complement = new Aggregate();
        
                    /* Q_C */
                    aggC_complement->_sum.push_back(_candidateMask[NUM_OF_VARIABLES]);
                    aggC_complement->_sum[0].set(_complementFunction[f]);

                    /* Q_L */
                    aggL_complement->_sum.push_back(_candidateMask[NUM_OF_VARIABLES]);
                    aggL_complement->_sum[0].set(0);
                    aggL_complement->_sum[0].set(_complementFunction[f]);
        
                    /* Q_Q */
                    aggQ_complement->_sum.push_back(_candidateMask[NUM_OF_VARIABLES]);
                    aggQ_complement->_sum[0].set(1);
                    aggQ_complement->_sum[0].set(_complementFunction[f]);

                    Query* query = new Query();
                    query->_root =  _td->getTDNode(_queryRootIndex[var]);
                    // _td->_root->_id;
                    query->_aggregates =
                        {aggC,aggL,aggQ,aggC_complement,
                         aggL_complement,aggQ_complement};
                    
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


void RegressionTree::classificationTreeQueries()
{
    Query* continuousQuery = new Query();
    continuousQuery->_root = _td->getTDNode(_queryRootIndex[_labelID]);
    continuousQuery->_fVars.set(_labelID);
    _compiler->addQuery(continuousQuery);
    
    // Simple count for each category
    Aggregate* countCategory = new Aggregate();
    countCategory->_sum.push_back(_candidateMask[NUM_OF_VARIABLES]);
    continuousQuery->_aggregates.push_back(countCategory);
    
    // Add to compiler list
    Query* countQuery = new Query();
    countQuery->_root = _td->getTDNode(_queryRootIndex[_labelID]); // TODO: is this ok?!
    _compiler->addQuery(countQuery);

    // Overall count of tuples satisfying the parent conditions
    Aggregate* count = new Aggregate();
    count->_sum.push_back(_candidateMask[NUM_OF_VARIABLES]);
    countQuery->_aggregates.push_back(count);

    QueryThresholdPair qtPair;
    qtPair.query = continuousQuery;
    qtPair.complementQuery = countQuery;    
    queryToThreshold.push_back(qtPair);

    _functionOfAggregate.push_back(nullptr);
    
    for (size_t var=0; var < NUM_OF_VARIABLES; ++var)
    {
        if (_features[var])
        {
            if (_categoricalFeatures[var])
            {
                Aggregate* agg = new Aggregate();
                agg->_sum.push_back(_candidateMask[NUM_OF_VARIABLES]);
                
                Query* query = new Query();
                query->_root = _td->getTDNode(_queryRootIndex[var]);
                query->_fVars.set(var);
                query->_fVars.set(_labelID);
                query->_aggregates.push_back(agg);
                
                _compiler->addQuery(query);

                /* Count Query keeps track of the count of each category
                 * independent of label */
                Aggregate* countagg = new Aggregate();
                countagg->_sum.push_back(_candidateMask[NUM_OF_VARIABLES]);
                
                Query* varCountQuery = new Query();
                varCountQuery->_root = _td->getTDNode(_queryRootIndex[var]);
                varCountQuery->_aggregates.push_back(countagg);
                varCountQuery->_fVars.set(var);
                _compiler->addQuery(varCountQuery);

                // TODO:TODO: Need to keep track of this count query somewhere!!

                QueryThresholdPair qtPair;
                qtPair.query = query;
                qtPair.varID = var;
                qtPair.complementQuery = varCountQuery;
                
                _varToQueryMap[var] = query;
                queryToThreshold.push_back(qtPair);
            }
            else /* Continuous Variable */
            {
                // TODO: perhaps push all continuous aggregates to the same
                // query?
                // TODO: we will also need the count overall
                _varToQueryMap[var] = continuousQuery;

                /* We create a query for each threshold for this variable */ 
                for (size_t f = 0; f < NUM_OF_FUNCTIONS; ++f)
                {
                    if (!_candidateMask[var][f])
                        continue;
                    
                    Aggregate* agg = new Aggregate();
                    agg->_sum.push_back(_candidateMask[NUM_OF_VARIABLES]);
                    agg->_sum[0].set(f);

                    // TODO: technically we can infer the comp agg from the
                    // first aggregate - but I will add it for now to make it easier
                    Aggregate* agg_complement = new Aggregate();
                    agg_complement->_sum.push_back(_candidateMask[NUM_OF_VARIABLES]);
                    agg_complement->_sum[0].set(_complementFunction[f]);

                    continuousQuery->_aggregates.push_back(agg);
                    continuousQuery->_aggregates.push_back(agg_complement);
                    
                    // The below should give us the size of the group - may not
                    // be necessary
                    Aggregate* agg_count = new Aggregate();
                    agg_count->_sum.push_back(_candidateMask[NUM_OF_VARIABLES]);
                    agg_count->_sum[0].set(f);
                    
                    // TODO: technically we can infer the comp agg from the
                    // first aggregate - but I will add it for now to make it easier
                    Aggregate* agg_count_complement = new Aggregate();
                    agg_count_complement->_sum.push_back(
                        _candidateMask[NUM_OF_VARIABLES]);
                    agg_count_complement->_sum[0].set(_complementFunction[f]);

                    countQuery->_aggregates.push_back(agg_count);
                    countQuery->_aggregates.push_back(agg_count_complement);
                    
                    // TODO: TODO: we need to keep track of the functions that
                    // are in the continuous query - the below does that kind of 
                    _functionOfAggregate.push_back(_compiler->getFunction(f));
                    _functionOfAggregate.push_back(
                        _compiler->getFunction(_complementFunction[f]));

                    _varOfFunction.push_back(var);
                    _varOfFunction.push_back(var);
                    
                    // QueryThresholdPair qtPair;
                    // qtPair.query = continuousQuery;
                    // qtPair.varID = var;
                    // qtPair.function = _compiler->getFunction(f);
                    // qtPair.complementQuery = countQuery;

                    // queryToThreshold.push_back(qtPair);

                    // QueryThresholdPair qtPair2;
                    // qtPair.query = continuousQuery;
                    // qtPair.varID = var;
                    // qtPair.function = _compiler->getFunction(_complementFunction[f]);
                    // qtPair.complementQuery = countQuery;
                    
                    // queryToThreshold.push_back(qtPair2);
                }
            }
        }
    }
}


void RegressionTree::loadFeatures()
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

    if (!_classification && stoi(typeOfLabel) != 0)
    {
        ERROR("The label needs to be continuous! ");
        exit(1);
    }
    else if (_classification && stoi(typeOfLabel) != 1)
    {
        ERROR("The label needs to be categorical! ");
        exit(1);
    }
        

    _labelID = _db->getAttributeIndex(labelName);

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
        
        int attributeID = _db->getAttributeIndex(attrName);
        bool categorical = stoi(typeOfFeature);
        int rootID = _db->getRelationIndex(rootName);

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
        if (_candidateMask[NUM_OF_VARIABLES].test(f))
        {
            Function* func = _compiler->getFunction(f);
            const var_bitset& fVars = func->_fVars;
            std::string fvarString = "";
            
            for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
            {
                if (fVars.test(var))
                {
                    Attribute* att = _db->getAttribute(var);
                    fvarString += typeToStr(att->_type)+" "+att->_name+",";
                }
            }
            fvarString.pop_back();
            functionHeaders += offset(1)+"double "+func->_name+"("+fvarString+");\n\n";
            functionSource += offset(1)+"double "+func->_name+"("+fvarString+")\n"+
                offset(1)+"{\n"+offset(2)+"return 1.0;\n"+offset(1)+"}\n";
        }
    }
    std::ofstream ofs(multifaq::dir::OUTPUT_DIRECTORY+"DynamicFunctions.h", std::ofstream::out);
    ofs << "#ifndef INCLUDE_DYNAMICFUNCTIONS_H_\n"<<
        "#define INCLUDE_DYNAMICFUNCTIONS_H_\n\n"<<
        "namespace lmfao\n{\n"<< functionHeaders <<
        "}\n\n#endif /* INCLUDE_DYNAMICFUNCTIONS_H_*/\n";    
    ofs.close();
    
    ofs.open(multifaq::dir::OUTPUT_DIRECTORY+"DynamicFunctions.cpp", std::ofstream::out);
    ofs << "#include \"DynamicFunctions.h\"\nnamespace lmfao\n{\n"+functionSource+"}\n";
    ofs.close();


    ofs.open(multifaq::dir::OUTPUT_DIRECTORY+"DynamicFunctionsGenerator.hpp", std::ofstream::out);
    ofs << dynamicFunctionsGenerator();
    ofs.close();

    ofs.open(multifaq::dir::OUTPUT_DIRECTORY+"RegressionTreeHelper.hpp", std::ofstream::out);
    ofs << dynamicFunctionsGenerator();
    ofs.close();
}



std::string RegressionTree::dynamicFunctionsGenerator()
{
    std::string condition[] = {" <= "," == "};
    
    std::string varList = "", relationMap = ""; 
    for (size_t var = 0; var< _db->numberOfAttributes(); ++var)
    {
        const std::string varName = _db->getAttribute(var)->_name;

	varList += "\""+varName+"\","; 
	relationMap += std::to_string(_queryRootIndex[var])+",";        
    }
    varList.pop_back();
    relationMap.pop_back();
    
    std::string relationName = "", varTypeList = "", conditionsString = ""; 
    for (size_t rel = 0; rel < _db->numberOfRelations(); ++rel)
    {
	relationName += "\""+_db->getRelation(rel)->_name+"\",";
        conditionsString += "\"\",";

        varTypeList += "\"";

        var_bitset& bag = _db->getRelation(rel)->_schemaMask;
        for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
        {
            if (bag[var])
            {
                Attribute* att = _db->getAttribute(var);
                varTypeList += typeToStr(att->_type)+" "+att->_name+",";
            }
        }
        varTypeList.pop_back();
        varTypeList += "\",";
    }
    relationName.pop_back();
    conditionsString.pop_back();
    varTypeList.pop_back();
    
    std::string headerFiles =offset(0)+
        "#include <fstream>\n"+
        "#include <vector>\n"+
        "#include \"RegressionTreeNode.hpp\"\n\n"+
        "#include <boost/spirit/include/qi.hpp>\n"+
        "#include <boost/spirit/include/phoenix_core.hpp>\n"+
        "#include <boost/spirit/include/phoenix_operator.hpp>\n\n"+
        "namespace qi = boost::spirit::qi;\n"+
        "namespace phoenix = boost::phoenix;\n\n";
    
    std::string genDynFuncs =
        "void generateDynamicFunctions(std::vector<Condition>& conditions)\n{\n"+
	offset(1)+"std::string variableMap[] = {"+varList+"};\n"+
	offset(1)+"std::string variableList[] = {"+varTypeList+"};\n"+
	offset(1)+"size_t relationMap[] = {"+relationMap+"};\n"+
	offset(1)+"std::string relationName[] = {"+relationName+"};\n"+
        offset(1)+"std::string conditionStr[] = {"+conditionsString+"};\n"+
        offset(1)+"for (Condition& c : conditions)\n"+
        offset(2)+"conditionStr[relationMap[c.variable]] += variableMap[c.variable] + "+
        "c.op + std::to_string(c.threshold) + \"&&\";\n"+
        offset(1)+"std::ofstream ofs(\"DynamicFunctions.cpp\", "+
        "std::ofstream::out);\n"+
        offset(1)+"ofs << \"#include \\\"DynamicFunctions.h\\\"\\n"+
        "namespace lmfao\\n{\\n\";\n"+
        offset(1)+"for (size_t rel = 0; rel < "+
        std::to_string(_db->numberOfRelations())+"; ++rel)\n"+offset(1)+"{\n"+
        offset(2)+"if (!conditionStr[rel].empty())\n"+offset(2)+"{\n"+
        offset(3)+"conditionStr[rel].pop_back();\n"+
        offset(3)+"conditionStr[rel].pop_back();\n"+
        offset(3)+"ofs << \"double f_\"+relationName[rel]+\"(\"+"+
        "variableList[rel]+\")\\n"+
        "{\\n\treturn (\"+conditionStr[rel]+\" ? 1.0 : 0.0);\\n}\\n\";\n"+
        offset(2)+"}\n"+offset(2)+"else\n"+offset(2)+"{\n"+
        offset(3)+"ofs << \"double f_\"+relationName[rel]+"+
        "\"(\"+variableList[rel]+\")\\n"+
        "{\\n\\treturn 1.0;\\n}\\n\";\n"+
        offset(2)+"}\n"+offset(1)+"}\n"+
        offset(1)+"ofs << \"}\\n\";\n"+offset(1)+"ofs.close();\n}\n";


    /********************************************************/
    /************ This is for loading Test Data *************/
    /********************************************************/
    
    // TODO: This should only output the features not all variables.
    // If changed it should be changed in the SQL generator as well.
    std::string attributeString = "",
        attrConstruct = offset(3)+"qi::phrase_parse(tuple.begin(),tuple.end(),";
        
    for (size_t var = 0; var < _db->numberOfAttributes(); ++var)
    {
        Attribute* att = _db->getAttribute(var);
        attrConstruct += "\n"+offset(4)+"qi::"+typeToStr(att->_type)+
            "_[phoenix::ref("+att->_name+") = qi::_1]>>";
        attributeString += offset(1)+typeToStr(att->_type) + " "+
            att->_name + ";\n";
    }

    // attrConstruct.pop_back();
    attrConstruct.pop_back();
    attrConstruct.pop_back();
    attrConstruct += ",\n"+offset(4)+"\'|\');\n";

    std::string testTuple = "\nstruct Test_tuple\n{\n"+
        attributeString+offset(1)+"Test_tuple(const std::string& tuple)\n"+
        offset(1)+"{\n"+attrConstruct+offset(1)+"}\n};\n\n";

    std::string loadFunction =
        "void loadTestDataset(std::vector<Test_tuple>& TestDataset)\n{\n"+
        offset(1)+"std::ifstream input;\n"+offset(1)+"std::string line;\n"+
        offset(1)+"input.open(\""+PATH_TO_DATA+"/test_data.tbl\");\n"+
        offset(1)+"if (!input)\n"+offset(1)+"{\n"+
        offset(2)+"std::cerr << \"TestDataset.tbl does is not exist.\\n\";\n"+
        offset(2)+"exit(1);\n"+offset(1)+"}\n"+
        offset(1)+"while(getline(input, line))\n"+
        offset(2)+"TestDataset.push_back(Test_tuple(line));\n"+
        offset(1)+"input.close();\n}\n\n";
    

    /****************************************************************/
    /************ This is for finding prediction & eval *************/
    /****************************************************************/

    std::string findPredSwitch=""; 
    for (size_t var = 0; var< _db->numberOfAttributes(); ++var)
    {
        const std::string& varName = _db->getAttribute(var)->_name;
        findPredSwitch += offset(3)+"case "+std::to_string(var)+" : "+
            "goLeft = (tuple."+varName+condition[_categoricalFeatures[var]]+
            "c.threshold); break;\n";       
    }
    
    std::string findPrediction =
        "double findPrediction(const RegTreeNode* node, const Test_tuple& tuple)\n{\n"+
        offset(1)+"if (node->isLeaf)\n"+
        offset(2)+"return node->prediction;\n\n"+
        offset(1)+"bool goLeft = true;\n"+
        offset(1)+"const Condition& c = node->condition;\n"+
        offset(1)+"switch (c.variable)\n"+
        offset(1)+"{\n"+findPredSwitch+offset(1)+"}\n"+
        offset(1)+"if (goLeft)\n"+
        offset(2)+"return findPrediction(node->lchild, tuple);\n"+
        offset(1)+"else\n"+
        offset(2)+"return findPrediction(node->rchild, tuple);\n}\n\n";
    
    std::string evalFunction = "void evaluateModel(const RegTreeNode* root)\n{\n"+
        offset(1)+"std::vector<Test_tuple> TestDataset;\n"+
        offset(1)+"loadTestDataset(TestDataset);\n"+
        offset(1)+"double pred, diff, error = 0.0;\n"+
        offset(1)+"for (const Test_tuple& tuple : TestDataset)\n"+offset(2)+"{\n"+
        offset(2)+"pred = findPrediction(root, tuple);\n"+
        offset(2)+"diff = tuple."+_db->getAttribute(_labelID)->_name+"-pred;\n"+
        offset(2)+"error += diff * diff;\n"+
        offset(1)+"}\n"+
        offset(1)+"error /= TestDataset.size();\n"+
        offset(1)+"std::cout << \"RMSE: \" << sqrt(error) << std::endl;\n}\n\n";
    
    return headerFiles+testTuple+loadFunction+findPrediction+evalFunction+genDynFuncs;
}


std::string RegressionTree::genVarianceComputation()
{
    std::vector<std::vector<std::string>> variancePerView(_compiler->numberOfViews());
    //TODO: rename this to thresholdPerView
    std::vector<std::vector<std::string>> functionPerView(_compiler->numberOfViews());
    std::vector<size_t> firstThreshold(NUM_OF_VARIABLES);
    std::vector<Query*> complementQueryPerView(_compiler->numberOfViews());
    
    for (QueryThresholdPair& qtPair : queryToThreshold)
    {
        if (_categoricalFeatures[qtPair.varID])
        {
            // Find the view that corresponds to the query
            Query* query = qtPair.query;
            size_t& viewID = query->_incoming[0].first;
            size_t& viewID2 = query->_incoming[1].first;
            size_t& viewID3 = query->_incoming[2].first;

            Query* complement = qtPair.complementQuery;
            const size_t& comp_viewID = complement->_incoming[0].first;

            if (viewID != viewID2 || viewID != viewID3)
                std::cout << "THERE IS AN ERROR IN genVarianceComputation" << std::endl;
        
            const size_t& count = query->_incoming[0].second;
            const size_t& linear = query->_incoming[1].second;
            const size_t& quad = query->_incoming[2].second;

            const size_t& comp_count = complement->_incoming[0].second;
            // const size_t& comp_linear=complement->_incoming[1].second;
            // const size_t& comp_quad = complement->_incoming[2].second;
        
            std::string viewTup = "V"+std::to_string(viewID)+"tuple";
        
            // variancePerView[viewID].push_back(
            //     "("+viewTup+".aggregates["+std::to_string(count)+"] > 0 ? "+
            //     viewTup+".aggregates["+std::to_string(quad)+"] - ("+
            //     viewTup+".aggregates["+std::to_string(linear)+"] * "+
            //     viewTup+".aggregates["+std::to_string(linear)+"]) / "+
            //     viewTup+".aggregates["+std::to_string(count)+"] : 999);\n"
            //     );
            
            variancePerView[viewID].push_back(
                viewTup+".aggregates["+std::to_string(quad)+"] - ("+
                viewTup+".aggregates["+std::to_string(linear)+"] * "+
                viewTup+".aggregates["+std::to_string(linear)+"]) / "+
                viewTup+".aggregates["+std::to_string(count)+"] + "+
                "difference[2] - (difference[1] * difference[1]) / difference[0];\n"
                );

            complementQueryPerView[viewID] = complement;
            
            Attribute* att = _db->getAttribute(qtPair.varID);

            // functionPerView[viewID].push_back(
            //     "\"Variable: "+std::to_string(qtPair.varID)+" "+
            //     "Threshold: \"+std::to_string(tuple."+att->_name+")+\" "+
            //     "Operator: = \";\n");

            // TODO:TODO:TODO:TODO:TODO:TODO: WHY DOES THIS SAY AGGREGATES[QUAD] !?!?!
            functionPerView[viewID].push_back(
                ".set("+std::to_string(qtPair.varID)+",tuple."+att->_name+",1,"+
                "&tuple.aggregates["+std::to_string(count)+"],"+
                "&V"+std::to_string(comp_viewID)+"[0].aggregates["+
                std::to_string(comp_count)+"]"+");\n");
        }
        else
        {   // Continuous variable
            
            // Find the view that corresponds to the query
            Query* query = qtPair.query;
            size_t& viewID = query->_incoming[0].first;
            size_t& viewID2 = query->_incoming[1].first;
            size_t& viewID3 = query->_incoming[2].first;

            size_t& cviewID = query->_incoming[3].first;
            size_t& cviewID2 = query->_incoming[4].first;
            size_t& cviewID3 = query->_incoming[5].first;

            if (viewID != viewID2 || viewID != viewID3 ||
                cviewID != viewID || cviewID2 != viewID || cviewID3 != viewID  )
                std::cout << "THERE IS AN ERROR IN genVarianceComputation" << std::endl;
        
            const size_t& count = query->_incoming[0].second;
            const size_t& linear = query->_incoming[1].second;
            const size_t& quad = query->_incoming[2].second;

            const size_t& compcount = query->_incoming[3].second;
            const size_t& complinear = query->_incoming[4].second;
            const size_t& compquad = query->_incoming[5].second;
        
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
                viewTup+".aggregates["+std::to_string(compcount)+"] : 1.79769e+308);\n"
                );
           
            // functionPerView[viewID].push_back(
            //     "\"Variable: "+std::to_string(qtPair.varID)+" "+
            //     "Threshold: "+std::to_string(qtPair.function->_parameter[0])+" "+
            //     "Operator: < \";\n");
            functionPerView[viewID].push_back(".set("+std::to_string(qtPair.varID)+","+
                std::to_string(qtPair.function->_parameter[0])+",0,"+
                "&V"+std::to_string(viewID)+"[0].aggregates["+std::to_string(count)+"],"+
                "&V"+std::to_string(cviewID)+"[0].aggregates["+
                std::to_string(compcount)+"]"+");\n");
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
            size_t& viewID = _varToQueryMap[var]->_incoming[0].first;   
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
                // categVariance += offset(2)+"for ("+viewString+"_tuple& "+
                //     viewString+"tuple : "+viewString+")\n";
                // for (size_t j = 0; j < variancePerView[viewID].size(); ++j)
                // {
                //     categVariance += offset(3)+
                //         "variance[categIdx++] = "+variancePerView[viewID][j];
                    
                //     categFunctionIndex += 
                //         offset(2)+"for (V"+std::to_string(viewID)+"_tuple& tuple : "+
                //         "V"+std::to_string(viewID)+")\n"+offset(3)+
                //         "thresholdMap[categIdx++] = "+functionPerView[viewID][j];
                // }
                
                if (variancePerView[viewID].size() > 1)
                {
                    ERROR("THIS IS ODD, EXCEPCTED SIZE IS 1;\n");
                    exit(1);
                }

                Query* complement = complementQueryPerView[viewID];
                const size_t& comp_viewID = complement->_incoming[0].first;
                const size_t& comp_count = complement->_incoming[0].second;
                const size_t& comp_linear = complement->_incoming[1].second;
                const size_t& comp_quad = complement->_incoming[2].second;

                if (comp_count != comp_linear-1 || comp_count != comp_quad-2 ||
                    comp_linear != comp_quad-1)
                {    
                    ERROR("We should have expected the aggregates to be contiguous!\n");
                    exit(1);
                }

                // std::cout << "complement: " << comp_viewID << "  " << viewID << "\n";
                // std::cout << complement->_fVars << std::endl;
                // std::cout << _compiler->getView(viewID)->_fVars << std::endl;
                
                categVariance += offset(2)+"compaggs = &V"+std::to_string(comp_viewID)+
                    "[0].aggregates["+std::to_string(comp_count)+"];\n"+
                    offset(2)+"for ("+viewString+"_tuple& "+viewString+"tuple : "+
                    viewString+")\n"+offset(2)+"{\n"+
                    offset(3)+"if("+viewString+"tuple.aggregates[0] == 0 || "+
                    viewString+"tuple.aggregates[0] == compaggs[0])\n"+
                    offset(3)+"{\n"+
                    offset(4)+"variance[categIdx++] = 1.79769e+308;\n"+
                    offset(4)+"continue;\n"+
                    offset(3)+"}\n"+
                    offset(3)+"for (size_t i=0; i < 3; ++i)\n"+
                    offset(4)+"difference[i] = compaggs[i] - "+
                    viewString+"tuple.aggregates[i];\n"+
                    offset(3)+"variance[categIdx++] = "+variancePerView[viewID][0]+
                    offset(2)+"}\n";
                
                // categFunctionIndex +=offset(2)+"for (V"+std::to_string(viewID)+
                //     "_tuple& tuple : V"+std::to_string(viewID)+")\n"+offset(3)+
                //     "thresholdMap[categIdx++] = "+functionPerView[viewID][0];
                categFunctionIndex +=offset(2)+"for (V"+std::to_string(viewID)+
                    "_tuple& tuple : V"+std::to_string(viewID)+")\n"+offset(3)+
                    "thresholdMap[categIdx++]"+functionPerView[viewID][0];
                
                // for (size_t j = 0; j < variancePerView[viewID].size(); ++j)
                // {
                //     categVariance += "variance[categIdx++] = "+
                //         variancePerView[viewID][0];
                //     categFunctionIndex += 
                //         offset(2)+"for (V"+std::to_string(viewID)+"_tuple& tuple : "+
                //         "V"+std::to_string(viewID)+")\n"+offset(3)+
                //         "thresholdMap[categIdx++] = "+functionPerView[viewID][j];
                // }
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

                    // contFunctionIndex += offset(2)+
                    //     "thresholdMap["+std::to_string(varianceCount)+"] = "+
                    //     functionPerView[viewID][j];
                    contFunctionIndex += offset(2)+
                        "thresholdMap["+std::to_string(varianceCount)+"]"+
                        functionPerView[viewID][j];
                    
                    ++varianceCount;
                }
            }
        }
    }

    std::string computeVariance = contVariance;
    
    if (!categVariance.empty())
    {    
        computeVariance += offset(2)+"size_t categIdx = "+
            std::to_string(numOfContThresholds)+";\n"+
            offset(2)+"double difference[3], *compaggs;\n"+
            categVariance;
    }
    
    computeVariance += "\n"+offset(2)+"double min_variance = variance[0];\n"+
        offset(2)+"size_t threshold = 0;\n\n"+
        offset(2)+"for (size_t t=1; t < numberOfThresholds; ++t)\n"+offset(2)+"{\n"+
        offset(3)+"if (variance[t] < min_variance)\n"+offset(3)+"{\n"+
        offset(4)+"min_variance = variance[t];\n"+offset(4)+"threshold = t;\n"+
        offset(3)+"}\n"+offset(2)+"}\n"+
        offset(2)+"std::cout << \"The minimum variance is: \" << min_variance <<"+
        "\" for variable \" << thresholdMap[threshold].varID << \" and threshold:"+
        " \" << thresholdMap[threshold].threshold << std::endl;\n"+
        offset(2)+"std::ofstream ofs(\"bestsplit.out\",std::ofstream::out);\n"+
        offset(2)+"ofs << std::fixed << min_variance << \"\\t\" << "+
        "thresholdMap[threshold].varID << \"\\t\" << "+
        "thresholdMap[threshold].threshold  << \"\\t\" << "+
        "thresholdMap[threshold].categorical  << \"\\t\" << "+
        "thresholdMap[threshold].aggregates[0]  << \"\\t\" << "+
        "thresholdMap[threshold].compAggregates[0] << \"\\t\" << "+
        "thresholdMap[threshold].aggregates[1]/thresholdMap[threshold].aggregates[0] << \"\\t\" << "+
        "thresholdMap[threshold].compAggregates[1]/thresholdMap[threshold].compAggregates[0] << std::endl;\n"+
        offset(2)+"ofs.close();\n";

    std::string functionIndex = contFunctionIndex;

    if (!categFunctionIndex.empty())
    {
        functionIndex += offset(2)+"size_t categIdx = "+
            std::to_string(numOfContThresholds)+";\n"+categFunctionIndex;
    }

    std::string thresholdStruct = offset(1)+"struct Threshold\n"+offset(1)+"{\n"+
        offset(2)+"size_t varID;\n"+offset(2)+"double threshold;\n"+
        offset(2)+"bool categorical;\n"+offset(2)+"double* aggregates;\n"+
        offset(2)+"double* compAggregates;\n"+offset(2)+
        "Threshold() : varID(0), threshold(0), categorical(false), "+
        "aggregates(nullptr), compAggregates(nullptr) {}\n"+
        offset(2)+"void set(size_t var, double t, bool c, double* agg, double *cagg)\n"+
        offset(2)+"{\n"+offset(3)+"varID = var;\n"+offset(3)+"threshold = t;\n"+
        offset(3)+"categorical = c;\n"+offset(3)+"aggregates = agg;\n"+
        offset(3)+"compAggregates = cagg;\n"+
        offset(2)+"}\n"+offset(1)+"};\n\n";
    
    return thresholdStruct+
        offset(1)+"size_t numberOfThresholds;\n"+
        offset(1)+"double* variance = nullptr;\n"+
        offset(1)+"Threshold* thresholdMap = nullptr;\n\n"+
        offset(1)+"void initCostArray()\n"+
        offset(1)+"{\n"+initVariance+
        // offset(2)+"thresholdMap = new std::string[numberOfThresholds];\n"+
        offset(2)+"thresholdMap = new Threshold[numberOfThresholds];\n"+
        functionIndex+offset(1)+"}\n\n"+
        offset(1)+"void computeCost()\n"+
        offset(1)+"{\n"+computeVariance+offset(1)+"}\n";    
}


std::string RegressionTree::genGiniComputation()
{
    // std::vector<std::vector<std::string>> giniPerView(_compiler->numberOfViews());
    std::vector<std::vector<std::string>> thresholdPerView(_compiler->numberOfViews());
    std::vector<Query*> complementQueryPerView(_compiler->numberOfViews());
    std::vector<size_t> firstThresholdPerVariable(NUM_OF_VARIABLES);
    
    QueryThresholdPair& continuousPair = queryToThreshold[0];
    // QueryThresholdPair& countPair = queryToThreshold[1];

    Query* continuousQuery = continuousPair.query;
    Query* countQuery = continuousPair.complementQuery;

    const size_t& continuousViewID = continuousQuery->_incoming[0].first;
    const size_t& countViewID = countQuery->_incoming[0].first;

    string viewStr = "V"+std::to_string(continuousViewID);
    string countViewStr = "V"+std::to_string(countViewID);
    string overallCountViewStr = "V"+std::to_string(countViewID);

    string label = _db->getAttribute(_labelID)->_name;
    
    string numAggs = std::to_string(_functionOfAggregate.size());

    std::string returnString = "";

    /* This creates the gini computation for continuous variables. */ 
    returnString += offset(2)+"double squaredSum["+numAggs+"] = {};\n"+
        offset(2)+"std::vector<double> prediction (numberOfThresholds*2, 0.0);\n"+
        offset(2)+"std::vector<double> largestCount (numberOfThresholds*2, 0.0);\n"+
        offset(2)+"for("+viewStr+"_tuple& tuple : "+viewStr+")\n"+offset(2)+"{\n"+
        offset(3)+"for (size_t agg = 1; agg < "+numAggs+"; ++agg)\n"+
        offset(3)+"{\n"+
        offset(4)+"squaredSum[agg] += tuple.aggregates[agg] * tuple.aggregates[agg];\n"+
        offset(4)+"if (tuple.aggregates[agg] > largestCount[agg])\n"+offset(4)+"{\n"+                                                  
        offset(5)+"largestCount[agg] = tuple.aggregates[agg];\n"+                                                                      
        offset(5)+"prediction[agg] = tuple."+label+";\n"+                                                                              
        offset(4)+"}\n"+offset(3)+"}\n"+offset(2)+"}\n"+
        offset(2)+countViewStr+"_tuple& tup = "+countViewStr+"[0];\n"+
        offset(2)+"for (size_t agg = 1, g = 0; agg < "+numAggs+"; agg += 2, ++g)\n"+
        offset(2)+"{\n"+
        offset(3)+"if (tup.aggregates[agg] > 0 && tup.aggregates[agg+1] > 0)\n"+
        offset(4)+"gini[g] = "+
        "(1 - squaredSum[agg]/(tup.aggregates[agg] * tup.aggregates[agg])) * "+
        "tup.aggregates[agg]/tup.aggregates[0] + "+
        "(1 - squaredSum[agg+1]/(tup.aggregates[agg+1] * tup.aggregates[agg+1])) * "+
        "tup.aggregates[agg+1]/tup.aggregates[0];\n"+
        offset(3)+"else\n"+offset(4)+"gini[g] = 999;\n"+offset(2)+"}\n";

    // If there are categorical variables we need to initialize some extra vars
    if (queryToThreshold.size() > 1)
    {
        returnString +=
            "\n"+offset(2)+"double lhs = 0.0, rhs = 0.0, complementCount = 0.0;\n"+
            offset(2)+"const double& overallCount = tup.aggregates[0];\n"+
            offset(2)+"size_t idx = 0, categIndex = "+
            std::to_string(_functionOfAggregate.size() / 2)+";\n\n";
    }

    // The below is just a check and can probably be removed ! 
    // size_t numOfContThresholds = 0;
    // for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
    // {
    //     if (!_features.test(var) || _categoricalFeatures.test(var))
    //         continue;
    //     numOfContThresholds += _thresholds[var].size();
    // }
    // if (numOfContThresholds != (_functionOfAggregate.size()-1) / 2)
    //     std::cout << "WHY IS THIS THIS CASE!?!?!?\n";
    // Check done! 

    std::string numOfThresholds = "numberOfThresholds = "+
        std::to_string(_functionOfAggregate.size() / 2);

    std::string thresholdMap = "";

#if defined(__GNUC__) && defined(NDEBUG) && !defined(__clang__)
    std::string sortAlgo = "__gnu_parallel::sort(";
#else
    std::string sortAlgo = "std::sort(";
#endif

    std::string sortViews = "";

    size_t idx = 0;
    for (size_t f = 1; f < _functionOfAggregate.size(); f += 2)
    {
        Function* function = _functionOfAggregate[f];
        thresholdMap += offset(2)+"thresholdMap["+to_string(idx++)+"].set("+
            to_string(_varOfFunction[f])+","+
            to_string(function->_parameter[0])+",0,"+
            "&"+countViewStr+"[0].aggregates["+to_string(f)+"],"+
            "&"+countViewStr+"[0].aggregates["+to_string(f+1)+"]);\n";
    }


    // Then iterate over the other queries and go from there
    for (size_t p = 1; p < queryToThreshold.size(); ++p)
    {
        QueryThresholdPair& qtPair = queryToThreshold[p];
        
        if (_categoricalFeatures[qtPair.varID])
        {                        
            // Find the view that corresponds to the query
            Query* query = qtPair.query;
            Query* varCountQuery = qtPair.complementQuery;
            
            size_t& viewID = query->_incoming[0].first;
            size_t& countViewID = varCountQuery->_incoming[0].first;

            // const size_t& count = query->_aggregates[0]->_incoming[0].second;
            // const size_t& varCount=varCountQuery->_aggregates[0]->_incoming[0].second;

            std::string viewStr = "V"+std::to_string(viewID);
            string labelViewStr = "V"+std::to_string(continuousViewID);
            std::string countViewStr = "V"+std::to_string(countViewID);
            
            std::string label = _db->getAttribute(_labelID)->_name;
            std::string var = _db->getAttribute(qtPair.varID)->_name;
            
            returnString += offset(2)+"lhs = 0.0; rhs = 0.0; idx = 0;\n"+
                offset(2)+"for (size_t v = 0; v < "+countViewStr+".size(); ++v)\n"+
                offset(2)+"{\n"+
                offset(3)+countViewStr+"_tuple& varCountTup = "+countViewStr+"[v];\n"+
                offset(3)+"if (varCountTup.aggregates[0] == 0.0)\n"+offset(3)+"{\n"+
                offset(4)+"while(varCountTup."+var+" == "+viewStr+"[idx]."+var+")\n"+
                offset(5)+"++idx;\n"+offset(4)+"gini[categIndex++] = 999;\n"+
                offset(4)+"continue;\n"+offset(3)+"}\n"+
                offset(3)+"for (size_t l = 0; l < "+labelViewStr+".size(); ++l)\n"+
                offset(3)+"{\n"+
                offset(4)+viewStr+"_tuple& tuple = "+viewStr+"[idx];\n"+
                offset(4)+"if ("+labelViewStr+"[l]."+label+" != tuple."+label+")\n"+
                offset(4)+"{\n"+offset(5)+"//TODO: what to do here in this case?!\n"+
                offset(5)+"rhs += "+labelViewStr+"[l].aggregates[0] * "+labelViewStr+
                "[l].aggregates[0];\n"+offset(5)+"continue;\n"+offset(4)+"}\n"+
                offset(4)+"lhs += tuple.aggregates[0] * tuple.aggregates[0];\n"+
                offset(4)+"complementCount = "+labelViewStr+"[l].aggregates[0] - "+
                "tuple.aggregates[0];\n"+
                offset(4)+"rhs += complementCount * complementCount;\n"+
                offset(4)+"if (tuple."+var+" != varCountTup."+var+") "+
                "std::cout << \"ERROR \\n\";\n"+                
                offset(4)+"if (tuple.aggregates[0] > largestCount[categIndex*2])\n"+offset(4)+"{\n"+
                offset(5)+"largestCount[categIndex*2] = tuple.aggregates[0];\n"+
                offset(5)+"prediction[categIndex*2] = tuple."+label+";\n"+
                offset(4)+"}\n"+
                offset(4)+"if (complementCount > largestCount[categIndex*2+1])\n"+offset(4)+"{\n"+
                offset(5)+"largestCount[categIndex*2+1] = complementCount;\n"+
                offset(5)+"prediction[categIndex*2+1] = tuple."+label+";\n"+
                offset(4)+"}\n"+
                offset(4)+"++idx;\n"+offset(3)+"}\n"+
                offset(3)+"gini[categIndex] = (1 - lhs/"+
                "(varCountTup.aggregates[0]*varCountTup.aggregates[0])) "+
                "* varCountTup.aggregates[0] / overallCount;\n"+
                offset(3)+"complementCount = overallCount-varCountTup.aggregates[0];\n"+
                offset(3)+"gini[categIndex] += (1- rhs/"+
                "(complementCount*complementCount)) * complementCount/overallCount;\n"+
                offset(3)+"lhs = 0.0; rhs = 0.0;\n"+
                offset(3)+"++categIndex;\n"+offset(2)+"}\n\n";
            
            numOfThresholds += " + V"+std::to_string(countViewID)+".size()";

            Attribute* att = _db->getAttribute(qtPair.varID);
            thresholdMap +=
                offset(2)+"for ("+countViewStr+"_tuple& tuple : "+countViewStr+")\n"+
                offset(3)+"thresholdMap[categIndex++].set("
                +std::to_string(qtPair.varID)+",tuple."+att->_name+",1,"+
                "&tuple.aggregates[0],&"+overallCountViewStr+"[0].aggregates[0]);\n";

            if ((size_t) _labelID < qtPair.varID)
            {
                sortViews += offset(2)+sortAlgo+viewStr+".begin(),"+viewStr+".end(),"+
                    "[ ](const "+viewStr+"_tuple& lhs, const "+viewStr+"_tuple& rhs)\n"+
                    offset(3)+"{\n"+
                    offset(4)+"if(lhs."+var+" != rhs."+var+")\n"+
                        offset(5)+"return lhs."+var+" < rhs."+var+";\n"+
                    offset(4)+"return lhs."+label+" < rhs."+label+";\n"+
                    offset(3)+"});\n";
            }
        }
        else
        {
            // Continuous variable
            cout << " WE SHOULD NEVER GET HERE !!! \n";
        }
    }

    
    std::string initGini = offset(2)+numOfThresholds+";\n"+
        offset(2)+"gini = new double[numberOfThresholds];\n"+
        offset(2)+"size_t categIndex = "+
        std::to_string(_functionOfAggregate.size()/2)+";\n";
    
    std::string thresholdStruct = offset(1)+"struct Threshold\n"+offset(1)+"{\n"+
        offset(2)+"size_t varID;\n"+offset(2)+"double threshold;\n"+
        offset(2)+"bool categorical;\n"+offset(2)+"double* aggregates;\n"+
        offset(2)+"double* compAggregates;\n"+offset(2)+
        "Threshold() : varID(0), threshold(0), categorical(false), "+
        "aggregates(nullptr), compAggregates(nullptr) {}\n"+
        offset(2)+"void set(size_t var, double t, bool c, double* agg, double *cagg)\n"+
        offset(2)+"{\n"+offset(3)+"varID = var;\n"+offset(3)+"threshold = t;\n"+
        offset(3)+"categorical = c;\n" +offset(3)+"aggregates = agg;\n"+
        offset(3)+"compAggregates = cagg;\n"+
        offset(2)+"}\n"+offset(1)+"};\n\n";


    returnString += "\n"+offset(2)+"double min_gini = gini[0];\n"+
        offset(2)+"size_t threshold = 0;\n\n"+
        offset(2)+"for (size_t t=1; t < numberOfThresholds; ++t)\n"+offset(2)+"{\n"+
        offset(3)+"if (gini[t] < min_gini)\n"+offset(3)+"{\n"+
        offset(4)+"min_gini = gini[t];\n"+offset(4)+"threshold = t;\n"+
        offset(3)+"}\n"+offset(2)+"}\n"+
        offset(2)+"std::cout << \"The minimum gini index is: \" << min_gini <<"+
        "\" for variable \" << thresholdMap[threshold].varID << \" and threshold:"+
        " \" << thresholdMap[threshold].threshold << std::endl;\n"+
        offset(2)+"std::ofstream ofs(\"bestsplit.out\",std::ofstream::out);\n"+
        offset(2)+"ofs << std::fixed << min_gini << \"\\t\" << "+
        "thresholdMap[threshold].varID << \"\\t\" << "+
        "thresholdMap[threshold].threshold  << \"\\t\" << "+
        "thresholdMap[threshold].categorical  << \"\\t\" << "+
        "thresholdMap[threshold].aggregates[0]  << \"\\t\" << "+
        "thresholdMap[threshold].compAggregates[0]  << \"\\t\" << "+
        "prediction[threshold*2]  << \"\\t\" << "+
        "prediction[threshold*2+1] << std::endl;\n"+
        offset(2)+"ofs.close();\n";
    
    return thresholdStruct+
        offset(1)+"size_t numberOfThresholds;\n"+
        offset(1)+"double* gini = nullptr;\n"+
        offset(1)+"Threshold* thresholdMap = nullptr;\n\n"+
        offset(1)+"void initCostArray()\n"+
        offset(1)+"{\n"+initGini+
        offset(2)+"thresholdMap = new Threshold[numberOfThresholds];\n"+
        thresholdMap+sortViews+offset(1)+"}\n\n"+
        offset(1)+"void computeCost()\n"+
        offset(1)+"{\n"+returnString+offset(1)+"}\n";
}



void RegressionTree::generateCode()
{
    std::string runFunction = offset(1)+"void runApplication()\n"+offset(1)+"{\n"+
        offset(2)+"int64_t startProcess = duration_cast<milliseconds>("+
        "system_clock::now().time_since_epoch()).count();\n"+
        offset(2)+"initCostArray();\n"+
        offset(2)+"computeCost();\n"+
        "\n"+offset(2)+"int64_t endProcess = duration_cast<milliseconds>("+
        "system_clock::now().time_since_epoch()).count()-startProcess;\n"+
        offset(2)+"std::cout << \"Run Application: \"+"+
        "std::to_string(endProcess)+\"ms.\\n\";\n\n"+
        offset(2)+"std::ofstream ofs(\"times.txt\",std::ofstream::out | " +
        "std::ofstream::app);\n"+
        offset(2)+"ofs << \"\\t\" << endProcess << std::endl;\n"+
        offset(2)+"ofs.close();\n\n"+
        // offset(2)+"evaluateModel();\n"+
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
    
    ofs << "#include \"ApplicationHandler.h\"\nnamespace lmfao\n{\n";
    if (_classification)
        ofs << genGiniComputation() << std::endl;
    else
        ofs << genVarianceComputation() << std::endl;
    ofs << runFunction << std::endl;
    ofs << "}\n";
    ofs.close();



    genDynamicFunctions();
}



void RegressionTree::initializeThresholds()
{
    if (DATASET_NAME.compare("example") == 0)
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
    
    if (DATASET_NAME.compare("retailer") == 0 ||
        DATASET_NAME.compare("usretailer") == 0 ||
        DATASET_NAME.compare("usretailer_ml") == 0)
    {
        // _thresholds =
        //     {
        //         {}, // locn
        //         {}, // dateid
        //         {}, // ksn 
        //         {}, // inventory_units
        //         {}, // zip 
        //         {1,8}, // rgn_code TODO: TODO: 
        //         {1,1,2,3,4,5}, // clim_zn_nbr  TODO:  TODO: 
        //         {6.8444,8.2101,8.38184,8.418,8.5584,8.6479,8.6752,8.75316,8.94666,9.1594,9.45,9.484,9.5933,9.74834,10.2716,10.49996,10.8474,11.67102,12.53952},
        //         {5.14708,6.59456,6.75694,6.9867,7.1428,7.23374,7.28834,7.34834,7.40032,7.4443,7.5089,7.58436,7.83488,7.97012,8.094,8.41024,8.94032,9.22284,9.88272},
        //         {4.8677,5.10068,5.29006,5.42888,5.5881,5.72704,5.87592,6.07782,6.29424,6.4877,6.7022,6.7022,6.7022,6.96112,7.2848,7.71394,8.0942,8.62438,9.62584},
        //         {5.463098,11.910442,15.46413,15.46413,15.46413,15.46413,15.46413,15.46413,15.46413,15.46413,15.46413,15.46413,15.46413,15.46413,15.46413,15.46413,15.46413,15.46413,24.701992},
        //         {9.63648,15.82272,19.10509,19.10509,19.10509,19.10509,19.10509,19.10509,19.10509,19.10509,19.10509,19.10509,19.10509,19.10509,19.10509,19.10509,19.10509,19.10509,26.72928},
        //         {0.922112,1.409274,2.01697,2.555082,3.14414,3.800304,4.524828,5.470554,6.587774,7.77957,8.17724,8.17724,8.17724,8.17724,8.17724,8.17724,10.988578,17.148604,26.414984},
        //         {2.51712,3.82464,4.93344,5.76864,6.5952,7.42752,8.44704,9.52992,11.07648,11.8131,11.8131,11.8131,11.8131,11.8131,11.8131,13.2768,15.78816,20.95776,30.01248},
        //         {2.279188,3.826404,5.346278,7.176834,9.25843,12.77042,14.91195,14.91195,14.91195,14.91195,14.91195,14.91195,14.91195,14.91195,14.91195,15.980424,21.257112,27.055744,35.04919},
        //         {5.2992,7.52544,9.41472,11.74464,14.2704,17.2944,19.01726,19.01726,19.01726,19.01726,19.01726,19.01726,19.01726,19.01726,19.01726,20.51712,25.6176,30.81888,38.58336},
        //         {0.49834,0.9072,1.257654,1.585742,1.98839,2.355,2.72782,3.20628,3.748112,4.33096,5.091512,5.48422,5.48422,5.48422,5.49914,6.523156,8.026004,11.287834,16.754654},
        //         {1.56672,2.50272,3.2256,3.9456,4.7664,5.35104,5.94144,6.5808,7.3008,8.2512,8.94215,8.94215,8.94215,8.94215,9.7488,11.088,12.90816,16.51392,23.36544},
        //         {0.879565,1.20835,1.48906,1.71446,1.9222,2.12759,2.33187,2.5418,2.767985,2.9511,3.099045,3.31318,3.48249,3.73503,3.97215,4.17808,4.53785,5.09063,5.90789},
        //         {0.367075,0.68876,0.86049,1.03922,1.21985,1.37419,1.49291,1.63308,1.76269,1.89735,2.04024,2.18968,2.35554,2.53027,2.6794,2.84528,3.07802,3.36536,3.86612},
        //         {0.005,0.00791,0.0101,0.0137,0.0176,0.0216,0.0259,0.03094,0.03878,0.0465,0.05501,0.06576,0.07733,0.09527,0.115,0.13914,0.180325,0.24962,0.41677},
        //         {0.0001,0.0002,0.0003,0.0004,0.0005,0.0006,0.0007,0.0009,0.001,0.0012,0.0014,0.0017,0.0021,0.0026,0.0033,0.0043,0.0064,0.00889,0.02037},
        //         {0.006005,0.01102,0.0183,0.027,0.0374,0.05186,0.069005,0.09134,0.111335,0.13965,0.1758,0.2129,0.26073,0.32154,0.3914,0.48164,0.6759749,0.93456,1.38585},
        //         {2.92,3.15,3.2915,3.412,3.51,3.593,3.67,3.75,3.8,3.87,3.93,3.98,4.03,4.097,4.16,4.24,4.32,4.44,4.6095},
        //         {0.36597,0.48414,0.5933,0.68434,0.758725,0.8501,0.930775,1.00754,1.098125,1.16395,1.227335,1.29482,1.36063,1.44301,1.52965,1.61848,1.733755,1.91537,2.163435},
        //         {0.41592,0.55556,0.66975,0.76792,0.853425,0.94507,1.03729,1.12624,1.20527,1.28435,1.351665,1.42398,1.502865,1.5908,1.681625,1.77568,1.900845,2.07273,2.343025},
        //         {0.230335,0.31326,0.3778,0.44232,0.487575,0.53859,0.59594,0.64704,0.707025,0.7437,0.7858,0.83144,0.88467,0.92641,0.9951,1.06248,1.136925,1.30247,1.45904},
        //         {0.36597,0.48414,0.5933,0.68434,0.758725,0.8501,0.930775,1.00754,1.098125,1.16395,1.227335,1.29482,1.36063,1.44301,1.52965,1.61848,1.733755,1.91537,2.163435},
        //         {0.160725,0.21877,0.2686,0.31692,0.350875,0.38626,0.428035,0.46528,0.500235,0.5303,0.55991,0.5898,0.621265,0.65571,0.706025,0.7628,0.82464,0.91217,1.051925},
        //         {0.43066,0.58418,0.734335,0.83388,0.9356,1.03524,1.13995,1.23788,1.3482,1.42665,1.509985,1.60746,1.706875,1.82,1.927075,2.03126,2.23329,2.46377,2.875135},
        //         {0.44948,0.616,0.755835,0.86612,0.9811,1.08723,1.195405,1.30296,1.42244,1.51905,1.589,1.70016,1.796245,1.90427,2.029825,2.1512,2.340305,2.60259,3.01861},
        //         {0.04231,0.06043,0.07603,0.09026,0.101825,0.11433,0.12977,0.1395,0.15,0.16135,0.1731,0.1838,0.198465,0.21439,0.23225,0.25838,0.29004,0.3277,0.395085},
        //         {0.016605,0.026,0.036715,0.04934,0.064225,0.07968,0.096705,0.11908,0.1389,0.16925,0.20311,0.24116,0.290205,0.37102,0.47285,0.61374,0.812529999999999,1.40992,2.73662},
        //         {}, // subcategory
        //         {}, // category
        //         {}, // categoryCluster
        //         {4.99,6.99,8.99,9.99,11.99,12.99,14.99,14.99,16.99,19.99,19.99,21.99,24.99,26.93,29.99,34.99,39.99,49.99,69.99},
        //         {}, // rain
        //         {}, // snow
        //         {-1,3,6,9,12,14,16,18,20,22,23,24,26,27,28,29,31,32,34}, // maxtemp
        //         {-10,-6,-3,-1,1,2,4,6,7,9,11,12,14,15,17,18,20,22,23}, // mintemp
        //         {2,3,5,5,6,6,8,8,9,10,11,11,13,13,14,16,18,19,23}, // meanwind
        //         {}, // thunder
            // };
        _thresholds = {
            {},// percentiles locn
            {46,88,130,172,214,249,291,333,382,424,473,522,564,606,641,683,739,781,830},// percentiles dateid
            {},// percentiles ksn
            {1,2,3,4,5,6,7,8,9,12,18},// percentiles inventoryunits
            {},// percentiles zip
            {1,7,8},// percentiles rgn_cd
            {1,2,3,4,5},// percentiles clim_zn_nbr
            {76256,83092,84179,84313,86479,86587,87406,88783,91117,93580,94667,95471,96934,101422,104231,106398,110376,118004,131912},// percentiles tot_area_sq_ft
            {61210,66856,68648,70878,72007,72739,73299,73828,74325,74975,75629,76943,79610,79947,82668,87043,90814,93024,99938},// percentiles sell_area_sq_ft
            {48718,50782,52775,54118,55694,56751,58277,59933,61633,63521,65815,67022,68145,71075,74732,78543,82488,87760,98866},// percentiles avghhi
            {5.36865,10.6255,15.4641,25.2152},// percentiles supertargetdistance
            {9.5616,14.8608,19.1051,28.1952},// percentiles supertargetdrivetime
            {0.80157,1.24274,1.87654,2.27422,2.86452,3.32434,3.98299,4.64786,5.50535,6.46847,7.57451,8.17724,8.73648,11.6259,17.8582,26.5214},// percentiles targetdistance
            {2.3904,3.5856,4.5504,5.4432,6.12,6.9264,7.7472,8.5824,9.5472,11.0304,11.8131,12.1536,13.6224,16.3152,21.2112,30.7152},// percentiles targetdrivetime
            {2.1313,3.38026,4.8032,6.10187,7.84792,10.1843,13.5148,14.912,17.8644,22.7298,28.5147,35.9525},// percentiles walmartdistance
            {5.0688,7.0704,8.784,10.6848,12.7152,15.1344,17.7696,19.0173,22.1184,26.9136,31.9104,39.3696},// percentiles walmartdrivetime
            {0.46603,0.85749,1.19303,1.4975,1.82683,2.16237,2.51034,2.92044,3.37405,3.88978,4.39309,5.11388,5.48422,5.86574,6.92208,8.28909,11.7377,17.1126},// percentiles walmartsupercenterdistance
            {1.4832,2.3904,2.9952,3.7008,4.5216,5.0976,5.6304,6.1488,6.7392,7.5024,8.3664,8.94215,9.1728,10.296,11.5056,13.4784,16.9344,24.2064},// percentiles walmartsupercenterdrivetime
            {8957,12292,15239,17620,19773,21757,24030,26248,28262,30041,31567,33662,35485,38069,40053,42313,46182,51532,60104},// percentiles population
            {3250,6765,8464,10265,12200,13828,15069,16663,18061,19317,20581,22124,23976,25547,27026,28745,30996,33971,39237},// percentiles white
            {52,80,108,149,187,223,265,321,398,472,553,661,784,972,1150,1412,1822,2530,4442},// percentiles asian
            {1,2,3,4,5,6,7,9,10,12,14,17,21,27,34,45,65,97,217},// percentiles pacific
            {65,122,204,292,395,552,736,950,1181,1444,1811,2145,2608,3187,3810,4681,6621,9111,12914},// percentiles black
            {29.2,31.6,33,34.2,35.2,36,36.8,37.5,38,38.7,39.2,39.8,40.2,40.9,41.6,42.3,43.2,44.3,45.9},// percentiles medianage
            {3795,4856,6012,7074,7849,8772,9562,10329,11170,11865,12436,13081,13862,14642,15548,16390,17499,19222,21844},// percentiles occupiedhouseunits
            {4239,5579,6791,7919,8826,9627,10541,11407,12260,12978,13685,14342,15251,16192,16982,17985,19086,20906,23586},// percentiles houseunits
            {2341,3156,3921,4492,4980,5563,6133,6641,7182,7618,7991,8541,8969,9407,10044,10710,11560,13145,14790},// percentiles families
            {3795,4856,6012,7074,7849,8772,9562,10329,11170,11865,12436,13081,13862,14642,15548,16390,17499,19222,21844},// percentiles households
            {1649,2200,2757,3192,3552,3992,4405,4752,5118,5450,5697,6018,6317,6712,7167,7704,8336,9224,10735},// percentiles husbwife
            {4374,5953,7484,8519,9772,10597,11646,12796,13712,14523,15425,16428,17307,18434,19482,20593,22505,25008,29305},// percentiles males
            {4682,6198,7607,9112,10122,11132,12287,13385,14437,15431,16181,17308,18159,19517,20530,21823,23642,26514,30550},// percentiles females
            {440,628,792,926,1047,1176,1326,1429,1549,1666,1772,1880,2013,2172,2358,2639,2924,3277,3951},// percentiles householdschildren
            {169,265,379,521,678,817,998,1203,1415,1733,2118,2475,2991,3947,5071,6454,8940,15723,33818},// percentiles hispanic
            {},// percentiles subcategory
            {},// percentiles category
            {},// percentiles categoryCluster
            {4.49,5.99,7.99,9.99,10.99,12.99,14.99,15.99,18.99,19.99,20.99,21.99,24.99,29.99,31.99,39.99,49.99},// percentiles prize
            {},// percentiles rain
            {},// percentiles snow
            {0,4,7,10,12,14,16,18,20,22,23,24,26,27,28,29,31,32,34},// percentiles maxtemp
            {-8,-4,-2,-1,1,3,4,6,8,9,11,12,13,15,17,18,20,22,23},// percentiles mintemp
            {2,3,5,6,8,9,10,11,12,13,14,15,17,19,23},// percentiles meanwind
            {},// percentiles thunder
        };
    }

    if (DATASET_NAME.compare("favorita") == 0 ||
        DATASET_NAME.compare("favorita_ml") == 0)
    {
        _thresholds = {
            {},// percentiles date
            {},// percentiles store
            {},// percentiles item
            {1,2,3,3.114,4,5,6,7,9,11,14,18,29},// percentiles unit_sales
            {},// percentiles onpromotion
            {35.36,40.05,43.04,44.47,45.47,46.12,47.17,48.13,49.07,49.85,51.61,53.01,54.59,60.25,90.88,93.84,97.03,100.52,104.06},// percentiles oilprize
            {},// percentiles holiday_type
            {},// percentiles locale
            {},// percentiles locale_id
            {},// percentiles transferred
            {711,856,973,1070,1157,1235,1304,1373,1453,1554,1681,1804,1945,2151,2390,2679,2997,3408,4070},// percentiles transactions
            {},// percentiles city
            {},// percentiles state
            {},// percentiles store_type
            {},// percentiles cluster
            {},// percentiles item_family
            {},// percentiles item_class
            {},// percentiles perishable
        };   
    }
    
    if (DATASET_NAME.compare("yelp") == 0)
    {
        _thresholds = {
            {},// percentiles business_id
            {},// percentiles user_id
            {},// percentiles review_id
            {1,2,3,4,5},// percentiles review_stars
            {},// percentiles review_year
            {},// percentiles review_month
            {},// percentiles review_day
            {0,1,2,3,5},// percentiles useful
            {0,1,2},// percentiles funny
            {0,1,2,3},// percentiles cool
            {},// percentiles city_id
            {},// percentiles state_id
            {33.3771,33.4429,33.4922,33.5105,33.6087,33.7134,35.2278,36.0296,36.0913,36.109,36.1161,36.1259,36.1474,36.1977,40.4438,41.4837,43.5426,43.6567,43.8425},// percentiles latitude
            {-115.258,-115.202,-115.175,-115.172,-115.157,-115.135,-112.393,-112.079,-112.037,-111.95,-111.925,-111.878,-111.687,-81.731,-80.8641,-80.6481,-79.713,-79.3985,-79.3156},// percentiles longitude
            {2.5,3,3.5,4,4.5},// percentiles business_stars
            {17,30,44,59,76,95,117,142,171,204,243,290,342,403,503,629,819,1169,1946},// percentiles reviewcount
            {},// percentiles is_open
            {2,3,5,6,8,11,14,17,22,27,35,45,58,76,103,142,201,307,539},// percentiles review_count
            {2008,2009,2010,2011,2012,2013,2014,2015,2016},// percentiles year_joined
            {0,1,2,3,5,8,12,18,28,47,86,180,551},// percentiles user_useful
            {0,1,2,3,4,7,12,25,61,221},// percentiles user_funny
            {0,1,2,3,5,10,22,64,294},// percentiles user_cool
            {0,1,2,3,4,7,12,21,47},// percentiles fans
            {2.44,2.95,3.14,3.32,3.43,3.53,3.61,3.68,3.75,3.82,3.88,3.96,4,4.09,4.18,4.29,4.42,4.61,5},// percentiles average_stars
            {0,1,3,7,17,58},// percentiles compliement_hot
            {0,1,2,4,9},// percentiles compliment_more
            {0,1,2,5},// percentiles compliment_profile
            {0,1,3},// percentiles compliment_cute
            {0,2},// percentiles compliment_list
            {0,1,2,3,5,8,17,46},// percentiles compliment_note
            {0,1,2,3,6,11,25,86},// percentiles compliment_plain
            {0,1,2,3,6,12,29,97},// percentiles compliment_cool
            {0,1,2,3,6,12,29,97},// percentiles compliment_funny
            {0,1,2,3,7,16,43},// percentiles compliment_writer
            {0,1,2,5,18},// percentiles compliment_photos
            {},// percentiles attribute_id
            {},// percentiles attribute_value
            {},// percentiles category_id
        };
    }


    if (DATASET_NAME.compare("tpc-ds") == 0)
    {
        _thresholds = {
            {},// percentiles ss_sold_date_sk
            {},// percentiles ss_sold_time_sk
            {},// percentiles ss_item_sk
            {},// percentiles ss_customer_sk
            {},// percentiles ss_hdemo_sk
            {},// percentiles ss_store_sk
            {6,11,16,21,27,32,37,42,47,50,54,59,64,69,74,80,85,90,95},// percentiles ss_quantity
            {1.53,6.84,12.14,17.45,22.76,28.08,33.39,38.69,44.01,48.92,52.5141,57.54,62.85,68.16,73.46,78.77,84.08,89.4,94.7},// percentiles ss_wholesale_cost
            {8.81,16.21,23.61,31,38.39,45.7786,51.071,57.28,64.66,71.81,77.12,83.53,90.9,98.3,106.2,115.22,125.74,138.6,155.96},// percentiles ss_list_price
            {1.51,3.41,5.63,8.16,10.99,14.15,17.64,21.49,25.74,30.44,35.381,39.36,44.97,51.99,60.09,69.57,77.29,89.43,109.73},// percentiles ss_sales_price
            {0,0.01,38.0374,134.98,322.38,1097.21},// percentiles ss_ext_discount_amt
            {19.25,67.1,126.42,187.16,258.39,359.04,482.03,626.82,796.68,996.3,1230.78,1508.52,1807.28,2074.05,2505.03,3072.3,3817.92,4872,6617.34},// percentiles ss_ext_sales_price
            {119.04,246.56,394.32,563.92,753.02,962.68,1194.44,1450.28,1721.52,1952.01,2228.94,2522.54,2816.07,3246.15,3739.86,4311.12,4988.85,5833.24,6995.59},// percentiles ss_ext_wholesale_cost
            {176.8,363.6,581.36,828.72,1104.51,1410.75,1749.34,2121.68,2478.58,2786.16,3257.64,3719.36,4133.66,4756.68,5491.32,6347.6,7378.54,8702.19,10689.5},// percentiles ss_ext_list_price
            {0,1.13,2.93,5.38,8.58,12.67,17.8,24.21,32.2,42.23,54.93,70.06,84.26,108.32,144.9,200.71,300.84,625.86},// percentiles ss_ext_tax
            {0,63.65,145.499,331.82,1097.46},// percentiles ss_coupon_amt
            {11.57,47.04,93.97,149.82,206.65,281.3,382.7,505.8,652.55,827.25,1035.82,1285.38,1571.48,1820.28,2205.36,2741.76,3454.88,4481.03,6208.4},// percentiles ss_net_paid
            {23.56,63.94,116.63,183.65,266.36,366.26,485.92,628.05,796.7,997.4,1235.85,1512.86,1731.58,1945.07,2354.29,2916.56,3664.12,4735.55,6540.75},// percentiles ss_net_paid_inc_tax
            {-4208.67,-3011.12,-2273.46,-1749.48,-1352.42,-1043.8,-837.986,-668.71,-492.94,-345.6,-226.72,-132.72,-61.06,-9.75,49.41,187.87,452.76,971.1,1810.38},// percentiles ss_net_profit
            {1181,1184,1186,1188,1193,1196,1198,1200,1205,1208,1210,1212,1217,1220,1222,1224,1229,1232,1234},// percentiles d_month_seq
            {5137,5151,5160,5166,5189,5203,5212,5218,5241,5255,5264,5271,5293,5307,5316,5323,5345,5360,5368},// percentiles d_week_seq
            {395,396,397,399,400,401,403,404,405,407,408,409,411,412},// percentiles d_quarter_seq
            {1998,1999,2000,2001,2002},// percentiles d_year
            {0,1,2,3,4,5,6},// percentiles d_dow
            {1,2,4,5,6,7,8,9,10,11,12},// percentiles d_moy
            {2,4,5,7,8,10,11,13,14,16,17,19,20,22,23,25,26,28,30},// percentiles d_dom
            {1,2,3,4},// percentiles d_qoy
            {1998,1999,2000,2001,2002},// percentiles d_fy_year
            {395,396,397,399,400,401,403,404,405,407,408,409,411,412},// percentiles d_fy_quarter_seq
            {5137,5151,5160,5166,5189,5203,5212,5218,5241,5255,5264,5271,5293,5307,5316,5323,5345,5360,5368},// percentiles d_fy_week_seq
            {0},// percentiles d_holiday
            {0,1},// percentiles d_weekend
            {1},// percentiles d_following_holiday
            {32856,35095,36897,38400,40107,42690,45006,47199,49203,51009,52511,54025,58532,61809,63304,64806,67059,69301,71555},// percentiles t_time
            {9,10,11,12,13,14,15,16,17,18,19},// percentiles t_hour
            {2,6,9,12,15,17,21,23,26,29,33,36,39,42,45,48,51,54,57},// percentiles t_minute
            {2,5,8,11,14,17,21,24,27,30,33,36,39,42,45,48,51,54,56},// percentiles t_second
            {0,1},// percentiles t_am_pm
            {},// percentiles t_shift
            {0.5,0.91,1.32,1.73,2.13,2.55,2.96,3.37,3.79,4.19,4.6,5.04,5.88,6.71,7.55,8.37,9.21,12.78,56.46},// percentiles i_current_price
            {0.27,0.49,0.71,0.94,1.16,1.38,1.6,1.84,2.11,2.38,2.68,3,3.36,3.79,4.35,5.13,6.21,8.38,31.21},// percentiles i_wholesale_cost
            {},// percentiles i_class_id
            {},// percentiles i_category_id
            {},// percentiles i_manufact_id
            {0,1,2,3,4,5},// percentiles i_size
            {3641,7478,11285,15103,18956,22804,26632,30429,34287,38104,41923,45731,49556,53421,57246,61067,64913,68746,72550},// percentiles i_formulation
            {},// percentiles i_color
            {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,20},// percentiles i_units
            {0, 50, 100, 150},// percentiles i_container
            {},// percentiles c_current_cdemo_sk
            {},// percentiles c_current_addr_sk
            {},// percentiles c_salutation
            {},// percentiles c_preferred_cust_flag
            {1,2,3,4,5,6,7,8,9,10,11,12},// percentiles c_birth_month
            {1927,1930,1934,1938,1941,1945,1948,1952,1955,1959,1963,1966,1970,1973,1977,1980,1984,1988,1991},// percentiles c_birth_year
            {},// percentiles c_birth_country
            {0,1},// percentiles cd_gender
            {},// percentiles cd_marital_status
            {},// percentiles cd_education_status
            {500,1000,1500,2000,2500,3000,3500,4000,4500,5000,5500,6000,6500,7000,7500,8000,8500,9000,9500},// percentiles cd_purchase_estimate
            {0,1,2,3},// percentiles cd_credit_rating
            {0,1,2,3,4,5,6},// percentiles cd_dep_count
            {0,1,2,3,4,5,6},// percentiles cd_dep_employed_count
            {0,1,2,3,4,5,6},// percentiles cd_dep_college_count
            {},// percentiles ca_street_type
            {},// percentiles ca_city
            {},// percentiles ca_county
            {},// percentiles ca_state
            {},// percentiles ca_zip
            {},// percentiles ca_country
            {-8,-7,-6,-5},// percentiles ca_gmt_offset
            {},// percentiles ca_location_type
            {},// percentiles hd_income_band_sk
            {0,1,2,3,4,5},// percentiles hd_buy_potential
            {0,1,2,3,4,5,6,7,8,9},// percentiles hd_dep_count
            {-1,0,1,2,3,4},// percentiles hd_vehicle_count
            {10001,20001,30001,40001,50001,60001,70001,80001,90001,100001,110001,120001,130001,140001,150001,160001,170001,180001,190001},// percentiles ib_lower_bound
            {20000,30000,40000,50000,60000,70000,80000,90000,100000,110000,120000,130000,140000,150000,160000,170000,180000,190000,200000},// percentiles ib_upper_bound
            {204,208,218,220,226,236,237,240,244,251,260,266,271,272,278,281,290,291,297},// percentiles s_number_employees
            {5250760,5527158,5644158,5813235,6035829,6465941,6589353,6995995,7261787,7610137,7825489,8047423,8393436,8555120,8618762,8886865,9206875,9341467,9599785},// percentiles s_floor_space
            {0,1,2},// percentiles s_hours
            {1,4,7,13,16,22,24,30,33,37,41,44,49,52,56,59,62,67,72},// percentiles s_manager
            {},// percentiles s_geography_class
            {},// percentiles s_division_id
            {},// percentiles s_company_id
            {},// percentiles s_city
            {},// percentiles s_county
            {},// percentiles s_state
            {},// percentiles s_country
            {-6,-5},// percentiles s_gmt_offset
            {0,0.01,0.02,0.03,0.05,0.06,0.07,0.08,0.09,0.1,0.11},// percentiles s_tax_percentage
        };
    }

    if (DATASET_NAME.compare("home_credit_default") == 0 || DATASET_NAME.compare("normalized_tables") == 0)
    {
        _thresholds = {
        {117814,135626,153441,171253,189066,206880,224692,242504,260317,278129,295942,313755,331568,349381,367193,385005,402818,420630,438443},// percentiles SK_ID_CURR
        {-1,0,1},// percentiles application_fact_TARGET
        {1,2},// percentiles application_fact_NAME_CONTRACT_TYPE
        {1,2},// percentiles application_fact_CODE_GENDER
        {1,2},// percentiles application_fact_FLAG_OWN_CAR
        {67500,81000,90000,99000,112500,126000,135000,153000,157500,166500,180000,189000,202500,225000,247500,270000,337500},// percentiles application_fact_AMT_INCOME_TOTAL
        {135000,180000,225000,252000,270000,299250,343800,408866,450000,500211,540000,585000,675000,725328,797558,900000,1.00692e+06,1.125e+06,1.35e+06},// percentiles application_fact_AMT_CREDIT
        {9000,11250,13482,14940,16722,18436.5,20250,21937.5,23539.5,25078.5,26509.5,28350,30289.5,32274,34960.5,37800,41692.5,46701,53914.5},// percentiles application_fact_AMT_ANNUITY
        {135000,171000,202500,225000,234000,270000,306000,360000,450000,454500,495000,594000,675000,774000,900000,1.035e+06,1.305e+06},// percentiles application_fact_AMT_GOODS_PRICE
        {0.00496,0.006629,0.007305,0.008866,0.010006,0.010643,0.014464,0.016612,0.018634,0.01885,0.019689,0.020713,0.02461,0.025164,0.028663,0.030755,0.031329,0.035792,0.04622},// percentiles application_fact_REGION_POPULATION_RELATIVE
        {1325687264,32735,33877,0,1065353216,32735,253697,0,500009,0,1325687296,32735,37566,0,32,32735,43682},// percentiles Application_Fact_DAYS_BIRTH
        {-23204,-22186,-21315,-20472,-19676,-18880,-18031,-17221,-16467,-15755,-15077,-14426,-13794,-13148,-12425,-11708,-11015,-10294,-9418},// percentiles application_fact_DAYS_EMPLOYED
        {-6740,-4888,-3893,-3250,-2781,-2380,-2012,-1709,-1459,-1224,-1022,-831,-647,-462,-290,-142,0},// percentiles application_fact_DAYS_REGISTRATION
        {-11420,-9938,-9024,-8232,-7477,-6770,-6101,-5452,-4940,-4502,-4053,-3527,-3011,-2528,-1995,-1481,-1057,-691,-332},// percentiles application_fact_DAYS_ID_PUBLISH
        {0},// percentiles application_fact_OWN_CAR_AGE
        {-4972,-4748,-4589,-4451,-4318,-4192,-4059,-3880,-3544,-3252,-2951,-2654,-2365,-2043,-1717,-1376,-1052,-732,-376},// percentiles application_fact_FLAG_WORK_PHONE
        {0,2,5,8,10,14,19},// percentiles application_fact_OCCUPATION_TYPE
        {0,1},// percentiles application_fact_WEEKDAY_APPR_PROCESS_START
        {0,1,2,3,4,5,6,7,10,12},// percentiles application_fact_ORGANIZATION_TYPE
        {1,2,3,4,5,6,7},// percentiles application_fact_EXT_SOURCE_1
        {1,3,5,6,8,10,13,15,20,28,36},// percentiles application_fact_EXT_SOURCE_2
        {0,0.216342,0.308213,0.38778,0.464495,0.54011,0.615442,0.692684,0.775507},// percentiles application_fact_EXT_SOURCE_3
        {0.126214,0.207421,0.270583,0.330409,0.383092,0.431197,0.471229,0.504474,0.534639,0.559973,0.583065,0.603843,0.623848,0.642621,0.660804,0.679731,0.698922,0.720689,0.746962},// percentiles application_fact_DAYS_LAST_PHONE_CHANGE
        {0},// percentiles application_fact_DOCUMENT_COUNT
        {-1289,-588,-221,0,0.172495,0.253963,0.315472,0.367291,0.411849,0.454321,0.495666,0.531686,0.567379,0.600658,0.633032,0.665855,0.697147,0.733815,0.773896},// percentiles application_fact_NEW_DOC_KURT
        {-2445,-2033,-1787,-1602,-1417,-1179,-976,-794,-655,-532,-413,-294,-148,-1,0,1},// percentiles application_fact_AGE_RANGE
        {0,1,20},// percentiles application_fact_EXT_SOURCES_PROD
        {0,1,2,4,20},// percentiles application_fact_EXT_SOURCES_WEIGHTED
        {0,3,4,20},// percentiles application_fact_EXT_SOURCES_MIN
        {0,1,2,3,4},// percentiles application_fact_EXT_SOURCES_MAX
        {0,0.0106414,0.042074,0.0748281,0.110444,0.150159,0.194486,0.250778,0.319516,0.426313,0.572086,0.710376},// percentiles application_fact_EXT_SOURCES_MEAN
        {0,0.159679,0.315472,0.455152,0.563435,0.651708,0.747156,1.85917,2.43326,2.8429,3.18701,3.52878,3.92602},// percentiles application_fact_EXT_SOURCES_NANMEDIAN
        {0.101459,0.159571,0.202276,0.243186,0.276672,0.310907,0.344349,0.376776,0.407867,0.438598,0.468021,0.497673,0.526295,0.554947,0.583411,0.612704,0.643459,0.678087,0.722393},// percentiles application_fact_EXT_SOURCES_VAR
        {0.266789,0.355639,0.41553,0.461799,0.500054,0.532588,0.559806,0.584266,0.606336,0.626304,0.644679,0.66174,0.679217,0.695599,0.712155,0.729567,0.748636,0.771362,0.80384},// percentiles application_fact_CREDIT_TO_ANNUITY_RATIO
        {0,0.00434483,0.0760325,0.275176,0.340039,0.383314,0.417733,0.447525,0.473667,0.498776,0.522481,0.54604,0.56947,0.592963,0.617826,0.644004,0.673029,0.70927},// percentiles application_fact_CREDIT_TO_GOODS_RATIO
        {0.0141631,0.129314,0.284536,0.353366,0.401303,0.441566,0.475366,0.507437,0.53707,0.565608,0.593697,0.620703,0.648286,0.67861,0.716101,8.34105,13.8722,19.5854,23.6372},// percentiles application_fact_ANNUITY_TO_INCOME_RATIO
        {0.000318616,0.00114315,0.00234738,0.00388582,0.0057735,0.00805738,0.0108033,0.0141934,0.0183142,0.0233947,0.029938,0.0389874,0.0530128,0.0990948,1,1.11616,1.198,1.4224,19.5018},// percentiles application_fact_CREDIT_TO_INCOME_RATIO
        {0.1225,0.176842,0.244533,0.613544,1.1386,8.39049,10.0567,12.6097,14.5773,16.4843,18.4341,19.5848,20,20.6943,23.37,25.2143,27.8396,30.9981,34.0835},// percentiles application_fact_INCOME_TO_EMPLOYED_RATIO
        {0.173,0.75,1,1.08316,1.1188,1.132,1.1584,1.1716,1.198,1.2376,1.31679,1.396,2.01053,3.17594,5},// percentiles application_fact_INCOME_TO_BIRTH_RATIO
        {-195.652,-80.597,-26.9092,0,0.0702807,0.0902,0.10608,0.1202,0.133929,0.147059,0.160356,0.175576,0.19265,0.212,0.235368,0.2666,0.316,0.49184,3},// percentiles application_fact_EMPLOYED_TO_BIRTH_RATIO
        {-55.8833,-15.9198,-10.7239,-7.3984,-4.3279,0.5,1.14815,1.52286,1.87421,2.17391,2.5,2.83,3.1717,3.6,4.0732,4.73829,5.4504,6.4331,8.07467},// percentiles application_fact_ID_TO_BIRTH_RATIO
        {-522.042,-290.152,-194.974,-143.495,-110.186,-86.3392,-67.6895,-52.9695,-40.7166,-29.5372,-19.3966,-11.6351,-5.84745,0,0.0192694,0.0740458,0.159734},// percentiles application_fact_CAR_TO_BIRTH_RATIO
        {-21.2851,-17.2051,-14.8156,-13.1391,-11.811,-10.6825,-9.6691,-8.72872,-7.86556,-7.00874,-6.16734,-5.279,-4.20011,-2.24865,0.0235644,0.0859057,0.151112,0.201792,0.285551},// percentiles application_fact_CAR_TO_EMPLOYED_RATIO
        {0,0.0131948,0.029602,0.0472244,0.0660084,0.0854935,0.106364,0.12999,0.156272,0.18309,0.212033,0.253046,0.304361,0.382926},// percentiles application_fact_PHONE_TO_BIRTH_RATIO
        {0,0.00327293,0.0418551,0.0756359,0.106751,0.134806,0.160541,0.178616,0.193656,0.208602,0.232525,0.268662,0.29405,0.317525,0.34354},// percentiles application_fact_BUREAU_INCOME_CREDIT_RATIO
        {0,0.0140099,0.0405046,0.0827753},// percentiles application_fact_BUREAU_ACTIVE_CREDIT_TO_INCOME_RATIO
        {0,0.4655,1.21998,2.83996},// percentiles application_fact_CURRENT_TO_APPROVED_CREDIT_MIN_RATIO
        {0,0.000876459,0.0133564,0.0218922,0.0298682,0.0381494,0.0470421,0.0567933,0.067693,0.0790111,0.0912665,0.105197,0.121104,0.14211,0.177631,0.4927,1.34158,2.85714,6.76417},// percentiles application_fact_CURRENT_TO_APPROVED_CREDIT_MAX_RATIO
        {0,0.0196633,0.0660912,0.126964,0.214135,0.324,0.447725,0.588144,0.747162,0.927911,1.13845,1.39565,1.71702,2.14605,2.75,3.63333,5.19048,8.875},// percentiles application_fact_CURRENT_TO_APPROVED_CREDIT_MEAN_RATIO
        {0,0.040366,0.10766,0.199338,0.326126,0.5,0.721858,1,1.37255,1.833,2.46862,3.28571,4.42857,6.22222,9.33333,16.6667},// percentiles application_fact_CURRENT_TO_APPROVED_ANNUITY_MAX_RATIO
        {0,0.0248138,0.0354174,0.0463069,0.0577062,0.0701169,0.0838118,0.0992903,0.117014,0.138453,0.163476,0.193728,0.231875,0.282262,0.348189,0.439556,0.578326,0.822222,1.40275},// percentiles application_fact_CURRENT_TO_APPROVED_ANNUITY_MEAN_RATIO
        {0,0.0618168,0.0957526,0.130439,0.166642,0.205932,0.251042,0.301975,0.361259,0.430038,0.50942,0.605583,0.723467,0.868432,1.05199,1.30874,1.66474,2.23009,3.4},// percentiles application_fact_PAYMENT_MIN_TO_ANNUITY_RATIO
        {0,0.0555982,0.084258,0.110744,0.139146,0.16926,0.202385,0.238474,0.278478,0.323791,0.375293,0.434786,0.505662,0.591519,0.701003,0.845495,1.0472,1.37036,2.02738},// percentiles application_fact_PAYMENT_MAX_TO_ANNUITY_RATIO
        {0,0.00189358,0.0732547,0.147501,0.204401,0.257464,0.311371,0.369808,0.431034,0.501497,0.579085,0.669672,0.777197,0.906522,1.0661,1.27097,1.56006,1.9969,2.86226},// percentiles application_fact_PAYMENT_MEAN_TO_ANNUITY_RATIO
        {0,0.0703551,0.143585,0.192239,0.237411,0.281584,0.327149,0.376426,0.43016,0.490313,0.558957,0.641096,0.741109,0.865142,1.02846,1.26513,1.65528,2.50747,6.10035},// percentiles application_fact_CTA_CREDIT_TO_ANNUITY_MAX_RATIO
        {1.35355e-06,9.2688e-05,0.000363757,0.00124575,0.00350816,0.0100304,0.0412612,0.0948617,0.141091,0.187145,0.237906,0.296367,0.365199,0.449464,0.562068,0.720208,0.964963,1.43973,2.86777},// percentiles application_fact_CTA_CREDIT_TO_ANNUITY_MEAN_RATIO
        {0.060471,0.203871,0.288834,0.369592,0.453155,0.54124,0.640897,0.757459,0.892176,1.03734,1.21769,1.47291,1.82727,2.31589,3.16265,4.65812,7.40865,12.6581,24.3281},// percentiles application_fact_DAYS_DECISION_MEAN_TO_BIRTH
        {0.0362979,0.147821,0.202308,0.250685,0.294966,0.340326,0.387328,0.435356,0.486701,0.543023,0.605736,0.679117,0.762993,0.863088,0.986298,1.13513,1.36354,1.73088,2.51271},// percentiles application_fact_DAYS_CREDIT_MEAN_TO_BIRTH
        {0,0.035772,0.061944,0.100449,0.181134,0.270916,0.334685,0.408497,0.470327,0.534568,0.595758,0.673748,0.761789,0.858617,0.971095,1.05954,1.19609,1.44355,1.864},// percentiles application_fact_DAYS_DECISION_MEAN_TO_EMPLOYED
        {0,0.0242597,0.0444606,0.0635492,0.087165,0.125959,0.201515,0.267824,0.321118,0.37245,0.421156,0.469325,0.520528,0.579482,0.647693,0.727273,0.831939,0.980516,1.22897},// percentiles application_fact_DAYS_CREDIT_MEAN_TO_EMPLOYED
        {0,0.00761075,0.0185022,0.025065,0.0311823,0.0373175,0.0435969,0.0500663,0.0566719,0.0634399,0.0709302,0.0793122,0.0893128,0.101553,0.117266,0.14055,0.190727,0.443728,1.23964},// percentiles application_DIM_2_IDX
        {0,1},// percentiles application_DIM_3_IDX
        {0,1,2,10,23,96,247},// percentiles application_DIM_4_IDX
        {0,7,12,14,35,158},// percentiles application_DIM_5_IDX
        {0,2,6,9,10,12,14,21,33,65,136,253,944},// percentiles application_DIM_6_IDX
        {0,1,7,14,31151,134862,216023},// percentiles application_DIM_7_IDX
        {0,1,2,9,128,105506},// percentiles application_DIM_8_IDX
        {0,1,11,76,30692,77358,151711,204976,260204},// percentiles application_DIM_9_IDX
        {2589,2590},// percentiles AMT_CREDIT_SUM_id
        {121},// percentiles credit_active_type_id
        {0},// percentiles bureau_aggregated_norm_sk_bureau_id_count
        {1541406720},// percentiles bureau_aggregated_norm_DAYS_CREDIT_min
        {32735},// percentiles bureau_aggregated_norm_DAYS_CREDIT_max
        {2.122e-314},// percentiles bureau_aggregated_norm_DAYS_CREDIT_avg
        {117000},// percentiles bureau_aggregated_norm_DAYS_CREDIT_ENDDATE_min
        {894766},// percentiles bureau_aggregated_norm_DAYS_CREDIT_ENDDATE_max
        {34951.5},// percentiles bureau_aggregated_norm_AMT_CREDIT_MAX_OVERDUE_max
        {679500},// percentiles bureau_aggregated_norm_AMT_CREDIT_MAX_OVERDUE_avg
        {0.006296},// percentiles bureau_aggregated_norm_AMT_CREDIT_SUM_max
        {-12900},// percentiles bureau_aggregated_norm_AMT_CREDIT_SUM_avg
        {-103},// percentiles bureau_aggregated_norm_AMT_CREDIT_SUM_sum
        {4.24399e-314},// percentiles bureau_aggregated_norm_AMT_CREDIT_SUM_DEBT_max
        {0},// percentiles bureau_aggregated_norm_AMT_CREDIT_SUM_DEBT_avg
        {2.12199e-314},// percentiles bureau_aggregated_norm_AMT_CREDIT_SUM_DEBT_sum
        {1.4854e-313},// percentiles bureau_aggregated_norm_AMT_ANNUITY_avg
        {5},// percentiles bureau_aggregated_norm_DEBT_CREDIT_DIFF_avg
        {39},// percentiles bureau_aggregated_norm_DEBT_CREDIT_DIFF_SUM
        {0.0330486},// percentiles bureau_aggregated_norm_MONTHS_BALANCE_AVG_AVG
        {0.484465},// percentiles bureau_aggregated_norm_MONTHS_BALANCE_COUNT_AVG
        {2.122e-314},// percentiles bureau_aggregated_norm_MONTHS_BALANCE_COUNT_SUM
        {0.39622},// percentiles bureau_aggregated_norm_STATUS_0_AVG
        {4.24399e-314},// percentiles bureau_aggregated_norm_STATUS_1_AVG
        {0},// percentiles bureau_aggregated_norm_STATUS_12345_AVG
        {1},// percentiles bureau_aggregated_norm_STATUS_C_AVG
        {20},// percentiles bureau_aggregated_norm_STATUS_X_AVG
        {2},// percentiles bureau_aggregated_norm_LL_AMT_CREDIT_SUM_OVERDUE_AVG
        {0.00634382},// percentiles bureau_aggregated_norm_LL_DEBT_CREDIT_DIFF_AVG
        {1.73922},// percentiles bureau_aggregated_norm_LL_STATUS_12345_AVG
        {0.0330486},// percentiles bureau_aggregated_norm_BUREAU_ACTIVE_DAYS_CREDIT_avg
        {0.484465},// percentiles bureau_aggregated_norm_BUREAU_ACTIVE_DAYS_CREDIT_ENDDATE_min
        {0.304578},// percentiles bureau_aggregated_norm_BUREAU_ACTIVE_DAYS_CREDIT_ENDDATE_max
        {0.39622},// percentiles bureau_aggregated_norm_BUREAU_ACTIVE_AMT_CREDIT_MAX_OVERDUE_max
        {0.0381619},// percentiles bureau_aggregated_norm_BUREAU_ACTIVE_AMT_CREDIT_MAX_OVERDUE_avg
        {25.6002},// percentiles bureau_aggregated_norm_BUREAU_ACTIVE_AMT_CREDIT_SUM_max
        {1.3168},// percentiles bureau_aggregated_norm_BUREAU_ACTIVE_AMT_CREDIT_SUM_sum
        {0.298731},// percentiles bureau_aggregated_norm_BUREAU_ACTIVE_AMT_CREDIT_SUM_DEBT_avg
        {7.64758},// percentiles bureau_aggregated_norm_BUREAU_ACTIVE_AMT_CREDIT_SUM_DEBT_sum
        {-1135.92},// percentiles bureau_aggregated_norm_BUREAU_ACTIVE_AMT_CREDIT_SUM_OVERDUE_max
        {-9.06977},// percentiles bureau_aggregated_norm_BUREAU_ACTIVE_AMT_CREDIT_SUM_OVERDUE_avg
        {0.0079845},// percentiles bureau_aggregated_norm_BUREAU_ACTIVE_DAYS_CREDIT_UPDATE_min
        {2012308708},// percentiles bureau_aggregated_norm_BUREAU_ACTIVE_DAYS_CREDIT_UPDATE_avg
        {0},// percentiles bureau_aggregated_norm_BUREAU_ACTIVE_DEBT_CREDIT_DIFF_avg
        {0},// percentiles bureau_aggregated_norm_BUREAU_ACTIVE_MONTHS_BALANCE_AVG_AVG
        {0.116744},// percentiles bureau_aggregated_norm_BUREAU_ACTIVE_MONTHS_BALANCE_COUNT_AVG
        {0.407038},// percentiles bureau_aggregated_norm_BUREAU_ACTIVE_MONTHS_BALANCE_COUNT_SUM
        {0},// percentiles bureau_aggregated_norm_BUREAU_CLOSED_DAYS_CREDIT_max
        {-398398571},// percentiles bureau_aggregated_norm_BUREAU_CLOSED_DAYS_CREDIT_VAR
        {0.600698},// percentiles bureau_aggregated_norm_BUREAU_CLOSED_DAYS_CREDIT_ENDDATE_max
        {0.193361},// percentiles bureau_aggregated_norm_BUREAU_CLOSED_AMT_CREDIT_MAX_OVERDUE_max
        {0.801348},// percentiles bureau_aggregated_norm_BUREAU_CLOSED_AMT_CREDIT_MAX_OVERDUE_avg
        {0.335534},// percentiles bureau_aggregated_norm_BUREAU_CLOSED_AMT_CREDIT_SUM_OVERDUE_avg
        {0.087537},// percentiles bureau_aggregated_norm_BUREAU_CLOSED_AMT_CREDIT_SUM_max
        {9.18785},// percentiles bureau_aggregated_norm_BUREAU_CLOSED_AMT_CREDIT_SUM_sum
        {0.691886},// percentiles bureau_aggregated_norm_BUREAU_CLOSED_AMT_CREDIT_SUM_avg
        {0.781243},// percentiles bureau_aggregated_norm_BUREAU_CLOSED_AMT_CREDIT_SUM_DEBT_max
        {0.49714},// percentiles bureau_aggregated_norm_BUREAU_CLOSED_AMT_CREDIT_SUM_DEBT_sum
        {0.0755592},// percentiles bureau_aggregated_norm_BUREAU_CLOSED_DAYS_CREDIT_UPDATE_max
        {0},// percentiles bureau_aggregated_norm_BUREAU_CLOSED_ENDDATE_DIF_avg
        {1.21166e-311},// percentiles bureau_aggregated_norm_BUREAU_CLOSED_STATUS_12345_AVG
        {2.97079e-312},// percentiles bureau_aggregated_norm_BUREAU_CREDIT_TYPE_1_DAYS_CREDIT_max
        {1},// percentiles bureau_aggregated_norm_BUREAU_CREDIT_TYPE_1_DAYS_CREDIT_avg
        {2.18959e-314},// percentiles bureau_aggregated_norm_BUREAU_CREDIT_TYPE_1_DAYS_CREDIT_ENDDATE_max
        {2.16499e-314},// percentiles bureau_aggregated_norm_BUREAU_CREDIT_TYPE_1_AMT_CREDIT_MAX_OVERDUE_max
        {6.95313e-310},// percentiles bureau_aggregated_norm_BUREAU_CREDIT_TYPE_1_AMT_CREDIT_MAX_OVERDUE_avg
        {6.95313e-310},// percentiles bureau_aggregated_norm_BUREAU_CREDIT_TYPE_1_AMT_CREDIT_SUM_max
        {2.18942e-314},// percentiles bureau_aggregated_norm_BUREAU_CREDIT_TYPE_1_AMT_CREDIT_SUM_avg
        {0},// percentiles bureau_aggregated_norm_BUREAU_CREDIT_TYPE_1_AMT_CREDIT_SUM_DEBT_avg
        {4.94066e-324},// percentiles bureau_aggregated_norm_BUREAU_CREDIT_TYPE_1_AMT_CREDIT_SUM_DEBT_max
        {2.18956e-314},// percentiles bureau_aggregated_norm_BUREAU_CREDIT_TYPE_1_DEBT_CREDIT_DIFF_avg
        {6.95332e-310},// percentiles bureau_aggregated_norm_BUREAU_CREDIT_TYPE_2_DAYS_CREDIT_max
        {136800568},// percentiles bureau_aggregated_norm_BUREAU_CREDIT_TYPE_2_DAYS_CREDIT_avg
        {2.16499e-314},// percentiles bureau_aggregated_norm_BUREAU_CREDIT_TYPE_2_DAYS_CREDIT_ENDDATE_max
        {2.16498e-314},// percentiles bureau_aggregated_norm_BUREAU_CREDIT_TYPE_2_AMT_CREDIT_MAX_OVERDUE_max
        {2.18959e-314},// percentiles bureau_aggregated_norm_BUREAU_CREDIT_TYPE_2_AMT_CREDIT_MAX_OVERDUE_avg
        {6.95313e-310},// percentiles bureau_aggregated_norm_BUREAU_CREDIT_TYPE_2_AMT_CREDIT_SUM_max
        {2.18943e-314},// percentiles bureau_aggregated_norm_BUREAU_CREDIT_TYPE_2_AMT_CREDIT_SUM_avg
        {0},// percentiles bureau_aggregated_norm_BUREAU_CREDIT_TYPE_2_AMT_CREDIT_SUM_DEBT_avg
        {2.18959e-314},// percentiles bureau_aggregated_norm_BUREAU_CREDIT_TYPE_2_AMT_CREDIT_SUM_DEBT_max
        {6.95332e-310},// percentiles bureau_aggregated_norm_BUREAU_CREDIT_TYPE_2_DEBT_CREDIT_DIFF_avg
        {2.18956e-314},// percentiles bureau_aggregated_norm_BUREAU_CREDIT_TYPE_3_DAYS_CREDIT_max
        {-353783216},// percentiles bureau_aggregated_norm_BUREAU_CREDIT_TYPE_3_DAYS_CREDIT_avg
        {2.18943e-314},// percentiles bureau_aggregated_norm_BUREAU_CREDIT_TYPE_3_DAYS_CREDIT_ENDDATE_max
        {2.18959e-314},// percentiles bureau_aggregated_norm_BUREAU_CREDIT_TYPE_3_AMT_CREDIT_MAX_OVERDUE_max
        {2.16499e-314},// percentiles bureau_aggregated_norm_BUREAU_CREDIT_TYPE_3_AMT_CREDIT_MAX_OVERDUE_avg
        {2.16499e-314},// percentiles bureau_aggregated_norm_BUREAU_CREDIT_TYPE_3_AMT_CREDIT_SUM_max
        {2.16499e-314},// percentiles bureau_aggregated_norm_BUREAU_CREDIT_TYPE_3_AMT_CREDIT_SUM_avg
        {6.95322e-310},// percentiles bureau_aggregated_norm_BUREAU_CREDIT_TYPE_3_AMT_CREDIT_SUM_DEBT_avg
        {2.18958e-314},// percentiles bureau_aggregated_norm_BUREAU_CREDIT_TYPE_3_AMT_CREDIT_SUM_DEBT_max
        {2.122e-314},// percentiles bureau_aggregated_norm_BUREAU_CREDIT_TYPE_3_DEBT_CREDIT_DIFF_avg
        {2.16994e-320},// percentiles bureau_aggregated_norm_BUREAU_CREDIT_TYPE_4_DAYS_CREDIT_max
        {-1029401945},// percentiles bureau_aggregated_norm_BUREAU_CREDIT_TYPE_4_DAYS_CREDIT_avg
        {2.16498e-314},// percentiles bureau_aggregated_norm_BUREAU_CREDIT_TYPE_4_DAYS_CREDIT_ENDDATE_max
        {0},// percentiles bureau_aggregated_norm_BUREAU_CREDIT_TYPE_4_AMT_CREDIT_MAX_OVERDUE_max
        {0},// percentiles bureau_aggregated_norm_BUREAU_CREDIT_TYPE_4_AMT_CREDIT_MAX_OVERDUE_avg
        {7.48509e-321},// percentiles bureau_aggregated_norm_BUREAU_CREDIT_TYPE_4_AMT_CREDIT_SUM_max
        {0},// percentiles bureau_aggregated_norm_BUREAU_CREDIT_TYPE_4_AMT_CREDIT_SUM_avg
        {6.95313e-310},// percentiles bureau_aggregated_norm_BUREAU_CREDIT_TYPE_4_AMT_CREDIT_SUM_DEBT_avg
        {2.18937e-314},// percentiles bureau_aggregated_norm_BUREAU_CREDIT_TYPE_4_AMT_CREDIT_SUM_DEBT_max
        {6.95326e-310},// percentiles bureau_aggregated_norm_BUREAU_CREDIT_TYPE_4_DEBT_CREDIT_DIFF_avg
        {4.94066e-323},// percentiles bureau_aggregated_norm_BUREAU_CREDIT_TYPE_5_DAYS_CREDIT_max
        {-353782080},// percentiles bureau_aggregated_norm_BUREAU_CREDIT_TYPE_5_DAYS_CREDIT_avg
        {6.95322e-310},// percentiles bureau_aggregated_norm_BUREAU_CREDIT_TYPE_5_DAYS_CREDIT_ENDDATE_max
        {4.42189e-321},// percentiles bureau_aggregated_norm_BUREAU_CREDIT_TYPE_5_AMT_CREDIT_MAX_OVERDUE_max
        {0},// percentiles bureau_aggregated_norm_BUREAU_CREDIT_TYPE_5_AMT_CREDIT_MAX_OVERDUE_avg
        {0},// percentiles bureau_aggregated_norm_BUREAU_CREDIT_TYPE_5_AMT_CREDIT_SUM_max
        {1.39065e-309},// percentiles bureau_aggregated_norm_BUREAU_CREDIT_TYPE_5_AMT_CREDIT_SUM_avg
        {0},// percentiles bureau_aggregated_norm_BUREAU_CREDIT_TYPE_5_AMT_CREDIT_SUM_DEBT_avg
        {3.23786e-319},// percentiles bureau_aggregated_norm_BUREAU_CREDIT_TYPE_5_AMT_CREDIT_SUM_DEBT_max
        {1.26481e-321},// percentiles bureau_aggregated_norm_BUREAU_CREDIT_TYPE_5_DEBT_CREDIT_DIFF_av
        {0},// percentiles bureau_agg_amt_credit_sum_dim_AMT_CREDIT_SUM_OVERDUE_max
        {0},// percentiles bureau_agg_amt_credit_sum_dim_AMT_CREDIT_SUM_OVERDUE_avg
        {0},// percentiles bureau_agg_amt_credit_sum_dim_AMT_CREDIT_SUM_OVERDUE_sum
        {0},// percentiles bureau_agg_credit_active_type_dim_CREDIT_ACTIVE_Active_AVG
        {0.4},// percentiles bureau_agg_credit_active_type_dim_CREDIT_ACTIVE_Closed_AVG
        {0.533333},// percentiles bureau_agg_credit_active_type_dim_CREDIT_ACTIVE_Sold_AVG
        {0},// percentiles bureau_agg_credit_active_type_dim_CREDIT_TYPE_1_AVG
        {0.0666667},// percentiles bureau_agg_credit_active_type_dim_CREDIT_TYPE_2_AVG
        {0},// percentiles bureau_agg_credit_active_type_dim_CREDIT_TYPE_4_AVG
        {0.0666667},// percentiles bureau_agg_credit_active_type_dim_CREDIT_TYPE_3_AVG
        {0.933333},// percentiles bureau_agg_credit_active_type_dim_CREDIT_TYPE_5_AVG
        {0,2250,2694.24,3198.42,3621.11,4099.95,4500,4940.1,5496.07,6069.33,6750,7405.69,8338.5,9238,10608.1,12235.5,14884.5,20425.6},// percentiles previous_app_fact_PREV_AMT_ANNUITY_MIN
        {0,4918.05,6648.8,8091.99,9437.04,10885.3,12223.2,13643.6,15338.4,17101.5,19318.7,22018,24246,27157.5,30801.7,35624.7,41278.4,47982.9,59074.3},// percentiles previous_app_fact_PREV_AMT_ANNUITY_MAX
        {0,4355.19,5493.19,6441.9,7283.01,8115.88,8949.96,9785.02,10680.8,11601.3,12614.6,13723.8,14960.3,16376.3,18025.3,20078.7,22681.2,26371.4,32842.9},// percentiles previous_app_fact_PREV_AMT_ANNUITY_MEAN
        {0,2250,3595.5,4500,5823,7170.98,9000,10386,12856.5,14985,19093.5,24421.5,40500},// percentiles previous_app_fact_PREV_AMT_DOWN_PAYMENT_MAX
        {0,1061.44,1852.5,2519.28,3301.48,4170,4941,6075,7407,9000,11533.5,15412.5,23058},// percentiles previous_app_fact_PREV_AMT_DOWN_PAYMENT_MEAN
        {3.85714,8,9.2,10,10.6,11,11.5,12,12.1818,12.5714,13,13.25,13.6667,14,14.4,15,15.3636,16,17},// percentiles previous_app_fact_PREV_HOUR_APPR_PROCESS_START_MEAN
        {0,0.0919918,0.100021,0.101826,0.104488,0.108909,0.108936,0.115713,0.162614,0.205603,0.217838,0.283206,0.391384},// percentiles previous_app_fact_PREV_RATE_DOWN_PAYMENT_MAX
        {0,0.0272348,0.044326,0.0522129,0.0639764,0.0797461,0.0984756,0.102347,0.107802,0.109854,0.140022,0.181515,0.229036},// percentiles previous_app_fact_PREV_RATE_DOWN_PAYMENT_MEAN
        {-2819,-2712,-2604,-2492,-2359,-2193,-1995,-1785,-1590,-1413,-1210,-1040,-888,-759,-646,-540,-436,-320,-43},// percentiles previous_app_fact_PREV_DAYS_DECISION_MIN
        {-1060556288,0},// percentiles previous_app_fact_PREV_DAYS_DECISION_MAX
        {-103,0},// percentiles previous_app_fact_PREV_DAYS_DECISION_MEAN
        {0,4.24399e-314},// percentiles previous_app_fact_PREV_DAYS_TERMINATION_MAX
        {0},// percentiles previous_app_fact_PREV_CREDIT_TO_ANNUITY_RATIO_MEAN
        {0,2.12199e-314},// percentiles previous_app_fact_PREV_CREDIT_TO_ANNUITY_RATIO_MAX
        {0,1.4854e-313},// percentiles previous_app_fact_PREV_APPLICATION_CREDIT_DIFF_MIN
        {0,5},// percentiles previous_app_fact_PREV_APPLICATION_CREDIT_DIFF_MAX
        {0,39},// percentiles previous_app_fact_PREV_APPLICATION_CREDIT_DIFF_MEAN
        {0,0.0330486},// percentiles previous_app_fact_PREV_APPLICATION_CREDIT_RATIO_MIN
        {0,0.484465},// percentiles previous_app_fact_PREV_APPLICATION_CREDIT_RATIO_MAX
        {0,2.122e-314},// percentiles previous_app_fact_PREV_APPLICATION_CREDIT_RATIO_MEAN
        {0,0.39622},// percentiles previous_app_fact_PREV_APPLICATION_CREDIT_RATIO_VAR
        {0,4.24399e-314},// percentiles previous_app_fact_PREV_DOWN_PAYMENT_TO_CREDIT_MEAN
        {0},// percentiles previous_app_fact_PREV_ACTIVE_SIMPLE_INTERESTS_MEAN
        {0,1},// percentiles previous_app_fact_PREV_ACTIVE_AMT_ANNUITY_MAX
        {0,20},// percentiles previous_app_fact_PREV_ACTIVE_AMT_ANNUITY_SUM
        {0,2},// percentiles previous_app_fact_PREV_ACTIVE_AMT_APPLICATION_MAX
        {0,0.00634382},// percentiles previous_app_fact_PREV_ACTIVE_AMT_APPLICATION_MEAN
        {0,1.73922},// percentiles previous_app_fact_PREV_ACTIVE_AMT_CREDIT_SUM
        {0,0.0330486},// percentiles previous_app_fact_PREV_ACTIVE_AMT_DOWN_PAYMENT_MAX
        {0,0.484465},// percentiles previous_app_fact_PREV_ACTIVE_AMT_DOWN_PAYMENT_MEAN
        {0,0.304578},// percentiles previous_app_fact_PREV_ACTIVE_DAYS_DECISION_MIN
        {0,0.39622},// percentiles previous_app_fact_PREV_ACTIVE_DAYS_DECISION_MEAN
        {0,0.0381619},// percentiles previous_app_fact_PREV_ACTIVE_DAYS_LAST_DUE_1ST_VERSION_MIN
        {0,25.6002},// percentiles previous_app_fact_PREV_ACTIVE_DAYS_LAST_DUE_1ST_VERSION_MAX
        {0,1.3168},// percentiles previous_app_fact_PREV_ACTIVE_DAYS_LAST_DUE_1ST_VERSION_MEAN
        {0,0.298731},// percentiles previous_app_fact_PREV_ACTIVE_AMT_PAYMENT_SUM
        {0,7.64758},// percentiles previous_app_fact_PREV_ACTIVE_INSTALMENT_PAYMENT_DIFF_MEAN
        {-1135.92,0},// percentiles previous_app_fact_PREV_ACTIVE_INSTALMENT_PAYMENT_DIFF_MAX
        {-9.06977,0},// percentiles previous_app_fact_PREV_ACTIVE_REMAINING_DEBT_MAX
        {0,0.0079845},// percentiles previous_app_fact_PREV_ACTIVE_REMAINING_DEBT_MEAN
        {0,0.285736},// percentiles previous_app_fact_PREV_ACTIVE_REMAINING_DEBT_SUM
        {0},// percentiles previous_app_fact_PREV_ACTIVE_REPAYMENT_RATIO_MEAN
        {0},// percentiles previous_app_fact_TOTAL_REPAYMENT_RATIO
        {0,0.116744},// percentiles previous_app_fact_APPROVED_AMT_ANNUITY_MIN
        {0,0.407038},// percentiles previous_app_fact_APPROVED_AMT_ANNUITY_MAX
        {0},// percentiles previous_app_fact_APPROVED_AMT_ANNUITY_MEAN
        {0,0.0155856},// percentiles previous_app_fact_APPROVED_AMT_CREDIT_MIN
        {0,0.600698},// percentiles previous_app_fact_APPROVED_AMT_CREDIT_MAX
        {0,0.193361},// percentiles previous_app_fact_APPROVED_AMT_CREDIT_MEAN
        {0,0.801348},// percentiles previous_app_fact_APPROVED_AMT_DOWN_PAYMENT_MAX
        {0,0.335534},// percentiles previous_app_fact_APPROVED_AMT_GOODS_PRICE_MAX
        {0,0.087537},// percentiles previous_app_fact_APPROVED_DAYS_DECISION_MIN
        {0,9.18785},// percentiles previous_app_fact_APPROVED_DAYS_DECISION_MEAN
        {0,0.691886},// percentiles previous_app_fact_APPROVED_DAYS_TERMINATION_MEAN
        {0,0.781243},// percentiles previous_app_fact_APPROVED_CREDIT_TO_ANNUITY_RATIO_MEAN
        {0,0.49714},// percentiles previous_app_fact_APPROVED_CREDIT_TO_ANNUITY_RATIO_MAX
        {0,0.0755592},// percentiles previous_app_fact_APPROVED_APPLICATION_CREDIT_DIFF_MAX
        {0},// percentiles previous_app_fact_APPROVED_APPLICATION_CREDIT_RATIO_MIN
        {0,1.21166e-311},// percentiles previous_app_fact_APPROVED_APPLICATION_CREDIT_RATIO_MAX
        {0,2.97079e-312},// percentiles previous_app_fact_APPROVED_APPLICATION_CREDIT_RATIO_MEAN
        {0,4.94066e-324},// percentiles previous_app_fact_APPROVED_DAYS_FIRST_DRAWING_MAX
        {0,2.18959e-314},// percentiles previous_app_fact_APPROVED_DAYS_FIRST_DRAWING_MEAN
        {0,2.16499e-314},// percentiles previous_app_fact_APPROVED_DAYS_FIRST_DUE_MIN
        {0,6.95313e-310},// percentiles previous_app_fact_APPROVED_DAYS_FIRST_DUE_MEAN
        {0,6.95313e-310},// percentiles previous_app_fact_APPROVED_DAYS_LAST_DUE_1ST_VERSION_MIN
        {0,2.18942e-314},// percentiles previous_app_fact_APPROVED_DAYS_LAST_DUE_1ST_VERSION_MAX
        {0},// percentiles previous_app_fact_APPROVED_DAYS_LAST_DUE_1ST_VERSION_MEAN
        {0,4.94066e-324},// percentiles previous_app_fact_APPROVED_DAYS_LAST_DUE_MAX
        {0,2.18956e-314},// percentiles previous_app_fact_APPROVED_DAYS_LAST_DUE_MEAN
        {0,6.95332e-310},// percentiles previous_app_fact_APPROVED_DAYS_LAST_DUE_DIFF_MIN
        {0,2.18958e-314},// percentiles previous_app_fact_APPROVED_DAYS_LAST_DUE_DIFF_MAX
        {0,2.16499e-314},// percentiles previous_app_fact_APPROVED_DAYS_LAST_DUE_DIFF_MEAN
        {0,2.16498e-314},// percentiles previous_app_fact_APPROVED_SIMPLE_INTERESTS_MIN
        {0,2.18959e-314},// percentiles previous_app_fact_APPROVED_SIMPLE_INTERESTS_MAX
        {0,6.95313e-310},// percentiles previous_app_fact_APPROVED_SIMPLE_INTERESTS_MEAN
        {0,2.18943e-314},// percentiles previous_app_fact_REFUSED_AMT_APPLICATION_MAX
        {0},// percentiles previous_app_fact_REFUSED_AMT_APPLICATION_MEAN
        {0,2.18959e-314},// percentiles previous_app_fact_REFUSED_AMT_CREDIT_MIN
        {0,6.95332e-310},// percentiles previous_app_fact_REFUSED_AMT_CREDIT_MAX
        {0,2.18956e-314},// percentiles previous_app_fact_REFUSED_DAYS_DECISION_MIN
        {0,6.95313e-310},// percentiles previous_app_fact_REFUSED_DAYS_DECISION_MAX
        {0,2.18943e-314},// percentiles previous_app_fact_REFUSED_DAYS_DECISION_MEAN
        {0,2.18959e-314},// percentiles previous_app_fact_REFUSED_APPLICATION_CREDIT_DIFF_MIN
        {0,2.16499e-314},// percentiles previous_app_fact_REFUSED_APPLICATION_CREDIT_DIFF_MAX
        {0,2.16499e-314},// percentiles previous_app_fact_REFUSED_APPLICATION_CREDIT_DIFF_MEAN
        {0,2.16499e-314},// percentiles previous_app_fact_REFUSED_APPLICATION_CREDIT_DIFF_VAR
        {0,6.95322e-310},// percentiles previous_app_fact_REFUSED_APPLICATION_CREDIT_RATIO_MIN
        {0,2.18958e-314},// percentiles previous_app_fact_REFUSED_APPLICATION_CREDIT_RATIO_MEAN
        {0,2.122e-314},// percentiles previous_app_fact_PREV_Consumer_AMT_CREDIT_SUM
        {0,2.16994e-320},// percentiles previous_app_fact_PREV_Consumer_AMT_ANNUITY_MEAN
        {0,5.22628e-299},// percentiles previous_app_fact_PREV_Consumer_AMT_ANNUITY_MAX
        {0,2.16498e-314},// percentiles previous_app_fact_PREV_Consumer_SIMPLE_INTERESTS_MIN
        {0},// percentiles previous_app_fact_PREV_Consumer_SIMPLE_INTERESTS_MEAN
        {0},// percentiles previous_app_fact_PREV_Consumer_SIMPLE_INTERESTS_MAX
        {0,7.48509e-321},// percentiles previous_app_fact_PREV_Consumer_SIMPLE_INTERESTS_VAR
        {0},// percentiles previous_app_fact_PREV_Consumer_APPLICATION_CREDIT_DIFF_MIN
        {0,6.95313e-310},// percentiles previous_app_fact_PREV_Consumer_APPLICATION_CREDIT_DIFF_VAR
        {0,2.18937e-314},// percentiles previous_app_fact_PREV_Consumer_APPLICATION_CREDIT_RATIO_MIN
        {0,6.95326e-310},// percentiles previous_app_fact_PREV_Consumer_APPLICATION_CREDIT_RATIO_MAX
        {0,4.94066e-323},// percentiles previous_app_fact_PREV_Consumer_APPLICATION_CREDIT_RATIO_MEAN
        {0,6.95313e-310},// percentiles previous_app_fact_PREV_Consumer_DAYS_DECISION_MAX
        {0,6.95322e-310},// percentiles previous_app_fact_PREV_Consumer_DAYS_LAST_DUE_1ST_VERSION_MAX
        {0,4.42189e-321},// percentiles previous_app_fact_PREV_Consumer_DAYS_LAST_DUE_1ST_VERSION_MEAN
        {0},// percentiles previous_app_fact_PREV_Consumer_CNT_PAYMENT_MEAN
        {0},// percentiles previous_app_fact_PREV_Cash_AMT_CREDIT_SUM
        {0,1.39065e-309},// percentiles previous_app_fact_PREV_Cash_AMT_ANNUITY_MEAN
        {0},// percentiles previous_app_fact_PREV_Cash_AMT_ANNUITY_MAX
        {0,3.23786e-319},// percentiles previous_app_fact_PREV_Cash_SIMPLE_INTERESTS_MIN
        {0,1.26481e-321},// percentiles previous_app_fact_PREV_Cash_SIMPLE_INTERESTS_MEAN
        {0,3.23786e-319},// percentiles previous_app_fact_PREV_Cash_SIMPLE_INTERESTS_MAX
        {0},// percentiles previous_app_fact_PREV_Cash_SIMPLE_INTERESTS_VAR
        {0},// percentiles previous_app_fact_PREV_Cash_APPLICATION_CREDIT_DIFF_MIN
        {0},// percentiles previous_app_fact_PREV_Cash_APPLICATION_CREDIT_DIFF_VAR
        {0},// percentiles previous_app_fact_PREV_Cash_APPLICATION_CREDIT_RATIO_MIN
        {-2.07034e-271,0},// percentiles previous_app_fact_PREV_Cash_APPLICATION_CREDIT_RATIO_MAX
        {0,2.42897e-319},// percentiles previous_app_fact_PREV_Cash_APPLICATION_CREDIT_RATIO_MEAN
        {0},// percentiles previous_app_fact_PREV_Cash_DAYS_DECISION_MAX
        {0},// percentiles previous_app_fact_PREV_Cash_DAYS_LAST_DUE_1ST_VERSION_MAX
        {-3.17637e-22,0},// percentiles previous_app_fact_PREV_Cash_DAYS_LAST_DUE_1ST_VERSION_MEAN
        {0,8.10021e-320},// percentiles previous_app_fact_PREV_Cash_CNT_PAYMENT_MEAN
        {0},// percentiles previous_app_fact_PREV_LAST12M_AMT_CREDIT_SUM
        {0},// percentiles previous_app_fact_PREV_LAST12M_AMT_ANNUITY_MEAN
        {0,2.64619e-260},// percentiles previous_app_fact_PREV_LAST12M_AMT_ANNUITY_MAX
        {0,2.64619e-260},// percentiles previous_app_fact_PREV_LAST12M_SIMPLE_INTERESTS_MEAN
        {0},// percentiles previous_app_fact_PREV_LAST12M_SIMPLE_INTERESTS_MAX
        {0},// percentiles previous_app_fact_PREV_LAST12M_DAYS_DECISION_MIN
        {0},// percentiles previous_app_fact_PREV_LAST12M_DAYS_DECISION_MEAN
        {0},// percentiles previous_app_fact_PREV_LAST12M_DAYS_LAST_DUE_1ST_VERSION_MIN
        {0},// percentiles previous_app_fact_PREV_LAST12M_DAYS_LAST_DUE_1ST_VERSION_MAX
        {0},// percentiles previous_app_fact_PREV_LAST12M_DAYS_LAST_DUE_1ST_VERSION_MEAN
        {0},// percentiles previous_app_fact_PREV_LAST12M_APPLICATION_CREDIT_DIFF_MIN
        {0},// percentiles previous_app_fact_PREV_LAST12M_APPLICATION_CREDIT_RATIO_MIN
        {0},// percentiles previous_app_fact_PREV_LAST12M_APPLICATION_CREDIT_RATIO_MAX
        {0},// percentiles previous_app_fact_PREV_LAST12M_APPLICATION_CREDIT_RATIO_MEAN
        {0,3.87479e-122},// percentiles previous_app_fact_PREV_LAST24M_AMT_CREDIT_SUM
        {0,2.15256e-133},// percentiles previous_app_fact_PREV_LAST24M_AMT_ANNUITY_MEAN
        {-4.84656e-120,0},// percentiles previous_app_fact_PREV_LAST24M_AMT_ANNUITY_MAX
        {-7.44225e-15,0},// percentiles previous_app_fact_PREV_LAST24M_SIMPLE_INTERESTS_MEAN
        {0,1.77996e-294},// percentiles previous_app_fact_PREV_LAST24M_SIMPLE_INTERESTS_MAX
        {0,3.38978e+50},// percentiles previous_app_fact_PREV_LAST24M_DAYS_DECISION_MIN
        {-7.60556e-121,0},// percentiles previous_app_fact_PREV_LAST24M_DAYS_DECISION_MEAN
        {-3.00461e-108,0},// percentiles previous_app_fact_PREV_LAST24M_DAYS_LAST_DUE_1ST_VERSION_MIN
        {0,4.51519e-39},// percentiles previous_app_fact_PREV_LAST24M_DAYS_LAST_DUE_1ST_VERSION_MAX
        {0,4.8888e+15},// percentiles previous_app_fact_PREV_LAST24M_DAYS_LAST_DUE_1ST_VERSION_MEAN
        {-1.07094e-146,0},// percentiles previous_app_fact_PREV_LAST24M_APPLICATION_CREDIT_DIFF_MIN
        {0,426334},// percentiles previous_app_fact_PREV_LAST24M_APPLICATION_CREDIT_RATIO_MIN
        {-1.58747e-301,0},// percentiles previous_app_fact_PREV_LAST24M_APPLICATION_CREDIT_RATIO_MAX
        {-2.26876e+11,0},// percentiles previous_app_fact_PREV_LAST24M_APPLICATION_CREDIT_RATIO_MEAN
        {-1996489698,338852},// percentiles previous_app_DIM_9_IDX
        {338852,159697700},// percentiles previous_app_DIM_6_IDX
        {-1965775054,338852},// percentiles previous_app_DIM_7_IDX
        {338852,1218367069},// percentiles previous_app_DIM_1_IDX
        {-995231787,338852},// percentiles previous_app_DIM_8_IDX
        {-852935439,338852},// percentiles previous_app_DIM_2_IDX
        {338852,1207802819},// percentiles previous_app_DIM_3_IDX
        {338852,258168222},// percentiles previous_app_DIM_11_IDX
        {-215231378,72970},// percentiles previous_app_DIM_4_IDX
        {-885506901,214996},// percentiles previous_app_DIM_10_IDX
        {331843,1706551433},// percentiles previous_app_DIM_5_IDX
        {0,1763621195},// percentiles previous_app_DIM_12_IDX
        {0,35551},// percentiles previous_app_DIM_13_IDX
        {-2914,-2883,-2737,-2595,-2461,-2302,-2108,-1883,-1668,-1472,-1268,-1068,-907,-767,-647,-536,-431,-313,-126},// percentiles installments_payments_fact_INS_DAYS_ENTRY_PAYMENT_MIN
        {-2059.49,-1723.76,-1534.73,-1403.79,-1284.12,-1168.31,-1055.45,-949.893,-853.903,-766.75,-684.821,-605.571,-529.764,-455,-385.333,-316.7,-248.957,-179.615,-78.8},// percentiles installments_payments_fact_INS_DAYS_ENTRY_PAYMENT_MEAN
        {0.18,10.08,54.855,137.88,833.895,2179.8,2885.09,3473.91,4019.89,4581.31,5199.3,5861.56,6655.01,7580.74,8714.34,10068.3,11856.3,14588.1,20208.1},// percentiles installments_payments_fact_INS_AMT_INSTALMENT_MIN
        {2436.97,5958.63,8274.19,10418.4,12639.9,15104.9,18015.8,21999,26476.1,32622.3,41273.1,51585.8,67500,89994.5,123423,173165,253975,405000,659320},// percentiles installments_payments_fact_INS_AMT_INSTALMENT_MAX
        {1741.53,4211.61,5361.03,6335.75,7247.59,8176.29,9086.44,10042.6,11054.8,12164.9,13400.7,14800.5,16456.4,18434.9,20956.2,24233.9,28753.5,35968.3,50141.3},// percentiles installments_payments_fact_INS_AMT_INSTALMENT_MEAN
        {16253,46487.9,69937.9,93697.2,118369,145060,175976,211579,254847,306144,369156,446588,540667,659936,815604,1.02087e+06,1.31126e+06,1.73863e+06,2.48653e+06},// percentiles installments_payments_fact_INS_AMT_INSTALMENT_SUM
        {0.045,1.44,3.735,9.765,24.03,51.75,99.675,204.66,522,1738.35,2900.34,3828.24,4710.28,5682.82,6846.16,8324.95,10161.8,12871.3,18186.4},// percentiles installments_payments_fact_INS_AMT_PAYMENT_MIN
        {2431.39,5970.47,8303.85,10473.2,12715.6,15217.9,18185,22275,26784.8,33157.7,42150.6,52543.8,68330.9,90769.1,126000,176812,258631,411578,675000},// percentiles installments_payments_fact_INS_AMT_PAYMENT_MAX
        {1623.25,3977.93,5100.61,6050.67,6950.9,7852.49,8777.9,9705.81,10722,11832.3,13080.8,14504.7,16210.2,18271.6,20922.7,24405.8,29298.3,37343.5,53594.1},// percentiles installments_payments_fact_INS_AMT_PAYMENT_MEAN
        {16076.4,45013.1,67859.7,91107,115155,141029,170863,205300,247379,297484,359035,435879,529916,649565,806621,1.0175e+06,1.31957e+06,1.77473e+06,2.57282e+06},// percentiles installments_payments_fact_INS_AMT_PAYMENT_SUM
        {0,0.0135135,0.0625,0.120567,0.2,0.306122,0.454545,0.675676,1.02439,1.66667,3.32258},// percentiles installments_payments_fact_INS_DPD_MEAN
        {0,0.0138889,0.0994152,0.302555,0.672515,1.33333,2.54339,4.97587,10.0833,22.2308,64.2574},// percentiles installments_payments_fact_INS_DPD_VAR
        {0.333333,3.42857,4.53846,5.41176,6.17526,6.89286,7.6,8.3125,9.02247,9.77273,10.5556,11.4091,12.3704,13.5,14.8033,16.45,18.6667,21.9,27.8095},// percentiles installments_payments_fact_INS_DBD_MEAN
        {0,6.02941,11.9821,18.5278,25.6,32.9849,40.6667,48.5641,56.9667,66.0966,76.25,87.5412,100.424,116.034,135.099,160.481,199.344,275.256,485.445},// percentiles installments_payments_fact_INS_DBD_VAR
        {-14778.2,-2787.45,-1.91473e-13,0,9.83238e-14},// percentiles installments_payments_fact_INS_PAYMENT_DIFFERENCE_MEAN
        {0.571429,0.931818,0.978571,1},// percentiles installments_payments_fact_INS_PAYMENT_RATIO_MEAN
        {0.00729217,0.321008,0.423254,0.5,0.56294,0.624994,0.683261,0.742835,0.8,0.847343,0.896552,0.935755,0.977528,1,1.13087},// percentiles installments_payments_fact_INS_LATE_PAYMENT_RATIO_MEAN
        {0,0.03125,0.0952381},// percentiles installments_payments_fact_INS_PAID_OVER_MEAN
        {-2431,-1425,-1265,-1167,-1092,-1021,-943,-861,-785,-712,-643,-573,-503,-432,-353,-273,-145,0},// percentiles installments_payments_fact_INS_36M_DAYS_ENTRY_PAYMENT_MIN
        {-1203.15,-959.5,-815.571,-723.593,-655.698,-600.25,-551.116,-506.224,-461.617,-419.5,-377.707,-336.429,-293.833,-250.571,-204.364,-157.571,-89.5,0},// percentiles installments_payments_fact_INS_36M_DAYS_ENTRY_PAYMENT_MEAN
        {0,1.35,18,67.5,224.1,1570.45,3360.96,4443.12,5367.02,6303.96,7328.74,8478.23,9685.22,11146,12953.4,15417.7,19489.5,27435.5},// percentiles installments_payments_fact_INS_36M_AMT_INSTALMENT_MIN
        {0,4350.06,7432.56,9924.34,12464.7,15311.5,18852.4,23368.3,28873.2,37081.5,47268.2,62104.1,84348.5,117166,167985,249270,401411,655889},// percentiles installments_payments_fact_INS_36M_AMT_INSTALMENT_MAX
        {0,2875.25,5034.62,6414.44,7628.88,8824.1,9985.47,11235.9,12597.9,14081.3,15818.5,17854.8,20387.1,23582.1,27723.8,33836,43833.9,64148.7},// percentiles installments_payments_fact_INS_36M_AMT_INSTALMENT_MEAN
        {0,25275,53822.3,77902.7,102556,128745,158610,193997,239021,294157,363092,449353,556742,698248,888048,1.1553e+06,1.55142e+06,2.23353e+06},// percentiles installments_payments_fact_INS_36M_AMT_INSTALMENT_SUM
        {0,0.45,3.6,15.21,44.01,94.86,211.275,562.68,1978.56,3656.39,4946.53,6190.88,7562.74,9099.45,10921.5,13287.4,17001,25060.1},// percentiles installments_payments_fact_INS_36M_AMT_PAYMENT_MIN
        {0,4360.81,7462.89,9975.6,12554.1,15446.4,19070.1,23641.6,29312.7,37951.8,48113.4,63564,86414.3,119669,171380,253776,407618,671933},// percentiles installments_payments_fact_INS_36M_AMT_PAYMENT_MAX
        {0,2744.86,4855.12,6180.47,7379.32,8576.67,9733.83,10984.4,12347.7,13846.7,15634.3,17740.8,20354.5,23706.5,28112.7,34832.3,45955.6,69627.6},// percentiles installments_payments_fact_INS_36M_AMT_PAYMENT_MEAN
        {0,24941.9,53021.2,76633.9,100931,126408,155826,190363,234849,289531,357528,444289,553602,697348,893765,1.17482e+06,1.60187e+06,2.34756e+06},// percentiles installments_payments_fact_INS_36M_AMT_PAYMENT_SUM
        {0,0.0416667,0.1,0.176471,0.290323,0.464286,0.770492,1.5},// percentiles installments_payments_fact_INS_36M_DPD_MEAN
        {0,0.0512091,0.19866,0.510256,1.09798,2.27273,5.30769,16.368},// percentiles installments_payments_fact_INS_36M_DPD_VAR
        {0,1.70642,3.38889,4.45652,5.35714,6.2,7,7.85714,8.71154,9.57143,10.5,11.5,12.6364,14,15.7143,18,21.4167,27.5556},// percentiles installments_payments_fact_INS_36M_DBD_MEAN
        {0,1.06667,5.53788,10.4,16.1111,22.4697,29.3788,36.7625,44.8,53.6964,63.8588,75.8097,90.1667,108.381,132.257,164.119,221.878,386.333},// percentiles installments_payments_fact_INS_36M_DBD_VAR
        {-18776.2,-3490.15,-1.58173e-13,0,4.0422e-14},// percentiles installments_payments_fact_INS_36M_PAYMENT_DIFFERENCE_MEAN
        {0,0.732143,0.931818,0.984536,1},// percentiles installments_payments_fact_INS_36M_PAYMENT_RATIO_MEAN
        {0,0.165694,0.345094,0.449094,0.533333,0.616546,0.700384,0.785114,0.857143,0.916667,0.970588,1,1.22448},// percentiles installments_payments_fact_INS_36M_LATE_PAYMENT_RATIO_MEAN
        {-2512,-2047,-1904,-1787,-1669,-1538,-1413,-1267,-1115,-993,-872,-764,-669,-577,-489,-399,-301,-157,0},// percentiles installments_payments_fact_INS_60M_DAYS_ENTRY_PAYMENT_MIN
        {-1515.8,-1320.84,-1167.71,-1036.6,-930.667,-843.125,-766.586,-697.543,-632.531,-570.412,-511.188,-452.1,-396.588,-341.333,-286,-230.767,-173,-96.8571,0},// percentiles installments_payments_fact_INS_60M_DAYS_ENTRY_PAYMENT_MEAN
        {0,1.125,16.2,67.5,190.395,1350,2801.07,3619.93,4356.94,5071.1,5822.82,6679.89,7633.26,8736.34,9971.28,11497.1,13530.3,16730.2,23583.7},// percentiles installments_payments_fact_INS_60M_AMT_INSTALMENT_MIN
        {0,3878.1,6906.87,9290.52,11591.4,14085,16974.5,20808,25404.5,31426.9,40059.2,50405.4,66147.3,88904.5,122262,172383,253584,404800,659035},// percentiles installments_payments_fact_INS_60M_AMT_INSTALMENT_MAX
        {0,2722.98,4715.99,5968.89,7051.38,8109.3,9130.94,10194.1,11311,12542.8,13882.9,15447.4,17262.7,19474.8,22274.1,25893.9,31044.5,39292.3,55771.4},// percentiles installments_payments_fact_INS_60M_AMT_INSTALMENT_MEAN
        {0,25279.8,52450,76120.8,100266,125965,154712,187311,227767,276927,336636,409667,500961,615834,766407,966259,1.24926e+06,1.66845e+06,2.39475e+06},// percentiles installments_payments_fact_INS_60M_AMT_INSTALMENT_SUM
        {0,0.27,2.475,7.155,21.195,50.31,102.51,214.56,554.805,1895.09,3280.99,4372.6,5419.4,6605.77,7994.07,9586.31,11635.4,14731.4,21310.2},// percentiles installments_payments_fact_INS_60M_AMT_PAYMENT_MIN
        {0,3882.42,6923.48,9322.29,11648.9,14179.2,17121.8,21090.9,25721.6,31874.1,40893.4,51318,67500,90000,125032,176078,258172,411364,675000},// percentiles installments_payments_fact_INS_60M_AMT_PAYMENT_MAX
        {0,2570.01,4508.44,5712.55,6778.02,7810.95,8844.19,9882.35,11007.8,12235,13591.4,15193.4,17057.7,19360.4,22290.3,26132,31703.7,41023.8,60213.2},// percentiles installments_payments_fact_INS_60M_AMT_PAYMENT_MEAN
        {0,24802.6,51106.8,74305.7,98120.7,123102,150960,182922,222282,270318,328763,401582,493265,608664,761476,966057,1.26229e+06,1.70922e+06,2.49296e+06},// percentiles installments_payments_fact_INS_60M_AMT_PAYMENT_SUM
        {0,0.05,0.105263,0.181818,0.285714,0.443182,0.684211,1.12766,2.23077},// percentiles installments_payments_fact_INS_60M_DPD_MEAN
        {0,0.0695122,0.231884,0.5625,1.14347,2.25794,4.75362,11.0712,33.1553},// percentiles installments_payments_fact_INS_60M_DPD_VAR
        {0,1.83333,3.66667,4.72727,5.61818,6.41667,7.19118,7.96875,8.73529,9.51351,10.3333,11.2,12.1978,13.3333,14.6842,16.375,18.6667,22,28},// percentiles installments_payments_fact_INS_60M_DBD_MEAN
        {0,1.4,6.26786,11.6667,17.8788,24.7,31.9,39.5338,47.6016,56.3143,65.944,76.9032,89.4646,104.491,123.184,146.964,181.324,248.091,438.564},// percentiles installments_payments_fact_INS_60M_DBD_VAR
        {-16368.6,-3103.45,-1.86563e-13,0,8.26813e-14},// percentiles installments_payments_fact_INS_60M_PAYMENT_DIFFERENCE_MEAN
        {0,0.75,0.94,0.983957,1},// percentiles installments_payments_fact_INS_60M_PAYMENT_RATIO_MEAN
        {0,0.167204,0.352833,0.45293,0.53122,0.604786,0.676186,0.75,0.818182,0.875,0.922,0.965184,1,1.17804},// percentiles installments_payments_fact_INS_60M_LATE_PAYMENT_RATIO_MEAN
        {-0.0985994,-0.030303,-0.0103873,-0.00262214,0,0.000243092,0.00404858,0.0128458,0.0345649,0.11218},// percentiles installments_payments_fact_120_TREND_DPD
        {-220.729,-58.287,-18.5552,-3.80251,0,7.08043,30.9609,84.128,225.982},// percentiles installments_payments_fact_120_TREND_PAID_OVER_AMOUNT
        {-0.13986,-0.0454545,-0.0034965,0,0.0034965,0.0524476,0.192308},// percentiles installments_payments_fact_12_TREND_DPD
        {-575.488,-171.769,0,180.119,609.251},// percentiles installments_payments_fact_12_TREND_PAID_OVER_AMOUNT
        {-0.106087,-0.0373626,-0.0134783,-2.86658e-17,0,0.0113043,0.04,0.136087},// percentiles installments_payments_fact_24_TREND_DPD
        {-354.845,-118.478,-33.9635,0,45.0498,141.617,380.492},// percentiles installments_payments_fact_24_TREND_PAID_OVER_AMOUNT
        {-0.0979021,-0.0307143,-0.0108641,-0.00291748,0,0.0030564,0.0120968,0.0357044,0.117647},// percentiles installments_payments_fact_60_TREND_DPD
        {-249.286,-69.0067,-23.7421,-3.97215,0,7.76373,36.5619,95.829,246.862},// percentiles installments_payments_fact_60_TREND_PAID_OVER_AMOUNT
        {0,0.025,0.125,0.259259,0.47191,0.85,1.83333},// percentiles installments_payments_fact_LAST_LOAN_DPD_mean
        {0,0.168995,0.447214,0.83205,1.33456,2.15883,4.22116},// percentiles installments_payments_fact_LAST_LOAN_DPD_std
        {0,1,2,5,9,17,41},// percentiles installments_payments_fact_LAST_LOAN_DPD_sum
        {0,0.25,0.358025,0.459677,0.55814,0.666667,0.769231,0.846154,0.909091,1},// percentiles installments_payments_fact_LAST_LOAN_LATE_PAYMENT_mean
        {0,33512.1},// percentiles installments_payments_fact_LAST_LOAN_PAID_OVER_AMOUNT_max
        {-3025.26,-1341.88,-539.077,-47.0782,0,5483.21},// percentiles installments_payments_fact_LAST_LOAN_PAID_OVER_AMOUNT_mean
        {-21284.9,-11699,-6797.79,-2925,0},// percentiles installments_payments_fact_LAST_LOAN_PAID_OVER_AMOUNT_min
        {0,933.754,2319.35,4098.63,7262.45,16800.6},// percentiles installments_payments_fact_LAST_LOAN_PAID_OVER_AMOUNT_std
        {-50819.5,-22105.7,-10360.5,-2250,0,56683.8},// percentiles installments_payments_fact_LAST_LOAN_PAID_OVER_AMOUNT_sum
        {0,0.181818},// percentiles installments_payments_fact_LAST_LOAN_PAID_OVER_mean
        {14,28,56,68,192,241,409,633,1366,2844,5802,10711,20158,36008,61955,101121,158291,234400,332749},// percentiles installments_payments_DIM_1_IDX
        {25,32,46,68,77,96,126,152,204,292,373,545,828,1108,1505,2084,2737,4610,35767},// percentiles installments_payments_DIM_8_IDX
        {172,173,198,245,397,676,933,1461,4803,27572,294938,324599},// percentiles installments_payments_DIM_2_IDX
        {12580,12584,12589,12592,12596,12604,12610,12621,12633,12654,12727,12792,13187,16595,24794,54608,246281,320526},// percentiles installments_payments_DIM_9_IDX
        {16752,16893,17318,18944,22450,30962,49203,85806,143486,207527,259326,289508,319511,325002},// percentiles installments_payments_DIM_3_IDX
        {4,8,21,28,29,35,36,54,87,156,529,1190,5621,8439,10764,12911,15898,170429,264124},// percentiles installments_payments_DIM_6_IDX
        {562,5960,6915,20611,49702,216499,297842,338212},// percentiles installments_payments_DIM_7_IDX
        {12954,240768,259777,301437,337584},// percentiles installments_payments_DIM_4_IDX
        {0,396,430,465,469,470,479,482,666,790,912,951,1069,1119,2569,8031,10130,208656},// percentiles installments_payments_DIM_5_IDX
        {0,6,7,9,11,12,14,16,19,21,24,26,30,34,38,44,51,61,78},// percentiles pos_cash_fact_POS_MONTHS_BALANCE_SIZE
        {0,112},// percentiles pos_cash_fact_POS_SK_DPD_MAX
        {0,6.94643e-310},// percentiles pos_cash_fact_POS_SK_DPD_MEAN
        {0},// percentiles pos_cash_fact_POS_SK_DPD_SUM
        {0},// percentiles pos_cash_fact_POS_SK_DPD_VAR
        {0},// percentiles pos_cash_fact_POS_SK_DPD_DEF_MEAN
        {0},// percentiles pos_cash_fact_POS_LATE_PAYMENT_MEAN
        {0},// percentiles pos_cash_fact_POS_LOAN_COMPLETED_MEAN
        {0},// percentiles pos_cash_fact_POS_REMAINING_INSTALMENTS_RATIO
        {0,337247},// percentiles pos_cash_DIM_4_IDX
        {0,337247},// percentiles pos_cash_DIM_1_IDX
        {0,337247},// percentiles pos_cash_DIM_2_IDX
        {0,337247},// percentiles pos_cash_DIM_3_IDX
        {0,93132.3,163336,258258},// percentiles credit_card_balance_fact_CC_AMT_BALANCE_MAX
        {0,31500,90000,139500},// percentiles credit_card_balance_fact_CC_AMT_DRAWINGS_ATM_CURRENT_MAX
        {0,45000,166500,347400},// percentiles credit_card_balance_fact_CC_AMT_DRAWINGS_ATM_CURRENT_SUM
        {0,67500,112500,180000},// percentiles credit_card_balance_fact_CC_AMT_DRAWINGS_CURRENT_MAX
        {0,135000,253568,504604},// percentiles credit_card_balance_fact_CC_AMT_DRAWINGS_CURRENT_SUM
        {0,8455.5,59535},// percentiles credit_card_balance_fact_CC_AMT_DRAWINGS_POS_CURRENT_MAX
        {0,12824.7,133064},// percentiles credit_card_balance_fact_CC_AMT_DRAWINGS_POS_CURRENT_SUM
        {0,4500,8100,12955.2},// percentiles credit_card_balance_fact_CC_AMT_INST_MIN_REGULARITY_MAX
        {0,1474.85,3464.84,6851.14},// percentiles credit_card_balance_fact_CC_AMT_INST_MIN_REGULARITY_MEAN
        {0,450,22500,64738.2,157500},// percentiles credit_card_balance_fact_CC_AMT_PAYMENT_TOTAL_CURRENT_MAX
        {0,45,3655.79,8001.82,17347.3},// percentiles credit_card_balance_fact_CC_AMT_PAYMENT_TOTAL_CURRENT_MEAN
        {0,837,107463,262479,548463},// percentiles credit_card_balance_fact_CC_AMT_PAYMENT_TOTAL_CURRENT_SUM
        {0,4719.55,2.90336e+07,1.57222e+08,9.24357e+08},// percentiles credit_card_balance_fact_CC_AMT_PAYMENT_TOTAL_CURRENT_VAR
        {0,92403.3,162399,255920},// percentiles credit_card_balance_fact_CC_AMT_TOTAL_RECEIVABLE_MAX
        {0,21681.8,63269.8,136657},// percentiles credit_card_balance_fact_CC_AMT_TOTAL_RECEIVABLE_MEAN
        {0,0.178947,0.657143,2.32},// percentiles credit_card_balance_fact_CC_CNT_DRAWINGS_CURRENT_MEAN
        {0,0.0588235,1.5},// percentiles credit_card_balance_fact_CC_CNT_DRAWINGS_POS_CURRENT_MEAN
        {0,0.0121951},// percentiles credit_card_balance_fact_CC_SK_DPD_MEAN
        {0,0.908129,1.02545,1.0578},// percentiles credit_card_balance_fact_CC_LIMIT_USE_MAX
        {0,0.218305,0.448616,0.719411},// percentiles credit_card_balance_fact_CC_LIMIT_USE_MEAN
        {0,0.297,1.00399},// percentiles credit_card_balance_fact_CC_PAYMENT_DIV_MIN_MIN
        {0,1.01126,10.3002},// percentiles credit_card_balance_fact_CC_PAYMENT_DIV_MIN_MEAN
        {0,246.96,174200},// percentiles credit_card_balance_fact_CC_LAST_AMT_BALANCE_MEAN
        {0,246.96,175487},// percentiles credit_card_balance_fact_CC_LAST_AMT_BALANCE_MAX
        {0,21883.5,63701,137668},// percentiles credit_card_balance_fact_INS_12M_AMT_BALANCE_MEAN
        {0,93132.3,163336,258258},// percentiles credit_card_balance_fact_INS_12M_AMT_BALANCE_MAX
        {0,0.908129,1.02545,1.0578},// percentiles credit_card_balance_fact_INS_12M_LIMIT_USE_MAX
        {0,0.218305,0.448616,0.719411},// percentiles credit_card_balance_fact_INS_12M_LIMIT_USE_MEAN
        {0,21883.5,63701,137668},// percentiles credit_card_balance_fact_INS_24M_AMT_BALANCE_MEAN
        {0,93132.3,163336,258258},// percentiles credit_card_balance_fact_INS_24M_AMT_BALANCE_MAX
        {0,0.908129,1.02545,1.0578},// percentiles credit_card_balance_fact_INS_24M_LIMIT_USE_MAX
        {0,0.218305,0.448616,0.719411},// percentiles credit_card_balance_fact_INS_24M_LIMIT_USE_MEAN
        {0,21883.5,63701,137668},// percentiles credit_card_balance_fact_INS_48M_AMT_BALANCE_MEAN
        {0,93132.3,163336,258258},// percentiles credit_card_balance_fact_INS_48M_AMT_BALANCE_MAX
        {0,0.908129,1.02545,1.0578},// percentiles credit_card_balance_fact_INS_48M_LIMIT_USE_MAX
        {0,0.218305,0.448616,0.719411},// percentiles credit_card_balance_fact_INS_48M_LIMIT_USE_MEAN
        };
    }
}
