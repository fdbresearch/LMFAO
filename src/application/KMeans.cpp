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
#include <thread>

#include <Launcher.h>
#include <CppGenerator.h>
#include <KMeans.h>

#if defined(__GNUC__) && defined(NDEBUG) && !defined(__clang__)
#define USE_OMP 1
#else
#define USE_OMP 0
#endif

static const char COMMENT_CHAR = '#';
static const char NUMBER_SEPARATOR_CHAR = ',';
static const char ATTRIBUTE_NAME_CHAR = ':';

using namespace std;

using namespace multifaq::params;
using namespace multifaq::dir;
using namespace multifaq::config;

namespace phoenix = boost::phoenix;
using namespace boost::spirit;

using namespace std;

const bool INCLUDE_EVALUATION = true;

std::unordered_map<size_t, size_t> clusterToVariableMap;

KMeans::KMeans(shared_ptr<Launcher> launcher, const int k) :
     _launcher(launcher), _k(k)
{
    _compiler = launcher->getCompiler();
    _td = launcher->getTreeDecomposition();
    _db = launcher->getDatabase();
    
    // TODO: this should be modified in case you want to a different k for each dimension
    _dimensionK = _k;
    
    loadFeatures();

    numberOfOriginalVariables = _db->numberOfAttributes();

    for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
    {
        if (!_isFeature[var])
            continue;

        Attribute* att = _db->getAttribute(var);
        TDNode* node = _td->getTDNode(_queryRootIndex[var]);

        if (_isCategoricalFeature[var]) 
            _td->addAttribute("cluster_"+att->_name,Type::Integer,{true,0.0},node->_id);
        else
            _td->addAttribute("cluster_"+att->_name,Type::Double,{true,0.0},node->_id);
            
        size_t clusterID = _db->numberOfAttributes()-1;
        
        clusterVariables.set(clusterID);

        if (_isCategoricalFeature[var])
            _isCategoricalFeature.set(clusterID);

        clusterToVariableMap[clusterID] = var;   
    }
}

KMeans::~KMeans()
{    
}

void KMeans::run()
{
    modelToQueries();
    _compiler->compile();
}

Query* countQuery = nullptr;

void KMeans::modelToQueries()
{
    // Count aggregate
    Aggregate* agg = new Aggregate();
    agg->_sum.push_back(prod_bitset());

    // Create count query
    Query* query = new Query();
    query->_aggregates.push_back(agg);
    query->_root = _td->_root;
    query->_fVars = clusterVariables;
    
    _compiler->addQuery(query);        
    countQuery = query;

    for (size_t var = 0; var < NUM_OF_VARIABLES; ++var) 
    {
        if (!_isFeature[var])
            continue;

        // Categorical variable
        Aggregate* agg = new Aggregate();
        agg->_sum.push_back(prod_bitset());

        // Create a query & Aggregate
        Query* query = new Query();
        query->_aggregates.push_back(agg);
        query->_root = _td->getTDNode(_queryRootIndex[var]);
        query->_fVars.set(var);

        _compiler->addQuery(query);
        
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

        varToQuery[var] = query;
    }
}


void KMeans::loadFeatures()
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

        _isFeature.set(attributeID);
        _features.set(attributeID);

        if (categorical)
            _isCategoricalFeature.set(attributeID);
        
        _queryRootIndex[attributeID] = rootID;
        
        /* Clear string stream. */
        ssLine.clear();
    }
}

void KMeans::generateCode()
{
    // Get view for grid query -- equivalent to count query
    std::string gridViewName = "V"+std::to_string(countQuery->_incoming[0].first);

    std::string runFunction = offset(1)+"void runApplication()\n"+offset(1)+"{\n"+
        offset(2)+"int64_t startProcess = duration_cast<milliseconds>("+
        "system_clock::now().time_since_epoch()).count();\n\n"+
        offset(2)+"computeGrid();\n"+
        offset(2)+"kMeans();\n\n"+
        offset(2)+"int64_t endProcess = duration_cast<milliseconds>("+
        "system_clock::now().time_since_epoch()).count()-startProcess;\n"+
        offset(2)+"std::cout << \"Run Application: \"+"+
        "std::to_string(endProcess)+\"ms.\\n\";\n\n"+
        offset(2)+"std::ofstream ofs(\"times.txt\",std::ofstream::out | " +
        "std::ofstream::app);\n"+
        offset(2)+"ofs << \"\\t\" << endProcess << std::endl;\n"+
        offset(2)+"ofs.close();\n\n"
        "#ifdef DUMP_DISTRIBUTION\n"+
        offset(2)+"double sum = 0.0;\n"+
        offset(2)+"std::map<double,size_t> distribution;\n"+
        offset(2)+"std::map<double,size_t>::iterator it;\n\n"+
        offset(2)+"for (const "+gridViewName+"_tuple& tuple : "+gridViewName+")\n"+
        offset(2)+"{\n"+
        offset(3)+"it = distribution.find(tuple.aggregates[0]);\n"+
        offset(3)+"if(it != distribution.end())\n"+
        offset(4)+"it->second += 1;\n"+
        offset(3)+"else\n"+
        offset(4)+"distribution[tuple.aggregates[0]] += 1;\n"+
        offset(3)+"sum += tuple.aggregates[0];\n"+
        offset(2)+"}\n"+
        offset(2)+"ofs.open(\"output/CoresetWeights.csv\");\n"+
        offset(2)+"for (const auto& p : distribution)\n"+
        offset(3)+"ofs << p.first << \",\" << p.second << \"\\n\";\n"+
        offset(2)+"ofs.close();\n"+
        offset(2)+"std::cout << sum << std::endl;\n"+"#endif\n"+
        offset(1)+"}\n";
    
    std::ofstream ofs(multifaq::dir::OUTPUT_DIRECTORY+"/ApplicationHandler.h", std::ofstream::out);
    ofs << "#ifndef INCLUDE_APPLICATIONHANDLER_HPP_\n"
        << "#define INCLUDE_APPLICATIONHANDLER_HPP_\n\n"
        << "#include \"DataHandler.h\"\n\n"
        << "namespace lmfao\n{\n"
        << offset(1)+"void runApplication();\n"
        << "}\n\n#endif /* INCLUDE_APPLICATIONHANDLER_HPP_*/\n";    
    ofs.close();

    ofs.open(multifaq::dir::OUTPUT_DIRECTORY+"/ApplicationHandler.cpp", std::ofstream::out);
    
    ofs << "#include \"ApplicationHandler.h\"\n";

    if (INCLUDE_EVALUATION)
        ofs << "#include <boost/spirit/include/qi.hpp>\n"
            << "#include <boost/spirit/include/phoenix_operator.hpp>\n"
            << "namespace qi = boost::spirit::qi;\n"
            << "namespace phoenix = boost::phoenix;\n\n";

    // std::shared_ptr<CppGenerator> cppGenerator =
    //     std::dynamic_pointer_cast<CppGenerator> (_launcher->getCodeGenerator());

    for (size_t group = 0; group < _compiler->numberOfViewGroups(); ++group)
        ofs << "#include \"ComputeGroup"+std::to_string(group)+".h\"\n";

    ofs << "\nnamespace lmfao\n{\n\n"
        << offset(1)+"const size_t k = "+std::to_string(_k)+";\n";

    // if (INCLUDE_EVALUATION)
    //     ofs << genModelEvaluationFunction() << std::endl;

    ofs << genComputeGridPointsFunction() << std::endl;
    ofs << runFunction << std::endl;
    ofs << "}\n";
    ofs.close();
}

