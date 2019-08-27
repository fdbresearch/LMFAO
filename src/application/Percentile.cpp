//--------------------------------------------------------------------
//
// Percentile.cpp
//
// Created on: Feb 1, 2018
// Author: Max
//
//--------------------------------------------------------------------

#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <fstream>

#include <Launcher.h>
#include <Percentile.h>

static const char COMMENT_CHAR = '#';
static const char NUMBER_SEPARATOR_CHAR = ',';
static const char ATTRIBUTE_NAME_CHAR = ':';

using namespace std;

using namespace multifaq::params;
using namespace multifaq::dir;
using namespace multifaq::config;

namespace phoenix = boost::phoenix;
using namespace boost::spirit;

const size_t numberOfPercentiles = 20;

Percentile::Percentile(shared_ptr<Launcher> launcher)
{
    _compiler = launcher->getCompiler();
    _td = launcher->getTreeDecomposition();
    _db = launcher->getDatabase();
}

Percentile::~Percentile()
{
}

void Percentile::run()
{    
    loadFeatures();
    modelToQueries();
    _compiler->compile();
}


void Percentile::modelToQueries()
{
    varToQuery = new Query*[NUM_OF_VARIABLES+1];

    Aggregate* agg = new Aggregate();
    agg->_sum.push_back(prod_bitset());

    Query* count = new Query();
    count->_root = _td->_root;
    count->_aggregates.push_back(agg);            
    _compiler->addQuery(count);

    varToQuery[NUM_OF_VARIABLES] = count;

    // Create a query & Aggregate
    for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
    {
        if (_isFeature[var] && !_isCategoricalFeature[var])
        {
            Aggregate* agg = new Aggregate();
            agg->_sum.push_back(prod_bitset());

            Query* query = new Query();
            query->_root = _td->getTDNode(_queryRootIndex[var]);
            query->_fVars.set(var);
            query->_aggregates.push_back(agg);
            
            _compiler->addQuery(query);

            varToQuery[var] = query;
        }
    }
}


void Percentile::loadFeatures()
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
            _isCategoricalFeature.set(attributeID);

        _queryRootIndex[attributeID] = rootID;

        /* Clear string stream. */
        ssLine.clear();
    }
}


