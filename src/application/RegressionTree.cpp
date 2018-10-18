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
    Query* complementQuery = nullptr;
};

std::vector<QueryThresholdPair> queryToThreshold;

RegressionTree::RegressionTree(
    const string& pathToFiles, shared_ptr<Launcher> launcher, bool classification) :
    _classification(classification), _pathToFiles(pathToFiles)
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

    genDynamicFunctions();

    if (_classification)
        classificationTreeQueries();
    else
        regressionTreeQueries();
    
    _compiler->compile();

    generateCode();
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
    for (size_t rel = 0; rel < _td->numberOfRelations(); ++rel)
    {
        TDNode* relation = _td->getRelation(rel);
        var_bitset& relationBag = relation->_bag;

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
                aggC->_agg.push_back(_candidateMask[NUM_OF_VARIABLES]);
                            
                /* Q_L */
                aggL->_agg.push_back(_candidateMask[NUM_OF_VARIABLES]);
                aggL->_agg[0].set(0);
        
                /* Q_Q */
                aggQ->_agg.push_back(_candidateMask[NUM_OF_VARIABLES]);
                aggQ->_agg[0].set(1);

                
                Query* query = new Query();
                query->_rootID = _queryRootIndex[var];  
                query->_aggregates = {aggC,aggL,aggQ};
                query->_fVars.set(var);
                
                _compiler->addQuery(query);

                Aggregate* compaggC = new Aggregate();
                Aggregate* compaggL = new Aggregate();
                Aggregate* compaggQ = new Aggregate();
                
                /* Q_C */
                compaggC->_agg.push_back(_candidateMask[NUM_OF_VARIABLES]);
                            
                /* Q_L */
                compaggL->_agg.push_back(_candidateMask[NUM_OF_VARIABLES]);
                compaggL->_agg[0].set(0);
        
                /* Q_Q */
                compaggQ->_agg.push_back(_candidateMask[NUM_OF_VARIABLES]);
                compaggQ->_agg[0].set(1);

                // We add an additional query without free vars to compute the
                // complement of each threshold 
                Query* complementQuery = new Query();
                complementQuery->_rootID = _queryRootIndex[var];  
                complementQuery->_aggregates = {compaggC,compaggL,compaggQ};

                _compiler->addQuery(complementQuery);

                QueryThresholdPair qtPair;
                qtPair.query = query;
                qtPair.varID = var;
                qtPair.function = nullptr;
                qtPair.complementQuery = complementQuery;
                
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
                    aggC->_agg.push_back(_candidateMask[NUM_OF_VARIABLES]);
                    aggC->_agg[0].set(f);

                    /* Q_L */
                    aggL->_agg.push_back(_candidateMask[NUM_OF_VARIABLES]);
                    aggL->_agg[0].set(0);
                    aggL->_agg[0].set(f);
        
                    /* Q_Q */
                    aggQ->_agg.push_back(_candidateMask[NUM_OF_VARIABLES]);
                    aggQ->_agg[0].set(1);
                    aggQ->_agg[0].set(f);
                                        
                    Aggregate* aggC_complement = new Aggregate();
                    Aggregate* aggL_complement = new Aggregate();
                    Aggregate* aggQ_complement = new Aggregate();
        
                    /* Q_C */
                    aggC_complement->_agg.push_back(_candidateMask[NUM_OF_VARIABLES]);
                    aggC_complement->_agg[0].set(_complementFunction[f]);

                    /* Q_L */
                    aggL_complement->_agg.push_back(_candidateMask[NUM_OF_VARIABLES]);
                    aggL_complement->_agg[0].set(0);
                    aggL_complement->_agg[0].set(_complementFunction[f]);
        
                    /* Q_Q */
                    aggQ_complement->_agg.push_back(_candidateMask[NUM_OF_VARIABLES]);
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


void RegressionTree::classificationTreeQueries()
{
    Query* continuousQuery = new Query();
    continuousQuery->_rootID = _queryRootIndex[_labelID];
    continuousQuery->_fVars.set(_labelID);
    _compiler->addQuery(continuousQuery);
    
    // Simple count for each category
    Aggregate* countCategory = new Aggregate();
    countCategory->_agg.push_back(_candidateMask[NUM_OF_VARIABLES]);
    continuousQuery->_aggregates.push_back(countCategory);
    
    // Add to compiler list
    Query* countQuery = new Query();
    countQuery->_rootID = _queryRootIndex[_labelID]; // TODO: is this ok?!
    _compiler->addQuery(countQuery);

    // Overall count of tuples satisfying the parent conditions
    Aggregate* count = new Aggregate();
    count->_agg.push_back(_candidateMask[NUM_OF_VARIABLES]);
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
                agg->_agg.push_back(_candidateMask[NUM_OF_VARIABLES]);
                
                Query* query = new Query();
                query->_rootID = _queryRootIndex[var];
                query->_fVars.set(var);
                query->_fVars.set(_labelID);
                query->_aggregates.push_back(agg);
                
                _compiler->addQuery(query);

                /* Count Query keeps track of the count of each category
                 * independent of label */
                Aggregate* countagg = new Aggregate();
                countagg->_agg.push_back(_candidateMask[NUM_OF_VARIABLES]);
                
                Query* varCountQuery = new Query();
                varCountQuery->_rootID = _queryRootIndex[var];  
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
                    agg->_agg.push_back(_candidateMask[NUM_OF_VARIABLES]);
                    agg->_agg[0].set(f);

                    // TODO: technically we can infer the comp agg from the
                    // first aggregate - but I will add it for now to make it easier
                    Aggregate* agg_complement = new Aggregate();
                    agg_complement->_agg.push_back(_candidateMask[NUM_OF_VARIABLES]);
                    agg_complement->_agg[0].set(_complementFunction[f]);

                    continuousQuery->_aggregates.push_back(agg);
                    continuousQuery->_aggregates.push_back(agg_complement);
                    
                    // The below should give us the size of the group - may not
                    // be necessary
                    Aggregate* agg_count = new Aggregate();
                    agg_count->_agg.push_back(_candidateMask[NUM_OF_VARIABLES]);
                    agg_count->_agg[0].set(f);
                    
                    // TODO: technically we can infer the comp agg from the
                    // first aggregate - but I will add it for now to make it easier
                    Aggregate* agg_count_complement = new Aggregate();
                    agg_count_complement->_agg.push_back(
                        _candidateMask[NUM_OF_VARIABLES]);
                    agg_count_complement->_agg[0].set(_complementFunction[f]);

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
        if (_candidateMask[NUM_OF_VARIABLES].test(f))
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


    ofs.open("runtime/cpp/DynamicFunctionsGenerator.hpp", std::ofstream::out);
    ofs << dynamicFunctionsGenerator();
    ofs.close();
}


std::string RegressionTree::dynamicFunctionsGenerator()
{
    std::string varList = "", relationMap = ""; 
    for (size_t var = 0; var< _td->numberOfAttributes(); ++var)
    {
	varList += "\""+_td->getAttribute(var)->_name+"\","; 
	relationMap += std::to_string(_queryRootIndex[var])+",";
    }
    varList.pop_back();
    relationMap.pop_back();
    
    std::string relationName = "", varTypeList = "", conditionsString = ""; 
    for (size_t rel = 0; rel < _td->numberOfRelations(); ++rel)
    {
	relationName += "\""+_td->getRelation(rel)->_name+"\",";
        conditionsString += "\"\",";

        varTypeList += "\"";

        var_bitset& bag = _td->getRelation(rel)->_bag;
        for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
        {
            if (bag[var])
            {
                Attribute* att = _td->getAttribute(var);
                varTypeList += typeToStr(att->_type)+" "+att->_name+",";
            }
        }
        varTypeList.pop_back();
        varTypeList += "\",";
    }
    relationName.pop_back();
    conditionsString.pop_back();
    varTypeList.pop_back();

    std::string headerFiles =
        "#include <fstream>\n#include <vector>\n#include \"RegressionTreeNode.hpp\"\n\n";
    
    return headerFiles+
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
        std::to_string(_td->numberOfRelations())+"; ++rel)\n"+offset(1)+"{\n"+
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
            size_t& viewID = query->_aggregates[0]->_incoming[0].first;
            size_t& viewID2 = query->_aggregates[1]->_incoming[0].first;
            size_t& viewID3 = query->_aggregates[2]->_incoming[0].first;

            Query* complement = qtPair.complementQuery;
            const size_t& comp_viewID = complement->_aggregates[0]->_incoming[0].first;

            if (viewID != viewID2 || viewID != viewID3)
                std::cout << "THERE IS AN ERROR IN genVarianceComputation" << std::endl;
        
            const size_t& count = query->_aggregates[0]->_incoming[0].second;
            const size_t& linear = query->_aggregates[1]->_incoming[0].second;
            const size_t& quad = query->_aggregates[2]->_incoming[0].second;

            const size_t& comp_count = complement->_aggregates[0]->_incoming[0].second;
            // const size_t& comp_linear=complement->_aggregates[1]->_incoming[0].second;
            // const size_t& comp_quad = complement->_aggregates[2]->_incoming[0].second;
        
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
            
            Attribute* att = _td->getAttribute(qtPair.varID);

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
                const size_t& comp_viewID =
                    complement->_aggregates[0]->_incoming[0].first;
                const size_t& comp_count =
                    complement->_aggregates[0]->_incoming[0].second;
                const size_t& comp_linear =
                    complement->_aggregates[1]->_incoming[0].second;
                const size_t& comp_quad =
                    complement->_aggregates[2]->_incoming[0].second;

                if (comp_count != comp_linear-1 || comp_count != comp_quad-2 ||
                    comp_linear != comp_quad-1)
                {    
                    ERROR("We should have expected the aggregates to be contiguous!\n");
                    exit(1);
                }

                std::cout << "complement: " << comp_viewID << "  " << viewID << "\n";
                std::cout << complement->_fVars << std::endl;
                std::cout << _compiler->getView(viewID)->_fVars << std::endl;
                
                categVariance += offset(2)+"compaggs = &V"+std::to_string(comp_viewID)+
                    "[0].aggregates["+std::to_string(comp_count)+"];\n"+
                    offset(2)+"for ("+viewString+"_tuple& "+viewString+"tuple : "+
                    viewString+")\n"+offset(2)+"{\n"+
                    offset(3)+"if("+viewString+"tuple.aggregates[0] == 0)\n"+
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
        offset(2)+"ofs << thresholdMap[threshold].varID << \"\\t\" << "+
        "thresholdMap[threshold].threshold  << \"\\t\" << "+
        "thresholdMap[threshold].categorical  << \"\\t\" << "+
        "thresholdMap[threshold].aggregates[0]  << \"\\t\" << "+
        "thresholdMap[threshold].compAggregates[0] << \"\\t\" << "+
        "thresholdMap[threshold].aggregates[1] << \"\\t\" << "+
        "thresholdMap[threshold].compAggregates[1] << std::endl;\n"+
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
    std::string returnString = "";
    
    std::vector<std::vector<std::string>> giniPerView(_compiler->numberOfViews());
    std::vector<std::vector<std::string>> thresholdPerView(_compiler->numberOfViews());
    std::vector<Query*> complementQueryPerView(_compiler->numberOfViews());
    std::vector<size_t> firstThresholdPerVariable(NUM_OF_VARIABLES);
    
    QueryThresholdPair& continuousPair = queryToThreshold[0];
    // QueryThresholdPair& countPair = queryToThreshold[1];

    Query* continuousQuery = continuousPair.query;
    Query* countQuery = continuousPair.complementQuery;

    const size_t& continuousViewID =
        continuousQuery->_aggregates[0]->_incoming[0].first;
    const size_t& countViewID =
        countQuery->_aggregates[0]->_incoming[0].first;

    string viewStr = "V"+std::to_string(continuousViewID);
    string countViewStr = "V"+std::to_string(countViewID);
    string overallCountViewStr = "V"+std::to_string(countViewID);

    string numAggs = std::to_string(_functionOfAggregate.size());

    /* This creates the gini computation for continuous variables. */ 
    returnString += offset(2)+"double squaredSum["+numAggs+"] = {}, "+
        "largestCount["+numAggs+"] = {};\n"+
        offset(2)+"for("+viewStr+"_tuple& tuple : "+viewStr+")\n"+offset(2)+"{\n"+
        offset(3)+"for (size_t agg = 1; agg < "+numAggs+"; ++agg)\n"+
        offset(3)+"{\n"+
        offset(4)+"squaredSum[agg] += tuple.aggregates[agg] * tuple.aggregates[agg];\n"+
        offset(4)+"largestCount[agg] = (tuple.aggregates[agg] > largestCount[agg] ? "+
        "tuple.aggregates[agg] : largestCount[agg]);\n"+
        offset(3)+"}\n"+
        offset(2)+"}\n"+
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

    // TODO: // TODO: // TODO: The below is just a check and can probably be removed ! 
    size_t numOfContThresholds = 0;
    for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
    {
        if (!_features.test(var) || _categoricalFeatures.test(var))
            continue;
        numOfContThresholds += _thresholds[var].size();
    }
    if (numOfContThresholds != (_functionOfAggregate.size()-1) / 2)
        std::cout << "WHY IS THIS THIS CASE!?!?!?\n";
    // TODO: // TODO: // TODO:

    std::string numOfThresholds = "numberOfThresholds = "+
        std::to_string(_functionOfAggregate.size() / 2);

    std::string thresholdMap = "";
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
            
            size_t& viewID = query->_aggregates[0]->_incoming[0].first;
            size_t& countViewID = varCountQuery->_aggregates[0]->_incoming[0].first;

            // const size_t& count = query->_aggregates[0]->_incoming[0].second;
            // const size_t& varCount=varCountQuery->_aggregates[0]->_incoming[0].second;

            std::string viewStr = "V"+std::to_string(viewID);
            string labelViewStr = "V"+std::to_string(continuousViewID);
            std::string countViewStr = "V"+std::to_string(countViewID);
            
            std::string label = _td->getAttribute(_labelID)->_name;
            std::string var = _td->getAttribute(qtPair.varID)->_name;
            
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
                offset(4)+"largestCount[categIndex*2] = (tuple.aggregates[0] > "+
                "largestCount[categIndex*2] ? tuple.aggregates[categIndex*2] : "+
                "largestCount[categIndex*2]);\n"+
                offset(4)+"largestCount[categIndex*2+1] = (complementCount > "+
                "largestCount[categIndex*2+1] ? complementCount : "+
                "largestCount[categIndex*2+1]);\n"+
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

            Attribute* att = _td->getAttribute(qtPair.varID);
            thresholdMap +=
                offset(2)+"for ("+countViewStr+"_tuple& tuple : "+countViewStr+")\n"+
                offset(3)+"thresholdMap[categIndex++].set("
                +std::to_string(qtPair.varID)+",tuple."+att->_name+",1,"+
                "&tuple.aggregates[0],&"+overallCountViewStr+"[0].aggregates[0]);\n";
        }
        else
        {
            // Continuous variable
            cout << " WE SHOULD NEVER GET HERE !!! \n";
        }
    }

    
    std::string initVariance = offset(2)+numOfThresholds+";\n"+
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
        offset(2)+"ofs << thresholdMap[threshold].varID << \"\\t\" << "+
        "thresholdMap[threshold].threshold  << \"\\t\" << "+
        "thresholdMap[threshold].categorical  << \"\\t\" << "+
        "thresholdMap[threshold].aggregates[0]  << \"\\t\" << "+
        "thresholdMap[threshold].compAggregates[0]  << \"\\t\" << "+
        "largestCount[threshold*2]  << \"\\t\" << "+
        "largestCount[threshold*2+1] << std::endl;\n"+
        offset(2)+"ofs.close();\n";
    
    return thresholdStruct+
        offset(1)+"size_t numberOfThresholds;\n"+
        offset(1)+"double* gini = nullptr;\n"+
        offset(1)+"Threshold* thresholdMap = nullptr;\n\n"+
        offset(1)+"void initCostArray()\n"+
        offset(1)+"{\n"+initVariance+
        offset(2)+"thresholdMap = new Threshold[numberOfThresholds];\n"+
        thresholdMap+offset(1)+"}\n\n"+
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
    if (_classification)
        ofs << genGiniComputation() << std::endl;
    else
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
        _pathToFiles.compare("../../data/usretailer") == 0 ||
        _pathToFiles.compare("../datasets/usretailer_ml") == 0)
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

    if (_pathToFiles.compare("data/favorita") == 0 ||
        _pathToFiles.compare("../../data/favorita") == 0 ||
        _pathToFiles.compare("../datasets/favorita_ml") == 0)
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


    if (_pathToFiles.compare("data/favorita") == 0 ||
        _pathToFiles.compare("../../data/favorita") == 0 ||
        _pathToFiles.compare("../datasets/favorita_ml") == 0)
    {
        _thresholds = {
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
        };
    }
}