std::string KMeans::genKMeansFunction()
{
    // Get view for grid query -- equivalent to count query
    const size_t& gridViewID = countQuery->_incoming[0].first;
    std::string gridViewName = "V"+std::to_string(gridViewID);
    View* gridView = _compiler->getView(gridViewID);

    const std::string numCategVar = std::to_string(_isCategoricalFeature.count()/2); 

    std::string returnString = "\n";

    // KMeans Function
    returnString += offset(1)+"void kMeans()\n"+offset(1)+"{\n"+
        offset(2)+"int64_t startProcess = duration_cast<milliseconds>("+
        "system_clock::now().time_since_epoch()).count();\n\n"+
        offset(2)+"const size_t grid_size = "+gridViewName+".size();\n"+
        offset(2)+"size_t best_cluster, iteration = 0;\n\n"+
        offset(2)+"double dist, min_dist, distance_to_mean["+numCategVar+" * k * k];\n"+
        offset(2)+"std::vector<size_t> assignments(grid_size,0);\n"+
        offset(2)+"bool clustersChanged = true;\n\n"+
        offset(2)+"Cluster_mean means[k] = {}, cluster_sums[k] = {};\n\n";

    // if (_isCategoricalFeature.any())
    //     returnString += offset(2)+"onehotEncodeCategVars();\n\n";
    // returnString += offset(2)+"// Initialize the means\n"+
    //     offset(2)+"for (size_t cluster = 0; cluster < k; ++cluster)\n"+
    //     offset(3)+"means[cluster] = "+gridViewName+"[rand() % "+
    //     gridViewName+".size()];\n\n";

    // Code that initializes the clusters
    returnString += genClusterInitialization(gridViewName);


    // If we are gcc, we can use openmp to paralellize the computation do loop
    // for KMeans
#if USE_OMP 

    size_t nthreads = std::thread::hardware_concurrency();
    std::string nthreadString = std::to_string(nthreads);
    
    // Open do loop
    returnString += offset(2)+"Cluster_mean localClusterSums[k*"+
        nthreadString+"] = {};\n"+
        offset(2)+"bool localClustersChanged["+nthreadString+"];\n\n"+
        offset(2)+"do\n"+offset(2)+"{\n"+
        offset(3)+"clustersChanged = false;\n"+
        offset(3)+"// Reset the cluster sums to default values \n"+
        offset(3)+"for (size_t cluster = 0; cluster < k; ++cluster)\n"+
        offset(4)+"cluster_sums[cluster].reset();\n\n"+
        offset(3)+"computeMeanDistance(&distance_to_mean[0], &means[0]);\n\n"+
        offset(3)+"#pragma omp parallel num_threads("+nthreadString+") "+
        "private(min_dist,dist,best_cluster)\n"+
        offset(3)+"{\n"+
        offset(4)+"size_t thread_num = omp_get_thread_num();\n"+
        offset(4)+"for (size_t cluster = 0; cluster < k; ++cluster)\n"+
        offset(4)+"localClusterSums[k*thread_num+cluster].reset();\n"+
        offset(4)+"localClustersChanged[thread_num] = false;\n\n"+
        offset(4)+"// iterate over grid points and find best cluster for each tuple\n"+
        offset(4)+"#pragma omp for\n"+
        offset(4)+"for (size_t tup = 0; tup < grid_size; ++tup)\n"+offset(4)+"{\n"+
        offset(5)+"min_dist = std::numeric_limits<double>::max();\n"+
        offset(5)+"best_cluster = k;\n\n"+
        offset(5)+"// Find clostest cluster to this grid point\n"+
        offset(5)+"for (size_t cluster = 0; cluster < k; ++cluster)\n"+offset(5)+"{\n"+
        offset(6)+"distance(dist, "+gridViewName+"[tup], means[cluster],"+
        " cluster, &distance_to_mean[0]);\n"+
        offset(6)+"if (dist < min_dist)\n"+offset(6)+"{\n"+
        offset(7)+"min_dist = dist;\n"+offset(7)+"best_cluster = cluster;\n"+
        offset(6)+"}\n"+offset(5)+"}\n"+
        offset(5)+"localClustersChanged[thread_num] = "+
        "localClustersChanged[thread_num] || (assignments[tup] != best_cluster);\n"+
        offset(5)+"assignments[tup] = best_cluster;\n"+
        offset(5)+"localClusterSums[k*thread_num+best_cluster] += "+
        gridViewName+"[tup];\n"+offset(4)+"}\n"+offset(3)+"}\n\n";

    std::string clusterCount = "size_t cluster_count = ",
        clusterChange = "clustersChanged =";
    for (size_t thread = 0; thread < nthreads; ++thread)
    {
        clusterCount += "localClusterSums["+std::to_string(thread)+"*k+cluster].count+";
        clusterChange += " localClustersChanged["+std::to_string(thread)+"] ||";
    }
    clusterCount.pop_back();
    clusterChange.pop_back();
    clusterChange.pop_back();

    clusterCount += ";\n";
    clusterChange += ";\n";
    
    std::string contLocalUpdates = "", categLocalUpdates = "";

    // Compute the one-hot encoding for each categorical variable 
    for (size_t var=0; var < NUM_OF_VARIABLES; ++var)
    {
        if (gridView->_fVars[var])
        {
            Attribute* att = _td->getAttribute(var);
            const std::string& attName = att->_name;

            if (_isCategoricalFeature[var])
            {
                const size_t& origVar = clusterToVariableMap[var];                
                const size_t origViewID =
                    varToQuery[origVar]->_aggregates[0]->_incoming[0].first;
                const std::string origView = "V"+std::to_string(origViewID);
                
                categLocalUpdates +=
                    offset(4)+"for (size_t i = 0; i < "+origView+".size(); ++i)\n"+
                    offset(6)+"means[cluster]."+attName+"[i] = (";

                  for (size_t thread = 0; thread < nthreads; ++thread)
                {
                    categLocalUpdates += "localClusterSums["+std::to_string(thread)+
                        "*k+cluster]."+attName+"[i]+";
                }
                categLocalUpdates.pop_back();
                categLocalUpdates += ") / cluster_count;\n";
            }
            else
            {
                contLocalUpdates += offset(4)+"means[cluster]."+attName+" = (";
                for (size_t thread = 0; thread < nthreads; ++thread)
                {
                    contLocalUpdates += "localClusterSums["+std::to_string(thread)+
                        "*k+cluster]."+attName+"+";
                }
                contLocalUpdates.pop_back();
                contLocalUpdates += ") / cluster_count;\n";
            }
        }
    }
        
    returnString += offset(3)+"// Combine the clusters and update mean\n"+
        offset(3)+clusterChange+
        offset(3)+"for (size_t cluster = 0; cluster < k; ++cluster)\n"+offset(3)+"{\n"+
        offset(4)+clusterCount+contLocalUpdates+categLocalUpdates+offset(3)+"}\n";
    
#else // If we are on clang and cannot use openmp

    // Open do loop
    returnString +=
        offset(2)+"do\n"+offset(2)+"{\n"+
        offset(3)+"clustersChanged = false;\n"+
        offset(3)+"// Reset the cluster sums to default values \n"+
        offset(3)+"for (size_t cluster = 0; cluster < k; ++cluster)\n"+
        offset(4)+"cluster_sums[cluster].reset();\n\n"+
        offset(3)+"computeMeanDistance(&distance_to_mean[0], &means[0]);\n\n"+
        offset(3)+"// iterate over grid points and find best cluster for each tuple\n"+
        offset(3)+"for (size_t tup = 0; tup < grid_size; ++tup)\n"+offset(3)+"{\n"+
        offset(4)+"min_dist = std::numeric_limits<double>::max();\n"+
        offset(4)+"best_cluster = k;\n\n"+
        offset(4)+"// Find clostest cluster to this grid point\n"+
        offset(4)+"for (size_t cluster = 0; cluster < k; ++cluster)\n"+offset(4)+"{\n"+
        offset(5)+"distance(dist, "+gridViewName+"[tup], means[cluster],"+
        " cluster, &distance_to_mean[0]);\n"+
        offset(5)+"if (dist < min_dist)\n"+offset(5)+"{\n"+
        offset(6)+"min_dist = dist;\n"+offset(6)+"best_cluster = cluster;\n"+
        offset(5)+"}\n"+offset(4)+"}\n"+
        offset(4)+"clustersChanged = clustersChanged || "+
        "(assignments[tup] != best_cluster);\n"+
        offset(4)+"assignments[tup] = best_cluster;\n"+
        offset(4)+"cluster_sums[best_cluster] += "+gridViewName+"[tup];\n"+
        offset(3)+"}\n\n";

    returnString += offset(3)+"// Update the means\n"+
        offset(3)+"for (size_t cluster = 0; cluster < k; ++cluster)\n"+offset(3)+"{\n"+
        offset(4)+"if (cluster_sums[cluster].count == 0)\n"+
        offset(5)+"continue;\n";

    std::string categUpdates = "";
    // Compute the one-hot encoding for each categorical variable 
    for (size_t var=0; var < NUM_OF_VARIABLES; ++var)
    {
        if (gridView->_fVars[var])
        {
            Attribute* att = _db->getAttribute(var);
            const std::string& attName = att->_name;

            if (_isCategoricalFeature[var])
            {
                const size_t& origVar = clusterToVariableMap[var];                
                const size_t origViewID = varToQuery[origVar]->_incoming[0].first;
                const std::string origView = "V"+std::to_string(origViewID);
                
                categUpdates +=
                    offset(4)+"for (size_t i = 0; i < "+origView+".size(); ++i)\n"+
                    offset(6)+"means[cluster]."+attName+"[i] = "+
                    "cluster_sums[cluster]."+attName+"[i] / "+
                    "cluster_sums[cluster].count;\n";
            }
            else
            {
                returnString += offset(4)+"means[cluster]."+attName+" = "+
                    "cluster_sums[cluster]."+attName+" / cluster_sums[cluster].count;\n";
            }
        }
    }

    returnString += categUpdates+offset(3)+"}\n";
#endif

    // returnString += 
    // "//TODO:TODO:check dispersion of the clusters and if clusters have converged\n";

    returnString += offset(3)+"++iteration;\n"+
        offset(2)+"} while(clustersChanged && iteration < 1000);\n\n"+
        offset(2)+"int64_t endProcess = duration_cast<milliseconds>("+
        "system_clock::now().time_since_epoch()).count()-startProcess;\n"+
        offset(2)+"std::ofstream ofs(\"times.txt\",std::ofstream::out | "+
        "std::ofstream::app);\n"+
        offset(2)+"ofs << \"\\t\" << endProcess << \"\\t\" << iteration;\n"+
        offset(2)+"ofs.close();\n\n"+
        offset(2)+"std::cout << \"Run kMeans: \"+"+
        "std::to_string(endProcess)+\"ms.\\n\";\n"+
        offset(2)+"std::cout << \"Number of Iterations: \"+"+
        "std::to_string(iteration)+\"\\n\";\n\n";
    
    if (INCLUDE_EVALUATION)
        returnString += offset(2)+"evaluateModel(means);\n";
    return returnString+offset(1)+"}\n\n";
}