void Percentile::generateCode()
{
    Query* countQuery = varToQuery[NUM_OF_VARIABLES];
    size_t& countViewID = countQuery->_incoming[0].first;
    size_t& countAggID = countQuery->_incoming[0].second;

    std::string numPercStr = to_string(numberOfPercentiles);

    std::string percentileComp =
        // offset(1)+"void computePercentiles()\n"+offset(1)+"{\n"+
        offset(2)+"size_t tupleCount = (size_t)V"+to_string(countViewID)+
        "[0].aggregates["+to_string(countAggID)+"], offset = 0, pIndex = 0;\n"+
        offset(2)+"size_t percentileCounts["+numPercStr+"];\n"+
        offset(2)+"for (size_t i = 1; i < "+numPercStr+"; i++)\n"+
        offset(3)+"percentileCounts[i-1] = ceil("+
        to_string(100.0/(numberOfPercentiles*100))+"*i*tupleCount);\n\n";

    std::string printPerc = "", printPercTensorFlow = "";
    
    // Create a query & Aggregate
    for (size_t var = 0; var < _db->numberOfAttributes(); ++var)
    {
        std::string& attName =  _db->getAttribute(var)->_name;

        if (_isFeature[var] && !_isCategoricalFeature[var])
        {
            Query* query = varToQuery[var];
            const size_t& viewID = query->_incoming[0].first;
            const size_t& aggID = query->_incoming[0].second;

            std::string viewStr = "V"+to_string(viewID);
            std::string typeStr = typeToStr(_db->getAttribute(var)->_type);
            
            percentileComp += offset(2)+"offset = 0; pIndex = 0;\n"+
                offset(2)+typeStr+" percentiles"+attName+"["+
                to_string(numberOfPercentiles-1)+"];\n"+
                offset(2)+"for ("+viewStr+"_tuple& tuple : "+viewStr+")\n"+
                offset(2)+"{\n"+
                offset(3)+"offset += tuple.aggregates["+to_string(aggID)+"];\n"+
                offset(3)+"while(offset >= percentileCounts[pIndex])\n"+offset(3)+"{\n"+
                offset(4)+"percentiles"+attName+"[pIndex] = tuple."+attName+";\n"+
                offset(4)+"++pIndex;\n"+offset(3)+"}\n"+
                offset(2)+"}\n\n";

            printPerc += offset(2)+"std::cout << \"{\"<<percentiles"+attName+"[0];\n"+
                offset(2)+"for (size_t p = 1; p < "+
                to_string(numberOfPercentiles-1)+"; ++p)\n"+
                offset(3)+"if (percentiles"+attName+"[p] != "+
                "percentiles"+attName+"[p-1])\n"+
                offset(4)+"std::cout << \",\" << percentiles"+attName+"[p];\n"+
                offset(2)+"std::cout << \"},// percentiles "+attName+"\"<< std::endl;\n";

            // 
            printPercTensorFlow += offset(2)+"std::cout << \"\'"+
                attName+"\': \" << \"[\"<<percentiles"+attName+"[0];\n"+
                offset(2)+"for (size_t p = 1; p < "+
                to_string(numberOfPercentiles-1)+"; ++p)\n"+
                offset(3)+"if (percentiles"+attName+"[p] != "+
                "percentiles"+attName+"[p-1])\n"+
                offset(4)+"std::cout << \",\" << percentiles"+attName+"[p];\n"+
                offset(2)+"std::cout << \"],  ## percentiles "+attName+
                "\"<< std::endl;\n";
        }
        else
        {
            printPerc += offset(2)+"std::cout << \"{},// percentiles "+attName+
                "\"<<std::endl;\n";
        }
    }
    
    std::string runFunction = offset(1)+"void runApplication()\n"+offset(1)+"{\n"+
        offset(2)+"int64_t startProcess = duration_cast<milliseconds>("+
        "system_clock::now().time_since_epoch()).count();\n"+percentileComp+
        "\n"+offset(2)+"int64_t endProcess = duration_cast<milliseconds>("+
        "system_clock::now().time_since_epoch()).count()-startProcess;\n"+
        offset(2)+"std::cout << \"Run Application: \"+"+
        "std::to_string(endProcess)+\"ms.\\n\";\n\n"+
        offset(2)+"std::ofstream ofs(\"times.txt\",std::ofstream::out | " +
        "std::ofstream::app);\n"+
        offset(2)+"ofs << \"\\t\" << endProcess << std::endl;\n"+
        offset(2)+"ofs.close();\n\n"+
        offset(2)+"std::cout << \"_thresholds = {\\n\";"+
        printPerc+
        // printPercTensorFlow+ // uncomment this for the percentiles used for tensorflow
        offset(2)+"std::cout << \"};\\n\";"+
        offset(1)+"}\n";
        
    std::ofstream ofs(
        multifaq::dir::OUTPUT_DIRECTORY+"ApplicationHandler.h", std::ofstream::out);
    ofs << "#ifndef INCLUDE_APPLICATIONHANDLER_HPP_\n"<<
        "#define INCLUDE_APPLICATIONHANDLER_HPP_\n\n"<<
        "#include \"DataHandler.h\"\n\n"<<
        "namespace lmfao\n{\n"<<
        offset(1)+"void runApplication();\n"<<
        "}\n\n#endif /* INCLUDE_APPLICATIONHANDLER_HPP_*/\n";    
    ofs.close();

    ofs.open(
        multifaq::dir::OUTPUT_DIRECTORY+"ApplicationHandler.cpp", std::ofstream::out);
    ofs << "#include \"ApplicationHandler.h\"\n"
        << "namespace lmfao\n{\n";
    ofs << runFunction << std::endl;
    ofs << "}\n";
    ofs.close();
    
}
