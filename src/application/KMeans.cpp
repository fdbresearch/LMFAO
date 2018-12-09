//--------------------------------------------------------------------
//
// KMeans.cpp
//
// Created on: Nov 2, 2018
// Author: Max
//
//--------------------------------------------------------------------


#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <fstream>

#include <Launcher.h>
#include <KMeans.h>

static const char COMMENT_CHAR = '#';
static const char NUMBER_SEPARATOR_CHAR = ',';
static const char ATTRIBUTE_NAME_CHAR = ':';

using namespace std;
using namespace multifaq::params;
namespace phoenix = boost::phoenix;
using namespace boost::spirit;

using namespace std;

KMeans::KMeans(
    const string& pathToFiles, shared_ptr<Launcher> launcher) :
    _pathToFiles(pathToFiles)
{
    _compiler = launcher->getCompiler();
    _td = launcher->getTreeDecomposition();
}

KMeans::~KMeans()
{
}

void KMeans::run()
{
    loadFeatures();
    modelToQueries();
    _compiler->compile();
}


void KMeans::modelToQueries()
{
    std::vector<std::pair<size_t,Query*>> categoricalQueries;
    std::vector<std::pair<size_t,Query*>> continuousQueries;
    
    for (size_t var = 0; var < NUM_OF_VARIABLES; ++var) 
    {
        if (!_isFeature[var])
            continue;

        // Categorical variable
        Aggregate* agg = new Aggregate();
        agg->_agg.push_back(prod_bitset());

        // Create a query & Aggregate
        Query* query = new Query();
        query->_aggregates.push_back(agg);
        query->_rootID = _queryRootIndex[var];
        query->_fVars.set(var);

        _compiler->addQuery(query);

        
        categoricalQueries.push_back(query);
            
        if (_isCategoricalFeature[var])
        {
            // Categorical variable
            categoricalQueries.push_back({var,query});
        }
        else
        {
            // Continuous variable
            continuousQueries.push_back({var,query});
        }
    }
}


void KMeans::loadFeatures()
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
        if (featureNo == 0 && categorical == 1)
        {
            ERROR("The label needs to be continuous! ");
            exit(1);
        }

        _isFeature.set(attributeID);
        _features.set(attributeID);

        if (categorical)
            _isCategoricalFeature.set(attributeID);
        
        _queryRootIndex[attributeID] = rootID;
        
        /* Clear string stream. */
        ssLine.clear();
    }
}

void KMeans::generateCode(const std::string& outputDirectory)
{
    // TODO: Generate code for KMeans
    
    // TODO: for each categorical query: sort the view by the count
    // Then select the top k-1 as clusters and the others are one clusters

    // TODO: I need to turn the kmeans per dimension into a grid
    // then find the weight for each grip point
    // the latter is done 
    
    std::string runFunction = offset(1)+"void runApplication()\n"+offset(1)+"{\n"+
        offset(2)+"int64_t startProcess = duration_cast<milliseconds>("+
        "system_clock::now().time_since_epoch()).count();\n"+
        offset(2)+"computeGridPoints();\n"+
        "\n"+offset(2)+"int64_t endProcess = duration_cast<milliseconds>("+
        "system_clock::now().time_since_epoch()).count()-startProcess;\n"+
        offset(2)+"std::cout << \"Run Application: \"+"+
        "std::to_string(endProcess)+\"ms.\\n\";\n\n"+
        offset(2)+"std::ofstream ofs(\"times.txt\",std::ofstream::out | " +
        "std::ofstream::app);\n"+
        offset(2)+"ofs << \"\\t\" << endProcess << std::endl;\n"+
        offset(2)+"ofs.close();\n\n"+
        offset(1)+"}\n";
    
    std::ofstream ofs(outputDirectory+"/ApplicationHandler.h", std::ofstream::out);
    ofs << "#ifndef INCLUDE_APPLICATIONHANDLER_HPP_\n"<<
        "#define INCLUDE_APPLICATIONHANDLER_HPP_\n\n"<<
        "#include \"DataHandler.h\"\n\n"<<
        "namespace lmfao\n{\n"<<
        offset(1)+"void runApplication();\n"<<
        "}\n\n#endif /* INCLUDE_APPLICATIONHANDLER_HPP_*/\n";    
    ofs.close();

    ofs.open(outputDirectory+"/ApplicationHandler.cpp", std::ofstream::out);
    
    ofs << "#include \"ApplicationHandler.h\"\nnamespace lmfao\n{\n";
    ofs << genComputeGridPointsFunction() << std::endl;
    ofs << runFunction << std::endl;
    ofs << "}\n";
    ofs.close();
}

std::string KMeans::genComputeGridPointsFunction()
{
#if defined(__GNUC__) && defined(NDEBUG) && !defined(__clang__)
    std::string sortAlgo = "__gnu_parallel::sort(";
#else
    std::string sortAlgo = "std::sort(";
#endif

    std::string returnString = offset(1)+"void computeClusters()\n"+offset(1)+"{\n";
    
    for (auto& varQueryPair : categoricalQueries)
    {
        // Get view for this query
        std::pair<size_t,size_t>& viewAggPair =
            varQueryPair.second->_aggregates[0]->_incoming[0];

        std::string viewName = "V"+std::to_string(viewAggPair.first);
        
        // Sort the view on the count
        returnString += offset(2)+sortAlgo+viewName+".begin(),"+
            viewName+".end(),[ ](const "+viewName+"_tuple& lhs, const "+viewName+
            "_tuple& rhs)\n"+offset(2)+"{\n"+
            offset(3)+"return lhs.aggregates[0] < rhs.aggregates[0];\n"+
            offset(2)+"});\n";
        
        // Create function for all k clusters
        for (size_t i = 0; i < k; ++i)
        {
            // TODO: we should have an array which stores the values of the
            // functions

            // the functions then get this value and generate the count for each
            // grid point

            // so here I just need to set the value in the array 
        }
    }
    

    for (auto& varQueryPair : categoricalQueries)
    {
        // Get view for this query
        std::pair<size_t,size_t>& viewAggPair =
            varQueryPair.second->_aggregates[0]->_incoming[0];

        std::string viewName = "V"+std::to_string(viewAggPair.first);
        Attribute* att = _td->getAttribute(varQueryPair.first);
        std::string& varName = att->_name;
        
        // Sort the view on the variable
        returnString += offset(2)+sortAlgo+viewName+".begin(),"+
            viewName+".end(),[ ](const "+viewName+"_tuple& lhs, const "+viewName+
            "_tuple& rhs)\n"+offset(2)+"{\n"+
            offset(3)+"return lhs."+varName+" < rhs."+varName+";\n"+
            offset(2)+"});\n";
        
        // Gen Dynamic programming code TODO: TODO: create cluster!!

        // First we create a vector of size number of tuples in View 
        // Then we use that vector to compute running sums

        // These running sums can be used to fill first row in matrix

        // 
    }
    

    returnString += offset(1)+"}\n";
    
    std::string returnString = offset(1)+"void computeGridPoints()\n"+offset(1)+"{\n";

    
    // Sort the categorical variables and choose the top k categories

    // TODO: How do we exploit the FDs? And generate grid?
    
    // TODO: What is the dimensionality of the Grid? -- d or m ?

    // TODO: What is the relational encoding of the Grid?
    
    return returnString+offset(1)+"}\n";
}