std::string KMeans::genModelEvaluationFunction()
{
    std::string attributeString = "",
        attrConstruct = offset(4)+"qi::phrase_parse(tuple.begin(),tuple.end(),";
    
    for (size_t var = 0; var < numberOfOriginalVariables; ++var)
    {
        Attribute* att = _db->getAttribute(var);
        attrConstruct += "\n"+offset(5)+"qi::"+typeToStr(att->_type)+
            "_[phoenix::ref("+att->_name+") = qi::_1]>>";
        attributeString += offset(2)+typeToStr(att->_type) + " "+
            att->_name + ";\n";
    }

    // attrConstruct.pop_back();
    attrConstruct.pop_back();
    attrConstruct.pop_back();
    attrConstruct += ",\n"+offset(5)+"\'|\');\n";

    std::string testTuple = offset(1)+"struct Test_tuple\n"+offset(1)+"{\n"+
        attributeString+offset(2)+"Test_tuple(const std::string& tuple)\n"+
        offset(2)+"{\n"+attrConstruct+offset(2)+"}\n"+offset(1)+"};\n\n";

    std::string loadFunction = offset(1)+"void loadTestDataset("+
        "std::vector<Test_tuple>& TestDataset)\n"+offset(1)+"{\n"+
        offset(2)+"std::ifstream input;\n"+offset(2)+"std::string line;\n"+
        offset(2)+"input.open(PATH_TO_DATA + \"/joinresult.tbl\");\n"+
        offset(2)+"if (!input)\n"+offset(2)+"{\n"+
        offset(3)+"std::cerr << \"TestDataset.tbl does is not exist.\\n\";\n"+
        offset(3)+"exit(1);\n"+offset(2)+"}\n"+
        offset(2)+"while(getline(input, line))\n"+
        offset(3)+"TestDataset.push_back(Test_tuple(line));\n"+
        offset(2)+"input.close();\n"+offset(1)+"}\n\n";

    size_t categVarIdx = 0;
    std::string distance = "", valueToMeanMap = "";
    for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
    {
        if (!_features[var])
            continue;

        Attribute* att = _db->getAttribute(var);
        std::string& varName = att->_name;
        
        if (_isCategoricalFeature[var])
        {
            const std::string& viewName = "V"+std::to_string(
                varToQuery[var]->_incoming[0].first);
            
            distance += "-2*means[cluster].cluster_"+varName+"["+
                varName+"_clusterIndex[tuple."+varName+"]]";

            valueToMeanMap += "\n"+offset(2)+"std::unordered_map<"+
                typeToStr(att->_type)+", size_t> "+
                varName+"_clusterIndex("+viewName+".size());\n"+
                offset(2)+"for (size_t l=0; l<"+viewName+".size(); ++l)\n"+
                offset(3)+varName+"_clusterIndex["+viewName+"[l]."+varName+"] = l;\n";

            ++categVarIdx;
        }
        else
            distance += "+((tuple."+att->_name+"-means[cluster].cluster_"+att->_name+")"+
                "*(tuple."+att->_name+"-means[cluster].cluster_"+att->_name+"))";
    }
    
    std::string numCategVar = std::to_string(_isCategoricalFeature.count()/2); 

    std::string precompMeanSum = "";
    for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
    {
        if (!clusterVariables[var] || !_isCategoricalFeature[var])
            continue;

        size_t origVar = clusterToVariableMap[var];
        const std::string& origView = "V"+std::to_string(
            varToQuery[origVar]->_incoming[0].first);

        const std::string& attName = _db->getAttribute(var)->_name;

        // precompMeanSum += offset(2)+"for (size_t cluster = 0; cluster < k; ++cluster)\n"+
        //     Offset(2)+"{\N"+offset(3)+"for (size_t i = 0; i < "+origView+".size(); ++i)\n"+
        //     offset(4)+"sum_mean_squared[idx] += std::pow(means[cluster]."
        //     +attName+"[i],2);\n"+offset(3)+"++idx;\n"+offset(2)+"}\n\n";
        
        precompMeanSum += offset(3)+"for (size_t i = 0; i < "+origView+".size(); ++i)\n"+
            offset(4)+"sum_mean_squared[cluster] += std::pow(mean_tuple."
            +attName+"[i],2);\n\n";
    }
    
    std::string evalFunction = offset(1)+"void evaluateModel(Cluster_mean* means)\n"+
        offset(1)+"{\n"+
        offset(2)+"std::vector<Test_tuple> TestDataset;\n"+
        offset(2)+"loadTestDataset(TestDataset);\n"+
        offset(2)+"size_t idx = 0;\n"+valueToMeanMap+"\n"+
        offset(2)+"double sum_mean_squared[k] = {};\n"+
        offset(2)+"for (size_t cluster = 0; cluster < k; ++cluster)\n"+
        offset(2)+"{\n"+offset(3)+"Cluster_mean& mean_tuple = means[cluster];\n"+
        precompMeanSum+offset(2)+"}\n"+
        offset(2)+"double distance, error = 0.0, "+
        "min_distance;\n"+
        offset(2)+"for (Test_tuple& tuple : TestDataset)\n"+offset(2)+"{\n"+
        offset(3)+"min_distance = std::numeric_limits<double>::max();\n"+
        offset(3)+"for (size_t cluster = 0; cluster < k; ++cluster)\n"+offset(3)+"{\n"+
        offset(4)+"distance = "+numCategVar+"+sum_mean_squared[cluster]"+distance+";\n"+
        offset(4)+"min_distance = std::min(distance, min_distance);\n"+
        offset(3)+"}\n"+offset(3)+"error += min_distance;\n"+
        offset(2)+"}\n"+offset(2)+
        "std::cout << \"Within Cluster l2-distance: \" << error << std::endl;\n"+
        offset(1)+"}\n";

    return testTuple + loadFunction + evalFunction;
}


std::string KMeans::genClusterTuple()
{
    // Get view for grid query -- equivalent to count query
    const size_t& gridViewID = countQuery->_incoming[0].first;
    std::string gridViewName = "V"+std::to_string(gridViewID);
    View* gridView = _compiler->getView(gridViewID);

    std::string varList = "", initList = "", resetList = "", deleteList = "",
        updateList = "", setList = "", rsumList = "", distList = "", categDistList = "",
        categResetList = "";

    size_t categVarIndex = 0;

    std::string numCategVar = std::to_string(_isCategoricalFeature.count()/2); 
    
    for (size_t var=0; var < NUM_OF_VARIABLES; ++var)
    {
        if (gridView->_fVars[var])
        {
            Attribute* att = _db->getAttribute(var);
            const std::string& attName = att->_name;
            
            if (_isCategoricalFeature[var])
            {
                const size_t& origVar = clusterToVariableMap[var];
                const size_t origViewID =
                    varToQuery[origVar]->_incoming[0].first;
                const std::string origView = "V"+std::to_string(origViewID);
                const std::string& origAtt = _db->getAttribute(origVar)->_name;
                
                varList += offset(2)+"double* "+attName+" = nullptr;\n";
                initList += offset(3)+attName+" = new double["+origView+".size()]();\n";
                deleteList += offset(3)+"delete[] "+attName+";\n";
                categResetList += offset(3)+"memset("+attName+
                    ", 0, "+origView+".size()*sizeof(double));\n";

                updateList += offset(3)+"if (tuple."+attName+" != "+
                    std::to_string(_dimensionK-1)+")\n"+
                    offset(4)+attName+"[tuple."+attName+"] += tuple.aggregates[0];\n"+
                    offset(3)+"else\n"+
                    offset(4)+"for (size_t t = "+std::to_string(_dimensionK-1)+"; "+
                    "t < "+origView+".size(); ++t)\n"+
                    offset(5)+attName+"[t] += "+origView+"[t].aggregates[0] * "+
                    "tuple.aggregates[0];\n";
                
                setList +=
                    offset(3)+"for (size_t t = 0; t < "+origView+".size(); ++t)\n"+
                    offset(4)+"if (tuple."+attName+" == "+origView+"[t]."+origAtt+")\n"+
                    offset(4)+"{\n"+offset(5)+attName+"[t] = 1;\n"+
                    offset(5)+"break;\n"+offset(4)+"}\n";

                categDistList += offset(2)+"dist += distance_to_mean[k*(k*"+
                    std::to_string(categVarIndex)+"+cluster) + tuple."+attName+"];\n";
                // distList += offset(2)+"dist += distance_to_mean[("+
                // std::to_string(categVarIndex)+" * k) + cluster] + distance_to_mean[("+
                //     numCategVar+"+"+std::to_string(categVarIndex)+"*k)*k + tuple."+
                //     attName+"];\n";
                
                rsumList += "\n"+offset(2)+
                    "for(size_t cluster = 0; cluster < k; ++cluster)\n"+
                    offset(2)+"{\n"+
                    offset(3)+"const Cluster_mean& mean_tuple = means[cluster];\n"+
                    offset(3)+"sum_mean_squared = 0.0;\n"+
                    offset(3)+"memset(&difference[0], 0, k*sizeof(double));\n"+
                    offset(3)+"for (size_t i = 0; i < "+origView+".size(); ++i)\n"+
                    offset(3)+"{\n"+
                    offset(4)+"sum_mean_squared += mean_tuple."+attName+"[i] * "+
                    "mean_tuple."+attName+"[i];\n"+
                    offset(4)+"difference[std::min(i, k-1)] += ("+
                    origView+"[i].aggregates[0]*"+origView+"[i].aggregates[0]) - 2*"+
                    origView+"[i].aggregates[0]*mean_tuple."+attName+"[i];\n"+
                    offset(3)+"}\n"+
                    offset(3)+"for(size_t cluster = 0; cluster < k; ++cluster)\n"+
                    offset(3)+"{\n"+
                    offset(4)+"distance_to_mean[sum_idx] = sum_mean_squared + "+
                    "difference[cluster];\n"+offset(4)+"++sum_idx;\n"+
                    offset(3)+"}\n"+offset(2)+"}\n";
                // rsumList += "\n"+offset(2)+
                //     "for(size_t cluster = 0; cluster < k; ++cluster)\n"+
                //     offset(2)+"{\n"+
                //     offset(3)+"const Cluster_mean& mean_tuple = means[cluster];\n"+
                //     offset(3)+"for (size_t i = 0; i < "+origView+".size(); ++i)\n"+
                //     offset(3)+"{\n"+
                //     offset(4)+"means_squared = mean_tuple."+attName+"[i] * "+
                //     "mean_tuple."+attName+"[i];\n"+
                //     offset(4)+"distance_to_mean[sum_idx] += means_squared;\n"+
                //     offset(4)+"diffs[diff_idx + std::min(i, k-1)] += "+
                //     "std::pow("+origView+"[i].aggregates[0] - mean_tuple."+
                //     attName+"[i], 2)"+" - means_squared;\n"+offset(3)+"}\n"+
                //     offset(3)+"++sum_idx;\n"+offset(3)+"diff_idx += k;\n"+
                //     offset(2)+"}\n";

                ++categVarIndex;
                // TODO: TODO: TODO: TODO: find correct view for each categ variable
            }
            else
            {
                varList +=  offset(2)+"double "+attName+" = 0.0;\n";
                // initList += attName+"(tuple."+attName+"),";
                resetList += offset(3)+ attName+" = 0.0;\n";
                updateList +=  offset(3)+attName+" += tuple."+attName+
                    " * tuple.aggregates[0];\n";
                setList +=  offset(3)+attName+" = tuple."+attName+";\n";

                distList += offset(2)+"dist += "+
                    "(tuple."+attName+" - mean_tuple."+attName+") * "+
                    "(tuple."+attName+" - mean_tuple."+attName+");\n";
            }
        }
    }
    
    return offset(1)+"struct Cluster_mean\n"+offset(1)+"{\n"+varList+
        offset(2)+"size_t count = 0;\n\n"+
        offset(2)+"Cluster_mean()\n"+offset(2)+"{\n"+initList+offset(2)+"}\n\n"+
        offset(2)+"~Cluster_mean()\n"+offset(2)+"{\n"+deleteList+offset(2)+"}\n\n"+
        offset(2)+"void reset()\n"+offset(2)+"{\n"+resetList+categResetList+
        offset(3)+"count = 0;\n"+offset(2)+"}\n\n"+
        offset(2)+"Cluster_mean& operator+=(const "+gridViewName+"_tuple& tuple)\n"+
        offset(2)+"{\n"+updateList+offset(3)+"count += tuple.aggregates[0];\n"+
        offset(3)+"return *this;\n"+offset(2)+"}\n"+
        offset(2)+"Cluster_mean& operator=(const "+gridViewName+"_tuple& tuple)\n"+
        offset(2)+"{\n"+categResetList+setList+offset(3)+"count = 0;\n"+
        offset(3)+"return *this;\n"+offset(2)+"}\n"+offset(1)+"};\n\n"+
        offset(1)+"void distance(double& dist, const "+gridViewName+"_tuple& tuple, "+
        "const Cluster_mean& mean_tuple, const size_t& cluster, "+
        "const double* distance_to_mean)\n"+offset(1)+"{\n"+
        offset(2)+"dist = 0.0;\n"+distList+categDistList+
        offset(1)+"}\n\n"+
        offset(1)+"void computeMeanDistance(double* distance_to_mean, "+
        "const Cluster_mean *means)\n"+offset(1)+"{\n"+
        offset(2)+"size_t sum_idx = 0;\n"+
        offset(2)+"double sum_mean_squared = 0.0, difference[k];\n"+
        rsumList+offset(1)+"}\n";
}


std::string KMeans::genComputeGridPointsFunction()
{
#if defined(__GNUC__) && defined(NDEBUG) && !defined(__clang__)
    std::string sortAlgo = "__gnu_parallel::sort(";
#else
    std::string sortAlgo = "std::sort(";
#endif

    std::string kString = std::to_string(_dimensionK);
    
    // Get view for count query
    const std::pair<size_t,size_t>& countViewAggPair = countQuery->_incoming[0];
    const View* countView = _compiler->getView(countViewAggPair.first);
    std::string countViewName = "V"+std::to_string(countViewAggPair.first);

    std::string returnString = genClusterTuple();

    if (INCLUDE_EVALUATION)
         returnString += genModelEvaluationFunction();
    returnString += genKMeansFunction();
    
    if (!continuousQueries.empty())
    {
        // Define the auxhiliary arrays
        returnString += offset(1)+"double *psum_linear = nullptr,*psum_quad = nullptr,"+
            "**D_matrix = nullptr;\n"+
            offset(1)+"size_t **T_matrix = nullptr, *rsum_count = nullptr;\n\n";

        // Define function for clustering continuous variables - with aux fields
        returnString += offset(1)+"void clusterContinuousVariable(size_t* offsets, "+
            "double* means, size_t n)\n"+offset(1)+"{\n"+
            offset(2)+"double lin = 0.0, quad = 0.0;\n"+
            offset(2)+"size_t count = 0;\n";

        // Compute the first row in D and T matrices. 
        returnString += offset(2)+"// for the first row - i.e. only one cluster \n"+
            offset(2)+"for (size_t i = 0; i < n; ++i)\n"+offset(2)+"{\n"+
            offset(3)+"lin = psum_linear[i];\n"+
            offset(3)+"quad = psum_quad[i];\n"+
            offset(3)+"D_matrix[0][i] = quad - (lin * lin) / rsum_count[i];\n"+
            offset(3)+"T_matrix[0][i] = 0;\n"+offset(2)+"}\n\n";

        returnString += offset(2)+"// for each clusters\n"+
            offset(2)+"for (size_t j = 1; j < "+kString+"; ++j)\n"+
            offset(2)+"{\n"+
            offset(3)+"// for the number of data points \n"+
            offset(3)+"for (size_t i = j; i < n; ++i)\n"+offset(3)+"{\n"+
            offset(4)+"lin = psum_linear[i] - psum_linear[j-1];\n"+
            offset(4)+"quad = psum_quad[i] - psum_quad[j-1];\n"+
            offset(4)+"count = rsum_count[i] - rsum_count[j-1];\n"+
            offset(4)+"double min_cost = D_matrix[j-1][i-1] + "+
            "quad - (lin * lin) / count;\n"+
            offset(4)+"D_matrix[j][i] = min_cost;\n"+
            offset(4)+"T_matrix[j][i] = j;\n"+
            offset(4)+"for (size_t i2 = j+1; i2 <= i; ++i2)\n"+offset(4)+"{\n"+
            offset(5)+"lin = psum_linear[i] - psum_linear[i2-1];\n"+
            offset(5)+"quad = psum_quad[i] - psum_quad[i2-1];\n"+
            offset(5)+"count = rsum_count[i] - rsum_count[i2-1];\n"+
            offset(5)+"double cost = D_matrix[j-1][i2-1] + "+
            "quad - (lin * lin) / count;\n"+
            offset(5)+"if (cost < min_cost)\n"+offset(5)+"{\n"+
            offset(6)+"D_matrix[j][i] = cost;\n"+
            offset(6)+"T_matrix[j][i] = i2;\n"+
            offset(6)+"min_cost = cost;\n"+
            offset(5)+"}\n"+offset(4)+"}\n"+
            offset(3)+"}\n"+offset(2)+"}\n";

        returnString += offset(2)+"size_t last = n;\n"+
            offset(2)+"for (size_t i = "+std::to_string(_dimensionK-1)+"; i > 0; --i)\n"+
            offset(2)+"{\n"+
            offset(3)+"offsets[i] = T_matrix[i][last-1];\n\n"+
            offset(3)+"lin = psum_linear[last-1] - "+
            "(offsets[i] > 0 ? psum_linear[offsets[i]-1] : 0);\n"+
            offset(3)+"count = rsum_count[last-1] - "+
            "(offsets[i] > 0 ? rsum_count[offsets[i]-1] : 0);\n"+
            offset(3)+"means[i] = lin / count;\n\n"+
            offset(3)+"last = offsets[i];\n"+
            offset(2)+"}\n"+
            offset(2)+"means[0] = psum_linear[last-1] / rsum_count[last-1];\n\n";

        returnString += offset(1)+"}\n\n";
    }
    
    returnString += offset(1)+"void computeGrid()\n"+offset(1)+"{\n"+
        offset(2)+"int64_t endProcess, endClustering;\n"+
        offset(2)+"int64_t startProcess = duration_cast<milliseconds>("+
        "system_clock::now().time_since_epoch()).count();\n\n"+
        offset(2)+"double size_of_query_result = "+countViewName+"[0].aggregates[0];\n";
    
    for (auto& varQueryPair : categoricalQueries)
    {        
        // Get view for this query
        std::pair<size_t,size_t>& viewAggPair =
            varQueryPair.second->_incoming[0];

        std::string viewName = "V"+std::to_string(viewAggPair.first);
        Attribute* att = _db->getAttribute(varQueryPair.first);
        std::string& varName = att->_name;

        returnString += "\n"+offset(2)+"// For categorical variable "+varName+"\n";
        
        // Sort the view on the count
        returnString += offset(2)+sortAlgo+viewName+".begin(),"+
            viewName+".end(),[ ](const "+viewName+"_tuple& lhs, const "+viewName+
            "_tuple& rhs)\n"+offset(2)+"{\n"+
            offset(3)+"return lhs.aggregates[0] < rhs.aggregates[0];\n"+
            offset(2)+"});\n";

        // returnString += offset(2)+"std::unordered_map<"+
        //     typeToStr(att->_type)+", size_t> "+
        //     varName+"_clusters("+viewName+".size());\n"+
        //     // offset(2)+varName+"_clusters.reserve("+viewName+".size());\n"+
        //     offset(2)+"for (size_t l=0; l<k-1; ++l)\n"+
        //     offset(3)+varName+"_clusters["+viewName+"[l]."+varName+"] = l;\n"+
        //     offset(2)+"for (size_t l=k-1; l<"+viewName+".size(); ++l)\n"+
        //     offset(3)+varName+"_clusters["+viewName+"[l]."+varName+"] = k-1;\n";
    }
    
    std::string maxString = "", offsetArrays = "", meanArrays = "";
    for (auto& varQueryPair : continuousQueries)
    {
        // Get view for this query
        const std::pair<size_t,size_t>& viewAggPair =
            varQueryPair.second->_incoming[0];
        
        maxString +=  "V"+std::to_string(viewAggPair.first)+".size(),";
        offsetArrays +=
            _db->getAttribute(varQueryPair.first)->_name+"_offsets["+
            kString+"] = {},";
        meanArrays += _db->getAttribute(varQueryPair.first)->_name+"_means["+
            kString+"] = {},";    }

    if (!continuousQueries.empty())
    {
        maxString.pop_back();
        offsetArrays.pop_back();
        meanArrays.pop_back();

        returnString += "\n"+offset(2)+"size_t "+offsetArrays+";\n"+
            offset(2)+"double "+meanArrays+";\n"+
            offset(2)+"size_t max_size = std::max({"+maxString+"});\n\n";

        returnString += offset(2)+"psum_linear = new double[max_size]();\n"+
            offset(2)+"psum_quad = new double[max_size]();\n"+
            offset(2)+"rsum_count = new size_t[max_size]();\n\n"+
            offset(2)+"D_matrix = new double*["+kString+"];\n"+
            offset(2)+"T_matrix = new size_t*["+kString+"];\n"+
            offset(2)+"for (size_t i = 0; i < "+kString+"; ++i)\n"+
            offset(2)+"{\n"+
            offset(3)+"D_matrix[i] = new double[max_size+1]();\n"+
            offset(3)+"T_matrix[i] = new size_t[max_size+1]();\n"+
            offset(2)+"}\n";        
    }
    
    
    for (auto& varQueryPair : continuousQueries)
    {
        // Get view for this query
        const std::pair<size_t,size_t>& viewAggPair =
            varQueryPair.second->_incoming[0];

        std::string viewName = "V"+std::to_string(viewAggPair.first);
        Attribute* att = _db->getAttribute(varQueryPair.first);
        std::string& varName = att->_name;

        returnString += "\n"+offset(2)+"if("+viewName+".size() <= "+
            kString+")\n"+
            offset(2)+"{\n"+offset(3)+"std::cout << \""+varName+
            " has less values than "+kString+";\" << std::endl;\n"+
            offset(3)+"for (size_t t = 0; t < "+viewName+".size(); ++t)\n"+
            offset(3)+"{\n"+
            offset(4)+varName+"_offsets[t] = t;\n"+
            offset(4)+varName+"_means[t] = "+viewName+"[t]."+varName+";\n"+
            offset(3)+"}\n"+
            offset(3)+"for (size_t t = "+viewName+".size(); t < "+kString+"; ++t)\n"+
            offset(3)+"{\n"+
            offset(4)+varName+"_offsets[t] = "+viewName+".size();\n"+
            offset(4)+varName+"_means[t] = std::numeric_limits<double>::infinity();\n"+
            offset(3)+"}\n"+
            offset(3)+"if ("+viewName+".size() != "+kString+")\n"+
            offset(4)+viewName+".push_back(std::numeric_limits<"+
            typeToStr(att->_type)+">::max());\n"+
            offset(2)+"}\n"+offset(2)+"else\n"+offset(2)+"{\n"+
            offset(3)+"// Compute prefix sums for variable "+varName+"\n"+
            offset(3)+"psum_linear[0] = "+viewName+"[0]."+varName+
            " * "+viewName+"[0].aggregates[0];\n"+
            offset(3)+"psum_quad[0] = "+viewName+"[0]."+varName+" * "+
            viewName+"[0]."+varName+" * "+viewName+"[0].aggregates[0];\n"+
            offset(3)+"rsum_count[0] = "+viewName+"[0].aggregates[0];\n"+
            offset(3)+"for(size_t t = 1; t < "+viewName+".size(); ++t)\n"+
            offset(3)+"{\n"+
            offset(4)+"const "+viewName+"_tuple& tup = "+viewName+"[t];\n"+
            offset(4)+"psum_linear[t] = psum_linear[t-1] + tup."+
            varName+" * tup.aggregates[0];\n"+
            offset(4)+"psum_quad[t] = psum_quad[t-1] + (tup."+varName+
            " * tup."+varName+" * tup.aggregates[0]);\n"+
            offset(4)+"rsum_count[t] = rsum_count[t-1] + tup.aggregates[0];\n"+
            offset(3)+"}\n\n"+
            offset(3)+"// Dynamic Programming algo for variable "+varName+"\n"+
            offset(3)+"clusterContinuousVariable(&"+varName+"_offsets[0],&"+
            varName+"_means[0],"+viewName+".size());\n"+
            offset(2)+"}\n";
    }

    // replace the cluster variables by their cluster ids
    returnString += "\n"+offset(2)+
        "// Update the cluster variables for each tuple\n";

    for (size_t rel = 0; rel < _db->numberOfRelations(); ++rel)
    {
        Relation* relation = _db->getRelation(rel);
        
        returnString += offset(2)+"for ("+relation->_name+"_tuple& tup : "+
            relation->_name+")\n"+offset(2)+"{\n";
        
        for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
        {
            if (!clusterVariables[var])
                continue;

            size_t origVar = clusterToVariableMap[var];

            if (_queryRootIndex[origVar] == rel)
            {
                Attribute* clusterAtt = _db->getAttribute(var);
                Attribute* origAtt = _db->getAttribute(origVar);

                const size_t origViewID =
                    varToQuery[origVar]->_incoming[0].first;
                
                if (_isCategoricalFeature[var])
                {
                    std::string updateFunction = "(tup."+origAtt->_name+" == "+
                        "V"+std::to_string(origViewID)+"["+std::to_string(_dimensionK-2)+"]."+
                        origAtt->_name+" ? "+std::to_string(_dimensionK-2)+" : "+
                        std::to_string(_dimensionK-1)+")"; 
                    
                    for (int c = _dimensionK-3; c >= 0; --c)
                    {
                       updateFunction = "(tup."+origAtt->_name+" == "+
                           "V"+std::to_string(origViewID)+"["+std::to_string(c)+"]."+
                           origAtt->_name+" ? "+std::to_string(c)+" : "+
                           updateFunction+")";
                    }

                    returnString += offset(3)+"tup."+clusterAtt->_name+" = "+
                        updateFunction+";\n";
                }
                else
                {
                    const std::string& attName = origAtt->_name;
                    
                    // compare value to each threshold and set k accordingly
                    std::string updateFunction = "(tup."+attName+" < "+
                        "V"+std::to_string(origViewID)+"["+
                        attName+"_offsets["+std::to_string(_dimensionK-1)+"]]."+
                        attName+" ? "+
                        attName+"_means["+std::to_string(_dimensionK-2)+"] : "+
                        attName+"_means["+std::to_string(_dimensionK-1)+"])";

                    for (int c = _dimensionK-3; c >= 0; --c)
                    {
                       updateFunction = "(tup."+origAtt->_name+" < "+
                           "V"+std::to_string(origViewID)+"["+
                           attName+"_offsets["+std::to_string(c+1)+"]]."+attName+" ? "+
                           attName+"_means["+std::to_string(c)+"] : "+
                           updateFunction+")";
                    }
                     
                    returnString += offset(3)+"tup."+clusterAtt->_name+" = "+
                        updateFunction+" ;\n";
                }
            }
        }

        returnString += offset(2)+"}\n";
    }


    returnString += "\n"+offset(2)+"endClustering = duration_cast<milliseconds>("+
        "system_clock::now().time_since_epoch()).count()-startProcess;\n\n";

    returnString += offset(2)+"for (size_t i = 0; i < "+kString+"; ++i)\n"+
        offset(2)+"{\n"+offset(3)+"delete[] D_matrix[i];\n"+
        offset(3)+"delete[] T_matrix[i];\n"+offset(2)+"}\n"+
        offset(2)+"delete[] D_matrix;\n"+
        offset(2)+"delete[] T_matrix;\n"+
        offset(2)+"delete[] psum_linear;\n"+
        offset(2)+"delete[] psum_quad;\n"+
        offset(2)+"delete[] rsum_count;\n\n";
    
    bool containsView = true;

    // std::shared_ptr<CppGenerator> cppGenerator =
    //     std::dynamic_pointer_cast<CppGenerator> (_launcher->getCodeGenerator());
            
    boost::dynamic_bitset<> recomputedGroups(_compiler->numberOfViewGroups());   
    boost::dynamic_bitset<> intermediateViews(_compiler->numberOfViews());
    boost::dynamic_bitset<> recomputedViews(_compiler->numberOfViews());
    
    for (size_t group = 0; group < _compiler->numberOfViewGroups(); ++group)
    {
        containsView = false;
        for (const size_t& view : _compiler->getViewGroup(group)._views)
        {
            if (view <= countViewAggPair.first)
            {
                // add view as deleted view
                containsView = true;
                recomputedViews.set(view);
            }

            View* v = _compiler->getView(view);
            if (v->_origin != v->_destination)
            {    
                intermediateViews.set(view);
            }
            
        }

        if (containsView)
        {
            // add this group to be cleared and recomputed
            recomputedGroups.set(group);
        }
    }

    for (size_t view = 0; view < _compiler->numberOfViews(); ++view)
    {
        if (intermediateViews[view] && !recomputedViews[view])
            returnString += offset(2)+"std::vector<V"+std::to_string(view)+"_tuple>()"+
                ".swap(V"+std::to_string(view)+");\n";
    }
    
    // for (size_t rel = 0; rel < _td->numberOfRelations(); ++rel)
    // {
    //     TDNode* node = _td->getRelation(rel);
    //     returnString += offset(2)+"sort"+node->_name+"();\n";
    // }#

    returnString += offset(2)+"startProcess = duration_cast<milliseconds>("+
        "system_clock::now().time_since_epoch()).count();\n\n";
    
        
    // clear views and re-compute the groups that are need to compute grid 
    for (size_t group = 0; group < _compiler->numberOfViewGroups(); ++group)
    {
        if (recomputedGroups[group])
        {
            for  (const size_t& view : _compiler->getViewGroup(group)._views)
                returnString += offset(2)+"V"+std::to_string(view)+".clear();\n";

            returnString += offset(2)+"computeGroup"+std::to_string(group)+"();\n";
        }
    }

    // We resort categorical variables and set their aggregates to the one-hot encoding
    if(_isCategoricalFeature.any())
    {
        returnString += offset(2)+"double last_cluster_size;\n";
    
        // Compute the one-hot encoding for each categorical variable 
        for (size_t var=0; var < NUM_OF_VARIABLES; ++var)
        {
            if (countView->_fVars[var] && _isCategoricalFeature[var])
            {
                Attribute* att = _db->getAttribute(var);
                const std::string& attName = att->_name;

                const size_t& origVar = clusterToVariableMap[var];
                const size_t origViewID = varToQuery[origVar]->_incoming[0].first;
                const std::string origView = "V"+std::to_string(origViewID);
                const std::string& origAttName = _db->getAttribute(origVar)->_name;

                returnString += "\n"+offset(2)+
                    "// For categorical variable "+origAttName+"\n";
        
                // Sort the view on the count
                returnString += offset(2)+sortAlgo+origView+".begin(),"+
                    origView+".end(),[ ](const "+origView+"_tuple& lhs, const "+origView+
                    "_tuple& rhs)\n"+offset(2)+"{\n"+
                    offset(3)+"return lhs.aggregates[0] < rhs.aggregates[0];\n"+
                    offset(2)+"});\n";
            
                returnString += offset(2)+"// one-hot encoding for "+attName+"\n"+
                    offset(2)+"last_cluster_size = size_of_query_result;\n"+
                    offset(2)+"for (size_t t = 0; t < "+origView+".size(); ++t)\n"+
                    offset(2)+"{\n"+
                    offset(3)+"if (t < "+std::to_string(_dimensionK-1)+")\n"+
                    offset(3)+"{\n"+
                    offset(4)+"last_cluster_size -= "+origView+"[t].aggregates[0];\n"+
                    offset(4)+""+origView+"[t].aggregates[0] = 1;\n"+
                    offset(3)+"}\n"+offset(3)+"else\n"+offset(3)+"{\n"+
                    offset(4)+""+origView+"[t].aggregates[0] = "+
                    origView+"[t].aggregates[0] / last_cluster_size;\n"+
                    offset(3)+"}\n"+offset(2)+"}\n";
            }
        }
    }
    
    returnString += "\n"+offset(2)+"endProcess = duration_cast<milliseconds>("+
        "system_clock::now().time_since_epoch()).count()-startProcess;\n\n"+
        // offset(2)+"endClustering -= startProcess;\n\n"+
        offset(2)+"std::ofstream ofs(\"times.txt\",std::ofstream::out | "+
        "std::ofstream::app);\n"+
        offset(2)+"ofs << \"\\t\" << endClustering << \"\\t\" "+
        "<< endProcess << \"\\t\" << "+countViewName+".size();\n"+
        offset(2)+"ofs.close();\n\n"+
        offset(2)+"std::cout << \"Clustering Dimensions: \"+"+
        "std::to_string(endClustering)+\"ms.\\n\";\n"+
        offset(2)+"std::cout << \"Compute Grid: \"+"+
        "std::to_string(endProcess)+\"ms.\\n\";\n"+
        offset(2)+"std::cout << \"Size of Grid: \" << "+
        countViewName+".size() << \" points.\\n\";\n";

    return returnString+offset(1)+"}\n\n";
}


std::string KMeans::genClusterInitialization(const std::string& gridName)
{
    std::vector<std::string> initList;


    /* Load the cluster initialization file into an input stream. */
    ifstream input(multifaq::dir::PATH_TO_FILES + "/cluster_init_"+DATASET_NAME+".conf");

    std::cout << multifaq::dir::PATH_TO_FILES + "/cluster_init_"+DATASET_NAME+".conf" << std::endl;

    if (!input)
    {
        std::cout << "INFO: no cluster initialization file provided. \n";
    }
    else
    {
        /* String and associated stream to receive lines from the file. */
        string line;
        stringstream ssLine;

        while (std::getline(input, line))
        {
            if (line[0] == COMMENT_CHAR)
            {
                continue;
            }
            initList.push_back(line+"\n");
        }
    }

    std::string returnString = offset(2)+"// Initialize the means\n";
    
    for (size_t str = 0; str < std::min(initList.size(), _k); ++str)
    {
        returnString += offset(2)+"means["+std::to_string(str)+"] = "+
            initList[str];
    }
    
    if (initList.size() < _k)
    {
        returnString += offset(2)+"for (size_t cluster = "+
            std::to_string(initList.size())+"; cluster < k; ++cluster)\n"+
            offset(3)+"means[cluster] += "+gridName+"[rand() % "+
            gridName+".size()];\n\n";
    }
    
    return returnString;
}
