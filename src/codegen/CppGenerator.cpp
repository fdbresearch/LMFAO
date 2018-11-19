//--------------------------------------------------------------------
//
// CppGenerator.cpp
//
// Created on: 9 Jan 2018
// Author: Max
//
//--------------------------------------------------------------------

#include <algorithm>
#include <fstream>
#include <string>
#include <queue>
#include <unordered_set>

#include <CppGenerator.h>
#include <TDNode.hpp>

#define OPTIMIZED

/* Turn this flag on to output times for each Group individually. */
#define BENCH_INDIVIDUAL 

//#define USE_MULTIOUTPUT_OPERATOR
//#define ENABLE_RESORT_RELATIONS
//#define MICRO_BENCH
//#define COMPRESS_AGGREGATES

// #ifdef ENABLE_RESORT_RELATIONS
// #undef USE_MULTIOUTPUT_OPERATOR
// const bool RESORT_RELATIONS = true; // TODO: this is not used currently
// #else
// const bool RESORT_RELATIONS = false;
// #endif

static bool GROUP_VIEWS;
static bool RESORT_RELATIONS;
static bool MICRO_BENCH;
static bool COMPRESS_AGGREGATES;

// #ifdef USE_MULTIOUTPUT_OPERATOR
// static bool GROUP_VIEWS = true;
// #else
// static bool GROUP_VIEWS = false;
// #endif

using namespace multifaq::params;

typedef boost::dynamic_bitset<> dyn_bitset;

CppGenerator::CppGenerator(const std::string path, const std::string outDirectory,
                           const bool multioutput_flag, const bool resort_flag,
                           const bool microbench_flag, const bool compression_flag,
                           std::shared_ptr<Launcher> launcher) :
    _pathToData(path), _outputDirectory(outDirectory)
{
    _td = launcher->getTreeDecomposition();
    _qc = launcher->getCompiler();

    GROUP_VIEWS = multioutput_flag;
    MICRO_BENCH = microbench_flag;
    COMPRESS_AGGREGATES = compression_flag;    
    
    if (resort_flag)
    {
        GROUP_VIEWS = false;
        RESORT_RELATIONS = true;
    }
}

CppGenerator::~CppGenerator()
{
    delete[] viewName;
    for (size_t rel = 0; rel < _td->numberOfRelations() + _qc->numberOfViews(); ++rel)
        delete[] sortOrders[rel];
    delete[] sortOrders;
    delete[] viewLevelRegister;
}

void CppGenerator::generateCode(const ParallelizationType parallelization_type,
                                bool hasApplicationHandler,
                                bool hasDynamicFunctions)
{
    DINFO("Start generate C++ code. \n");

    _hasApplicationHandler = hasApplicationHandler;
    _hasDynamicFunctions = hasDynamicFunctions;
    
    sortOrders = new size_t*[_td->numberOfRelations() + _qc->numberOfViews()];
    viewName = new std::string[_qc->numberOfViews()];
    viewLevelRegister  = new size_t[_qc->numberOfViews()];
    
    for (size_t rel = 0; rel < _td->numberOfRelations(); ++rel)
        sortOrders[rel] = new size_t[_td->getRelation(rel)->_bag.count()]();
    
    for (size_t view = 0; view < _qc->numberOfViews(); ++view)
    {   
        sortOrders[view + _td->numberOfRelations()] =
            new size_t[_qc->getView(view)->_fVars.count()]();
        viewName[view] = "V"+std::to_string(view);
    }
    
    variableDependency.resize(NUM_OF_VARIABLES);
    
    // Find all the dependencies amongst the variables
    for (size_t rel = 0; rel < _td->numberOfRelations(); ++rel)
    {
        var_bitset nodeBag = _td->getRelation(rel)->_bag;

        for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
        {
            if (nodeBag[var])
                variableDependency[var] |= nodeBag;
        }
    }
    
    // FIRST WE CREATE THE VARIABLE ORDER FOR EACH RELATION
    size_t orderIdx = 0;
    for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
    {
        if (_td->_root->_bag[var])
        {
            sortOrders[_td->_root->_id][orderIdx] = var;
            ++orderIdx;
        }
    }
    
    createRelationSortingOrder(_td->_root, _td->_root->_id);
    
    createGroupVariableOrder();

    bool domainParallelism = parallelization_type == DOMAIN_PARALLELIZATION ||
        parallelization_type == BOTH_PARALLELIZATION;

    computeParallelizeGroup(domainParallelism);
    
#ifdef OLD
    createVariableOrder();
#endif

    genDataHandler();
    
#ifdef OPTIMIZED
    for (size_t group = 0; group < viewGroups.size(); ++group)
        genComputeGroupFiles(group);


    bool taskParallelism = parallelization_type == TASK_PARALLELIZATION ||
        parallelization_type == BOTH_PARALLELIZATION;

    genMainFunction(taskParallelism);
    
    genMakeFile();
#endif
    
}

void CppGenerator::genDataHandler()
{
    std::string header = std::string("#ifndef INCLUDE_DATAHANDLER_HPP_\n")+
        "#define INCLUDE_DATAHANDLER_HPP_\n\n"+
        "#include <algorithm>\n"+
        "#include <chrono>\n"+
        "#include <cstring>\n"+
        "#include <fstream>\n"+
        "#include <iostream>\n" +
        "#include <thread>\n" +
        "#include <unordered_map>\n" +
        "#include <vector>\n\n";

#if defined(__GNUC__) && defined(NDEBUG) && !defined(__clang__)
    header += "#include <parallel/algorithm>\n";
#endif

    if (MICRO_BENCH)
    {
        header += "#define INIT_MICRO_BENCH(timer)\\\n"+offset(1)+
            "auto begin_##timer = std::chrono::high_resolution_clock::now();\\\n"+
            offset(1)+
            "auto finish_##timer = std::chrono::high_resolution_clock::now();\\\n"+
            offset(1)+"std::chrono::duration<double>  elapsed_##timer = "+
            "finish_##timer - begin_##timer;\\\n"+
            offset(1)+"double time_spent_##timer = elapsed_##timer.count();\\\n"+
            offset(1)+"double global_time_##timer = 0;\n";

        header += std::string("#define BEGIN_MICRO_BENCH(timer) begin_##timer = ")+
            "std::chrono::high_resolution_clock::now();\n";
        header += std::string("#define END_MICRO_BENCH(timer) finish_##timer = ")+
            "std::chrono::high_resolution_clock::now();\\\n"+
            offset(1)+"elapsed_##timer = finish_##timer - begin_##timer;\\\n"+
            offset(1)+"time_spent_##timer = elapsed_##timer.count();\\\n"+
            offset(1)+"global_time_##timer += time_spent_##timer;\n";

        header += std::string("#define PRINT_MICRO_BENCH(timer) std::cout << ")+
            "#timer << \": \" << std::to_string(global_time_##timer) << std::endl;\n\n";
    }
    
    header += "using namespace std::chrono;\n\nnamespace lmfao\n{\n"+
        offset(1)+"//const std::string PATH_TO_DATA = \"/Users/Maximilian/Documents/"+
        "Oxford/LMFAO/"+_pathToData+"\";\n"+
        offset(1)+"const std::string PATH_TO_DATA = \"../../"+_pathToData+"\";\n\n";

    std::ofstream ofs(_outputDirectory+"DataHandler.h", std::ofstream::out);

    ofs << header << genTupleStructs()
        << genCaseIdentifiers() << std::endl;
    ofs <<  "}\n\n#endif /* INCLUDE_DATAHANDLER_HPP_*/\n";    
    ofs.close();

    ofs.open(_outputDirectory+"DataHandler.cpp", std::ofstream::out);
    ofs << genTupleStructConstructors() << std::endl;
    ofs.close();
}


void CppGenerator::genComputeGroupFiles(size_t group)
{
    std::ofstream ofs(_outputDirectory+"ComputeGroup"+std::to_string(group)+".h",
                      std::ofstream::out);

    ofs << "#ifndef INCLUDE_COMPUTEGROUP"+std::to_string(group)+"_HPP_\n"+
        "#define INCLUDE_COMPUTEGROUP"+std::to_string(group)+"_HPP_\n\n"+
        "#include \"DataHandler.h\"\n\nnamespace lmfao\n{\n";
    if (_parallelizeGroup[group])
    {
        // ofs << offset(1)+"void computeGroup"+std::to_string(group)+"(";

        // for (const size_t& viewID : viewGroups[group])
        // {
        //     ofs << "std::vector<"+viewName[viewID]+"_tuple>& "+viewName[viewID]+",";
        //     if (_requireHashing[viewID])
        //     {
        //         ofs << "std::unordered_map<"+viewName[viewID]+"_key,"+
        //             "size_t, "+viewName[viewID]+"_keyhash>& "+viewName[viewID]+"_map,";
        //     }
        // }
        // ofs << "size_t lowerptr, size_t upperptr);\n";
        // ofs << offset(1)+"void
        // computeGroup"+std::to_string(group)+"Parallelized();\n";
         ofs << offset(1)+"void computeGroup"+std::to_string(group)+"();\n";
    }
    else
    {
        ofs << offset(1)+"void computeGroup"+std::to_string(group)+"();\n"; 
    }
    
    ofs << "}\n\n#endif /* INCLUDE_COMPUTEGROUP"+std::to_string(group)+"_HPP_*/\n";
    ofs.close();
        
    ofs.open(_outputDirectory+"ComputeGroup"+std::to_string(group)+".cpp",
             std::ofstream::out);

    // TODO:TODO:TODO: We should avoid including DynamicFunctions.h if it is not
    // needed.
    
    ofs << "#include \"ComputeGroup"+std::to_string(group)+".h\"\n";

    if (_hasDynamicFunctions)
        ofs << "#include \"DynamicFunctions.h\"\n";
    
    ofs << "namespace lmfao\n{\n";
    ofs << genComputeGroupFunction(group) << std::endl;
    ofs << "}\n";
    ofs.close();
}


void CppGenerator::genMainFunction(bool parallelize)
{
    std::ofstream ofs(_outputDirectory+"main.cpp",std::ofstream::out);

    ofs << "#ifdef TESTING \n"+offset(1)+"#include <iomanip>\n"+"#endif\n";
    
    ofs << "#include \"DataHandler.h\"\n";
    for (size_t group = 0; group < viewGroups.size(); ++group)
        ofs << "#include \"ComputeGroup"+std::to_string(group)+".h\"\n";

    if (_hasApplicationHandler)
        ofs << "#include \"ApplicationHandler.h\"\n";
    
    ofs << "\nnamespace lmfao\n{\n";    
    // offset(1)+"//const std::std::ring PATH_TO_DATA = \"/Users/Maximilian/Documents/"+
    // "Oxford/LMFAO/"+_pathToData+"\";\n"+
    // offset(1)+"const std::string PATH_TO_DATA = \"../../"+_pathToData+"\";\n\n";

    for (size_t relID = 0; relID < _td->numberOfRelations(); ++relID)
    {
        const std::string& relName = _td->getRelation(relID)->_name;
        ofs << offset(1)+"std::vector<"+relName+"_tuple> "+relName+";" << std::endl;
    }
    
    for (size_t viewID = 0; viewID < _qc->numberOfViews(); ++viewID)
        ofs << offset(1)+"std::vector<"+viewName[viewID]+"_tuple> "+
            viewName[viewID]+";" << std::endl;
    
    ofs << genLoadRelationFunction() << std::endl;

    for (size_t relID = 0; relID < _td->numberOfRelations(); ++relID)
        ofs << genSortFunction(relID) << std::endl;
    
    ofs << genRunFunction(parallelize) << std::endl;
    ofs << genTestFunction() << genDumpFunction() << "}\n\n";
 
    ofs << "int main(int argc, char *argv[])\n{\n"+// #ifndef MULTITHREAD\n"+
        offset(1)+"std::cout << \"run lmfao\" << std::endl;\n"+
        offset(1)+"lmfao::run();\n"+//"#else\n"+
        // offset(1)+"std::cout << \"run multithreaded lmfao\" << std::endl;\n"+
        // offset(1)+"lmfao::runMultithreaded();\n#endif\n"+
        "#ifdef TESTING\n"+offset(1)+"lmfao::ouputViewsForTesting();\n#endif\n"+
        "#ifdef DUMP_OUTPUT\n"+offset(1)+"lmfao::dumpOutputViews();\n#endif\n"+
        offset(1)+"return 0;\n};\n";
    ofs.close();
}


void CppGenerator::genMakeFile()
{
    std::string objectList = "main.o datahandler.o ";
    if (_hasApplicationHandler)
        objectList += "application.o ";
    if (_hasDynamicFunctions)
        objectList += "dynamic.o ";
    
    std::string objectConstruct = "";
    for (size_t group = 0; group < viewGroups.size(); ++group)
    {
        std::string groupString = std::to_string(group);
        objectList += "computegroup"+groupString+".o ";
        objectConstruct += "computegroup"+groupString+".o : ComputeGroup"+
            groupString+".cpp\n\t$(CXX) $(FLAG) $(CXXFLAG) -xc++ -c ComputeGroup"+
            groupString+".cpp -o computegroup"+groupString+".o\n\n";
    }
    
    std::ofstream ofs(_outputDirectory+"Makefile",std::ofstream::out);

    ofs << "CXXFLAG  += -std=c++11 -O2 -pthread -mtune=native -ftree-vectorize";

#if defined(__GNUC__) && defined(NDEBUG) && !defined(__clang__)
    ofs << " -fopenmp";
#endif

    ofs << "\n# -pthread -O2 -mtune=native -fassociative-math -freciprocal-math "
        << "-fno-signed-zeros -v -ftime-report -fno-stack-protector\n\n";

    ofs << "lmfao : "+objectList+"\n\t$(CXX) $(CXXFLAG) "+objectList+"-o lmfao\n\n";

    ofs << "main.o : main.cpp\n"
        << "\t$(CXX) $(FLAG) $(CXXFLAG) -c main.cpp -o main.o\n\n"
        << "datahandler.o : DataHandler.cpp\n"
        << "\t$(CXX) $(FLAG) $(CXXFLAG) -c DataHandler.cpp -o datahandler.o\n\n";
    if (_hasApplicationHandler)
        ofs << "application.o : ApplicationHandler.cpp\n"
            << "\t$(CXX) $(FLAG) $(CXXFLAG) -c ApplicationHandler.cpp "
            << "-o application.o\n\n";
    if (_hasDynamicFunctions)
        ofs << "dynamic.o : DynamicFunctions.cpp\n"
            << "\t$(CXX) $(FLAG) $(CXXFLAG) -c DynamicFunctions.cpp "
            << "-o dynamic.o\n\n";

    ofs << objectConstruct;

    ofs << ".PHONY : multithread\n"
        << "multi : FLAG = -DMULTITHREAD\n"
        << "multi : lmfao\n\n"
        << ".PHONY : precompilation\n"
        << "precomp : main.o\n"
        << "precomp : datahandler.o\n"
        << "precomp : application.o\n\n"
        << ".PHONY : testing\n"
        << "test : FLAG = -DTESTING\n"
        << "test : lmfao\n\n"
        << ".PHONY : dump\n"
        << "dump : FLAG = -DDUMP_OUTPUT\n"
        << "dump : lmfao\n\n"
        << ".PHONY : clean\n"
        << "clean :\n"
	<< "\trm *.o lmfao";

    ofs.close();
}


std::string CppGenerator::genSortFunction(const size_t& rel_id)
{
    TDNode* relation = _td->getRelation(rel_id);
    std::string relName = relation->_name;

#if defined(__GNUC__) && defined(NDEBUG) && !defined(__clang__)
    std::string sortAlgo = "__gnu_parallel::sort(";
#else
    std::string sortAlgo = "std::sort(";
#endif
    
    std::string returnString = offset(1)+"void sort"+relName+"()\n"+offset(1)+"{\n";

#ifdef BENCH_INDIVIDUAL
    returnString += offset(2)+"int64_t startProcess = duration_cast<milliseconds>("+
        "system_clock::now().time_since_epoch()).count();\n\n";
#endif

    returnString += offset(2)+sortAlgo+relName+".begin(),"+
        relName+".end(),[ ](const "+relName+"_tuple& lhs, const "+relName+
        "_tuple& rhs)\n"+offset(2)+"{\n";

    // size_t orderIdx = 0;
    // for (const size_t& var : varOrder)
    // {
    //     if (baseRelation->_bag[var])
    //     {
    //         const std::string& attrName = _td->getAttribute(var)->_name;
    //         returnString += offset(3)+"if(lhs."+attrName+" != rhs."+attrName+")\n"+
    //             offset(4)+"return lhs."+attrName+" < rhs."+attrName+";\n";
    //         // Reset the relSortOrder array for future calls 
    //         sortOrders[view->_origin][orderIdx] = var;
    //         ++orderIdx;
    //     }
    // }

    for (size_t var = 0; var < relation->_bag.count(); ++var)
    {
        const std::string& attrName = _td->getAttribute(sortOrders[rel_id][var])->_name;
        returnString += offset(3)+"if(lhs."+attrName+" != rhs."+attrName+")\n"+
            offset(4)+"return lhs."+attrName+" < rhs."+attrName+";\n";
    }
    returnString += offset(3)+"return false;\n"+offset(2)+"});\n";

#ifdef BENCH_INDIVIDUAL
    returnString += "\n"+offset(2)+"int64_t endProcess = duration_cast<milliseconds>("+
        "system_clock::now().time_since_epoch()).count()-startProcess;\n"+
        offset(2)+"std::cout << \"Sort Relation "+relName+": \"+"+
        "std::to_string(endProcess)+\"ms.\\n\";\n\n";    
        // offset(2)+"std::ofstream ofs(\"times.txt\",std::ofstream::out | " +
        // "std::ofstream::app);\n"+
        // offset(2)+"ofs << \"\\t\" << endProcess;\n"+
        // offset(2)+"ofs.close();\n";
#endif
    
    return returnString+offset(1)+"}\n";
}


void CppGenerator::computeViewOrdering()
{
    incomingViews = new std::vector<size_t>[_qc->numberOfViews()]();
    viewToGroupMapping = new size_t[_qc->numberOfViews()];

    std::vector<std::vector<size_t>> viewsPerNode(
        _td->numberOfRelations(), std::vector<size_t>());
    
    for (size_t viewID = 0; viewID < _qc->numberOfViews(); ++viewID)
    {
        View* view = _qc->getView(viewID);
        TDNode* baseRelation = _td->getRelation(view->_origin);
        
        size_t numberIncomingViews =
            (view->_origin == view->_destination ? baseRelation->_numOfNeighbors :
             baseRelation->_numOfNeighbors - 1);
        
        std::vector<bool> incViewBitset(_qc->numberOfViews());
        for (size_t aggNo = 0; aggNo < view->_aggregates.size(); ++aggNo)
        {
            Aggregate* aggregate = view->_aggregates[aggNo];

            size_t incOffset = 0;
            // First find the all views that contribute to this Aggregate
            for (size_t i = 0; i < aggregate->_agg.size(); ++i)
            {
                for (size_t j = 0; j < numberIncomingViews; ++j)
                {
                    const size_t& incViewID = aggregate->_incoming[incOffset].first;
               
                    // Indicate that this view contributes to some aggregate
                    incViewBitset[incViewID] = 1;
                    ++incOffset;
                }
            }
        }        
        // For each incoming view, check if it contains any vars from the
        // variable order and then add it to the corresponding info
        for (size_t incViewID=0; incViewID < _qc->numberOfViews(); ++incViewID)
            if (incViewBitset[incViewID])
                incomingViews[viewID].push_back(incViewID);

        // Keep track of the views that origin from this node 
        viewsPerNode[view->_origin].push_back(viewID);
    }

    /* Next compute the topological order of the views */
    dyn_bitset processedViews(_qc->numberOfViews());
    dyn_bitset queuedViews(_qc->numberOfViews());
    
    // create orderedViewList -> which is empty
    std::vector<size_t> orderedViewList;
    std::queue<size_t> nodesToProcess;
    
    // Find all views that originate from leaf nodes
    for (const size_t& nodeID : _td->_leafNodes)
    {
        for (const size_t& viewID : viewsPerNode[nodeID])
        {
            if (incomingViews[viewID].size() == 0)
            {
                queuedViews.set(viewID);
                nodesToProcess.push(viewID);
            }
        }
    }
    
    while (!nodesToProcess.empty()) // there is a node in the set
    {
        size_t viewID = nodesToProcess.front();
        nodesToProcess.pop();

        orderedViewList.push_back(viewID);
        processedViews.set(viewID);

        const size_t& destination = _qc->getView(viewID)->_destination;
        for (const size_t& destView : viewsPerNode[destination])
        {
            // check if this has not been processed (case for root nodes)
            if (!processedViews[destView])
            {
                bool candidate = true;
                for (const size_t& incView : incomingViews[destView])
                {    
                    if (!processedViews[incView])
                    {
                        candidate = false;
                        break;
                    }
                }
                if (candidate && !queuedViews[destView])
                {
                    queuedViews.set(destView);
                    nodesToProcess.push(destView);
                }
            }
        }
    }

    /* Now generate the groups of views according to topological order*/
    size_t prevOrigin = _qc->getView(orderedViewList[0])->_origin;
    viewGroups.push_back({orderedViewList[0]});

    viewToGroupMapping[orderedViewList[0]] = 0;    
    size_t currentGroup = 0;
    for (size_t viewID = 1; viewID < orderedViewList.size(); ++viewID)
    {
        size_t origin = _qc->getView(orderedViewList[viewID])->_origin;
        if (GROUP_VIEWS && prevOrigin == origin)
        {
            // Combine the two into one set of
            viewGroups[currentGroup].push_back(orderedViewList[viewID]);
            viewToGroupMapping[orderedViewList[viewID]] = currentGroup;
        }
        else
        {
            // Create a new group
            viewGroups.push_back({orderedViewList[viewID]});
            ++currentGroup;
            viewToGroupMapping[orderedViewList[viewID]] = currentGroup;
        }
        prevOrigin = origin;
    }

    /**************** PRINT OUT **********************/
    DINFO("Ordered View List: ");
    for (size_t& i : orderedViewList)
        DINFO(i << ", ");
    DINFO(std::endl);
    DINFO("View Groups: ");
    for (auto& group : viewGroups) {
        for (auto& i : group)
            DINFO(i << "  ");
        DINFO("|");
    }
    DINFO(std::endl);
    /**************** PRINT OUT **********************/
}





/* TODO: Technically there is no need to pre-materialise this! */
void CppGenerator::createGroupVariableOrder()
{
    computeViewOrdering();
    
    groupVariableOrder = new std::vector<size_t>[viewGroups.size()]();
    groupIncomingViews = new std::vector<size_t>[viewGroups.size()]();
    groupViewsPerVarInfo = new std::vector<size_t>[viewGroups.size()];

    groupVariableOrderBitset.resize(viewGroups.size());
    
    for (size_t group = 0; group < viewGroups.size(); ++group)
    {
        var_bitset joinVars;
        std::vector<bool> incViewBitset(_qc->numberOfViews());
        
        TDNode* baseRelation = _td->getRelation(
            _qc->getView(viewGroups[group][0])->_origin);

        // TODO:TODO: is it ok to add this in ?!??
        // we are adding the variables in the intersection with the destination
        TDNode* destRelation = _td->getRelation(
            _qc->getView(viewGroups[group][0])->_destination);

        if (baseRelation->_id != destRelation->_id)
            joinVars |= destRelation->_bag & baseRelation->_bag;

        for (const size_t& viewID : viewGroups[group])
        {
            View* view = _qc->getView(viewID);

            TDNode* destRelation = _td->getRelation(view->_destination);

            size_t numberIncomingViews =
                (view->_origin == view->_destination ? baseRelation->_numOfNeighbors :
                 baseRelation->_numOfNeighbors - 1);

            // if ther are not inc views we use the fVars as free variable
            if (numberIncomingViews == 0)
                joinVars |= (view->_fVars & destRelation->_bag);
            else
            {
                for (size_t aggNo = 0; aggNo < view->_aggregates.size(); ++aggNo)
                {
                    Aggregate* aggregate = view->_aggregates[aggNo];

                    size_t incOffset = 0;
                    // First find the all views that contribute to this Aggregate
                    for (size_t i = 0; i < aggregate->_agg.size(); ++i)
                    {
                        for (size_t j = 0; j < numberIncomingViews; j++)
                        {
                            const size_t& incViewID =
                                aggregate->_incoming[incOffset].first;


                            // TODO: TODO: TODO: MAKE SURE THAT YOU ALSO
                            // CONSIDER INTERSECTION OF FREE VARS THAT ARE
                            // SHARED B/W TWO VIEWS BUT NOT IN BAG
                            
                            // Add the intersection of the view and the base
                            // relation to joinVars
                            joinVars |= (_qc->getView(incViewID)->_fVars &
                                         baseRelation->_bag);
                            // Indicate that this view contributes to some aggregate
                            incViewBitset[incViewID] = 1;
                            ++incOffset;
                        }                        
                    }
                }
            }
        }

        for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
            if (joinVars[var])
                groupVariableOrder[group].push_back(var);

        groupVariableOrderBitset[group] |= joinVars;
        
        groupViewsPerVarInfo[group].resize(groupVariableOrder[group].size() *
                                       (_qc->numberOfViews() + 2), 0);

        // For each variable, check if the baseRelation contains this variable
        for (size_t var=0; var < groupVariableOrder[group].size(); ++var)
        {
            if (baseRelation->_bag[groupVariableOrder[group][var]])
            {
                size_t idx = var * (_qc->numberOfViews() + 2);
                size_t& off = groupViewsPerVarInfo[group][idx];
                groupViewsPerVarInfo[group][idx+1] = _qc->numberOfViews();
                ++off;
            }
        }

        // For each incoming view, check if it contains any vars from the
        // variable order and then add it to the corresponding info
        for (size_t incViewID=0; incViewID < _qc->numberOfViews(); ++incViewID)
        {
            if (incViewBitset[incViewID])
            {
                groupIncomingViews[group].push_back(incViewID);
                const var_bitset& viewFVars = _qc->getView(incViewID)->_fVars;
                    
                for (size_t var=0; var < groupVariableOrder[group].size(); ++var)
                {
                    if (viewFVars[groupVariableOrder[group][var]])
                    {
                        size_t idx = var * (_qc->numberOfViews() + 2);
                        size_t& off = groupViewsPerVarInfo[group][idx];
                        groupViewsPerVarInfo[group][idx+off+1] = incViewID;
                        ++off;
                    }
                }
            }
        }      
    }

    _requireHashing = new bool[_qc->numberOfViews()]();

    for (size_t viewID = 0; viewID < _qc->numberOfViews(); ++viewID)
    {
        View* view = _qc->getView(viewID);
        bool hash = false;
        size_t orderIdx = 0;
        for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
        {
            if (view->_fVars[var])
            {
                if (sortOrders[view->_origin][orderIdx] != var)
                    hash = true;
                ++orderIdx;
            }
        }
        _requireHashing[viewID] = hash;
    }
}





inline std::string CppGenerator::offset(size_t off)
{
    return std::string(off*3, ' ');
}

std::string CppGenerator::typeToStr(Type t)
{
    switch(t)
    {
    case Type::Integer : return "int";
    case Type::Double : return "double";            
    case Type::Short : return "short";
    case Type::U_Integer : return "size_t";
    default :
        ERROR("This type does not exist \n");
        exit(1);
    }
}

std::string CppGenerator::genHeader()
{   
    std::string s = "DATAHANDLER";
    std::string header = "#ifndef INCLUDE_"+s+"_HPP_\n"+
        "#define INCLUDE_"+s+"_HPP_\n\n"+
        "#include <algorithm>\n"+
        "#include <chrono>\n"+
        "#include <cstring>\n"+
        "#include <fstream>\n"+
        "#include <iostream>\n" +
        "#include <thread>\n" +
        "#include <unordered_map>\n" +
        "#include <vector>\n\n";

#if defined(__GNUC__) && defined(NDEBUG) && !defined(__clang__)
   header += "#include <parallel/algorithm>\n";
#endif
    
   header += "using namespace std::chrono;\n\nnamespace lmfao\n{\n"+
        offset(1)+"//const std::string PATH_TO_DATA = \"/Users/Maximilian/Documents/"+
        "Oxford/LMFAO/"+_pathToData+"\";\n"+
        offset(1)+"const std::string PATH_TO_DATA = \"../../"+_pathToData+"\";\n\n";

   return header;
}

std::string CppGenerator::genTupleStructs()
{
    std::string tupleStruct = "", attrConversion = "", attrConstruct = "",
        attrAssign = "", attrParse = "";

    for (size_t relID = 0; relID < _td->numberOfRelations(); ++relID)
    {
        TDNode* rel = _td->getRelation(relID);
        const std::string& relName = rel->_name;
        const var_bitset& bag = rel->_bag;

        tupleStruct += offset(1)+"struct "+relName+"_tuple\n"+
            offset(1)+"{\n"+offset(2);

        for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
        {
            if (bag[var])
            {
                Attribute* att = _td->getAttribute(var);
                tupleStruct += typeToStr(att->_type)+" "+att->_name+";\n"+offset(2);
            }
        }
        tupleStruct += relName+"_tuple(const std::string& tuple);\n"+offset(1)+"};\n\n"+
            offset(1)+"extern std::vector<"+rel->_name+"_tuple> "+rel->_name+";\n\n";
    }

    for (size_t viewID = 0; viewID < _qc->numberOfViews(); ++viewID)
    {
        View* view = _qc->getView(viewID);

        attrConstruct = "";
        
        tupleStruct += offset(1)+"struct "+viewName[viewID]+"_tuple\n"+
            offset(1)+"{\n";

        for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
        {
            if (view->_fVars[var])
            {
                Attribute* att = _td->getAttribute(var);
                tupleStruct += offset(2)+typeToStr(att->_type)+" "+att->_name+";\n";
                attrConstruct += "const "+typeToStr(att->_type)+"& "+att->_name+",";
            }
        }

        tupleStruct += offset(2)+"double aggregates["+
            std::to_string(view->_aggregates.size())+"] = {};\n";
        
        if (!attrConstruct.empty())
        {
            attrConstruct.pop_back();
            tupleStruct += offset(2)+viewName[viewID]+"_tuple("+
                attrConstruct+");\n"+offset(1)+"};\n\n"+
                offset(1)+"extern std::vector<"+viewName[viewID]+"_tuple> "+
                viewName[viewID]+";\n\n";
        }
        else
            tupleStruct += offset(2)+viewName[viewID]+"_tuple();\n"+offset(1)+
                "};\n\n"+offset(1)+"extern std::vector<"+viewName[viewID]+"_tuple> "+
                viewName[viewID]+";\n\n";
    }
    return tupleStruct;
}

std::string CppGenerator::genTupleStructConstructors()
{
    std::string tupleStruct = offset(0)+"#include \"DataHandler.h\"\n"+
        "#include <boost/spirit/include/qi.hpp>\n"+
        "#include <boost/spirit/include/phoenix_core.hpp>\n"+
        "#include <boost/spirit/include/phoenix_operator.hpp>\n\n"+
        "namespace qi = boost::spirit::qi;\n"+
        "namespace phoenix = boost::phoenix;\n\n"+
        "namespace lmfao\n{\n";
    
    std::string attrConstruct = "", attrAssign = "";
    
    for (size_t relID = 0; relID < _td->numberOfRelations(); ++relID)
    {
        TDNode* rel = _td->getRelation(relID);
        const std::string& relName = rel->_name;
        const var_bitset& bag = rel->_bag;

        attrConstruct = offset(2)+"qi::phrase_parse(tuple.begin(),tuple.end(),";
        // size_t field = 0;
        for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
        {
            if (bag[var])
            {
                Attribute* att = _td->getAttribute(var);
                attrConstruct += "\n"+offset(3)+"qi::"+typeToStr(att->_type)+
                    "_[phoenix::ref("+att->_name+") = qi::_1]>>";
            }
        }
        // attrConstruct.pop_back();
        attrConstruct.pop_back();
        attrConstruct.pop_back();
        attrConstruct += ",\n"+offset(3)+"\'|\');\n"+offset(1);

        tupleStruct += offset(1)+relName+"_tuple::"+relName+
            "_tuple(const std::string& tuple)\n"+offset(1)+"{\n"+attrConstruct+"}\n\n";
    }

    for (size_t viewID = 0; viewID < _qc->numberOfViews(); ++viewID)
    {
        View* view = _qc->getView(viewID);

        attrConstruct = "", attrAssign = "";

        for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
        {
            if (view->_fVars[var])
            {
                Attribute* att = _td->getAttribute(var);
                attrConstruct += "const "+typeToStr(att->_type)+"& "+att->_name+",";
                attrAssign += att->_name+"("+att->_name+"),";
            }
        }        
        if (!attrConstruct.empty())
        {
            attrAssign.pop_back();
            attrConstruct.pop_back();
            
            tupleStruct +=  offset(1)+viewName[viewID]+"_tuple::"+
                viewName[viewID]+"_tuple("+attrConstruct+") : "+
                attrAssign+"{}\n\n";
        }
        else
            tupleStruct += offset(1)+viewName[viewID]+"_tuple::"+
                viewName[viewID]+"_tuple(){}\n\n";
    }
    return tupleStruct+"}\n";
}

    
std::string CppGenerator::genCaseIdentifiers()
{
    std::string returnString = offset(1)+"enum ID {";

    for (size_t rel = 0; rel < _td->numberOfRelations(); ++rel)
        returnString += "\n"+offset(2)+_td->getRelation(rel)->_name+"_ID,";

    for (size_t view = 0; view < _qc->numberOfViews(); ++view)
        returnString += "\n"+offset(2)+viewName[view]+"_ID,";
    returnString.pop_back();

    return returnString+"\n"+offset(1)+"};\n";
}

std::string CppGenerator::genLoadRelationFunction()
{
    std::string returnString = offset(1)+"void loadRelations()\n"+offset(1)+"{\n"+
        offset(2)+"std::ifstream input;\n"+offset(2)+"std::string line;\n";
    
    for (size_t rel = 0; rel < _td->numberOfRelations(); ++rel)
    {
        std::string rel_name = _td->getRelation(rel)->_name;
        
        returnString += "\n"+offset(2)+rel_name+".clear();\n";

        returnString += offset(2)+"input.open(PATH_TO_DATA + \"/"+rel_name+".tbl\");\n"+
            offset(2)+"if (!input)\n"+offset(2)+"{\n"+
            offset(3)+"std::cerr << PATH_TO_DATA + \""+rel_name+
            ".tbl does is not exist. \\n\";\n"+
            offset(3)+"exit(1);\n"+offset(2)+"}\n"+
            offset(2)+"while(getline(input, line))\n"+
            offset(3)+rel_name+".push_back("+rel_name+"_tuple(line));\n"+
            offset(2)+"input.close();\n";            
    }   
    return returnString+offset(1)+"}\n";
}








void CppGenerator::computeParallelizeGroup(bool paralleize_groups)
{
    _parallelizeGroup = new bool[viewGroups.size()]();
    _threadsPerGroup = new size_t[viewGroups.size()]();

    if (!paralleize_groups)
        return;
    
    for (size_t gid = 0; gid < viewGroups.size(); ++gid)
    {
        TDNode* node = _td->getRelation(_qc->getView(viewGroups[gid][0])->_origin);

        if (node->_threads > 1)
        {
            _parallelizeGroup[gid] = true;            
            _threadsPerGroup[gid] = 8;
        }        
    }
}



std::string CppGenerator::genComputeGroupFunction(size_t group_id)
{
    const std::vector<size_t>& varOrder = groupVariableOrder[group_id];
    const var_bitset& varOrderBitset = groupVariableOrderBitset[group_id];
    const std::vector<size_t>& viewsPerVar = groupViewsPerVarInfo[group_id];
    const std::vector<size_t>& incViews = groupIncomingViews[group_id];

    // DO WE NEED THIS ?
    TDNode* baseRelation =_td->getRelation(
        _qc->getView(viewGroups[group_id][0])->_origin);

    std::string relName = baseRelation->_name;

    std::string hashStruct = "";
    std::string hashFunct = "";

    std::string groupString = "Group"+std::to_string(group_id);
    
    for (const size_t& viewID : viewGroups[group_id])
    {
        if (_requireHashing[viewID])
        {
            View* view = _qc->getView(viewID);

            std::string attrConstruct = "";
            std::string attrAssign = "";
            std::string equalString = "";

            // Creates a struct which is used for hashing the key of the views 
            hashStruct += offset(1)+"struct "+viewName[viewID]+"_key\n"+
                offset(1)+"{\n"+offset(2);

            hashFunct += offset(1)+"struct "+viewName[viewID]+"_keyhash\n"+
                offset(1)+"{\n"+offset(2)+"size_t operator()(const "+viewName[viewID]+
                "_key &key ) const\n"+offset(2)+"{\n"+offset(3)+"size_t h = 0;\n";
            
            for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
            {
                if (view->_fVars[var])
                {
                    Attribute* att = _td->getAttribute(var);
                    std::string type = typeToStr(att->_type);

                    hashStruct += type+" "+att->_name+";\n"+offset(2);
                    attrConstruct += type+" "+att->_name+",";
                    attrAssign += offset(3)+"this->"+ att->_name + " = "+
                        att->_name+";\n";
                    hashFunct += offset(3)+"h ^= std::hash<"+type+">()(key."+
                        att->_name+")+ 0x9e3779b9 + (h<<6) + (h>>2);\n";
                    // hashFunct += offset(3)+"hash_combine(h, key."+att->_name+");\n"; 
                    equalString += " this->"+ att->_name + " == other."+att->_name+" &&";
                }
            }
            attrConstruct.pop_back();
            equalString.pop_back();
            equalString.pop_back();

            hashStruct +=  viewName[viewID]+"_key("+attrConstruct+")\n"+offset(2)+
                "{\n"+attrAssign+offset(2)+"}\n"+
                offset(2)+"bool operator==(const "+viewName[viewID]+
                "_key &other) const\n"+offset(2)+"{\n"+offset(3)+
                "return"+equalString+";\n"+offset(2)+"}\n"+offset(1)+"};\n\n";

            hashFunct += offset(3)+"return h;\n"+offset(2)+"}\n"+offset(1)+"};\n\n";
        }
    }
    
    std::string headerString = hashStruct+hashFunct+offset(1)+"void computeGroup"+
        std::to_string(group_id);

    if (!_parallelizeGroup[group_id])
    {
        headerString += "()\n"+offset(1)+"{\n";

        for (const size_t& viewID : viewGroups[group_id])
        {
            if (_requireHashing[viewID])
            {
                headerString += offset(2)+"std::unordered_map<"+viewName[viewID]+"_key,"+
                    "size_t,"+viewName[viewID]+"_keyhash> "+viewName[viewID]+"_map;\n"+
                    offset(2)+"std::unordered_map<"+viewName[viewID]+"_key,"  +
                    "size_t,"+viewName[viewID]+"_keyhash>::iterator "+
                    viewName[viewID]+"_iterator;\n"+
                    offset(2)+"std::pair<std::unordered_map<"+viewName[viewID]+"_key,"  +
                    "size_t,"+viewName[viewID]+"_keyhash>::iterator,bool> "+
                    viewName[viewID]+"_pair;\n"+
                    offset(2)+viewName[viewID]+"_map.reserve(1000000);\n";
            }
        }
    }
    else
    {
        headerString += "Parallelized(";
        for (const size_t& viewID : viewGroups[group_id])
        {
            headerString += "std::vector<"+viewName[viewID]+"_tuple>& "+
                viewName[viewID]+",";
            if (_requireHashing[viewID])
            {
                headerString += "std::unordered_map<"+viewName[viewID]+"_key,"+
                    "size_t, "+viewName[viewID]+"_keyhash>& "+viewName[viewID]+"_map,";
            }
        }
        headerString += "size_t lowerptr, size_t upperptr)\n"+offset(1)+"{\n";

        for (const size_t& viewID : viewGroups[group_id])
        {
            if (_requireHashing[viewID])
            {
                headerString += 
                    offset(2)+"std::unordered_map<"+viewName[viewID]+"_key,"  +
                    "size_t,"+viewName[viewID]+"_keyhash>::iterator "+
                    viewName[viewID]+"_iterator;\n"+
                    offset(2)+"std::pair<std::unordered_map<"+viewName[viewID]+"_key,"  +
                    "size_t,"+viewName[viewID]+"_keyhash>::iterator,bool> "+
                    viewName[viewID]+"_pair;\n"+
                    offset(2)+viewName[viewID]+"_map.reserve(500000);\n";
            }
        }
    }

    if (RESORT_RELATIONS)
    {
        // TODO: ADD THE CODE THAT CHECKS FOR SORTING OF RELATIONS

        ERROR("WE NEED TO FIX THE VARIBALE ORDER - fVARS need to come first \n");
        exit(1);
    
        /* 
         * Compares viewSortOrder with the varOrder required - if orders are
         * different we resort 
         */
        if (viewGroups[group_id].size() > 1)
        {
            ERROR("We should only have one outgoing view in case we resort \n");
            exit(1);
        }

        size_t viewID = viewGroups[group_id][0];
        View* view = _qc->getView(viewID);
    
        if (resortRelation(view->_origin, viewID))
        {
            headerString += offset(2)+"std::sort("+relName+".begin(),"+
                relName+".end(),[ ](const "+relName+"_tuple& lhs, const "+relName+
                "_tuple& rhs)\n"+offset(2)+"{\n";

            size_t orderIdx = 0;
            for (const size_t& var : varOrder)
            {
                if (baseRelation->_bag[var])
                {
                    const std::string& attrName = _td->getAttribute(var)->_name;
                    headerString += offset(3)+"if(lhs."+attrName+" != rhs."+attrName+")\n"+
                        offset(4)+"return lhs."+attrName+" < rhs."+attrName+";\n";
                    // Reset the relSortOrder array for future calls 
                    sortOrders[view->_origin][orderIdx] = var;
                    ++orderIdx;
                }
            }
            headerString += offset(3)+"return false;\n"+offset(2)+"});\n";
        }
    
        for (const size_t& incViewID : incViews)
        {
            /*
             * Compares viewSortOrder with the varOrder required - if orders are
             * different we resort
             */
            if (resortView(incViewID, viewID))
            {
                headerString += offset(2)+"std::sort("+viewName[incViewID]+".begin(),"+
                    viewName[incViewID]+".end(),"+"[ ](const "+viewName[incViewID]+
                    "_tuple& lhs, const "+viewName[incViewID]+"_tuple& rhs)\n"+
                    offset(2)+"{\n";

                View* incView = _qc->getView(incViewID);
                size_t orderIdx = 0;
                for (const size_t& var : varOrder)
                {
                    if (incView->_fVars[var])
                    {
                        const std::string& attrName = _td->getAttribute(var)->_name;
                        headerString += offset(3);
                        if (orderIdx+1 < incView->_fVars.count())
                            headerString += "if(lhs."+attrName+" != rhs."+
                                attrName+")\n"+offset(4);
                        headerString += "return lhs."+attrName+" < rhs."+attrName+";\n";
                        // Reset the viewSortOrder array for future calls
                        sortOrders[incViewID + _td->numberOfRelations()][orderIdx] = var;
                        ++orderIdx;
                    }
                }
                //headerString += offset(3)+"return 0;\n";
            
                headerString += offset(2)+"});\n";
            }
        }
    }

    if(MICRO_BENCH)
        headerString += offset(2)+"INIT_MICRO_BENCH("+groupString+"_timer_aggregate);\n"+
            offset(2)+"INIT_MICRO_BENCH("+groupString+"_timer_post_aggregate);\n"+
            offset(2)+"INIT_MICRO_BENCH("+groupString+"_timer_seek);\n"+
            offset(2)+"INIT_MICRO_BENCH("+groupString+"_timer_while);\n";

#ifdef BENCH_INDIVIDUAL
    headerString += offset(2)+"int64_t startProcess = duration_cast<milliseconds>("+
        "system_clock::now().time_since_epoch()).count();\n\n";
#endif

    // Generate min and max values for the join varibales 
    headerString += genMaxMinValues(varOrder);

    std::string numOfJoinVarString = std::to_string(varOrder.size());

    // Creates the relation-ptr and other helper arrays for the join algo
    // Need to know how many relations per variable
    std::string arrays = "";
    for (size_t v = 0; v < varOrder.size(); ++v)
    {
        size_t idx = v * (_qc->numberOfViews() + 2);
        arrays += std::to_string(viewsPerVar[idx])+",";
    }
    arrays.pop_back();

    headerString += offset(2) +"size_t rel["+numOfJoinVarString+"] = {}, "+
        "numOfRel["+numOfJoinVarString+"] = {"+arrays+"};\n" + 
        offset(2) + "bool found["+numOfJoinVarString+"] = {}, "+
        "atEnd["+numOfJoinVarString+"] = {}, addTuple["+numOfJoinVarString+"] = {};\n";

    // Lower and upper pointer for the base relation
    headerString += genPointerArrays(
        baseRelation->_name, numOfJoinVarString, _parallelizeGroup[group_id]);

    // Lower and upper pointer for each incoming view
    for (const size_t& viewID : incViews)            
        headerString += genPointerArrays(
            viewName[viewID],numOfJoinVarString, false);
    
// #ifdef PREVIOUS
//     std::string closeAddTuple = "";
//     for (const size_t& viewID : viewGroups[group_id])
//     {
//         View* view = _qc->getView(viewID);

//         returnString += offset(2)+"double aggregates_"+viewName[viewID]+
//             "["+std::to_string(view->_aggregates.size())+"] = {};\n";
        
//         if (view->_fVars.count() == 0)
//         {
//             returnString += offset(2)+"bool addTuple_"+viewName[viewID]+" = false;\n";

//             closeAddTuple += offset(2)+"if (addTuple_"+viewName[viewID]+")\n"+
//                  offset(2)+"{\n"+offset(3)+viewName[viewID]+".push_back({});\n";
            
//             closeAddTuple+=offset(3)+"memcpy(&"+viewName[viewID]+"[0].aggregates[0],"+
//                 "&aggregates_"+viewName[viewID]+"[0], sizeof(double) * "+
//                 std::to_string(view->_aggregates.size())+");\n"+
//                 offset(2)+"}\n";
//         }
//     }
//     // for each join var --- create the string that does the join
//     returnString += genGroupLeapfrogJoinCode(group_id, *baseRelation, 0);
    
//     returnString += closeAddTuple;
// #else
    // array that stores the indexes of the aggregates 
    finalAggregateIndexes.resize(_qc->numberOfViews());
    newFinalAggregates.resize(_qc->numberOfViews());
    numAggregateIndexes = new size_t[_qc->numberOfViews()]();
    
    viewLoopFactors.resize(_qc->numberOfViews());
    outViewLoopID.resize(_qc->numberOfViews());
    outViewLoop.resize(_qc->numberOfViews());
    
    aggregateGenerator(group_id, *baseRelation);
    
    /******************* PRINT OUT *****************************/
    // if (group_id == 4)
    // {
    //     for (size_t& viewID : viewGroups[4])
    //     {
    //         std::cout << viewID << " " << viewLevelRegister[viewID] <<" : ";
    //         for (size_t incID : incomingViews[viewID])
    //             std::cout << incID << " ";
    //         std::cout << std::endl;
    //     }

    //     std::cout << baseRelation->_id  << "  " << relName << std::endl;
    // }
    /******************* PRINT OUT *****************************/


    headerString += offset(2)+"size_t ptr_"+relName+" = 0;\n";

    headerString += offset(2)+"double";
    for (const size_t& viewID : viewGroups[group_id])
        headerString += " *aggregates_"+viewName[viewID]+" = nullptr,";
    for (const size_t& viewID : incViews)
        headerString += " *aggregates_"+viewName[viewID]+" = nullptr,";
    headerString.pop_back();
    headerString += ";\n";
    
    for (const size_t& viewID : incViews)
    {
        View* view = _qc->getView(viewID);
        if ((view->_fVars & ~varOrderBitset).any())
            headerString += offset(2)+"size_t ptr_"+viewName[viewID]+" = 0;\n";
    }
    
    // We now declare the register Arrrays!
    size_t numLocalProducts = 0;
    size_t numViewProducts = 0;
    size_t numAggregates = 0;
    size_t numPostAggregates = 0;
    
    std::string aggReg = "";
    for (size_t depth = 0; depth < varOrder.size(); depth++)
    {
        if (!newAggregateRegister[depth].empty())
        {
            aggReg += " aggregateRegister_"+std::to_string(depth)+
                "["+std::to_string(newAggregateRegister[depth].size())+"],";
        }

        // updating the remapping arrays 
        aggregateRemapping[depth].resize(newAggregateRegister[depth].size(), 100);
        localProductRemapping[depth].resize(localProductList[depth].size(), 100);
        viewProductRemapping[depth].resize(viewProductList[depth].size(), 100);
        
        numLocalProducts += localProductList[depth].size();
        numLocalProducts += localProductList[varOrder.size() + depth].size();
        
        numViewProducts += viewProductList[depth].size();
        numViewProducts += viewProductList[varOrder.size() + depth].size();

        numAggregates += newAggregateRegister[depth].size();
        numPostAggregates += postRegisterList[depth].size();
    }


    aggregateCounter = 0;
    productCounter = 0;
    viewCounter = 0;
    postCounter = 0;

    // for each join var --- create the string that does the join
    std::string returnString = genGroupLeapfrogJoinCode(group_id, *baseRelation, 0);
    
    std::string outputString = "";
    // Here we add the code that outputs tuples to the view 
    for (size_t viewID : viewGroups[group_id])
    {
        if (viewLevelRegister[viewID] == varOrder.size())
        {
            // just add string to push to view
            outputString += offset(2)+viewName[viewID]+".emplace_back();\n"+
                offset(2)+"aggregates_"+viewName[viewID]+" = "+
                viewName[viewID]+".back().aggregates;\n";


            if (COMPRESS_AGGREGATES)
            {
                
                AggregateIndexes bench, aggIdx;
                std::bitset<7> increasingIndexes;
        
                mapAggregateToIndexes(bench,aggregateComputation[viewID][0],
                                      viewID,varOrder.size());
        
                size_t off = 0;
                for (size_t aggID = 1;
                     aggID < aggregateComputation[viewID].size(); ++aggID)
                {
                    const AggregateTuple& tuple = aggregateComputation[viewID][aggID];

                    aggIdx.reset();
                    mapAggregateToIndexes(aggIdx,tuple,viewID,varOrder.size());

                    if (aggIdx.isIncrement(bench,off,increasingIndexes))
                    {
                        ++off;
                    }
                    else
                    {
                        // If not, output bench and all offset aggregates
                        outputString += outputFinalRegTupleString(
                            bench,off,increasingIndexes,2);
                
                        // make aggregate the bench and set offset to 0
                        bench = aggIdx;
                        off = 0;
                        increasingIndexes.reset();
                    }
                }

                // If not, output bench and all offset
                // aggregateString
                outputString += outputFinalRegTupleString(bench,off,increasingIndexes,2);
            }
            else
            {
                for (const AggregateTuple& aggTuple : aggregateComputation[viewID]) 
                {
                    outputString += offset(2)+
                        "aggregates_"+viewName[aggTuple.viewID]+"["+
                        std::to_string(aggTuple.aggID)+"] += ";
                                                              
                    if (aggTuple.post.first < varOrder.size())
                    {
                        std::string post = std::to_string(
                            postRemapping[aggTuple.post.first][aggTuple.post.second]);
                        outputString += "postRegister["+post+"]*";
                    }
                    else
                    {
                        ERROR("THIS SHOULD NOT HAPPEN - no post aggregate?!");
                        exit(1);
                    }
        
                    outputString.pop_back();
                    outputString += ";\n";    
                }  
            }
        }        
    }
    
    returnString += outputString;

    // // Add the aggregates for the case where the view has no free variables
    // for (const size_t& viewID : viewGroups[group_id])
    // {
    //     if (viewLevelRegister[viewID] == varOrder.size())
    //         returnString += finalAggregateString[viewLevelRegister[viewID]];
    // }
    
    if (!_parallelizeGroup[group_id])
    {
        for (const size_t& viewID : viewGroups[group_id])
        {
            if (_requireHashing[viewID])
            {
                View* view = _qc->getView(viewID);
            
#if defined(__GNUC__) && defined(NDEBUG) && !defined(__clang__)
                std::string sortAlgo = "__gnu_parallel::sort(";
#else
                std::string sortAlgo = "std::sort(";
#endif
                returnString += offset(2)+sortAlgo+viewName[viewID]+".begin(),"+
                    viewName[viewID]+".end(),[ ](const "+viewName[viewID]+
                    "_tuple& lhs, const "+viewName[viewID]+"_tuple& rhs)\n"+
                    offset(2)+"{\n";

                var_bitset intersection = view->_fVars &
                    _td->getRelation(view->_destination)->_bag;
                
                for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
                {
                    if (intersection[var])
                    {   
                        const std::string& attrName = _td->getAttribute(var)->_name;
                    
                        returnString += offset(3)+
                            "if(lhs."+attrName+" != rhs."+attrName+")\n"+offset(4)+
                            "return lhs."+attrName+" < rhs."+attrName+";\n";
                    }
                }
                var_bitset additional = view->_fVars & ~intersection;
                for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
                {
                    if (additional[var])
                    {   
                        const std::string& attrName = _td->getAttribute(var)->_name;
                    
                        returnString += offset(3)+
                            "if(lhs."+attrName+" != rhs."+attrName+")\n"+offset(4)+
                            "return lhs."+attrName+" < rhs."+attrName+";\n";
                    }
                }
                
                returnString += offset(3)+"return false;\n"+offset(2)+"});\n";
            }
        }
    }

#ifdef BENCH_INDIVIDUAL
    // returnString += "\n"+offset(2)+"std::cout << \"Compute Group "+
    //     std::to_string(group_id)+" process: \"+"+
    //     "std::to_string(duration_cast<milliseconds>("+
    //     "system_clock::now().time_since_epoch()).count()-startProcess)+\"ms.\\n\";\n\n";
    returnString += "\n"+offset(2)+"int64_t endProcess = duration_cast<milliseconds>("+
        "system_clock::now().time_since_epoch()).count()-startProcess;\n"+
        offset(2)+"std::cout <<\"Compute Group "+std::to_string(group_id)+
        " process: \"+"+"std::to_string(endProcess)+\"ms.\\n\";\n"+
        offset(2)+"std::ofstream ofs(\"times.txt\",std::ofstream::out| "+
        "std::ofstream::app);\n"+
        offset(2)+"ofs << \"\\t\" << endProcess;\n"+
        offset(2)+"ofs.close();\n\n";
#endif

    if (MICRO_BENCH)
        returnString += offset(2)+
            "PRINT_MICRO_BENCH("+groupString+"_timer_aggregate);\n"+
            offset(2)+"PRINT_MICRO_BENCH("+groupString+"_timer_post_aggregate);\n"+
            offset(2)+"PRINT_MICRO_BENCH("+groupString+"_timer_seek);\n"+
            offset(2)+"PRINT_MICRO_BENCH("+groupString+"_timer_while);\n";
    
    if (_parallelizeGroup[group_id])
    {
        returnString += offset(1)+"}\n";

        returnString += offset(1)+"void computeGroup"+std::to_string(group_id)+
            "()\n"+offset(1)+"{\n";

        for (const size_t& viewID : viewGroups[group_id])
        {
            for (size_t t = 0; t < _threadsPerGroup[group_id]; ++t)
            {
                returnString += offset(2)+"std::vector<"+viewName[viewID]+
                    "_tuple> "+viewName[viewID]+"_"+std::to_string(t)+";\n";
            }
            
            if (_requireHashing[viewID])
            {    
                for (size_t t = 0; t < _threadsPerGroup[group_id]; ++t)
                {
                    returnString += offset(2)+"std::unordered_map<"+
                        viewName[viewID]+"_key,"+"size_t,"+viewName[viewID]+
                        "_keyhash> "+viewName[viewID]+"_"+std::to_string(t)+"_map;\n";
                }
            }
            
        }

        returnString += offset(2)+"size_t lower, upper;\n";

        std::string relName = baseRelation->_name;
        for (size_t t = 0; t < _threadsPerGroup[group_id]; ++t)
        {
            returnString += offset(2)+"lower = (size_t)("+relName+".size() * ((double)"+
                std::to_string(t)+"/"+std::to_string(_threadsPerGroup[group_id])+"));\n"+
                offset(2)+"upper = (size_t)("+relName+".size() * ((double)"+
                std::to_string(t+1)+"/"+
                std::to_string(_threadsPerGroup[group_id])+")) - 1;\n"+
                offset(2)+"std::thread thread"+std::to_string(t)+
                "(computeGroup"+std::to_string(group_id)+"Parallelized,";
            
            for (const size_t& viewID : viewGroups[group_id])
            {
                 returnString += "std::ref("+viewName[viewID]+"_"+
                     std::to_string(t)+"),";
                if (_requireHashing[viewID])
                    returnString += "std::ref("+viewName[viewID]+"_"+
                        std::to_string(t)+"_map),";
            }
            returnString += "lower,upper);\n";
        }

        for (size_t t = 0; t < _threadsPerGroup[group_id]; ++t)
            returnString += offset(2)+"thread"+std::to_string(t)+".join();\n";

        for (size_t& viewID : viewGroups[group_id])
        {
            if (_requireHashing[viewID])
            {
                returnString += offset(2)+viewName[viewID]+".reserve(";
                for (size_t t = 0; t < _threadsPerGroup[group_id]; ++t)
                    returnString += viewName[viewID]+"_"+std::to_string(t)+
                        ".size()+";
                returnString.pop_back();
                returnString += ");\n";
                
                returnString += offset(2)+viewName[viewID]+".insert("+viewName[viewID]+
                    ".end(), "+viewName[viewID]+"_0.begin(), "+
                    viewName[viewID]+"_0.end());\n";

                for (size_t t = 1; t < _threadsPerGroup[group_id]; ++t)
                {
                    returnString += offset(2)+"for (const std::pair<"+
                        viewName[viewID]+"_key,size_t>& key : "+viewName[viewID]+
                        "_"+std::to_string(t)+"_map)\n"+offset(2)+"{\n"+
                        offset(3)+"auto it = "+viewName[viewID]+"_0"
                        "_map.find(key.first);\n"+
                        offset(3)+"if (it != "+viewName[viewID]+"_0_map.end())\n"+
                        offset(3)+"{\n";

                    returnString += offset(4)+"for (size_t agg = 0; agg < "+
                        std::to_string(_qc->getView(viewID)->_aggregates.size())+
                        "; ++agg)\n"+
                        offset(5)+viewName[viewID]+"[it->second].aggregates[agg] += "+
                        viewName[viewID]+"_"+std::to_string(t)+
                        "[key.second].aggregates[agg];\n";
                        
                        // returnString+=offset(4)+viewName[viewID]+"[it->second].agg_"+
                        //    std::to_string(viewID)+"_"+std::to_string(agg)+" += "+
                        //    viewName[viewID]+"_"+std::to_string(t)+"[key.second].agg_"+
                        // //     std::to_string(viewID)+"_"+std::to_string(agg)+";\n";
                        // returnString += offset(4)+viewName[viewID]+"[it->second].
                        //     aggregates["+std::to_string(agg)+"] += "+
                        //     viewName[viewID]+"_"+std::to_string(t)+"[key.second].
                        //     aggregates["+std::to_string(agg)+"];\n";
                    
                    returnString += offset(3)+"}\n"+offset(3)+"else\n"+offset(3)+"{\n"+
                        offset(4)+viewName[viewID]+
                        "_0_map[key.first] = "+viewName[viewID]+
                        ".size();\n"+offset(4)+viewName[viewID]+".push_back("+
                        viewName[viewID]+"_"+std::to_string(t)+"[key.second]);\n"+
                        offset(3)+"}\n"+ offset(2)+"}\n";
                }
            }
            else
            {
                View* view = _qc->getView(viewID);

                if (view->_fVars.any())
                {
                
                    returnString += offset(2)+viewName[viewID]+".reserve(";
                    for (size_t t = 0; t < _threadsPerGroup[group_id]; ++t)
                        returnString += viewName[viewID]+"_"+std::to_string(t)+
                            ".size()+";
                    returnString.pop_back();
                    returnString += ");\n";

                    returnString += offset(2)+viewName[viewID]+".insert("+
                        viewName[viewID]+".end(), "+viewName[viewID]+"_0.begin(), "+
                        viewName[viewID]+"_0.end());\n";

                    for (size_t t = 1; t < _threadsPerGroup[group_id]; ++t)
                    {
                        returnString += offset(2)+"if(";

                        for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
                        {
                            if (view->_fVars[var])
                            {
                                Attribute* attr = _td-> getAttribute(var);
                                returnString += viewName[viewID]+".back()."+
                                    attr->_name+"=="+viewName[viewID]+"_"+
                                    std::to_string(t)+"[0]."+attr->_name+"&&";
                            }
                        }
                        returnString.pop_back();
                        returnString.pop_back();
                        returnString += ")\n"+offset(2)+"{\n";

                        returnString += offset(3)+"for (size_t agg = 0; agg < "+
                            std::to_string(view->_aggregates.size())+"; ++agg)\n"+
                            offset(4)+viewName[viewID]+".back().aggregates[agg] += "+
                            viewName[viewID]+"_"+std::to_string(t)+
                            "[0].aggregates[agg];\n";
                        
                        // for (size_t agg = 0; agg < view->_aggregates.size(); ++agg)
                        // {    
                        //     returnString += offset(3)+viewName[viewID]+".back().agg_"+
                        //         std::to_string(viewID)+"_"+std::to_string(agg)+" += "+
                        //         viewName[viewID]+"_"+std::to_string(t)+"[0].agg_"+
                        //         std::to_string(viewID)+"_"+std::to_string(agg)+";\n";
                        // }
                        
                        returnString += offset(3)+viewName[viewID]+".insert("+
                            viewName[viewID]+".end(), "+viewName[viewID]+"_"+
                            std::to_string(t)+".begin()+1, "+
                            viewName[viewID]+"_"+std::to_string(t)+".end());\n"+
                            offset(2)+"}\n";
                    
                        returnString += offset(2)+"else\n"+offset(2)+"{\n"+
                            offset(3)+viewName[viewID]+".insert("+
                            viewName[viewID]+".end(), "+viewName[viewID]+"_"+
                            std::to_string(t)+".begin(), "+
                            viewName[viewID]+"_"+std::to_string(t)+".end());\n"+
                            offset(2)+"}\n";
                    }
                }
                else
                {
                    returnString += offset(2)+viewName[viewID]+".insert("+
                        viewName[viewID]+".end(), "+viewName[viewID]+"_0.begin(), "+
                        viewName[viewID]+"_0.end());\n";

                    returnString += offset(2)+"for (size_t agg = 0; agg < "+
                        std::to_string(view->_aggregates.size())+"; ++agg)\n"+
                        offset(3)+viewName[viewID]+"[0].aggregates[agg] += ";
                        
                        for (size_t t = 1; t < _threadsPerGroup[group_id]; ++t)
                        {
                            returnString += viewName[viewID]+"_"+std::to_string(t)+
                                "[0].aggregates[agg]+";
                        }
                        returnString.pop_back();
                        returnString += ";\n";
                }
            }
        }

        for (const size_t& viewID : viewGroups[group_id])
        {
            if (_requireHashing[viewID])
            {
                View* view = _qc->getView(viewID);
            
#if defined(__GNUC__) && defined(NDEBUG) && !defined(__clang__)
                std::string sortAlgo = "__gnu_parallel::sort(";
#else
                std::string sortAlgo = "std::sort(";
#endif
                returnString += offset(2)+sortAlgo+viewName[viewID]+".begin(),"+
                    viewName[viewID]+".end(),[ ](const "+viewName[viewID]+
                    "_tuple& lhs, const "+viewName[viewID]+"_tuple& rhs)\n"+
                    offset(2)+"{\n";

                var_bitset intersection = view->_fVars &
                    _td->getRelation(view->_destination)->_bag;
                
                for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
                {
                    if (intersection[var])
                    {   
                        const std::string& attrName = _td->getAttribute(var)->_name;
                    
                        returnString += offset(3)+
                            "if(lhs."+attrName+" != rhs."+attrName+")\n"+offset(4)+
                            "return lhs."+attrName+" < rhs."+attrName+";\n";
                    }
                }
                var_bitset additional = view->_fVars & ~intersection;
                for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
                {
                    if (additional[var])
                    {   
                        const std::string& attrName = _td->getAttribute(var)->_name;
                    
                        returnString += offset(3)+
                            "if(lhs."+attrName+" != rhs."+attrName+")\n"+offset(4)+
                            "return lhs."+attrName+" < rhs."+attrName+";\n";
                    }
                }

                // for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
                // {
                //     if (view->_fVars[var])
                //     {   
                //         const std::string& attrName = _td->getAttribute(var)->_name;
                //         returnString += offset(3)+
                //             "if(lhs."+attrName+" != rhs."+attrName+")\n"+offset(4)+
                //             "return lhs."+attrName+" < rhs."+attrName+";\n";
                //     }
                // }
                returnString += offset(3)+"return false;\n"+offset(2)+"});\n";
            }
        }
    }

    // We now declare the register Arrrays!
    if (aggregateCounter > 0)
        headerString += offset(2)+"double aggregateRegister["+
            std::to_string(aggregateCounter)+"] = {};\n";    
    if (productCounter > 0)
        headerString += offset(2)+"double localRegister["+
            std::to_string(productCounter)+"] = {};\n";
    if (viewCounter > 0)
        headerString += offset(2)+"double viewRegister["+
            std::to_string(viewCounter)+"] = {};\n";
    if (postCounter > 0)
        headerString += offset(2)+"double postRegister["+
            std::to_string(postCounter)+"] = {};\n";
    
    return headerString + returnString + offset(1)+"}\n";
}



void CppGenerator::createRelationSortingOrder(TDNode* node, const size_t& parent_id)
{
    // get intersection with parent Node node
    for (size_t neigh = 0; neigh < node->_numOfNeighbors; ++neigh)
    {
        if (node->_neighbors[neigh] != parent_id)
        {
            TDNode* neighbor = _td->getRelation(node->_neighbors[neigh]);
            const size_t& neighID = neighbor->_id;
            
            var_bitset interaction = node->_bag & neighbor->_bag;
            
            size_t orderIdx = 0;
            for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
            {
                if (interaction[var])
                {
                    // add to sort order
                    sortOrders[neighID][orderIdx] = var;
                    ++orderIdx;
                }
            }

            var_bitset additionalVars =  ~node->_bag & neighbor->_bag;
            for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
            {
                if (additionalVars[var])
                {
                    // add to sort order
                    sortOrders[neighID][orderIdx] = var;
                    ++orderIdx;
                }
            } 
            createRelationSortingOrder(neighbor, node->_id);
        }
    }
}


void CppGenerator::computeViewLevel(size_t group_id, const TDNode& node)
{
    const std::vector<size_t>& varOrder = groupVariableOrder[group_id];
    const var_bitset& varOrderBitset = groupVariableOrderBitset[group_id];

    coveredVariables.clear();
    coveredVariables.resize(varOrder.size());
    
    addableViews.reset();
    addableViews.resize(varOrder.size() * (_qc->numberOfViews()+1));

    for (const size_t& viewID : viewGroups[group_id])
    {
        View* view = _qc->getView(viewID);
        bool setViewLevel = false;
        
        // if this view has no free variables it should be outputted before the join 
        if (view->_fVars.none())
        {
            viewLevelRegister[viewID] = varOrder.size();
            setViewLevel = true;
        }
        
        var_bitset varsToCover = view->_fVars;

        var_bitset dependentViewVars;
        for (size_t var=0; var<NUM_OF_VARIABLES;++var)
        {
            if (view->_fVars[var])
                dependentViewVars |= variableDependency[var];
        }
        dependentViewVars &= ~varOrderBitset;

        // TODO: (consider) should we check for overlap for bag vars that are
        // not in varOrder?
        var_bitset nonJoinVars = view->_fVars & ~varOrderBitset & ~node._bag;
        
        if (nonJoinVars.any())
        {
            //  if this is the case then look for overlapping functions
            for (size_t aggNo = 0; aggNo < view->_aggregates.size(); ++aggNo)
            {
                Aggregate* aggregate = view->_aggregates[aggNo];

                for (size_t i = 0; i < aggregate->_agg.size(); ++i)
                {
                    const prod_bitset& product = aggregate->_agg[i];
                    prod_bitset considered;
                        
                    for (size_t f = 0; f < NUM_OF_FUNCTIONS; ++f)
                    {
                        if (product.test(f) && !considered[f])
                        {
                            Function* function = _qc->getFunction(f);
                                
                            // check if this function is covered
                            if ((function->_fVars & dependentViewVars).any())
                            {
                                var_bitset dependentFunctionVars;
                                for (size_t var=0; var<NUM_OF_VARIABLES;++var)
                                {
                                    if (function->_fVars[var])
                                        dependentFunctionVars |= variableDependency[var];
                                }
                                dependentFunctionVars &= ~varOrderBitset;

                                // if it is - find functions that overlap
                                bool overlaps;
                                var_bitset overlapVars = function->_fVars;
                                prod_bitset overlapFunc;
                                overlapFunc.set(f);

                                do
                                {
                                    overlaps = false;
                                    overlapVars |= function->_fVars;

                                    for (size_t f2 = 0; f2 < NUM_OF_FUNCTIONS; ++f2)
                                    {
                                        if (product[f2] && !considered[f2] && !overlapFunc[f2])
                                        {
                                            Function* otherFunction = _qc->getFunction(f);
                                            // check if functions overlap
                                            if ((otherFunction->_fVars & dependentFunctionVars).any())
                                            {
                                                considered.set(f2);
                                                overlapFunc.set(f2);
                                                overlaps = true;
                                                overlapVars |= otherFunction->_fVars;
                                            }
                                        }
                                    }
                                } while(overlaps);

                                varsToCover |= overlapVars;
                            }
                        }
                    }                        
                }    
            }
        }
        
        // check when the freeVars of the view are covered - this determines
        // when we need to add the declaration of aggregate array and push to
        // view vector !
        size_t depth = 0;
        var_bitset coveredVariableOrder;
 
        for (const size_t& var : varOrder)
        {
            coveredVariableOrder.set(var);
            coveredVariables[depth].set(var);
            
            var_bitset intersection = node._bag & varOrderBitset;
            if ((intersection & coveredVariableOrder) == intersection) 
                coveredVariables[depth] |= node._bag;

            for (const size_t& incViewID : incomingViews[viewID]) 
            {
                // check if view is covered !
                View* incView = _qc->getView(incViewID);
                
                var_bitset viewJoinVars = incView->_fVars & varOrderBitset;
                if ((viewJoinVars & coveredVariableOrder) == viewJoinVars) 
                {
                    addableViews.set(incViewID + (depth * (_qc->numberOfViews()+1)));
                    coveredVariables[depth] |= incView->_fVars;
                }
            }

            if (!setViewLevel && (varsToCover & coveredVariables[depth]) == varsToCover)
            {
                // this is the depth where we add the declaration of aggregate
                // array and push to the vector !
                viewLevelRegister[viewID] = depth;
                setViewLevel = true;
            }
            ++depth;
        }
    }
}


void CppGenerator::aggregateGenerator(size_t group_id, const TDNode& node)
{
    const std::vector<size_t>& varOrder = groupVariableOrder[group_id];
    const var_bitset& varOrderBitset = groupVariableOrderBitset[group_id];
    
    aggregateHeader.clear();

    // Compute the level of the views in this group 
    computeViewLevel(group_id,node);
    
    // TODO: TODO: TODO: Make sure the below is necessary 
    localProductList.clear();
    localProductMap.clear();
    contributingViewList.clear();
    
    viewProductList.clear();
    viewProductMap.clear();

    newAggregateRegister.clear();
    aggregateRegisterMap.clear();
    
    productToVariableMap.clear();
    productToVariableRegister.clear();

    postRegisterList.clear();
    postRegisterMap.clear();

    totalLoopFactors.clear();
    totalLoopFactorList.clear();
    
    productLoopID.clear();
    incViewLoopID.clear();

    aggregateComputation.clear();
    
    localProductList.resize(varOrder.size() * 2);
    localProductMap.resize(varOrder.size()* 2);
    contributingViewList.resize(varOrder.size() * 2);
    
    viewProductList.resize(varOrder.size() * 2);
    viewProductMap.resize(varOrder.size() * 2);

    newAggregateRegister.resize(varOrder.size());
    aggregateRegisterMap.resize(varOrder.size());

    postRegisterList.resize(varOrder.size());
    postRegisterMap.resize(varOrder.size());

    totalLoopFactors.resize(varOrder.size() * 2);
    totalLoopFactorList.resize(varOrder.size());
    
    productLoopID.resize(varOrder.size() * 2);
    incViewLoopID.resize(varOrder.size() * 2);
    aggRegLoopID.resize(varOrder.size() * 2);

    aggregateRemapping.resize(newAggregateRegister.size());
    localProductRemapping.resize(localProductList.size());
    viewProductRemapping.resize(viewProductList.size());

    postRemapping.clear();
    postRemapping.resize(varOrder.size() + 1);

    productToVariableMap.resize(varOrder.size()*2);
    productToVariableRegister.resize(varOrder.size()*2);

    aggregateComputation.resize(_qc->numberOfViews());

    newAggregateRemapping.resize(varOrder.size());
    // localProductRemapping.resize(varOrder.size());
    // viewProductRemapping.resize(varOrder.size());
    // TODO: TODO: TODO: TODO: TODO: check if the above is correct 
    
    for (const size_t& viewID : viewGroups[group_id])
    {
        View* view = _qc->getView(viewID);


        size_t numberIncomingViews =
            (view->_origin == view->_destination ? node._numOfNeighbors :
             node._numOfNeighbors - 1);
        
        if (numberIncomingViews !=
            view->_aggregates[0]->_incoming.size() / view->_aggregates[0]->_agg.size())
        {
            ERROR("Why is this?! \n ");
            ERROR(view->_origin << " " << view->_destination << " " <<
                  node._numOfNeighbors << " " <<
                  view->_aggregates[0]->_incoming.size()  << "  " << 
                  view->_aggregates[0]->_agg.size()  << "\n");

            for (size_t aggNo = 0; aggNo < view->_aggregates.size(); ++aggNo)
            {
                size_t incCounter = 0;
                for (size_t inc = 0; inc <
                         view->_aggregates[aggNo]->_incoming.size(); ++inc)
                {
                    std::cout << view->_aggregates[aggNo]->_incoming[incCounter].first
                              << " " <<
                        view->_aggregates[aggNo]->_incoming[incCounter].second << " - ";
                    incCounter++;
                }
                std::cout << std::endl;
            }
            exit(1);
        }

        dyn_bitset loopFactors(_qc->numberOfViews()+1);

        // Compute the loops that are required by this view. 
        for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
        {
            if (view->_fVars[var])
            {
                if (!varOrderBitset[var])
                {
                    // if var is in relation - use relation 
                    if (node._bag[var])
                    {                                   
                        loopFactors.set(_qc->numberOfViews());
                    }
                    else // else find corresponding view 
                    {
                        for (const size_t& incViewID : incomingViews[viewID])
                        {
                            if (_qc->getView(incViewID)->_fVars[var])
                                loopFactors.set(incViewID);
                        }
                    }
                }
            }
        }

        outViewLoop[viewID] = loopFactors;
        
        // We add loop Factors for this view to the totalLoopFactors and keep
        // track of the id.
        // if (viewLevelRegister[viewID] != varOrder.size())
        // {            
        //     size_t depDepth = varOrder.size() + viewLevelRegister[viewID];
        //     auto loopit = totalLoopFactors[depDepth].find(loopFactors);
        //     if (loopit != totalLoopFactors[depDepth].end())
        //     {
        //         outViewLoopID[viewID] = loopit->second;
        //     }
        //     else
        //     {
        //         size_t loopID = totalLoopFactors[depDepth].size();
        //         outViewLoopID[viewID] = loopID;            
        //         totalLoopFactors[depDepth][loopFactors] = loopID;
        //     }
        // }
        
        for (size_t aggNo = 0; aggNo < view->_aggregates.size(); ++aggNo)
        {
            // We use a copy of the actual aggregate
            Aggregate aggregate = *(view->_aggregates[aggNo]);

            std::vector<std::pair<size_t, size_t>>
                localAggReg(aggregate._agg.size(),{varOrder.size(), 0});

            dependentComputation.clear();
            dependentComputation.resize(aggregate._agg.size());

            // computeAggregateRegister(&aggregate,group_id,viewID,aggNo,0,localAggReg);
            registerAggregatesToVariables
                (&aggregate,group_id,viewID,aggNo,0,localAggReg);
        }
    }

    // DINFO("Registered Aggregates to Variables \n"); 
}




void CppGenerator::registerAggregatesToVariables(
    Aggregate* aggregate, const size_t group_id,
    const size_t view_id, const size_t agg_id, const size_t depth,
    std::vector<std::pair<size_t, size_t>>& localAggReg)
{
    std::vector<std::pair<size_t,size_t>>& incoming = aggregate->_incoming;

    View* view = _qc->getView(view_id);
    TDNode* node = _td->getRelation(view->_origin);
    const var_bitset& relationBag = node->_bag;
    
    const std::vector<size_t>& varOrder = groupVariableOrder[group_id];
    const var_bitset& varOrderBitset = groupVariableOrderBitset[group_id];
    
    const size_t numLocalProducts = aggregate->_agg.size();
    const size_t numberIncomingViews =
        (view->_origin == view->_destination ? node->_numOfNeighbors :
         node->_numOfNeighbors - 1);
    
    dyn_bitset loopFactors(_qc->numberOfViews()+1);

    std::vector<std::pair<size_t, size_t>>
        localComputation(numLocalProducts, {varOrder.size(), 0});

    std::vector<std::pair<size_t,size_t>> viewReg;
    prod_bitset localFunctions, considered;

    // TODO: we computed this before - perhaps reuse?! 
    // Compute dependent variables on the variables in the head of view:
    var_bitset dependentVariables;
    var_bitset nonVarOrder = view->_fVars & ~varOrderBitset;

    for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
    {
        if (nonVarOrder[var])
        {
            // find bag that contains this variable
            if (relationBag[var])
                dependentVariables |= relationBag & ~varOrderBitset;
            else
            {
                // check incoming views
                for (const size_t& incViewID : incomingViews[view_id])
                {
                    View* incView = _qc->getView(incViewID);
                    if (incView->_fVars[var])
                        dependentVariables |= incView->_fVars & ~varOrderBitset;
                }
            }
        }
    }
    
    // go over each product and check if parts of it can be computed at current depth
    size_t incCounter = 0;
    for (size_t prodID = 0; prodID < numLocalProducts; ++prodID)
    {
        prod_bitset &product = aggregate->_agg[prodID];

        considered.reset();
        localFunctions.reset();
        
        for (size_t f = 0; f < NUM_OF_FUNCTIONS; ++f)
        {
            if (product.test(f) && !considered[f])
            {
                Function* function = _qc->getFunction(f);
                considered.set(f);
                
                // check if this function is covered
                if ((function->_fVars & coveredVariables[depth]) == function->_fVars)
                {   
                    // get all variables that this function depends on. 
                    var_bitset dependentFunctionVars;
                    for (size_t var=0; var<NUM_OF_VARIABLES;++var)
                    {
                        if (function->_fVars[var])
                            dependentFunctionVars |= variableDependency[var];
                    }
                    dependentFunctionVars &= ~varOrderBitset;

                    // Find all functions that overlap / depend on this function
                    bool overlaps, computeHere;
                    var_bitset overlapVars;
                    prod_bitset overlapFunc = prod_bitset().set(f);

                    do
                    {
                        overlaps = false;
                        computeHere = true;
                        overlapVars |= function->_fVars;

                        for (size_t f2 = 0; f2 < NUM_OF_FUNCTIONS; ++f2)
                        {
                            if (product[f2] && !considered[f2] && !overlapFunc[f2])
                            {
                                Function* otherFunction = _qc->getFunction(f);
                                const var_bitset& otherFVars = otherFunction->_fVars;
                                // check if functions depend on each other
                                // if ((otherFunction->_fVars & nonVarOrder).any()) 
                                if ((otherFVars & dependentFunctionVars).any())
                                {
                                    considered.set(f2);
                                    overlapFunc.set(f2);
                                    overlaps = true;
                                    overlapVars |= otherFunction->_fVars;
                                    // check if otherFunction is covered
                                    if ((otherFVars & coveredVariables[depth]) !=
                                        otherFVars)
                                    {
                                        // if it is not covered then compute entire
                                        // group at later stage
                                        computeHere = false;
                                    }
                                }
                            }
                        }
                    } while(overlaps && !computeHere);
                                    
                    if (computeHere)
                    {
                        // check if the variables are dependent on vars in head
                        // of outcoming view
                        if (((overlapVars & ~varOrderBitset) & dependentVariables).any())
                        {
                            if (viewLevelRegister[view_id] != depth)
                                ERROR("ERROR ERROR: in dependent computation:"
                                      <<" viewLevelRegister[viewID] != depth\n");
                            
                            dependentComputation[prodID].product |= overlapFunc;
                        }
                        else // add all functions that overlap together at this level!
                            localFunctions |= overlapFunc;

                        // remove overlapping functions from product
                        product &= ~overlapFunc;
                    }
                }
            }
        }
        
        // go over each incoming views and check if their incoming products can
        // be computed at this level. 
        viewReg.clear();

        for (size_t n = 0; n < numberIncomingViews; ++n)
        {
            size_t& incViewID = incoming[incCounter].first;

            if (addableViews[incViewID + (depth * (_qc->numberOfViews()+1))])
            {
                View* incView = _qc->getView(incViewID);

                // TODO: I think the below statement is fine but should we use
                // dependent variables instead?! - here we just check if any of
                // the vars from incView are present in the vars for view
                if (((incView->_fVars & ~varOrderBitset) & view->_fVars).none())
                {
                    viewReg.push_back(incoming[incCounter]);

                    /************** PRINT OUT ****************/
                    // if (group_id == 5 &&  incoming[incCounter].first == 2 &&
                    //     incoming[incCounter].second == 125)
                    // {
                    //     std::cout << "##--------" <<
                    //         incoming[incCounter].first << " - " <<
                    //         incoming[incCounter].second << std::endl;
                    // }
                    /************** PRINT OUT ****************/
                }
                else
                {
                    // if (depth == viewLevelRegister[view_id])
                    // {
                    // std::cout << "This view has an additional variable: "+
                    //     viewName[incViewID]+"\nviewAgg: aggs_"+viewName[incViewID]
                    //     +"-"+" viewLevelReg: "<<viewLevelRegister[view_id]<<
                    //     " depth: " << depth << std::endl;
                    dependentComputation[prodID].view.push_back(
                        incoming[incCounter]);
                    // }
                    // if (depth != viewLevelRegister[view_id])
                    // {
                    //     ERROR(depth << "  " << incViewID << "  " << view_id << " "
                    //           << viewLevelRegister[view_id] << " "+node->_name+" ");
                    //     ERROR(view->_fVars << std::endl);
                    //     ERROR("IS THIS A MISTAKE?!\n");
                    // }       
                }

                // This incoming view will no longer be considered 
                incoming[incCounter].first = _qc->numberOfViews();
            }
            ++incCounter;
        }
        
        // Check if this product has already been computed, if not we add it
        // to the list
        if (localFunctions.any() || !viewReg.empty() ||
            (depth+1 == varOrder.size() &&
                (dependentVariables & relationBag & nonVarOrder).none()))
        {                
            ProductAggregate prodAgg;
            prodAgg.product = localFunctions;
            prodAgg.viewAggregate = viewReg;
            
            if (depth <= viewLevelRegister[view_id] &&
                viewLevelRegister[view_id] != varOrder.size())
            {
                prodAgg.previous = localAggReg[prodID];
            }
            else
                prodAgg.previous = {varOrder.size(), 0};

            if (depth+1 == varOrder.size() &&
                (dependentVariables & relationBag & nonVarOrder).none())
                prodAgg.multiplyByCount = true;
            else
                prodAgg.multiplyByCount = false;
            
            auto prod_it = productToVariableMap[depth].find(prodAgg);
            if (prod_it != productToVariableMap[depth].end())
            {
                // Then update the localAggReg for this product
                localComputation[prodID] = {depth,prod_it->second};                
            }
            else
            {
                size_t newProdID =  productToVariableRegister[depth].size();
                // If so, add it to the aggregate register
                productToVariableMap[depth][prodAgg] = newProdID;
                localComputation[prodID] = {depth,newProdID};
                productToVariableRegister[depth].push_back(prodAgg);                
            }
            
            localAggReg[prodID] = localComputation[prodID];

            /******** PRINT OUT **********/
            // dyn_bitset contribViews(_qc->numberOfViews()+1);
            // for (auto& p : viewReg)
            //     contribViews.set(p.first);
            
            // std::cout << "group_id " << group_id << " depth: " << depth << std::endl;
            // std::cout << genProductString(*node, contribViews, localFunctions)
            //           << std::endl;
            /******** PRINT OUT **********/
        }    
        // if we didn't set a new computation - check if this level is the view Level:
        else if (depth == viewLevelRegister[view_id])
        {
            // Add the previous aggregate to local computation
            localComputation[prodID] = localAggReg[prodID];
            // TODO: (CONSIDER) think about keeping this separate as above
        }
        else if (depth+1 == varOrder.size())
        {
            ERROR("WE HAVE A PRODUCT THAT DOES NOT OCCUR AT THE LOWEST LEVEL!?");
            exit(1);
        }
    }
    
    // recurse to next depth ! 
    if (depth != varOrder.size()-1)
    {
        registerAggregatesToVariables(
            aggregate,group_id,view_id,agg_id,depth+1,localAggReg);
    }
    else
    {
        // resetting aggRegisters !        
        for (size_t i = 0; i < numLocalProducts; ++i)
            localAggReg[i] = {varOrder.size(),0};
    }
    
    if (depth > viewLevelRegister[view_id] ||
        viewLevelRegister[view_id] == varOrder.size())
    {
        // now adding aggregates coming from below to running sum
        for (size_t prodID = 0; prodID < numLocalProducts; ++prodID)
        {
            if (localComputation[prodID].first < varOrder.size() ||
                localAggReg[prodID].first < varOrder.size())
            {
                PostAggRegTuple postTuple;                
                postTuple.local = localComputation[prodID];
                postTuple.post = localAggReg[prodID];

                auto postit = postRegisterMap[depth].find(postTuple);
                if (postit != postRegisterMap[depth].end())
                {
                    localAggReg[prodID] = {depth,postit->second};
                }
                else
                {
                    size_t postProdID = postRegisterList[depth].size();
                    postRegisterMap[depth][postTuple] = postProdID;
                    localAggReg[prodID] = {depth, postProdID};
                    postRegisterList[depth].push_back(postTuple);
                }
            }
            else if (depth+1 != varOrder.size())
            {
                ERROR("ERROR ERROR ERROR - postAggregate is empty ");
                ERROR("we would expect it to contain something || ");
                ERROR(depth << " : " << varOrder.size()<< "  " << prodID << "\n");
                exit(1);
            }
        }
    }
    
    if (depth == viewLevelRegister[view_id] || 
        (depth == 0 && viewLevelRegister[view_id] == varOrder.size()))
    {
        // We now add both components to the respecitve registers
        std::pair<bool,size_t> regTupleProduct;
        std::pair<bool,size_t> regTupleView;

        // now adding aggregates to final computation 
        for (size_t prodID = 0; prodID < numLocalProducts; ++prodID)
        {
            // TODO: Why not avoid this loop and put the vectors into the
            // AggregateTuple
            // This is then one aggregateTuple for one specific Aggregate for
            // this view
            // THen add this aggregate to the specific view and not to depth
            
            const prod_bitset&  dependentFunctions =
                dependentComputation[prodID].product;
            const std::vector<std::pair<size_t,size_t>>& dependentViewReg =
                dependentComputation[prodID].view;

            ProductAggregate prodAgg;
            prodAgg.product = dependentFunctions;
            prodAgg.viewAggregate = dependentViewReg;
            prodAgg.previous = {varOrder.size(), 0};
            
            AggregateTuple aggComputation;
            aggComputation.viewID = view_id;
            aggComputation.aggID = agg_id;
            if (viewLevelRegister[view_id] != varOrder.size()) 
                aggComputation.local = localComputation[prodID];
            else
                 aggComputation.local = {varOrder.size(), 0};
            aggComputation.post = localAggReg[prodID];
            aggComputation.dependentComputation = dependentComputation[prodID];
            aggComputation.dependentProdAgg = prodAgg;

            if (dependentFunctions.any() || !dependentViewReg.empty())
                aggComputation.hasDependentComputation = true;
            
            // if (dependentFunctions.any() || !dependentViewReg.empty())
            // {                
            //     ProductAggregate prodAgg;
            //     prodAgg.product = dependentFunctions;
            //     prodAgg.viewAggregate = dependentViewReg;
            //     prodAgg.previous = {varOrder.size(), 0};
            //     auto prod_it = productToVariableMap[depDepth].find(prodAgg);
            //     if (prod_it != productToVariableMap[depDepth].end())
            //     {
            //         // Then update the localAggReg for this product
            //         aggComputation.dependentProduct = {depDepth,prod_it->second};
            //     }
            //     else
            //     {
            //         // If so, add it to the aggregate register
            //         size_t newProdID = productToVariableRegister[depDepth].size();
            //         productToVariableMap[depDepth][prodAgg] = newProdID;
            //         aggComputation.dependentProduct = {depDepth,newProdID};
            //         productToVariableRegister[depDepth].push_back(prodAgg);
            //     }
            // }
            // else
            // {
            //     aggComputation.dependentProduct = {varOrder.size(), 0};
            // }
            
            aggregateComputation[view_id].push_back(aggComputation);
        }
    }
}















prod_bitset CppGenerator::computeLoopMasks(
    prod_bitset presentFunctions, dyn_bitset consideredLoops,
    const var_bitset& varOrderBitset, const var_bitset& relationBag,
    const dyn_bitset& contributingViews, dyn_bitset& nextVariable,
    const dyn_bitset& prefixLoops)
{
    dyn_bitset loopFactors(_qc->numberOfViews()+1);
    dyn_bitset nextLoops(_qc->numberOfViews()+1);
    
    prod_bitset loopFunctionMask;
    
    // then find all loops required to compute these functions and create masks
    for (size_t f = 0; f < NUM_OF_FUNCTIONS; f++)
    {
        if (presentFunctions[f])
        {
            const var_bitset& functionVars = _qc->getFunction(f)->_fVars;
            loopFactors.reset();
            
            for (size_t v = 0; v < NUM_OF_VARIABLES; ++v)
            {
                if (functionVars[v] && !varOrderBitset[v])
                {
                    if (relationBag[v])
                        loopFactors.set(_qc->numberOfViews());
                    else
                    {
                        for (size_t incID = 0; incID < _qc->numberOfViews(); ++incID)
                        {
                            if (contributingViews[incID])
                            {
                                const var_bitset& incViewBitset =
                                    _qc->getView(incID)->_fVars;

                                if (incViewBitset[v])
                                {
                                    loopFactors.set(incID);
                                    break;
                                }
                            }
                        }
                    }
                }
            }

            loopFactors &= ~prefixLoops;
            
            if (loopFactors == consideredLoops)
            {
                // Add this function to the mask for this loop
                loopFunctionMask.set(f);
            }
            else
            {
                if ((consideredLoops & loopFactors) == consideredLoops)
                {
                    dyn_bitset remainingLoops = loopFactors & ~consideredLoops;
                    if (remainingLoops.any())
                    {
                        size_t highestPosition = 0;
                        // need to continue with this the next loop
                        for (size_t view=_qc->numberOfViews(); view > 0; view--)
                        {
                            if (consideredLoops[view])
                            {
                                highestPosition = view;
                                break;
                            }
                        }

                        size_t next = remainingLoops.find_next(highestPosition);
                        if (next != boost::dynamic_bitset<>::npos)
                            nextLoops.set(next);
                        else
                        {
                            ERROR("We found a remaining Loop which is not higher " <<
                                  " than the highest considered loop?!\n");
                            exit(1);
                        }
                    }
                }
            }
        }
    }

    // Then use the registrar to split the functions to their loops
    
    size_t numOfLoops = listOfLoops.size();
    listOfLoops.push_back(consideredLoops);
    functionsPerLoop.push_back(loopFunctionMask);
    functionsPerLoopBranch.push_back(loopFunctionMask);
    viewsPerLoop.push_back(nextVariable);
    nextLoopsPerLoop.push_back(std::vector<size_t>());
        
    std::vector<size_t>& nextLoopIDs = nextLoopsPerLoop[numOfLoops];

    presentFunctions &= ~loopFunctionMask;

    // Recurse to the next loops
    for (size_t viewID=0; viewID < _qc->numberOfViews() + 1; ++viewID)
    {
        if (nextLoops[viewID])
        {
            dyn_bitset nextConsideredLoops = consideredLoops;
            nextConsideredLoops.set(viewID);

            nextVariable.reset();
            nextVariable.set(viewID);
            
            nextLoopIDs.push_back(listOfLoops.size());
            
            prod_bitset nextLoopFunctions = computeLoopMasks(
                presentFunctions,nextConsideredLoops,varOrderBitset,
                relationBag,contributingViews,nextVariable, prefixLoops);
            
            functionsPerLoopBranch[numOfLoops] |= nextLoopFunctions;
        }
    }
    
    return functionsPerLoopBranch[numOfLoops];
}


void CppGenerator::registerAggregatesToLoops(size_t depth, size_t group_id)
{
    const var_bitset& varOrderBitset = groupVariableOrderBitset[group_id];
    const size_t maxDepth = groupVariableOrder[group_id].size();
    const var_bitset& relationBag =
        _td->getRelation(_qc->getView(viewGroups[group_id][0])->_origin)->_bag;

    listOfLoops.clear();
    functionsPerLoop.clear();
    functionsPerLoopBranch.clear();
    viewsPerLoop.clear();
    nextLoopsPerLoop.clear();

    // go through the ProductAggregates and find functions computed here 
    prod_bitset presentFunctions;    
    dyn_bitset contributingViews(_qc->numberOfViews()+1);
    for (const ProductAggregate& prodAgg : productToVariableRegister[depth])
    {
        presentFunctions |= prodAgg.product;
        for (const std::pair<size_t,size_t>& p : prodAgg.viewAggregate)
            contributingViews.set(p.first);
    }

    /*************** PRINT OUT **************/
    // const TDNode& node =
    //     *_td->getRelation(_qc->getView(viewGroups[group_id][0])->_origin);
    // std::cout << genProductString(node,contributingViews,presentFunctions)<<std::endl;
    /*************** PRINT OUT **************/
    
    dyn_bitset consideredLoops(_qc->numberOfViews()+1);
    dyn_bitset nextVariable(_qc->numberOfViews()+1);
    dyn_bitset prefixLoops(_qc->numberOfViews()+1);

    // compute the function mask for each loop 
    computeLoopMasks(presentFunctions, consideredLoops, varOrderBitset,
                     relationBag, contributingViews, nextVariable, prefixLoops);

    
    // The first register loop should all views that we do not iterate over! 
    viewsPerLoop[0] |= contributingViews;
    for (size_t j = 1; j < viewsPerLoop.size(); ++j)
        viewsPerLoop[0] &= ~viewsPerLoop[j];
    
    newAggregateRegister.clear();
    aggregateRegisterMap.clear();
    newAggregateRegister.resize(listOfLoops.size());
    aggregateRegisterMap.resize(listOfLoops.size());

    localProductMap.clear();
    localProductList.clear();
    localProductList.resize(listOfLoops.size());
    localProductMap.resize(listOfLoops.size());

    viewProductMap.clear();
    viewProductList.clear();
    viewProductList.resize(listOfLoops.size());
    viewProductMap.resize(listOfLoops.size());
    
    for(ProductAggregate& prodAgg : productToVariableRegister[depth])
    {    
        size_t currentLoop = 0;
        bool loopsOverRelation = false;
        prodAgg.correspondingLoopAgg =
            addProductToLoop(prodAgg, currentLoop, loopsOverRelation, maxDepth);
    }
}


std::pair<size_t,size_t> CppGenerator::addProductToLoop(
    ProductAggregate& prodAgg, size_t& currentLoop, bool& loopsOverRelation,
    const size_t& maxDepth)
{
    const size_t thisLoopID = currentLoop;
    const dyn_bitset& thisLoop = listOfLoops[currentLoop];

    dyn_bitset viewsForThisLoop = viewsPerLoop[currentLoop];
    loopsOverRelation = viewsForThisLoop[_qc->numberOfViews()];
    
    viewsForThisLoop.reset(_qc->numberOfViews());
 
    std::pair<bool,size_t> regTupleProduct = {false,0};  

    // check if this product requires computation over this loop loopmask
    prod_bitset loopFunctions = prodAgg.product & functionsPerLoop[currentLoop];
    if (loopFunctions.any())
    {
        // This should be add to the loop not depth 
        auto proditerator = localProductMap[currentLoop].find(loopFunctions);
        if (proditerator != localProductMap[currentLoop].end())
        {
            // set this local product to the id given by the map
            regTupleProduct = {true, proditerator->second};
        }
        else
        {
            size_t nextID = localProductList[currentLoop].size();
            localProductMap[currentLoop][loopFunctions] = nextID;
            regTupleProduct = {true, nextID};
            localProductList[currentLoop].push_back(loopFunctions);
        }
    }

    bool newViewProduct = false;
    bool singleViewAgg = false;
    
    std::pair<size_t, size_t> regTupleView = {2147483647,2147483647};

    if (!prodAgg.viewAggregate.empty())
    {
        if (viewsForThisLoop.count() > 1)
        {
            newViewProduct = true;
       
            std::vector<std::pair<size_t, size_t>> viewReg;

            size_t viewID = viewsForThisLoop.find_first();
            while (viewID != boost::dynamic_bitset<>::npos)
            {
                for (const std::pair<size_t, size_t>& viewAgg : prodAgg.viewAggregate)
                {
                    if (viewAgg.first == viewID)
                    {
                        viewReg.push_back(viewAgg);
                        break;
                    }
                }

                viewID = viewsPerLoop[currentLoop].find_next(viewID);
            }
            auto viewit = viewProductMap[currentLoop].find(viewReg);
            if (viewit != viewProductMap[currentLoop].end())
            {
                // set this local product to the id given by the map
                regTupleView = {currentLoop, viewit->second};            
            }
            else
            {
                size_t nextID =  viewProductList[currentLoop].size();
                viewProductMap[currentLoop][viewReg] = nextID;
                regTupleView = {currentLoop, nextID};
                viewProductList[currentLoop].push_back(viewReg);
            }
        }
        else if (viewsForThisLoop.count() == 1)
        {
            // Inside a loop there would only be a single view agg, so we could avoid
            // explicitly materializing the view Agg and simply multiply directly with
            // the aggregate from the view - this could be captured in the aggregate
            // computation that captures both the local aggregate and the view Agg
            singleViewAgg = true;
        
            // reuse the simple aggregate from the corresponding view
            size_t viewID = viewsForThisLoop.find_first();

            for (const std::pair<size_t, size_t>& viewAgg : prodAgg.viewAggregate)
            {
                if (viewAgg.first == viewID)
                {
                    regTupleView = viewAgg;
                    break;
                }
            }

            /*************** PRINT OUT **************/
            // if (regTupleView.first == 2)
            // {
            //     std::cout << "################## " <<
            //         regTupleView.first << "  " << regTupleView.second << std::endl;
            // }
            /*************** PRINT OUT **************/

        }
    }
    
    
    std::pair<size_t,size_t> previousComputation = {listOfLoops.size(),0};
    
    for (size_t next=0; next < nextLoopsPerLoop[thisLoopID].size(); ++next)
    {
        size_t nextLoop = nextLoopsPerLoop[thisLoopID][next];

        if (((listOfLoops[nextLoop] & thisLoop) != thisLoop))
        {
            ERROR("There is a problem with nextLoopsPerLoop\n");
            exit(1);
        }
        
        if ((functionsPerLoopBranch[nextLoop] & prodAgg.product).any())
        {
            std::pair<size_t, size_t> postComputation =
                addProductToLoop(prodAgg, nextLoop, loopsOverRelation, maxDepth);
            
            if (next + 1 != nextLoopsPerLoop[thisLoopID].size() &&
                (functionsPerLoopBranch[nextLoopsPerLoop[thisLoopID][next+1]]&
                 prodAgg.product).any())
            {
                // Add this product to this loop with the 'prev'Computation
                AggRegTuple postRegTuple;
                postRegTuple.postLoopAgg = true;
                postRegTuple.previous = previousComputation; 
                postRegTuple.postLoop = postComputation;
                
                // TODO: add the aggregate to the register!!
                auto regit = aggregateRegisterMap[nextLoop].find(postRegTuple);
                if (regit != aggregateRegisterMap[nextLoop].end())
                {
                    previousComputation = {nextLoop,regit->second};
                }
                else
                {
                    // If so, add it to the aggregate register
                    size_t newID = newAggregateRegister[nextLoop].size();
                    aggregateRegisterMap[nextLoop][postRegTuple] = newID;
                    newAggregateRegister[nextLoop].push_back(postRegTuple);
                    
                    previousComputation =  {nextLoop,newID};
                }
            }
            else
            {
                previousComputation = postComputation;
            }
            
        }
    }
    
    // while(currentLoop < listOfLoops.size()-1)
    // {
    //     currentLoop++;
    //     if (((listOfLoops[currentLoop] & thisLoop) == thisLoop))
    //     {
    //         if ((functionsPerLoopBranch[currentLoop] & prodAgg.product).any())
    //         {
    //             std::pair<size_t, size_t> postComputation =
    //                 addProductToLoop(prodAgg,currentLoop,loopsOverRelation,maxDepth);
    //             /**************** PRINT OUT ******************/
    //             // std::cout << "HERE \n";
    //             // std::cout << listOfLoops[currentLoop] <<"  "<<thisLoop<<std::endl;
    //             /**************** PRINT OUT ******************/
    //             // Add this product to this loop with the 'prev'Computation
    //             AggRegTuple postRegTuple;
    //             postRegTuple.postLoopAgg = true;
    //             postRegTuple.localAgg = previousComputation; // TODO:TODO:TODO:
    //             postRegTuple.postLoop = postComputation;
    //             // TODO: add the aggregate to the register!!
    //             auto regit = aggregateRegisterMap[currentLoop].find(postRegTuple);
    //             if (regit != aggregateRegisterMap[currentLoop].end())
    //                 previousComputation = {currentLoop,regit->second};
    //             else
    //             {
    //                 // If so, add it to the aggregate register
    //                 size_t newID = newAggregateRegister[currentLoop].size();
    //                 aggregateRegisterMap[currentLoop][postRegTuple] = newID;
    //                 newAggregateRegister[currentLoop].push_back(postRegTuple); 
    //                 previousComputation =  {currentLoop,newID};;
    //             }
    //         }
    //     }
    //     else break;
    // }

    // Check if this product has already been computed, if not we add it to the list
    AggRegTuple regTuple;
    regTuple.product = regTupleProduct;
    regTuple.viewAgg = regTupleView;

    /********* PRINT OUT ********/
    // if (regTupleView.first == 2)
    // {
    //     std::cout << "###########/>>>>>>>>> " <<
    //         regTuple.viewAgg.first << "  " << regTuple.viewAgg.second << std::endl;
    // }
    /********* PRINT OUT ********/
           
    regTuple.newViewProduct = newViewProduct;
    regTuple.singleViewAgg = singleViewAgg;
    regTuple.postLoop = previousComputation;

    regTuple.previous = {listOfLoops.size(),0};
    regTuple.multiplyByCount = false;
    
    // Multiply by previous #computation -- if needed
    if (thisLoop.none())
    {
        if (prodAgg.previous.first < maxDepth)
        {            
            const ProductAggregate& prevProdAgg =
                productToVariableRegister[prodAgg.previous.first]
                [prodAgg.previous.second];
            regTuple.previous = prevProdAgg.correspondingLoopAgg;
            regTuple.prevDepth = prodAgg.previous.first;
        }
        // Mutliply by count if needed
        if (!loopsOverRelation && prodAgg.multiplyByCount)
            regTuple.multiplyByCount = true;
    }

    auto regit = aggregateRegisterMap[thisLoopID].find(regTuple);
    if (regit != aggregateRegisterMap[thisLoopID].end())
    {
        previousComputation = {thisLoopID,regit->second};
    }
    else
    {
        // If so, add it to the aggregate register
        size_t newID = newAggregateRegister[thisLoopID].size();
        aggregateRegisterMap[thisLoopID][regTuple] = newID;
        newAggregateRegister[thisLoopID].push_back(regTuple);
        
        previousComputation =  {thisLoopID,newID};
    }

    return previousComputation;
}



// std::string CppGenerator::genAggLoopString(
//     const TDNode& node, const size_t currentLoop,size_t depth,
//     const dyn_bitset& contributingViews,const size_t numOfOutViewLoops)
// {
//     const size_t thisLoopID = currentLoop;
//     const dyn_bitset& thisLoop = listOfLoops[thisLoopID];
//     size_t numOfLoops = thisLoop.count() + numOfOutViewLoops;
    
//     std::string returnString = "";
//     std::string depthString = std::to_string(depth);
        
//     // Print all products for this considered loop
//     for (size_t prodID = 0; prodID < localProductList[thisLoopID].size(); ++prodID)
//     {
//         const prod_bitset& product = localProductList[thisLoopID][prodID];

//         bool decomposedProduct = false;
//         std::string productString = "";

//         // We now check if parts of the product have already been computed 
//         if (product.count() > 1)
//         {
//             prod_bitset subProduct = product;
//             prod_bitset removedFunctions;

//             size_t subProductID;

//             bool foundSubProduct = false;
//             while (!foundSubProduct && subProduct.count() > 1)
//             {
//                 size_t highestPosition = 0;
//                 // need to continue with this the next loop
//                 for (size_t f = NUM_OF_FUNCTIONS; f > 0; f--)
//                 {
//                     if (product[f])
//                     {
//                         highestPosition = f;
//                         break;
//                     }
//                 }

//                 subProduct.reset(highestPosition);
//                 removedFunctions.set(highestPosition);

//                 auto local_it = localProductMap[thisLoopID].find(subProduct);
//                 if (local_it != localProductMap[thisLoopID].end())
//                 {
//                     foundSubProduct = true;
//                     subProductID = local_it->second;
//                 }
//             }

//             if (foundSubProduct)
//             {
//                 std::string local = std::to_string(
//                     localProductRemapping[thisLoopID][subProductID]);

//                 productString += "localRegister["+local+"]*";
                    
//                 auto local_it = localProductMap[thisLoopID].find(removedFunctions);
//                 if (local_it != localProductMap[thisLoopID].end())
//                 {
//                     foundSubProduct = false;
//                     subProductID = local_it->second;

//                     local = std::to_string(
//                         localProductRemapping[thisLoopID][subProductID]);
//                     productString += "localRegister["+local+"]";
//                 }
//                 else
//                 {
//                     productString += genProductString(
//                         node,contributingViews,removedFunctions);
//                 }

//                 decomposedProduct = true;
//             }
//         }
        
//         // We turn the product into a string
//         if (!decomposedProduct)
//             productString += genProductString(node,contributingViews,product);

//         returnString += offset(3+depth+numOfLoops)+"localRegister["+
//             std::to_string(productCounter)+"] = "+productString + ";\n";
        
//         // increment the counter and update the remapping array
//         localProductRemapping[thisLoopID][prodID] = productCounter;
//         ++productCounter;
//     }

//     for (size_t viewProd = 0; viewProd < viewProductList[thisLoopID].size(); ++viewProd)
//     {
//         std::string viewProduct = "";
//         for (const std::pair<size_t, size_t>& viewAgg :
//                  viewProductList[thisLoopID][viewProd])
//         {
//             viewProduct += "aggregates_"+viewName[viewAgg.first]+"["+
//                 std::to_string(viewAgg.second)+"]*";
//         }   
//         viewProduct.pop_back();

//         returnString += offset(3+depth+numOfLoops)+
//             "viewRegister["+std::to_string(viewCounter)+"] = "+
//             viewProduct +";\n";
            
//         // Increment the counter and update the remapping array
//         viewProductRemapping[thisLoopID][viewProd] = viewCounter;
//         ++viewCounter;
//     }

    
//     // Recurse to the next loop(s) if possible
//     for (size_t next=0; next < nextLoopsPerLoop[thisLoopID].size(); ++next)
//     {
//         size_t nextLoop = nextLoopsPerLoop[thisLoopID][next];

//         if (((listOfLoops[nextLoop] & thisLoop) == thisLoop))
//         {
//             size_t viewID = viewsPerLoop[nextLoop].find_first();

//             std::string closeLoopString = "";
//             if (viewID < _qc->numberOfViews())
//             {
//                 returnString +=
//                     offset(3+depth+numOfLoops)+"ptr_"+viewName[viewID]+
//                     " = lowerptr_"+viewName[viewID]+"["+depthString+"];\n"+
//                     offset(3+depth+numOfLoops)+"while(ptr_"+viewName[viewID]+
//                     " <= upperptr_"+viewName[viewID]+"["+depthString+"])\n"+
//                     offset(3+depth+numOfLoops)+"{\n"+
//                     offset(4+depth+numOfLoops)+viewName[viewID]+
//                     "_tuple &"+viewName[viewID]+"Tuple = "+viewName[viewID]+
//                     "[ptr_"+viewName[viewID]+"];\n"+
//                     offset(4+depth+numOfLoops)+"double *aggregates_"+viewName[viewID]+
//                     " = "+viewName[viewID]+"Tuple.aggregates;\n";
                
//                 closeLoopString += offset(4+depth+numOfLoops)+"++ptr_"+viewName[viewID]+
//                     ";\n"+offset(3+depth+numOfLoops)+"}\n"+closeLoopString;
//             }
//             else 
//             {
//                 returnString +=
//                     offset(3+depth+numOfLoops)+"ptr_"+node._name+
//                     " = lowerptr_"+node._name+"["+depthString+"];\n"+
//                     offset(3+depth+numOfLoops)+"while(ptr_"+node._name+
//                     " <= upperptr_"+node._name+"["+depthString+"])\n"+
//                     offset(3+depth+numOfLoops)+"{\n"+
//                     offset(4+depth+numOfLoops)+node._name +
//                     "_tuple &"+node._name+"Tuple = "+node._name+
//                     "[ptr_"+node._name+"];\n";
                
//                 closeLoopString = offset(4+depth+numOfLoops)+"++ptr_"+node._name+";\n"+
//                     offset(3+depth+numOfLoops)+"}\n"+closeLoopString;
//             }

//             returnString += genAggLoopString(
//                 node, nextLoop, depth, contributingViews, 0);
            
//             returnString += closeLoopString;

//             // After recurse, add the aggregates that have been added with post FLAG
//             for (size_t aggID = 0; aggID <
//                      newAggregateRegister[nextLoop].size(); ++aggID)
//             {
//                 const AggRegTuple& regTuple = newAggregateRegister[nextLoop][aggID];
        
//                 if (regTuple.postLoopAgg)
//                 {
//                     // We use a mapping to the correct index
//                     returnString += offset(3+depth+numOfLoops)+
//                         "aggregateRegister["+std::to_string(aggregateCounter)+"] += ";

//                     if (regTuple.previous.first < listOfLoops.size())
//                     {
//                         std::string local = std::to_string(
//                             newAggregateRemapping[depth]
//                             [regTuple.previous.first]
//                             [regTuple.previous.second]);
//                         returnString += "aggregateRegister["+local+"]*";
//                     }

//                     if (regTuple.postLoop.first < listOfLoops.size())
//                     {
//                         std::string post = std::to_string(
//                             newAggregateRemapping[depth][regTuple.postLoop.first]
//                             [regTuple.postLoop.second]); 
//                         returnString += "aggregateRegister["+post+"]*";
//                     }
                        
//                     returnString.pop_back();
//                     returnString += ";\n";
            
//                     // Increment the counter and update the remapping array
//                     newAggregateRemapping[depth][nextLoop][aggID] = aggregateCounter;
//                     ++aggregateCounter;
//                 }
//             }
//         }
//         else break;
//     }

//     for (size_t aggID = 0; aggID < newAggregateRegister[thisLoopID].size(); ++aggID)
//     {
//         const AggRegTuple& regTuple = newAggregateRegister[thisLoopID][aggID];
        
//         if (!regTuple.postLoopAgg)
//         {
//             // We use a mapping to the correct index
//             returnString += offset(3+depth+numOfLoops)+
//                 "aggregateRegister["+std::to_string(aggregateCounter)+"] += ";

//             if (regTuple.previous.first < listOfLoops.size())
//             {
//                 std::string prev = std::to_string(
//                     newAggregateRemapping[regTuple.prevDepth]
//                     [regTuple.previous.first]
//                     [regTuple.previous.second]);
                
//                 returnString += "aggregateRegister["+prev+"]*";
//             }
            
//             if (regTuple.product.first)
//             {
//                 std::string local = std::to_string(
//                     localProductRemapping[thisLoopID][regTuple.product.second]
//                     );
//                 returnString += "localRegister["+local+"]*";
//             }
        
//             if (regTuple.newViewProduct)
//             {
//                 std::string view = std::to_string(
//                     viewProductRemapping[thisLoopID][regTuple.viewAgg.second]);
//                 returnString += "viewRegister["+view+"];";
//             }
//             else if (regTuple.singleViewAgg)
//             {
//                 returnString += "aggregates_"+viewName[regTuple.viewAgg.first]+"["+
//                     std::to_string(regTuple.viewAgg.second)+"]*";
//             }
            
//             if (regTuple.multiplyByCount)
//                 returnString +=  "count*";

//             if (regTuple.postLoop.first < listOfLoops.size())
//             {
//                 std::string postLoop = std::to_string(
//                     newAggregateRemapping[depth][regTuple.postLoop.first]
//                     [regTuple.postLoop.second]);
//                 returnString += "aggregateRegister["+postLoop+"]*";
//             }
            
//             returnString.pop_back();
//             returnString += ";\n";
            
//             // Increment the counter and update the remapping array
//             newAggregateRemapping[depth][thisLoopID][aggID] = aggregateCounter;
//             ++aggregateCounter;
//         }
//     }
    
//     return returnString;
// }



std::string CppGenerator::genDependentAggLoopString(
    const TDNode& node, const size_t currentLoop,size_t depth,
    const dyn_bitset& contributingViews, const size_t maxDepth,std::string& resetString)
{
    const size_t thisLoopID = currentLoop;

    const DependentLoop& depLoop = depListOfLoops[thisLoopID];
    const dyn_bitset& thisLoop = depLoop.loopFactors;

    // std::cout << "currentLoop: " << thisLoop << std::endl;
    
    size_t numOfLoops = thisLoop.count();

    std::string returnString = "";

    std::string depthString = std::to_string(depth);
        
    // Print all products for this considered loop
    for (size_t prodID = 0; prodID < localProductList[thisLoopID].size(); ++prodID)
    {
        const prod_bitset& product = localProductList[thisLoopID][prodID];

        // We turn the product into a string 
        std::string productString = genProductString(node,contributingViews,product);

        returnString += offset(3+depth+numOfLoops)+"localRegister["+
            std::to_string(productCounter)+"] = "+productString+";\n";
        
        // increment the counter and update the remapping array
        depLocalProductRemapping[thisLoopID][prodID] = productCounter;
        ++productCounter;
    }

    // for (size_t viewProd = 0; viewProd < viewProductList[thisLoopID].size();
    // ++viewProd)
    // {
    //     std::string viewProduct = "";
    //     for (const std::pair<size_t, size_t>& viewAgg :
    //              viewProductList[thisLoopID][viewProd])
    //     {
    //         viewProduct += "aggregates_"+viewName[viewAgg.first]+"["+
    //             std::to_string(viewAgg.second)+"]*";
    //     }   
    //     viewProduct.pop_back();
    //     returnString += offset(3+depth+numOfLoops)+
    //         "viewRegister["+std::to_string(viewCounter)+"] = "+
    //         viewProduct +";\n";
    //     // Increment the counter and update the remapping array
    //     viewProductRemapping[thisLoopID][viewProd] = viewCounter;
    //     ++viewCounter;
    // }
    
    for (size_t aggID = 0; aggID < newAggregateRegister[thisLoopID].size(); ++aggID)
    {
        const AggRegTuple& regTuple = newAggregateRegister[thisLoopID][aggID];
        
        if (regTuple.preLoopAgg)
        {
            // We use a mapping to the correct index
            returnString += offset(3+depth+numOfLoops)+
                "aggregateRegister["+std::to_string(aggregateCounter)+"] = ";

            if (regTuple.previous.first < depListOfLoops.size())
            {
                std::string prev = std::to_string(
                    depAggregateRemapping[regTuple.previous.first]
                    [regTuple.previous.second]);
                
                returnString += "aggregateRegister["+prev+"]*";
            }
            
            if (regTuple.product.first)
            {
                std::string local = std::to_string(
                    depLocalProductRemapping[thisLoopID][regTuple.product.second]
                    );
                returnString += "localRegister["+local+"]*";
            }
        
            if (regTuple.singleViewAgg)
            {
                returnString += "aggregates_"+viewName[regTuple.viewAgg.first]+"["+
                    std::to_string(regTuple.viewAgg.second)+"]*";
            }
            
            returnString.pop_back();
            returnString += ";\n";

            // Increment the counter and update the remapping array
            depAggregateRemapping[thisLoopID][aggID] = aggregateCounter;
            ++aggregateCounter;
        }
    }
    
    // Recurse to the next loop(s) if possible
    for (size_t next=0; next < depLoop.next.size(); ++next)
    {
        size_t nextLoop = depLoop.next[next];
        DependentLoop& nextDepLoop = depListOfLoops[nextLoop];
        
        if (((nextDepLoop.loopFactors & thisLoop) == thisLoop))
        {
            size_t viewID = nextDepLoop.loopVariable;

            std::string resetAggregates = "";
            std::string loopString = "", closeLoopString = "";
            if (viewID < _qc->numberOfViews())
            {
                loopString +=
                    offset(3+depth+numOfLoops)+"ptr_"+viewName[viewID]+
                    " = lowerptr_"+viewName[viewID]+"["+depthString+"];\n"+
                    offset(3+depth+numOfLoops)+"while(ptr_"+viewName[viewID]+
                    " <= upperptr_"+viewName[viewID]+"["+depthString+"])\n"+
                    offset(3+depth+numOfLoops)+"{\n"+
                    offset(4+depth+numOfLoops)+viewName[viewID]+
                    "_tuple &"+viewName[viewID]+"Tuple = "+viewName[viewID]+
                    "[ptr_"+viewName[viewID]+"];\n"+
                    offset(4+depth+numOfLoops)+"double *aggregates_"+viewName[viewID]+
                    " = "+viewName[viewID]+"Tuple.aggregates;\n";
                
                closeLoopString += offset(4+depth+numOfLoops)+"++ptr_"+viewName[viewID]+
                    ";\n"+offset(3+depth+numOfLoops)+"}\n"+closeLoopString;
            }
            else 
            {
                loopString +=
                    offset(3+depth+numOfLoops)+"ptr_"+node._name+
                    " = lowerptr_"+node._name+"["+depthString+"];\n"+
                    offset(3+depth+numOfLoops)+"while(ptr_"+node._name+
                    " <= upperptr_"+node._name+"["+depthString+"])\n"+
                    offset(3+depth+numOfLoops)+"{\n"+
                    offset(4+depth+numOfLoops)+node._name +
                    "_tuple &"+node._name+"Tuple = "+node._name+
                    "[ptr_"+node._name+"];\n";
                
                closeLoopString = offset(4+depth+numOfLoops)+"++ptr_"+node._name+";\n"+
                    offset(3+depth+numOfLoops)+"}\n"+closeLoopString;
            }

            // std::cout<< "Call genDependentAggLoopString from 1 \n";
            loopString += genDependentAggLoopString(
                node, nextLoop, depth, contributingViews, maxDepth, resetAggregates);
            
            returnString += resetAggregates+loopString+closeLoopString;

            // After recurse, add the aggregates that have been added with post FLAG
            for (size_t aggID = 0; aggID <
                     newAggregateRegister[nextLoop].size(); ++aggID)
            {
                const AggRegTuple& regTuple = newAggregateRegister[nextLoop][aggID];
        
                if (regTuple.postLoopAgg)
                {

                    // TODO: TODO:  Make sure this is correct -- Can we use
                    // equality here?

                    // We use a mapping to the correct index
                    returnString += offset(3+depth+numOfLoops)+
                        "aggregateRegister["+std::to_string(aggregateCounter)+"] = ";
                    
                    if (regTuple.previous.first < depListOfLoops.size())
                    {
                        std::string local = std::to_string(
                            newAggregateRemapping[depth]
                            [regTuple.previous.first]
                            [regTuple.previous.second]);
                        returnString += "aggregateRegister["+local+"]*";
                    }

                    if (regTuple.postLoop.first < depListOfLoops.size())
                    {
                        std::string post = std::to_string(
                            newAggregateRemapping[depth][regTuple.postLoop.first]
                            [regTuple.postLoop.second]); 
                        returnString += "aggregateRegister["+post+"]*";
                    }
                    
                    returnString.pop_back();
                    returnString += ";\n";
            
                    // Increment the counter and update the remapping array
                    depAggregateRemapping[nextLoop][aggID] = aggregateCounter;
                    ++aggregateCounter;
                }
            }
        }
        else break;
    }


    size_t startingOffset = aggregateCounter;

    for (size_t aggID = 0; aggID < newAggregateRegister[thisLoopID].size(); ++aggID)
    {
        const AggRegTuple& regTuple = newAggregateRegister[thisLoopID][aggID];
        
        if (!regTuple.postLoopAgg && !regTuple.preLoopAgg)
        {
            // We use a mapping to the correct index
            returnString += offset(3+depth+numOfLoops)+
                "aggregateRegister["+std::to_string(aggregateCounter)+"] += ";

            if (regTuple.previous.first < depListOfLoops.size())
            {
                std::string prev = std::to_string(
                    depAggregateRemapping[regTuple.previous.first]
                    [regTuple.previous.second]);
                
                returnString += "aggregateRegister["+prev+"]*";
            }
            
            if (regTuple.product.first)
            {
                std::string local = std::to_string(
                    depLocalProductRemapping[thisLoopID][regTuple.product.second]
                    );
                returnString += "localRegister["+local+"]*";
            }
        
            if (regTuple.singleViewAgg)
            {
                returnString += "aggregates_"+viewName[regTuple.viewAgg.first]+"["+
                    std::to_string(regTuple.viewAgg.second)+"]*";
            }
            
            if (regTuple.postLoop.first < depListOfLoops.size())
            {
                std::string postLoop = std::to_string(
                    newAggregateRemapping[depth][regTuple.postLoop.first]
                    [regTuple.postLoop.second]);
                returnString += "aggregateRegister["+postLoop+"]*";
            }
            
            returnString.pop_back();
            returnString += ";\n";
            
            // Increment the counter and update the remapping array
            depAggregateRemapping[thisLoopID][aggID] = aggregateCounter;
            ++aggregateCounter;
        }
    }

    if (aggregateCounter > startingOffset)
    {
        size_t resetOffset = (numOfLoops == 0 ? 3+depth : 2+depth+numOfLoops);
        resetString += offset(resetOffset)+"memset(&aggregateRegister["+
            std::to_string(startingOffset)+"], 0, sizeof(double) * "+
            std::to_string(aggregateCounter-startingOffset)+");\n";
    }
    
    size_t outViewID = depLoop.outView.find_first();
    while (outViewID != boost::dynamic_bitset<>::npos)
    {
        View* view = _qc->getView(outViewID);
        
        // Add the code that outputs tuples for the views           
        std::string viewVars = "";            
        for (size_t v = 0; v < NUM_OF_VARIABLES; ++v)
        {
            if (view->_fVars[v])
            {
                if (node._bag[v])
                {
                    viewVars += node._name+"Tuple."+
                        _td->getAttribute(v)->_name+",";
                }
                else
                {
                    for (const size_t& incViewID : incomingViews[outViewID])
                    {
                        View* view = _qc->getView(incViewID);
                        if (view->_fVars[v])
                        {
                            viewVars += viewName[incViewID]+"Tuple."+
                                _td->getAttribute(v)->_name+",";
                            break;
                        }
                    }
                }
            }
        }
        viewVars.pop_back();

        // size_t offDepth = numOfLoops+depth+3;
        size_t offDepth = (depth + 1 < maxDepth ?
                           4+depth+numOfLoops : 3+depth+numOfLoops);
        
        if (_requireHashing[outViewID])
        {
            returnString +=
                offset(offDepth)+viewName[outViewID]+"_pair = "+
                // viewName[outViewID]+"_map.insert(std::make_pair("+
                // viewName[outViewID]+"_key("+viewVars+"),"+
                // viewName[outViewID]+".size()));\n"+
                viewName[outViewID]+"_map.insert({{"+viewVars+"},"+
                viewName[outViewID]+".size()});\n"+
                // viewName[outViewID]+"_map.emplace(std::piecewise_construct,"
                // "std::forward_as_tuple("+viewVars+"),"+
                // "std::forward_as_tuple("+viewName[outViewID]+".size()));\n"+
                offset(offDepth)+"if ("+viewName[outViewID]+"_pair.second)\n"+
                offset(offDepth)+"{\n"+
                offset(offDepth+1)+viewName[outViewID]+
                ".emplace_back("+viewVars+");\n"+
                offset(offDepth)+"}\n"+
                offset(offDepth)+"aggregates_"+viewName[outViewID]+
                " = "+viewName[outViewID]+"["+viewName[outViewID]+
                "_pair.first->second].aggregates;\n";
                // "//-----------------------------\n"+
                // offset(offDepth)+viewName[outViewID]+"_iterator"+
                // " = "+viewName[outViewID]+"_map.find({"+viewVars+"});\n"+
                // offset(offDepth)+"if ("+viewName[outViewID]+"_iterator"
                // " != "+viewName[outViewID]+"_map.end())\n"+
                // offset(offDepth)+"{\n"+
                // offset(offDepth+1)+"aggregates_"+viewName[outViewID]+
                // " = "+viewName[outViewID]+"["+viewName[outViewID]+
                // "_iterator->second].aggregates;\n"+
                // offset(offDepth)+"}\n"+
                // offset(offDepth)+"else\n"+
                // offset(offDepth)+"{\n"+
                // offset(offDepth+1)+"size_t idx_"+viewName[outViewID]+
                // " = "+viewName[outViewID]+".size();\n"+
                // offset(offDepth+1)+viewName[outViewID]+
                // // "_map.emplace("+viewVars+",idx_"+viewName[outViewID]+");\n"+
                // "_map[{"+viewVars+"}] = idx_"+viewName[outViewID]+";\n"+
                // offset(offDepth+1)+viewName[outViewID]+
                // ".emplace_back("+viewVars+");\n"+
                // // ".push_back({"+viewVars+"});\n"+
                // offset(offDepth+1)+"aggregates_"+viewName[outViewID]+
                // " = "+viewName[outViewID]+"[idx_"+viewName[outViewID]+
                // "].aggregates;\n"+
                // offset(offDepth)+"}\n";
        }
        else
        {
            // just add string to push to view
            returnString += offset(offDepth)+viewName[outViewID]+
                ".emplace_back("+viewVars+");\n"+offset(offDepth)+
                // ".push_back({"+viewVars+"});\n"+offset(offDepth)+
                "aggregates_"+viewName[outViewID]+" = "+viewName[outViewID]+
                ".back().aggregates;\n";
        }

        if (COMPRESS_AGGREGATES)
        {
            AggregateIndexes bench, aggIdx;
            std::bitset<7> increasingIndexes;
        
            mapAggregateToIndexes(bench,aggregateComputation[outViewID][0],
                                  outViewID,maxDepth);
        
            size_t off = 0;
            for (size_t aggID = 1; aggID <
                     aggregateComputation[outViewID].size(); ++aggID)
            {
                const AggregateTuple& tuple = aggregateComputation[outViewID][aggID];

                aggIdx.reset();
                mapAggregateToIndexes(aggIdx,tuple,outViewID,maxDepth);

                // if (outViewID == 0)
                //     std::cout << aggID << " : " << increasingIndexes << std::endl;

                if (aggIdx.isIncrement(bench,off,increasingIndexes))
                {
                    ++off;
                }
                else
                {
                    // If not, output bench and all offset aggregates
                    returnString += outputFinalRegTupleString(
                        bench,off,increasingIndexes,offDepth);
                
                    // make aggregate the bench and set offset to 0
                    bench = aggIdx;
                    off = 0;
                    increasingIndexes.reset();
                }
            }

            // If not, output bench and all offset
            // aggregateString
            returnString += outputFinalRegTupleString(
                bench,off,increasingIndexes,offDepth);
        }
        else 
        {
            for(const AggregateTuple& aggTuple : aggregateComputation[outViewID])
            {
                returnString += offset(offDepth)+"aggregates_"+
                    viewName[outViewID]+"["+std::to_string(aggTuple.aggID)+
                    "] += ";

                std::string aggBody = "";
                if (aggTuple.local.first < maxDepth)
                {
                    const ProductAggregate& prodAgg =
                        productToVariableRegister[aggTuple.local.first]
                        [aggTuple.local.second];
        
                    const std::pair<size_t,size_t>& correspondingLoopAgg =
                        prodAgg.correspondingLoopAgg;
                    
                    std::string prev = std::to_string(
                        newAggregateRemapping[aggTuple.local.first]
                        [correspondingLoopAgg.first]
                        [correspondingLoopAgg.second]);
                
                    aggBody += "aggregateRegister["+prev+"]*";
                }
                if (aggTuple.post.first < maxDepth)
                {
                    std::string post = std::to_string(
                        postRemapping[aggTuple.post.first]
                        [aggTuple.post.second]);
                    aggBody += "postRegister["+post+"]*";
                }

                if (aggTuple.hasDependentComputation)
                {
                    const std::pair<size_t,size_t>& correspondingLoopAgg =
                        aggTuple.dependentProdAgg.correspondingLoopAgg;
                
                    std::string prev = std::to_string(
                        depAggregateRemapping[correspondingLoopAgg.first]
                        [correspondingLoopAgg.second]);

                    aggBody += "aggregateRegister["+prev+"]*";
                }
                // TODO:TODO:TODO: ---> make sure this is correct
                if (aggBody.empty())
                    aggBody = "1*";
                returnString += aggBody;
                returnString.pop_back();
                returnString += ";\n";            
            }
        }

        outViewID = depLoop.outView.find_next(outViewID);
    }
    
    return returnString;
}


std::string CppGenerator::outputAggRegTupleString(
    AggregateIndexes& idx, size_t numOfAggs, const std::bitset<7>& increasing,
    size_t stringOffset, bool postLoop)
{  
    std::string outString = "";
    
    if (numOfAggs > LOOPIFY_THRESHOLD)
    {
        outString += offset(stringOffset)+
            "for (size_t i = 0; i < "+std::to_string(numOfAggs+1)+";++i)\n";

        if (postLoop)
            outString += offset(stringOffset+1)+
                "aggregateRegister["+std::to_string(idx.indexes[7])+"+i] = ";
        else 
            outString += offset(stringOffset+1)+
                "aggregateRegister["+std::to_string(idx.indexes[7])+"+i] += ";

        if (idx.present[0])
        {
            std::string k = std::to_string(idx.indexes[0]);
            if (increasing[0])
                k += "+i";
            outString +="aggregateRegister["+k+"]*";
        }

        if (idx.present[1])
        {
            std::string k = std::to_string(idx.indexes[1]);
            if (increasing[1])
                k += "+i";
            outString +="localRegister["+k+"]*";
        }

        if (idx.present[2])
        {
            std::string k = std::to_string(idx.indexes[2]);
            if (increasing[2])
                k += "+i";
            outString +="viewRegister["+k+"]*";
        }
        if (idx.present[3])
        {
            // TODO: CHECK THAT index 4 is also presenth
            size_t viewID = idx.indexes[3];                
            if (increasing[3])
            {
                ERROR("VIEW ID SHOULD NEVER INCREASE!!! \n");
                exit(1);
            }
            
            std::string aggID = std::to_string(idx.indexes[4]);
            if (increasing[4])
                aggID += "+i";

            outString += "aggregates_"+viewName[viewID]+"["+aggID+"]*";
        }

        if (idx.present[5])
            outString +=  "count*";

        if (idx.present[6])
        {
            std::string k = std::to_string(idx.indexes[6]);            
            if (increasing[6])
                k += "+i";
            outString += "aggregateRegister["+k+"]*";
        }

        outString.pop_back();
        outString += ";\n";
    }
    else
    {
        // Print out all aggregates individually
        for (size_t i = 0; i < numOfAggs+1; ++i)
        {
            // We use a mapping to the correct index
            if (postLoop)
                outString += offset(stringOffset)+
                    "aggregateRegister["+std::to_string(idx.indexes[7]+i)+"] = ";
            else
                outString += offset(stringOffset)+
                    "aggregateRegister["+std::to_string(idx.indexes[7]+i)+"] += ";

            if (idx.present[0])
            {
                size_t k = idx.indexes[0];
                if (increasing[0])
                    k += i;
                outString +="aggregateRegister["+std::to_string(k)+"]*";
            }

            if (idx.present[1])
            {
                size_t k = idx.indexes[1];
                if (increasing[1])
                    k += i;
                outString +="localRegister["+std::to_string(k)+"]*";
            }

            if (idx.present[2])
            {
                size_t k = idx.indexes[2];
                if (increasing[2])
                    k += i;
                outString +="viewRegister["+std::to_string(k)+"]*";
            }
            if (idx.present[3])
            {
                // TODO: CHECK THAT index 4 is also presenth
                size_t viewID = idx.indexes[3];                
                if (increasing[3])
                {
                    ERROR("VIEW ID SHOULD NEVER INCREASE!!! \n");
                    exit(1);
                }

                size_t aggID = idx.indexes[4];
                if (increasing[4])
                    aggID += i;

                outString += "aggregates_"+viewName[viewID]+"["+
                    std::to_string(aggID)+"]*";
            }

            if (idx.present[5])
                outString +=  "count*";

            if (idx.present[6])
            {
                size_t k = idx.indexes[6];
                if (increasing[6])
                    k += i;
                outString += "aggregateRegister["+std::to_string(k)+"]*";
            }

            outString.pop_back();
            outString += ";\n";
        }
    }

    return outString;
}


std::string CppGenerator::outputPostRegTupleString(
    AggregateIndexes& idx, size_t numOfAggs, const std::bitset<7>& increasing,
    size_t stringOffset)
{
    std::string outString = "";
    
    if (numOfAggs > LOOPIFY_THRESHOLD)
    {
        outString += offset(stringOffset)+
            "for (size_t i = 0; i < "+std::to_string(numOfAggs+1)+";++i)\n";
        outString += offset(stringOffset+1)+
            "postRegister["+std::to_string(idx.indexes[2])+"+i] += ";
        
        if (idx.present[0])
        {
            std::string k = std::to_string(idx.indexes[0]);
            if (increasing[0])
                k += "+i";
            outString +="aggregateRegister["+k+"]*";
        }
        // else
        // {
        //     ERROR("WE EXPECT LOCAL IN POST REG TO BE SET!! \n");
        //     //  exit(1);
        // }

        if (idx.present[1])
        {
            std::string k = std::to_string(idx.indexes[1]);
            if (increasing[1])
                k += "+i";
            outString +="postRegister["+k+"]*";
        }
        
        outString.pop_back();
        outString += ";\n";
    }
    else
    {
        // Print out all aggregates individually
        for (size_t i = 0; i < numOfAggs+1; ++i)
        {
            // We use a mapping to the correct index
            outString += offset(stringOffset)+
                "postRegister["+std::to_string(idx.indexes[2]+i)+"] += ";
            
            if (idx.present[0])
            {
                size_t k = idx.indexes[0];
                if (increasing[0])
                    k += i;
                outString +="aggregateRegister["+std::to_string(k)+"]*";
            }
            else
            {
                ERROR("WE EXPECT LOCAL IN POST REG TO BE SET!! \n");
                // exit(1);
            }
            
            if (idx.present[1])
            {
                size_t k = idx.indexes[1];
                if (increasing[1])
                    k += i;
                outString +="postRegister["+std::to_string(k)+"]*";
            }
            
            outString.pop_back();
            outString += ";\n";
        }
    }

    return outString;
}


std::string CppGenerator::outputFinalRegTupleString(
    AggregateIndexes& idx, size_t numOfAggs, const std::bitset<7>& increasing,
    size_t stringOffset)
{
    std::string outString = "";
    
    if (numOfAggs > LOOPIFY_THRESHOLD)
    {
        outString += offset(stringOffset)+
            "for (size_t i = 0; i < "+std::to_string(numOfAggs+1)+";++i)\n";

        std::string aggIDString = std::to_string(idx.indexes[4]);
        if (increasing[4])
            aggIDString += "+i";
        
        outString += offset(stringOffset+1)+"aggregates_"+viewName[idx.indexes[3]]+
            "["+aggIDString+"] += ";

        std::string aggString = "";
        if (idx.present[0])
        {
            std::string k = std::to_string(idx.indexes[0]);
            if (increasing[0])
                k += "+i";
            aggString +="aggregateRegister["+k+"]*";
        }

        if (idx.present[1])
        {
            std::string k = std::to_string(idx.indexes[1]);
            if (increasing[1])
                k += "+i";
            aggString +="postRegister["+k+"]*";
        }

        if (idx.present[2])
        {
            std::string k = std::to_string(idx.indexes[2]);
            if (increasing[2])
                k += "+i";
            aggString +="aggregateRegister["+k+"]*";
        }

        if (aggString.empty())
        {
            // std::cout << "aggString is empty - compressed!\n";
            aggString = "1*";
        }
        else
            aggString.pop_back();
        
        outString += aggString;
        outString += ";\n";
    }
    else
    {
        // Print out all aggregates individually
        for (size_t i = 0; i < numOfAggs+1; ++i)
        {
            size_t aggID = idx.indexes[4];
            if (increasing[4])
                aggID += i;
        
            // We use a mapping to the correct index
            outString += offset(stringOffset)+"aggregates_"+viewName[idx.indexes[3]]+
                "["+std::to_string(aggID)+"] += ";

            std::string aggString = "";
            
            if (idx.present[0])
            {
                size_t k = idx.indexes[0];
                if (increasing[0])
                    k += i;
                aggString +="aggregateRegister["+std::to_string(k)+"]*";
            }
            
            if (idx.present[1])
            {
                size_t k = idx.indexes[1];
                if (increasing[1])
                    k += i;
                aggString +="postRegister["+std::to_string(k)+"]*";
            }

            if (idx.present[2])
            {
                size_t k = idx.indexes[2];
                if (increasing[2])
                    k += i;
                aggString +="aggregateRegister["+std::to_string(k)+"]*";
            }

            if (aggString.empty())
            {
                // std::cout << "aggString is empty --- non-compressed !\n";
                aggString = "1";
            }
            else
                aggString.pop_back();
        
            outString += aggString;
            outString += ";\n";
        }
    }

    return outString;
}


std::string CppGenerator::genAggLoopStringCompressed(
    const TDNode& node, const size_t currentLoop,size_t depth,
    const dyn_bitset& contributingViews,const size_t numOfOutViewLoops,
    std::string& resetString)
{
    const size_t thisLoopID = currentLoop;
    const dyn_bitset& thisLoop = listOfLoops[thisLoopID];
    size_t numOfLoops = thisLoop.count() + numOfOutViewLoops;
    
    std::string returnString = "";
    std::string depthString = std::to_string(depth);
        
    // Print all products for this considered loop
    for (size_t prodID = 0; prodID < localProductList[thisLoopID].size(); ++prodID)
    {
        const prod_bitset& product = localProductList[thisLoopID][prodID];
        
        // std::cout << product << std::endl;
        
        bool decomposedProduct = false;
        std::string productString = "";

        // We now check if parts of the product have already been computed 
        if (product.count() > 1)
        {
            prod_bitset subProduct = product;
            prod_bitset removedFunctions;

            size_t subProductID;

            bool foundSubProduct = false;
            while (!foundSubProduct && subProduct.count() > 1)
            {
                size_t highestPosition = 0;
                // need to continue with this the next loop
                for (size_t f = NUM_OF_FUNCTIONS; f > 0; f--)
                {
                    if (subProduct[f])
                    {
                        highestPosition = f;
                        break;
                    }
                }
                
                subProduct.reset(highestPosition);
                removedFunctions.set(highestPosition);

                auto local_it = localProductMap[thisLoopID].find(subProduct);
                if (local_it != localProductMap[thisLoopID].end() &&
                    local_it->second < prodID)
                {
                    foundSubProduct = true;
                    subProductID = local_it->second;
                }
            }
            

            if (foundSubProduct)
            {
                std::string local = std::to_string(
                    localProductRemapping[thisLoopID][subProductID]);

                productString += "localRegister["+local+"]*";
                    
                auto local_it = localProductMap[thisLoopID].find(removedFunctions);
                if (local_it != localProductMap[thisLoopID].end() &&
                    local_it->second < prodID)
                {
                    foundSubProduct = false;
                    subProductID = local_it->second;

                    local = std::to_string(
                        localProductRemapping[thisLoopID][subProductID]);
                    productString += "localRegister["+local+"]";
                }
                else
                {
                    productString += genProductString(
                        node,contributingViews,removedFunctions);
                }

                decomposedProduct = true;
            }
        }
        
        // We turn the product into a string
        if (!decomposedProduct)
            productString += genProductString(node,contributingViews,product);

        returnString += offset(3+depth+numOfLoops)+"localRegister["+
            std::to_string(productCounter)+"] = "+productString + ";\n";
        
        // increment the counter and update the remapping array
        localProductRemapping[thisLoopID][prodID] = productCounter;
        ++productCounter;
    }

    for (size_t viewProd = 0; viewProd < viewProductList[thisLoopID].size(); ++viewProd)
    {   
        std::string viewProduct = "";
        for (const std::pair<size_t, size_t>& viewAgg : viewProductList[thisLoopID][viewProd])
        {
            viewProduct += "aggregates_"+viewName[viewAgg.first]+"["+
                std::to_string(viewAgg.second)+"]*";
        }   
        viewProduct.pop_back();

        returnString += offset(3+depth+numOfLoops)+
            "viewRegister["+std::to_string(viewCounter)+"] = "+
            viewProduct +";\n";

        // Increment the counter and update the remapping array
        viewProductRemapping[thisLoopID][viewProd] = viewCounter;
        ++viewCounter;
    }

    
    // Recurse to the next loop(s) if possible
    for (size_t next=0; next < nextLoopsPerLoop[thisLoopID].size(); ++next)
    {
        size_t nextLoop = nextLoopsPerLoop[thisLoopID][next];

        if (((listOfLoops[nextLoop] & thisLoop) == thisLoop))
        {
            size_t viewID = viewsPerLoop[nextLoop].find_first();

            std::string loopString = "", closeLoopString = "";
            if (viewID < _qc->numberOfViews())
            {
                loopString +=
                    offset(3+depth+numOfLoops)+"ptr_"+viewName[viewID]+
                    " = lowerptr_"+viewName[viewID]+"["+depthString+"];\n"+
                    offset(3+depth+numOfLoops)+"while(ptr_"+viewName[viewID]+
                    " <= upperptr_"+viewName[viewID]+"["+depthString+"])\n"+
                    offset(3+depth+numOfLoops)+"{\n"+
                    offset(4+depth+numOfLoops)+viewName[viewID]+
                    "_tuple &"+viewName[viewID]+"Tuple = "+viewName[viewID]+
                    "[ptr_"+viewName[viewID]+"];\n"+
                    offset(4+depth+numOfLoops)+"double *aggregates_"+viewName[viewID]+
                    " = "+viewName[viewID]+"Tuple.aggregates;\n";
                
                closeLoopString += offset(4+depth+numOfLoops)+"++ptr_"+viewName[viewID]+
                    ";\n"+offset(3+depth+numOfLoops)+"}\n"+closeLoopString;
            }
            else 
            {
                loopString +=
                    offset(3+depth+numOfLoops)+"ptr_"+node._name+
                    " = lowerptr_"+node._name+"["+depthString+"];\n"+
                    offset(3+depth+numOfLoops)+"while(ptr_"+node._name+
                    " <= upperptr_"+node._name+"["+depthString+"])\n"+
                    offset(3+depth+numOfLoops)+"{\n"+
                    offset(4+depth+numOfLoops)+node._name +
                    "_tuple &"+node._name+"Tuple = "+node._name+
                    "[ptr_"+node._name+"];\n";
                
                closeLoopString = offset(4+depth+numOfLoops)+"++ptr_"+node._name+";\n"+
                    offset(3+depth+numOfLoops)+"}\n"+closeLoopString;
            }

            std::string resetAggregates = "";

            loopString += genAggLoopStringCompressed(
                node, nextLoop, depth, contributingViews, 0, resetAggregates);

            returnString += resetAggregates + loopString + closeLoopString;

            if (COMPRESS_AGGREGATES)
            {                    
                // After recurse, add the aggregates that have been added with post FLAG
                AggregateIndexes bench, aggIdx;
                std::bitset<7> increasingIndexes;
                size_t aggID = 0, off = 0;

                bool addedAggregates = false;

                // Loop to identify the first bench
                for (; aggID <
                         newAggregateRegister[nextLoop].size(); ++aggID)
                {
                    const AggRegTuple& regTuple = newAggregateRegister[nextLoop][aggID];
        
                    if (regTuple.postLoopAgg)
                    {
                        mapAggregateToIndexes(aggIdx,regTuple,depth,nextLoop);

                        // Increment the counter and update the remapping array
                        newAggregateRemapping[depth][nextLoop][aggID] = aggregateCounter;
                        ++aggregateCounter;

                        ++aggID; // TODO: Make sure that this is necessary!!
                        addedAggregates = true;
                        break;
                    }
                }   
            
                // Loop over all other aggregates 
                for (; aggID <
                         newAggregateRegister[nextLoop].size(); ++aggID)
                {
                    const AggRegTuple& regTuple = newAggregateRegister[nextLoop][aggID];
        
                    if (regTuple.postLoopAgg)
                    {
                        aggIdx.reset();
                    
                        mapAggregateToIndexes(aggIdx,regTuple,depth,nextLoop);

                        if (aggIdx.isIncrement(bench,off,increasingIndexes))
                        {
                            ++off;
                        }
                        else
                        {
                            // If not, output bench and all offset aggregates
                            returnString += outputAggRegTupleString(
                                bench,off,increasingIndexes, 3+depth+numOfLoops,true);
                        
                            // make aggregate the bench and set offset to 0
                            bench = aggIdx;
                            off = 0;
                        }

                        // Increment the counter and update the remapping array
                        newAggregateRemapping[depth][nextLoop][aggID] = aggregateCounter;
                        ++aggregateCounter;
                    }
                }

                // Output bench -- and any further aggregates (if there were any)
                if (addedAggregates)
                    returnString += outputAggRegTupleString(
                        bench,off,increasingIndexes,3+depth+numOfLoops,true);
            }
            else
            {
                // here we output the aggregates uncompressed -TODO: we could probably
                // remove this and incorporate it in the previous part
            
                // After recurse, add the aggregates that have been added with post FLAG
                for (size_t aggID = 0; aggID <
                         newAggregateRegister[nextLoop].size(); ++aggID)
                {
                    const AggRegTuple& regTuple = newAggregateRegister[nextLoop][aggID];
        
                    if (regTuple.postLoopAgg)
                    {
                        // We use a mapping to the correct index
                        returnString += offset(3+depth+numOfLoops)+
                            "aggregateRegister["+std::to_string(aggregateCounter)+"] = ";

                        if (regTuple.previous.first < listOfLoops.size())
                        {
                            std::string local = std::to_string(
                                newAggregateRemapping[depth]
                                [regTuple.previous.first]
                                [regTuple.previous.second]);
                            returnString += "aggregateRegister["+local+"]*";
                        }

                        if (regTuple.postLoop.first < listOfLoops.size())
                        {
                            std::string post = std::to_string(
                                newAggregateRemapping[depth][regTuple.postLoop.first]
                                [regTuple.postLoop.second]); 
                            returnString += "aggregateRegister["+post+"]*";
                        }
                        
                        returnString.pop_back();
                        returnString += ";\n";
            
                        // Increment the counter and update the remapping array
                        newAggregateRemapping[depth][nextLoop][aggID] = aggregateCounter;
                        ++aggregateCounter;
                    }
                }
            }
        }
        else break;
    }
    
    size_t startingOffset = aggregateCounter;
    
    if (COMPRESS_AGGREGATES)
    {        
        // After recurse, add the aggregates that have been added with post FLAG
        AggregateIndexes bench, aggIdx;
        std::bitset<7> increasingIndexes;
        size_t aggID = 0, off = 0;
    
        bool addedAggregates = false;

        // Loop to identify the first bench
        for (; aggID < newAggregateRegister[thisLoopID].size(); ++aggID)
        {
            const AggRegTuple& regTuple = newAggregateRegister[thisLoopID][aggID];
        
            if (!regTuple.postLoopAgg)
            {
                mapAggregateToIndexes
                    (bench,newAggregateRegister[thisLoopID][aggID],depth,thisLoopID);
            
                addedAggregates = true;

                // Increment the counter and update the remapping array
                newAggregateRemapping[depth][thisLoopID][aggID] = aggregateCounter;
                ++aggregateCounter;

                ++aggID;
                break;
            }
        }

        // Loop over all other aggregates 
        for (; aggID < newAggregateRegister[thisLoopID].size(); ++aggID)
        {
            const AggRegTuple& regTuple = newAggregateRegister[thisLoopID][aggID];
        
            if (!regTuple.postLoopAgg)
            {
                aggIdx.reset();
            
                mapAggregateToIndexes
                    (aggIdx,newAggregateRegister[thisLoopID][aggID],depth,thisLoopID);

                if (aggIdx.isIncrement(bench,off,increasingIndexes))
                {
                    ++off;
                }
                else
                {
                    // If not, output bench and all offset aggregates
                    returnString += outputAggRegTupleString(bench,off,increasingIndexes,
                                                            3+depth+numOfLoops,false);
                
                    // make aggregate the bench and set offset to 0
                    bench = aggIdx;
                    off = 0;
                }

                // Increment the counter and update the remapping array
                newAggregateRemapping[depth][thisLoopID][aggID] = aggregateCounter;
                ++aggregateCounter;
            }
        }

        // Output bench -- and any further aggregates (if there were any)
        if (addedAggregates)
            returnString += outputAggRegTupleString(
                bench,off,increasingIndexes,3+depth+numOfLoops,false);
    }
    else
    {
        
        for (size_t aggID = 0; aggID < newAggregateRegister[thisLoopID].size(); ++aggID)
        {
            const AggRegTuple& regTuple = newAggregateRegister[thisLoopID][aggID];
        
            if (!regTuple.postLoopAgg)
            {
                // We use a mapping to the correct index
                returnString += offset(3+depth+numOfLoops)+
                    "aggregateRegister["+std::to_string(aggregateCounter)+"] += ";

                if (regTuple.previous.first < listOfLoops.size())
                {
                    std::string prev = std::to_string(
                        newAggregateRemapping[regTuple.prevDepth]
                        [regTuple.previous.first]
                        [regTuple.previous.second]);
                
                    returnString += "aggregateRegister["+prev+"]*";
                }
            
                if (regTuple.product.first)
                {
                    std::string local = std::to_string(
                        localProductRemapping[thisLoopID][regTuple.product.second]
                        );
                    returnString += "localRegister["+local+"]*";
                }
        
                if (regTuple.newViewProduct)
                {
                    std::string view = std::to_string(
                        viewProductRemapping[thisLoopID][regTuple.viewAgg.second]);
                    returnString += "viewRegister["+view+"];";
                }
                else if (regTuple.singleViewAgg)
                {
                    returnString += "aggregates_"+viewName[regTuple.viewAgg.first]+"["+
                        std::to_string(regTuple.viewAgg.second)+"]*";
                }
            
                if (regTuple.multiplyByCount)
                    returnString +=  "count*";

                if (regTuple.postLoop.first < listOfLoops.size())
                {
                    std::string postLoop = std::to_string(
                        newAggregateRemapping[depth][regTuple.postLoop.first]
                        [regTuple.postLoop.second]);
                    returnString += "aggregateRegister["+postLoop+"]*";
                }
            
                returnString.pop_back();
                returnString += ";\n";
            
                // Increment the counter and update the remapping array
                newAggregateRemapping[depth][thisLoopID][aggID] = aggregateCounter;
                ++aggregateCounter;
            }
        }
    }
    

    if (aggregateCounter > startingOffset)
    {
        size_t resetOffset = (numOfLoops == 0 ? 3+depth : 2+depth+numOfLoops);
        resetString += offset(resetOffset)+"memset(&aggregateRegister["+
            std::to_string(startingOffset)+"], 0, sizeof(double) * "+
            std::to_string(aggregateCounter-startingOffset)+");\n";
    }
    
    return returnString;
}


void CppGenerator::mapAggregateToIndexes(
    AggregateIndexes& index, const AggRegTuple& aggregate, const size_t& depth,
    const size_t& thisLoopID)
{
    index.reset();

    // TODO: need depth + thisLoopID
    if (aggregate.previous.first < listOfLoops.size())
    {
        // TODO: IS THIS REALLY ALWAYS DEPTH - 1 ?!?!?
        size_t prev = newAggregateRemapping[aggregate.prevDepth]
            [aggregate.previous.first]
            [aggregate.previous.second];
        // size_t prev = newAggregateRemapping[depth-1][aggregate.previous.first]
        //     [aggregate.previous.second];

        index.present.set(0);
        index.indexes[0] = prev;
    }
            
    if (aggregate.product.first)
    {
        size_t local =
            localProductRemapping[thisLoopID][aggregate.product.second];
        
        index.present.set(1);
        index.indexes[1] = local;
    }
        
    if (aggregate.newViewProduct)
    {
        size_t view = 
            viewProductRemapping[thisLoopID][aggregate.viewAgg.second];

        index.present.set(2);
        index.indexes[2] = view;
    }
    
    else if (aggregate.singleViewAgg)
    {
        index.present.set(3);
        index.indexes[3] = aggregate.viewAgg.first;

        index.present.set(4);
        index.indexes[4] = aggregate.viewAgg.second;
    }
            
    if (aggregate.multiplyByCount)
    {
        index.present.set(5);
        index.indexes[5] = 1;
    }

    if (aggregate.postLoop.first < listOfLoops.size())
    {
        size_t postLoop = 
            newAggregateRemapping[depth][aggregate.postLoop.first]
            [aggregate.postLoop.second];

        index.present.set(6);
        index.indexes[6] = postLoop;
    }

    index.indexes[7] = aggregateCounter;
}


void CppGenerator::mapAggregateToIndexes(
    AggregateIndexes& index, const PostAggRegTuple& aggregate,
    const size_t& depth, const size_t& maxDepth)
{
    index.reset();

    if (aggregate.local.first < maxDepth)
    {
        const ProductAggregate& prodAgg =
            productToVariableRegister[aggregate.local.first][aggregate.local.second];
        
        const std::pair<size_t,size_t>& correspondingLoopAgg =
            prodAgg.correspondingLoopAgg;
        
        size_t local = newAggregateRemapping[depth]
            [correspondingLoopAgg.first]
            [correspondingLoopAgg.second];
        
        index.present.set(0);
        index.indexes[0] = local;
    }
            
    if (aggregate.post.first < maxDepth)
    {
        size_t post = postRemapping[aggregate.post.first][aggregate.post.second];
        
        index.present.set(1);
        index.indexes[1] = post;
    }
    
    index.indexes[2] = postCounter;
}

void CppGenerator::mapAggregateToIndexes(
    AggregateIndexes& index, const AggregateTuple& aggregate,
    const size_t& viewID, const size_t& maxDepth)
{
    index.reset();
    
    if (aggregate.local.first < maxDepth)
    {
        const ProductAggregate& prodAgg =
            productToVariableRegister[aggregate.local.first]
            [aggregate.local.second];
        
        const std::pair<size_t,size_t>& correspondingLoopAgg =
            prodAgg.correspondingLoopAgg;
                    
        size_t local = newAggregateRemapping[aggregate.local.first]
            [correspondingLoopAgg.first]
            [correspondingLoopAgg.second];

        index.present.set(0);
        index.indexes[0] = local;        
    }
    
    if (aggregate.post.first < maxDepth)
    {
        size_t post = postRemapping[aggregate.post.first][aggregate.post.second];
        
        index.present.set(1);
        index.indexes[1] = post;
    }

    if (aggregate.hasDependentComputation)
    {
        const std::pair<size_t,size_t>& correspondingLoopAgg =
            aggregate.dependentProdAgg.correspondingLoopAgg;
                
        size_t depComp = 
            depAggregateRemapping[correspondingLoopAgg.first]
            [correspondingLoopAgg.second];

        index.present.set(2);
        index.indexes[2] = depComp;        
    }

    index.present.set(3);
    index.indexes[3] = viewID;

    index.present.set(4);
    index.indexes[4] = aggregate.aggID;
}


prod_bitset CppGenerator::computeDependentLoops(
    size_t view_id, prod_bitset presentFunctions, var_bitset relationBag,
    dyn_bitset contributingViews, var_bitset varOrderBitset,
    dyn_bitset addedOutLoops, size_t thisLoopID)
{
    dyn_bitset loopFactors(_qc->numberOfViews()+1);
    dyn_bitset nextLoops(_qc->numberOfViews()+1);

    prod_bitset loopFunctionMask;

    const dyn_bitset& consideredLoops = depListOfLoops[thisLoopID].loopFactors;

    size_t highestPosition = 0;
    // need to continue with this the next loop
    for (size_t view=_qc->numberOfViews(); view > 0; view--)
    {
        if (consideredLoops[view])
        {
            highestPosition = view;
            break;
        }
    }

    
    dyn_bitset nonAddedOutLoops = outViewLoop[view_id] & ~addedOutLoops;

    // then find all loops required to compute these functions and create masks
    for (size_t f = 0; f < NUM_OF_FUNCTIONS; f++)
    {
        if (presentFunctions[f])
        {
            const var_bitset& functionVars = _qc->getFunction(f)->_fVars;
            loopFactors.reset();
            
            for (size_t v = 0; v < NUM_OF_VARIABLES; ++v)
            {
                if (functionVars[v] && !varOrderBitset[v])
                {
                    if (relationBag[v])
                        loopFactors.set(_qc->numberOfViews());
                    else
                    {
                        for (size_t incID = 0; incID < _qc->numberOfViews(); ++incID)
                        {
                            if (contributingViews[incID])
                            {
                                View* incView = _qc->getView(incID);
                                const var_bitset& incViewBitset = incView->_fVars;
                                if (incViewBitset[v])
                                {
                                    loopFactors.set(incID);
                                    break;
                                }
                            }
                        }
                    }
                }
            }

            // TODO: TODO: Consider this again 
            if ((loopFactors & nonAddedOutLoops).any() ||
                (nonAddedOutLoops.any() && (loopFactors & ~outViewLoop[view_id]).any()))
                continue;
            
            loopFactors |= addedOutLoops;
            
            if (loopFactors == consideredLoops)
            {
                // Add this function to the mask for this loop
                loopFunctionMask.set(f);
            }
            else
            {
                if ((consideredLoops & loopFactors) == consideredLoops)
                {
                    dyn_bitset remainingLoops = loopFactors & ~consideredLoops;
                    if (remainingLoops.any())
                    {
                        size_t next = remainingLoops.find_next(highestPosition);
                        if (next != boost::dynamic_bitset<>::npos)
                            nextLoops.set(next);
                        else
                        {
                            ERROR("We found a remaining Loop which is not higher " <<
                                  " than the highest considered loop?!\n");
                            exit(1);
                        }
                    }
                }
            }
        }
    }
    
    presentFunctions &= ~loopFunctionMask;
    
    // Then use the registrar to split the functions to their loops
    DependentLoop& depLoop = depListOfLoops[thisLoopID];
    if (loopFunctionMask.any())
        depLoop.functionMask.push_back({view_id, loopFunctionMask});
    
    // Recurse to the next loops
    for (size_t viewID=0; viewID < _qc->numberOfViews() + 1; ++viewID)
    {
        if (nextLoops[viewID])
        {
            dyn_bitset nextConsideredLoops = consideredLoops;
            nextConsideredLoops.set(viewID);

            if (outViewLoop[view_id][viewID])
            {
                ERROR("THERE IS A MISTAKE WITH NEXT CONSIDERED LOOPS IN DEP COMP\n");
                exit(1);
            }

            // TODO: check the next loops for the given DependentLoop
            // if any of them use the same loop -- then reuse this one

            bool found = false;
            size_t nextLoopID;
            for (const size_t& potNextLoop : depLoop.next)
            {
                if (depListOfLoops[potNextLoop].loopFactors == nextConsideredLoops)
                {
                    found = true;
                    nextLoopID = potNextLoop;
                }
            }

            if (!found)
            {
                nextLoopID = depListOfLoops.size();

                // otherwise add a new loop to the list with the correct loops and
                // loopVariable and then add this new loop to next of this loop
                DependentLoop newDepLoop(_qc->numberOfViews());
                newDepLoop.loopFactors = nextConsideredLoops;
                newDepLoop.loopVariable = viewID;

                // std::cout << "#########" << newDepLoop.loopFactors << std::endl;
                
                depListOfLoops.push_back(newDepLoop);

                // std::cout << depLoop.next.size() << std::endl;
                depListOfLoops[thisLoopID].next.push_back(nextLoopID);
            }

            prod_bitset branch =
                computeDependentLoops(view_id,presentFunctions,relationBag,
                                      contributingViews, varOrderBitset,
                                      addedOutLoops,nextLoopID);

            depListOfLoops[thisLoopID].branchFunctions |= branch;
        }
    }

    if (outViewLoop[view_id] == consideredLoops)
        depListOfLoops[thisLoopID].outView.set(view_id);

    if (consideredLoops == addedOutLoops && nonAddedOutLoops.any())
    {
        size_t nextOutLoop = nonAddedOutLoops.find_first();
        if (nextOutLoop != boost::dynamic_bitset<>::npos)
        {
            dyn_bitset nextConsideredLoops = consideredLoops;
            nextConsideredLoops.set(nextOutLoop);

            bool found = false;
            size_t nextLoopID;
            for (const size_t& potNextLoop : depLoop.next)
            {
                if (depListOfLoops[potNextLoop].loopFactors == nextConsideredLoops)
                {
                    found = true;
                    nextLoopID = potNextLoop;
                }
            }

            if (!found)
            {
                nextLoopID = depListOfLoops.size();

                // otherwise add a new loop to the list with the correct loops and
                // loopVariable and then add this new loop to next of this loop
                DependentLoop newDepLoop(_qc->numberOfViews());
                newDepLoop.loopFactors = nextConsideredLoops;
                newDepLoop.loopVariable = nextOutLoop;
                
                
                depListOfLoops.push_back(newDepLoop);

                depListOfLoops[thisLoopID].next.push_back(nextLoopID);
            }

            addedOutLoops.set(nextOutLoop);

            prod_bitset branch =                
                computeDependentLoops(view_id,presentFunctions,relationBag,
                                      contributingViews,varOrderBitset,
                                      addedOutLoops,nextLoopID);

            depListOfLoops[thisLoopID].branchFunctions |= branch;
        }
    }
    
    return depListOfLoops[thisLoopID].branchFunctions;
}



std::pair<size_t,size_t> CppGenerator::addDependentProductToLoop(
    ProductAggregate& prodAgg, const size_t view_id, size_t& currentLoop,
    const size_t& maxDepth, std::pair<size_t,size_t>& prevAgg)
{
    const size_t thisLoopID = currentLoop;
    const DependentLoop& thisLoop = depListOfLoops[currentLoop];
    
    size_t maskID = 0;
    bool hasMask = false;
    
    dyn_bitset nonAddedOutLoops = outViewLoop[view_id] & ~thisLoop.loopFactors;
    
    for (size_t id = 0; id < thisLoop.functionMask.size(); ++id)
    {
        if (thisLoop.functionMask[id].first == view_id)
        {
            maskID = id;
            hasMask = true;
            break;
        }
    }

    std::pair<bool,size_t> regTupleProduct = {false,0};
    
    if (hasMask)
    {
        const prod_bitset& fMask = thisLoop.functionMask[maskID].second;
    
        // check if this product requires computation over this loop loopmask
        prod_bitset loopFunctions = prodAgg.product & fMask;
        if (loopFunctions.any())
        {
            // This should be add to the loop not depth 
            auto proditerator = localProductMap[currentLoop].find(loopFunctions);
            if (proditerator != localProductMap[currentLoop].end())
            {
                // set this local product to the id given by the map
                regTupleProduct = {true, proditerator->second};
            }
            else
            {
                size_t nextID = localProductList[currentLoop].size();
                localProductMap[currentLoop][loopFunctions] = nextID;
                regTupleProduct = {true, nextID};
                localProductList[currentLoop].push_back(loopFunctions);
            }
        }
    }
    
    bool singleViewAgg = false;
    
    std::pair<size_t, size_t> regTupleView;
    if (!prodAgg.viewAggregate.empty() && thisLoop.loopVariable != _qc->numberOfViews())
    {
        if (outViewLoop[view_id][thisLoop.loopVariable] || regTupleProduct.first)
        {
            singleViewAgg = true;
        
            for (const std::pair<size_t, size_t>& viewAgg : prodAgg.viewAggregate)
            {
                if (viewAgg.first == thisLoop.loopVariable)
                {
                    regTupleView = viewAgg;
                    break;
                }
            }
        }
    }
    
    std::pair<size_t,size_t> previousLoopComp = {depListOfLoops.size(),0};
    std::pair<size_t, size_t> postComputation =  {depListOfLoops.size(),0};
    
    // If this aggregate is part of the 
    if (nonAddedOutLoops.any())
    {
        if ((regTupleProduct.first || singleViewAgg) && 
            (thisLoop.loopFactors & outViewLoop[view_id]) == thisLoop.loopFactors)
        {
            AggRegTuple regTuple;
            regTuple.product = regTupleProduct;
            
            regTuple.singleViewAgg = singleViewAgg;
            regTuple.viewAgg = regTupleView;

            regTuple.preLoopAgg = true;
            regTuple.previous = prevAgg;
            regTuple.prevDepth = viewLevelRegister[view_id];
            
            auto regit = aggregateRegisterMap[thisLoopID].find(regTuple);
            if (regit != aggregateRegisterMap[thisLoopID].end())
            {
                previousLoopComp = {thisLoopID,regit->second};
            }
            else
            {
                // If so, add it to the aggregate register
                size_t newID = newAggregateRegister[thisLoopID].size();
                
                aggregateRegisterMap[thisLoopID][regTuple] = newID;
                newAggregateRegister[thisLoopID].push_back(regTuple);
        
                previousLoopComp =  {thisLoopID,newID};
            }

            prevAgg = previousLoopComp;
        }
        
        // need to find the loop that adds the next OutLoop
        size_t nextOutLoop = nonAddedOutLoops.find_first();
        
        bool found = false;
        dyn_bitset nextConsideredLoops = thisLoop.loopFactors;
        nextConsideredLoops.set(nextOutLoop);
        
        for (size_t next=0; next < thisLoop.next.size(); ++next)
        {
            size_t nextLoopID = thisLoop.next[next];
            DependentLoop& nextLoop = depListOfLoops[nextLoopID];

            if (nextLoop.loopFactors == nextConsideredLoops)
            {
                addDependentProductToLoop(
                    prodAgg,view_id,nextLoopID,maxDepth,prevAgg);
                found = true;
                break;
            }
        }

        if (!found)
        {
            ERROR("There is a problem with nextOutLoop in addDepProd\n");
            exit(1);
        }
    }
    else
    {
        for (size_t next=0; next < thisLoop.next.size(); ++next)
        {
            size_t nextLoopID = thisLoop.next[next];
            DependentLoop& nextLoop = depListOfLoops[nextLoopID];

            if (((nextLoop.loopFactors & thisLoop.loopFactors) != thisLoop.loopFactors))
            {
                ERROR("There is a problem with nextLoopFactors in addDepProd\n");
                exit(1);
            }

            // We only want to go into this loop if there is somehting
            // for this prodAGG for this view
            if ((thisLoop.branchFunctions & prodAgg.product).any())
            {
                std::pair<size_t, size_t> pAgg = {};
                postComputation =
                    addDependentProductToLoop(prodAgg, view_id, nextLoopID, maxDepth,
                                              pAgg);
            
                if (next + 1 != thisLoop.next.size() &&
                    (thisLoop.branchFunctions & prodAgg.product).any())
                {
                    // Add this product to this loop with the 'prev'Computation
                    AggRegTuple postRegTuple;
                    postRegTuple.postLoopAgg = true;
                    postRegTuple.previous = previousLoopComp; 
                    postRegTuple.postLoop = postComputation;
                    postRegTuple.prevDepth = viewLevelRegister[view_id];

                    // TODO: add the aggregate to the register!!
                    auto regit = aggregateRegisterMap[nextLoopID].find(postRegTuple);
                    if (regit != aggregateRegisterMap[nextLoopID].end())
                    {
                        previousLoopComp = {nextLoopID,regit->second};
                    }
                    else
                    {
                        // If so, add it to the aggregate register
                        size_t newID = newAggregateRegister[nextLoopID].size();
                        aggregateRegisterMap[nextLoopID][postRegTuple] = newID;
                        newAggregateRegister[nextLoopID].push_back(postRegTuple);
                    
                        previousLoopComp =  {nextLoopID,newID};
                    }
                }
                else
                {
                    previousLoopComp = postComputation;
                }
            }
        }

        // Check if this product has already been computed, if not we add it to the
        // list
        if (regTupleProduct.first || singleViewAgg ||
            prevAgg.first < depListOfLoops.size())
        {
            AggRegTuple regTuple;
            regTuple.product = regTupleProduct;
            regTuple.viewAgg = regTupleView;
            regTuple.singleViewAgg = singleViewAgg;
            regTuple.postLoop = previousLoopComp;
            
            regTuple.previous = prevAgg;
            
            auto regit = aggregateRegisterMap[thisLoopID].find(regTuple);
            if (regit != aggregateRegisterMap[thisLoopID].end())
            {
                previousLoopComp = {thisLoopID,regit->second};
            }
            else
            {
                // If so, add it to the aggregate register
                size_t newID = newAggregateRegister[thisLoopID].size();
                aggregateRegisterMap[thisLoopID][regTuple] = newID;
                newAggregateRegister[thisLoopID].push_back(regTuple);
        
                previousLoopComp =  {thisLoopID,newID};
            }

            prevAgg = previousLoopComp;
        }
    }
    
    return previousLoopComp;
}


void CppGenerator::registerDependentComputationToLoops(size_t depth, size_t group_id)
{
    const var_bitset& varOrderBitset = groupVariableOrderBitset[group_id];
    const size_t maxDepth = groupVariableOrder[group_id].size();
    const var_bitset& relationBag =
        _td->getRelation(_qc->getView(viewGroups[group_id][0])->_origin)->_bag;

    depListOfLoops.clear();


    dyn_bitset consideredLoops(_qc->numberOfViews()+1);
    
    DependentLoop depLoop(_qc->numberOfViews());
    depLoop.loopFactors = consideredLoops;
    depLoop.loopVariable = NUM_OF_VARIABLES;
    
    depListOfLoops.push_back(depLoop);
    
    // TODO: add empty loop to the list of dependentLoops
    for (const size_t viewID : viewGroups[group_id])
    {
        if (viewLevelRegister[viewID] == depth)
        {
            // go through the ProductAggregates and find functions computed here 
            prod_bitset presentFunctions;
            dyn_bitset contributingViews(_qc->numberOfViews()+1);
            dyn_bitset addedOutLoops(_qc->numberOfViews()+1);
            
            for (const AggregateTuple& depAgg : aggregateComputation[viewID])
            {
                presentFunctions |= depAgg.dependentProdAgg.product;
                for (const auto& p : depAgg.dependentProdAgg.viewAggregate)
                    contributingViews.set(p.first);
            }
            
            computeDependentLoops(viewID,presentFunctions,relationBag,contributingViews,
                                  varOrderBitset,addedOutLoops,0);
        }
    }
    
    localProductList.clear();
    localProductMap.clear();
    localProductList.resize(depListOfLoops.size());
    localProductMap.resize(depListOfLoops.size());
    
    newAggregateRegister.clear();
    aggregateRegisterMap.clear();
    newAggregateRegister.resize(depListOfLoops.size());
    aggregateRegisterMap.resize(depListOfLoops.size());
    

    // TODO: TODO: reset the List and Maps for aggregates and
    
    for (const size_t viewID : viewGroups[group_id])
    {
        if (viewLevelRegister[viewID] == depth)
        {
            for (AggregateTuple& depAgg : aggregateComputation[viewID])
            {
                ProductAggregate& prodAgg = depAgg.dependentProdAgg;

                size_t currentLoop = 0;
                std::pair<size_t, size_t> prevAgg = {depListOfLoops.size(), 0};
                
                addDependentProductToLoop(
                    prodAgg, viewID, currentLoop, maxDepth, prevAgg);

                prodAgg.correspondingLoopAgg = prevAgg;
            }
        }
    }
}


std::string CppGenerator::genProductString(
    const TDNode& node, const dyn_bitset& contributingViews, const prod_bitset& product)
{
    std::string productString = "";
    // Turn each function into string
    for (size_t f = 0; f < NUM_OF_FUNCTIONS; f++)
    {
        if (product[f])
        {
            Function* function = _qc->getFunction(f);
            const var_bitset& functionVars = function->_fVars;

            std::string freeVarString = "";

            for (size_t v = 0; v < NUM_OF_VARIABLES; ++v)
            {                        
                if (functionVars[v])
                {
                    if (node._bag[v])
                    {
                        freeVarString += node._name+"Tuple."+
                            _td->getAttribute(v)->_name+",";                            
                    }
                    else
                    {
                        bool notFound = true;
                        for (size_t incViewID = 0; incViewID <
                                 _qc->numberOfViews(); ++incViewID)
                        {
                            if (contributingViews[incViewID])
                            {
                                View* incView = _qc->getView(incViewID);

                                if (incView->_fVars[v])
                                {
                                    freeVarString += viewName[incViewID]+"Tuple."+
                                        _td->getAttribute(v)->_name+",";
                                    notFound = false;
                                    break;
                                }                  
                            }
                        }

                        if (notFound)
                        {
                            ERROR("The loop hasn't been found - THIS IS A MISTAKE\n");
                            std::cout <<  node._name << std::endl;
                            std::cout << product << std::endl;
                            exit(1);
                        }
                    }
                }
            }

            freeVarString.pop_back();
            productString += getFunctionString(function,freeVarString)+"*";
        }
    }
            
    if (!productString.empty())
        productString.pop_back();
    else
        productString += "1";
    
    return productString;    
}





std::string CppGenerator::genGroupLeapfrogJoinCode(
    size_t group_id, const TDNode& node, size_t depth)
{
    const std::vector<size_t>& varOrder = groupVariableOrder[group_id];
    const std::vector<size_t>& viewsPerVar = groupViewsPerVarInfo[group_id];
    const std::vector<size_t>& incViews = groupIncomingViews[group_id];

    const std::string& attrName = _td->getAttribute(varOrder[depth])->_name;
    const std::string& relName = node._name;
    
    size_t idx = depth * (_qc->numberOfViews() + 2);
    size_t numberContributing = viewsPerVar[idx];
    // bool addTuple;

    std::string depthString = std::to_string(depth);
    std::string returnString = "";
    
    size_t off = 1;
    if (depth > 0)
    {
        returnString +=
            offset(2+depth)+"upperptr_"+relName+"["+depthString+"] = "+
            "upperptr_"+relName+"["+std::to_string(depth-1)+"];\n"+
            offset(2+depth)+"lowerptr_"+relName+"["+depthString+"] = "+
            "lowerptr_"+relName+"["+std::to_string(depth-1)+"];\n";

        for (const size_t& viewID : incViews)
        {   
            returnString +=
                offset(2+depth)+"upperptr_"+viewName[viewID]+"["+depthString+"] = "+
                "upperptr_"+viewName[viewID]+"["+std::to_string(depth-1)+"];\n"+
                offset(2+depth)+"lowerptr_"+viewName[viewID]+"["+depthString+"] = "+
                "lowerptr_"+viewName[viewID]+"["+std::to_string(depth-1)+"];\n";
        }
    }

    if (numberContributing > 1)
    {
        // Ordering Relations 
        returnString += genGroupRelationOrdering(relName, depth, group_id);
    }
    else
    {
        std::string factor;        
        if (viewsPerVar[idx+1] == _qc->numberOfViews())
            factor = relName;
        else
            factor = viewName[viewsPerVar[idx+1]];
        
        returnString += offset(2+depth)+"max_"+attrName+" = "+
            factor+"[0]."+attrName+";\n";
    }

    // if (depth + 1 != varOrder.size())
    //     addTuple = true;
    // else
    //     addTuple = false;
    
    // Update rel pointers 
    returnString += offset(2+depth)+ "rel["+depthString+"] = 0;\n"+
        offset(2+depth)+ "atEnd["+depthString+"] = false;\n";

    // resetting the post register before we start the loop 
    if (!postRegisterList[depth].empty())
    {
        size_t firstidx = 0;
        for (size_t d = varOrder.size()-1; d > depth; d--)
            firstidx += postRegisterList[d].size();
        returnString += offset(2+depth)+"memset(&postRegister["+
            std::to_string(firstidx)+"], 0, sizeof(double) * "+
            std::to_string(postRegisterList[depth].size())+");\n";
    }
    
    // Start while loop of the join 
    returnString += offset(2+depth)+"while(!atEnd["+depthString+"])\n"+
        offset(2+depth)+"{\n";

    if (numberContributing > 1)
    {
        if (MICRO_BENCH)
           returnString += offset(3+depth)+
               "BEGIN_MICRO_BENCH(Group"+std::to_string(group_id)+"_timer_seek);\n";
        
        // Seek Value
        returnString += offset(3+depth)+"found["+depthString+"] = false;\n"+
            offset(3+depth)+"// Seek Value\n" +
            offset(3+depth)+"do\n" +
            offset(3+depth)+"{\n" +
            offset(4+depth)+"switch(order_"+attrName+"[rel["+
            depthString+"]].second)\n" +
            offset(4+depth)+"{\n";
        
        off = 1;
        if (viewsPerVar[idx+1] == _qc->numberOfViews())
        {
            // do base relation first
            returnString += seekValueCase(depth, relName, attrName,
                                          _parallelizeGroup[group_id]);
            off = 2;
        }
        
        for (; off <= numberContributing; ++off)
        {
            const size_t& viewID = viewsPerVar[idx+off];
            // case for this view
            returnString += seekValueCase(depth,viewName[viewID],attrName,false);
        }
        returnString += offset(4+depth)+"}\n";
        
        // Close the do loop
        returnString += offset(3+depth)+"} while (!found["+
            depthString+"] && !atEnd["+
            depthString+"]);\n" +
            offset(3+depth)+"// End Seek Value\n";
        // End seek value

        // check if atEnd
        returnString += offset(3+depth)+"// If atEnd break loop\n"+
            offset(3+depth)+"if(atEnd["+depthString+"])\n"+
            offset(4+depth)+"break;\n";

        if (MICRO_BENCH)
            returnString += offset(3+depth)+"END_MICRO_BENCH(Group"+
                std::to_string(group_id)+"_timer_seek);\n";
    }

    if (MICRO_BENCH)
        returnString += offset(3+depth)+
            "BEGIN_MICRO_BENCH(Group"+std::to_string(group_id)+"_timer_while);\n";

    off = 1;
    if (viewsPerVar[idx+1] == _qc->numberOfViews())
    {
        // Set upper = lower and update ranges
        returnString += offset(3+depth)+
            "upperptr_"+relName+"["+depthString+"] = "+
            "lowerptr_"+relName+"["+depthString+"];\n";

        // update range for base relation 
        returnString += updateRanges(depth, relName, attrName,
                                     _parallelizeGroup[group_id]);
        off = 2;
    }

    for (; off <= numberContributing; ++off)
    {
        size_t viewID = viewsPerVar[idx+off];

        returnString += offset(3+depth)+
            "upperptr_"+viewName[viewID]+"["+depthString+"] = "+
            "lowerptr_"+viewName[viewID]+"["+depthString+"];\n";
        
        returnString += updateRanges(depth,viewName[viewID],attrName,false);
    }

    if (MICRO_BENCH)
        returnString += offset(3+depth)+"END_MICRO_BENCH(Group"+std::to_string(group_id)+
            "_timer_while);\n";

#ifndef PREVIOUS
    registerAggregatesToLoops(depth,group_id);

    newAggregateRemapping[depth].resize(listOfLoops.size());
    localProductRemapping.resize(listOfLoops.size());
    viewProductRemapping.resize(listOfLoops.size());

    size_t numAggsRegistered = 0;
    for (size_t loop = 0; loop < listOfLoops.size(); loop++)
    {
        // updating the remapping arrays 
        newAggregateRemapping[depth][loop].resize(
            newAggregateRegister[loop].size(),10000);
        localProductRemapping[loop].resize(
            localProductList[loop].size(),10000);
        viewProductRemapping[loop].resize(
            viewProductList[loop].size(),10000);

        numAggsRegistered += newAggregateRegister[loop].size();
    }
    
    // setting the addTuple bool to false;
    returnString += offset(3+depth)+"addTuple["+depthString+"] = false;\n";


    if (MICRO_BENCH)
        returnString += offset(3+depth)+
            "BEGIN_MICRO_BENCH(Group"+std::to_string(group_id)+"_timer_aggregate);\n";
    
    if (numAggsRegistered > 0)
    {
        returnString += offset(3 + depth)+"memset(&aggregateRegister["+
            std::to_string(aggregateCounter)+"], 0, sizeof(double) * "+
            std::to_string(numAggsRegistered)+");\n";
    }
   
    // We add the definition of the Tuple references and aggregate pointers
    // TODO: This could probably be simplified TODO: TODO: TODO: !!!!!
    returnString += offset(3+depth)+relName +"_tuple &"+node._name+"Tuple = "+
        relName+"[lowerptr_"+relName+"["+depthString+"]];\n";

    for (const size_t& viewID : incViews)
    {
        returnString += offset(3+depth)+viewName[viewID]+
            "_tuple &"+viewName[viewID]+"Tuple = "+viewName[viewID]+
            "[lowerptr_"+viewName[viewID]+"["+depthString+"]];\n"+
            offset(3+depth)+"double *aggregates_"+viewName[viewID]+
            " = "+viewName[viewID]+"Tuple.aggregates;\n";
    }

    if (depth + 1 == varOrder.size())
        returnString += offset(3+depth)+"double count = upperptr_"+relName+
            "["+depthString+"] - lowerptr_"+relName+"["+depthString+"] + 1;\n";
    
    // dyn_bitset consideredLoops(_qc->numberOfViews()+1);
    // returnString += newgenAggregateString(node, consideredLoops, depth, group_id);

    size_t loopID = 0;
    dyn_bitset contributingViews(_qc->numberOfViews()+1);

    std::string resetString = "", loopString = "";
    loopString += genAggLoopStringCompressed(
        node,loopID,depth,contributingViews,0,resetString);
    // returnString += genAggLoopString(node,loopID,depth,contributingViews,0);

    returnString += resetString + loopString;
    
    if (MICRO_BENCH)
        returnString += offset(3+depth)+"END_MICRO_BENCH(Group"+std::to_string(group_id)+
            "_timer_aggregate);\n";

    
    // then you would need to go to next variable
    if (depth+1 < varOrder.size())
    {
        // leapfrogJoin for next join variable 
        returnString += genGroupLeapfrogJoinCode(group_id, node, depth+1);
    }
    else
    {
        returnString += offset(3+depth)+"memset(addTuple, true, sizeof(bool) * "+
            std::to_string(varOrder.size())+");\n";
    }

    size_t postOff = (depth+1 < varOrder.size() ? 4+depth : 3+depth);

    std::string postComputationString = "";
    
    // for (const PostAggRegTuple& tuple : postRegisterList[depth])
    // {
    //     const ProductAggregate& prodAgg =
    //         productToVariableRegister[tuple.local.first][tuple.local.second];
        
    //     const std::pair<size_t,size_t>& correspondingLoopAgg =
    //         prodAgg.correspondingLoopAgg;

    //     // if (group_id == 3 && depth == 0)
    //     //     std::cout << "************************************ " <<
    //     //         correspondingLoopAgg.first << "  " <<
    //     //         correspondingLoopAgg.second << std::endl;
        
    //     std::string local = std::to_string(
    //         newAggregateRemapping[depth][correspondingLoopAgg.first]
    //         [correspondingLoopAgg.second]);

    //     returnString += offset(postOff)+"postRegister["+std::to_string(postCounter)+
    //         "] += aggregateRegister["+local+"]";

    //     if (tuple.post.first < varOrder.size())
    //     {
    //         std::string post = std::to_string(
    //             postRemapping[tuple.post.first][tuple.post.second]);

    //         returnString += " * postRegister["+post+"];\n";
    //     }
    //     else
    //          returnString += ";\n";
        
    //     postRemapping[depth].push_back(postCounter);
    //     ++postCounter;
    // }

    if (postRegisterList[depth].size() > 0)
    {
        AggregateIndexes bench, aggIdx;
        std::bitset<7> increasingIndexes;
        
        mapAggregateToIndexes(bench,postRegisterList[depth][0],depth,varOrder.size());

        postRemapping[depth].push_back(postCounter);
        ++postCounter;

        size_t offset = 0;
        for (size_t aggID = 1; aggID < postRegisterList[depth].size(); ++aggID)
        {
            const PostAggRegTuple& tuple = postRegisterList[depth][aggID];

            aggIdx.reset();
            
            mapAggregateToIndexes(aggIdx,tuple,depth,varOrder.size());

            if (aggIdx.isIncrement(bench,offset,increasingIndexes))
            {
                ++offset;
            }
            else
            {
                // If not, output bench and all offset aggregates
                postComputationString += outputPostRegTupleString(
                    bench,offset,increasingIndexes, postOff);
                        
                // make aggregate the bench and set offset to 0
                bench = aggIdx;
                offset = 0;
            }

            postRemapping[depth].push_back(postCounter);
            ++postCounter;
        }

        // If not, output bench and all offset aggregates
        postComputationString += outputPostRegTupleString(
            bench,offset,increasingIndexes, postOff);
    }
    
    registerDependentComputationToLoops(depth, group_id);

    depAggregateRemapping.resize(depListOfLoops.size());
    depLocalProductRemapping.resize(depListOfLoops.size());
    
    size_t depNumAggsRegistered = 0;
    for (size_t loop = 0; loop < depListOfLoops.size(); loop++)
    {
        // updating the remapping arrays 
        depAggregateRemapping[loop].resize(
            newAggregateRegister[loop].size(),10000);
        depLocalProductRemapping[loop].resize(
            localProductList[loop].size(),10000);

        depNumAggsRegistered += newAggregateRegister[loop].size();
    }
    
    // if (depNumAggsRegistered > 0)
    // {
    //     returnString += offset(3 + depth)+"memset(&aggregateRegister["+
    //         std::to_string(aggregateCounter)+"], 0, sizeof(double) * "+
    //         std::to_string(depNumAggsRegistered)+");\n";
    // }
    resetString = "";
    loopString = "";

    size_t depLoopID = 0;
    dyn_bitset depContributingViews(_qc->numberOfViews()+1);
    loopString += genDependentAggLoopString(
        node,depLoopID,depth,depContributingViews,varOrder.size(),resetString);

    postComputationString += resetString+loopString;

    if (depth+1 < varOrder.size() && !postComputationString.empty())
        // The computation below is only done if we have satisfying join values 
        returnString += offset(3+depth)+"if (addTuple["+depthString+"]) \n"+
            offset(3+depth)+"{\n";
    
    if (MICRO_BENCH)
        returnString += offset(3+depth)+
            "BEGIN_MICRO_BENCH(Group"+std::to_string(group_id)+
            "_timer_post_aggregate);\n";

    returnString += postComputationString;

    if (MICRO_BENCH)
        returnString += offset(3+depth)+"END_MICRO_BENCH(Group"+std::to_string(group_id)+
            "_timer_post_aggregate);\n";

    if (depth+1 < varOrder.size() && !postComputationString.empty())
        returnString += offset(3+depth)+"}\n";
    
#endif
    
#ifdef PREVIOUS 
    // for (const size_t& viewID : viewGroups[group_id])
    // {
    //     View* view = _qc->getView(viewID);
        
    //     if (!_requireHashing[viewID] && depth + 1 ==
    //         (view->_fVars & groupVariableOrderBitset[group_id]).count())
    //     {
    //         if (addTuple)
    //             returnString += offset(3+depth)+
    //                 "bool addTuple_"+viewName[viewID]+" = false;\n";
    //         returnString += offset(3+depth)+"memset(aggregates_"+viewName[viewID]+
    //             ", 0, sizeof(double) * "+std::to_string(view->_aggregates.size())+");\n";
    //     }
    // }

    // // then you would need to go to next variable
    // if (depth+1 < varOrder.size())
    // {
    //     // leapfrogJoin for next join variable 
    //     returnString += genGroupLeapfrogJoinCode(group_id, node, depth+1);
    // }
    // else
    // {
    //     returnString += offset(3+depth)+
    //         "/****************AggregateComputation*******************/\n";

    //     /******************** LOCAL COMPUTATION *********************/

    //     // generate an array that computes all local computation required
    //     // create map that gives index to this local aggregate - this is
    //     // used to create the array  

    //     // TODO: QUESTION : how do we handle loops?
    //     std::unordered_map<std::vector<prod_bitset>, size_t> localComputationAggregates;
    //     std::unordered_map<std::string, size_t> childAggregateComputation;

    //     std::vector<std::string> localAggregateIndexes;
    //     std::vector<std::string> childAggregateIndexes;
        
    //     std::vector<std::vector<size_t>>
    //         aggregateComputationIndexes(_qc->numberOfViews());

    //     // size_t cachedChildAggregate = 0;
    //     // size_t cachedLocalAggregate = 0;
        
    //     std::vector<bool> allLoopFactors(_qc->numberOfViews() + 1);
    //     std::unordered_map<std::vector<bool>,std::string> aggregateSection;

    //     /* For each view in this group ... */
    //     for (const size_t& viewID : viewGroups[group_id])
    //     {
    //         View* view = _qc->getView(viewID);

    //         if (!_requireHashing[viewID] && depth + 1 >
    //             (view->_fVars & groupVariableOrderBitset[group_id]).count()) 
    //             returnString += offset(3+depth)+"addTuple_"+
    //                 viewName[viewID]+" = true;\n";

    //         size_t numberIncomingViews =
    //             (view->_origin == view->_destination ? node._numOfNeighbors :
    //              node._numOfNeighbors - 1);

    //         std::vector<bool> loopFactors(_qc->numberOfViews() + 1);
    //         size_t numLoopFactors = 0;

    //         std::string aggregateString = "";

    //         if (_requireHashing[viewID])
    //         {
    //             for (size_t v = 0; v < NUM_OF_VARIABLES; ++v)
    //             {
    //                 if (view->_fVars[v])
    //                 {
    //                     if (!groupVariableOrderBitset[group_id][v])
    //                     {
    //                         // if var is in relation - use relation 
    //                         if (node._bag[v])
    //                         {                                                            
    //                             if (!loopFactors[_qc->numberOfViews()])
    //                                 ++numLoopFactors;
                                                        
    //                             loopFactors[_qc->numberOfViews()] = 1;
    //                             allLoopFactors[_qc->numberOfViews()] = 1;

    //                         }
    //                         else // else find corresponding view 
    //                         {
    //                             for (const size_t& incViewID : incViews)
    //                             {
    //                                 if (_qc->getView(incViewID)->_fVars[v])
    //                                 {
    //                                     if (!loopFactors[incViewID])
    //                                         ++numLoopFactors;
                                                        
    //                                     loopFactors[incViewID] = 1;
    //                                     allLoopFactors[incViewID] = 1;
    //                                 }
    //                             }
    //                         }
    //                     }
    //                 }
    //             }
    //         }
            
    //         for (size_t aggID = 0; aggID < view->_aggregates.size(); ++aggID)
    //         {
    //             // get the aggregate pointer
    //             Aggregate* aggregate = view->_aggregates[aggID];
            
    //             std::string agg = "";
    //             size_t aggIdx = 0, incomingCounter = 0;
    //             for (size_t i = 0; i < aggregate->_agg.size(); ++i)
    //             {                   
    //                 // This loop defines the local aggregates computed at this node
    //                 std::string localAgg = "";
    //                 while (aggIdx < aggregate->_m[i])
    //                 {    
    //                     const prod_bitset& product = aggregate->_agg[aggIdx];           
    //                     for (size_t f = 0; f < NUM_OF_FUNCTIONS; ++f)
    //                     {
    //                         if (product.test(f))
    //                         {
    //                             Function* function = _qc->getFunction(f);
    //                             const var_bitset& functionVars = function->_fVars;

    //                             std::string freeVarString = "";
    //                             for (size_t v = 0; v < NUM_OF_VARIABLES; ++v)
    //                             {
    //                                 if (functionVars[v])
    //                                 {
    //                                     // CHECK IF IT IS IN THE BAG OF THE RELATION
    //                                     if (node._bag[v])
    //                                     {
    //                                         if (groupVariableOrderBitset[group_id][v])
    //                                         {
    //                                             // freeVarString += relName+"[lowerptr_"+
    //                                             //     relName+"["+depthString+"]]."+
    //                                             //     _td->getAttribute(v)->_name+",";

    //                                             freeVarString += relName+"Pointer."+
    //                                                 _td->getAttribute(v)->_name+",";
    //                                         }
    //                                         else
    //                                         {
    //                                             // freeVarString +=
    //                                             //     relName+"[ptr_"+relName+"]."+
    //                                             //     _td->getAttribute(v)->_name+",";

    //                                             freeVarString += relName+"Pointer."+
    //                                                 _td->getAttribute(v)->_name+",";

    //                                             if (!loopFactors[_qc->numberOfViews()])
    //                                                 ++numLoopFactors;
                                                        
    //                                             loopFactors[_qc->numberOfViews()] = 1;
    //                                             allLoopFactors[_qc->numberOfViews()] = 1;
    //                                         }
    //                                     }
    //                                     else
    //                                     {
    //                                         // IF NOT WE NEED TO FIND THE CORRESPONDING VIEW
    //                                         size_t incCounter = incomingCounter;
    //                                         bool found = false;
    //                                         while(!found && incCounter < aggregate->_o[i])
    //                                         {
    //                                             for (size_t n = 0; n <
    //                                                      numberIncomingViews; ++n)
    //                                             {
    //                                                 const size_t& viewID =
    //                                                     aggregate->_incoming[incCounter];

    //                                                 View* incView = _qc->getView(viewID);

    //                                                 if (incView->_fVars[v])
    //                                                 {
    //                                                     /* variable is part of varOrder */
    //                                                     if (groupVariableOrderBitset[group_id][v])
    //                                                     {
    //                                                         // // add V_ID.var_name to the fVarString
    //                                                         // freeVarString += viewName[viewID]+"[lowerptr_"+
    //                                                         //     viewName[viewID]+"["+depthString+"]]."+
    //                                                         //     _td->getAttribute(v)->_name+",";
                                                            
    //                                                         freeVarString += viewName[viewID]+"[lowerptr_"+
    //                                                             viewName[viewID]+"["+depthString+"]]."+
    //                                                             _td->getAttribute(v)->_name+",";
    //                                                     }
    //                                                     else
    //                                                     {
    //                                                         // add V_ID.var_name to the fVarString
    //                                                         freeVarString +=
    //                                                             viewName[viewID]+"[ptr_"+viewName[viewID]+"]."+
    //                                                             _td->getAttribute(v)->_name+",";

    //                                                         if (!loopFactors[viewID])
    //                                                             ++numLoopFactors;
    //                                                         loopFactors[viewID] = 1;
    //                                                         allLoopFactors[viewID] = 1;
    //                                                     }
                                                        
    //                                                     found = true;
    //                                                     break; 
    //                                                 }
    //                                                 incCounter += 2;
    //                                             }
    //                                         }
    //                                     }
    //                                 }
    //                             }

    //                             if (freeVarString.size() == 0)
    //                             {
    //                                 ERROR("We have a problem - function has no freeVars. \n");
    //                                 exit(1);
    //                             }

    //                             if (freeVarString.size() != 0)
    //                                 freeVarString.pop_back();

    //                             // This is just one function that we want to
    //                             // multiply with other functions and 
    //                             // turn function into a string of cpp-code
    //                             localAgg += getFunctionString(function,freeVarString)+"*";
    //                         }
    //                     }

    //                     if (localAgg.size() != 0)
    //                         localAgg.pop_back();
    //                     localAgg += "+";
    //                     ++aggIdx;
    //                 }
    //                 localAgg.pop_back();
                    
    //                 // TODO: Could the if condition be only with loopFactors?
    //                 // If there is no local aggregate function then we simply
    //                 // add the count for this group.
    //                 if (localAgg.size() == 0 && !loopFactors[_qc->numberOfViews()]) 
    //                     localAgg = "upperptr_"+relName+"["+depthString+"]-"+
    //                         "lowerptr_"+relName+"["+depthString+"]+1";
                    
    //                 // if (localAgg.size() == 0 && loopFactors[_qc->numberOfViews()]) 
    //                 //     localAgg = "1";

    //                 // This loop adds the aggregates that come from the views below
    //                 std::string viewAgg = "";
    //                 while(incomingCounter < aggregate->_o[i])
    //                 {
    //                     for (size_t n = 0; n < numberIncomingViews; ++n)
    //                     {
    //                         size_t viewID = aggregate->_incoming[incomingCounter];
    //                         size_t aggID = aggregate->_incoming[incomingCounter+1];

    //                         if (loopFactors[viewID])
    //                             viewAgg += "aggregates_"+viewName[viewID]+"["+std::to_string(aggID)+"]*";
    //                         else
    //                             viewAgg += "aggregates_"+viewName[viewID]+"["+std::to_string(aggID)+"]*";                            
    //                         incomingCounter += 2;
    //                     }
    //                     viewAgg.pop_back();
    //                     viewAgg += "+";
    //                 }
    //                 if (!viewAgg.empty())
    //                     viewAgg.pop_back();
    //                 if (!viewAgg.empty() && !localAgg.empty())
    //                     agg += "("+localAgg+")*("+viewAgg+")+";
    //                 else 
    //                     agg += localAgg+viewAgg+"+"; // One is empty
    //             } // end localAggregate (i.e. one product
    //             agg.pop_back();

    //             if (agg.size() == 0 && !loopFactors[_qc->numberOfViews()]) 
    //                 agg = "upperptr_"+relName+"["+depthString+"]-"+
    //                     "lowerptr_"+relName+"["+depthString+"]+1";
    //             if (agg.size() == 0 && loopFactors[_qc->numberOfViews()]) 
    //                 agg = "1";

    //             if (!_requireHashing[viewID]) 
    //                 aggregateString += offset(3+depth+numLoopFactors)+
    //                     "aggregates_"+viewName[viewID]+"["+std::to_string(aggID)+"] += "+
    //                     agg+";\n";
    //             else
    //                 aggregateString += offset(3+depth+numLoopFactors)+
    //                     viewName[viewID]+"[idx_"+viewName[viewID]+"].aggregates["+
    //                     std::to_string(aggID)+"] += "+agg+";\n"; 
    //         } // end complete Aggregate

    //         std::string findTuple = "";
    //         if (_requireHashing[viewID])
    //         {
    //             // size_t varCounter = 0;
    //             std::string mapVars = "";            
    //             for (size_t v = 0; v < NUM_OF_VARIABLES; ++v)
    //             {
    //                 if (view->_fVars[v])
    //                 {
    //                     if (node._bag[v])
    //                     {
    //                         if (loopFactors[_qc->numberOfViews()]) 
    //                             mapVars += relName+"[ptr_"+relName+"]."+
    //                                 _td->getAttribute(v)->_name+",";
    //                         else
    //                             mapVars += relName+"[lowerptr_"+relName+"["+
    //                                 depthString+"]]."+_td->getAttribute(v)->_name+",";
    //                     }
    //                     else
    //                     {
    //                         for (const size_t& incViewID : incViews)
    //                         {
    //                             View* view = _qc->getView(incViewID);
    //                             if (view->_fVars[v])
    //                             {
    //                                 if (loopFactors[incViewID])
    //                                     mapVars += viewName[incViewID]+"[ptr_"+
    //                                         viewName[incViewID]+"]."+
    //                                         _td->getAttribute(v)->_name+",";
    //                                 else 
    //                                     mapVars += viewName[incViewID]+"[lowerptr_"+
    //                                         viewName[incViewID]+"["+depthString+"]]."+
    //                                         _td->getAttribute(v)->_name+",";
    //                                 break;
    //                             }                                
    //                         }                            
    //                     }
    //                 }
    //             }
    //             mapVars.pop_back();

    //             std::string mapAggs = "", aggRef = "";
    //             aggRef += "&aggregates_"+viewName[viewID]+" = "+
    //                     viewName[viewID]+"[idx_"+viewName[viewID]+"].aggregates;\n";

    //             findTuple += offset(3+depth+numLoopFactors)+"size_t idx_"+viewName[viewID]+" = 0;\n"+
    //                 offset(3+depth+numLoopFactors)+"auto it_"+viewName[viewID]+" = "+viewName[viewID]+"_map.find({"+mapVars+"});\n"+
    //                 offset(3+depth+numLoopFactors)+"if (it_"+viewName[viewID]+" != "+viewName[viewID]+"_map.end())\n"+
    //                 offset(3+depth+numLoopFactors)+"{\n"+
    //                 offset(4+depth+numLoopFactors)+"idx_"+viewName[viewID]+" = it_"+viewName[viewID]+"->second;\n"+
    //                 offset(3+depth+numLoopFactors)+"}\n"+offset(3+depth+numLoopFactors)+"else\n"+
    //                 offset(3+depth+numLoopFactors)+"{\n"+
    //                 offset(4+depth+numLoopFactors)+viewName[viewID]+"_map[{"+mapVars+"}] = "+viewName[viewID]+".size();\n"+
    //                 offset(4+depth+numLoopFactors)+"idx_"+viewName[viewID]+" = "+viewName[viewID]+".size();\n"+
    //                 offset(4+depth+numLoopFactors)+viewName[viewID]+".push_back({"+mapVars+","+mapAggs+"});\n"+
    //                 offset(3+depth+numLoopFactors)+"}\n";
    //         }
            
    //         auto it = aggregateSection.find(loopFactors);
    //         if (it != aggregateSection.end())
    //             it->second += findTuple+aggregateString;
    //         else
    //             aggregateSection[loopFactors] = findTuple+aggregateString; 
    //     } // end viewID

    //     // We are now printing out the aggregates in their respective loops 
    //     std::vector<bool> loopConstructed(_qc->numberOfViews() + 1);
        
    //     std::string pointers = "";

    //     if (allLoopFactors[_qc->numberOfViews()])
    //         pointers += "ptr_"+relName+",";
        
    //     for (size_t viewID = 0; viewID < _qc->numberOfViews(); ++viewID)
    //     {
    //         if (allLoopFactors[viewID])
    //             pointers += "ptr_"+viewName[viewID]+",";
    //     }

    //     if (!pointers.empty())
    //     {
    //         pointers.pop_back();
    //         returnString += offset(3+depth)+"size_t "+pointers+";\n";
    //     }

    //     returnString += offset(3+depth)+relName+"_tuple &"+relName+"Pointer = "+
    //         relName+"[lowerptr_"+relName+"["+depthString+"]];\n";

    //     for (size_t incViewID : incViews)
    //         returnString += offset(3+depth)+"double *aggregates_"+viewName[incViewID]+
    //             " = "+viewName[incViewID]+"[lowerptr_"+viewName[incViewID]+"["+
    //             depthString+"]].aggregates;\n";
        
    //     for (const auto& loopCondition : aggregateSection)
    //     {
    //         size_t loopCounter = 0;
    //         std::string closeLoopString = "";

    //         // Loop over relation
    //         if (loopCondition.first[_qc->numberOfViews()]) 
    //         {   
    //             returnString += offset(3+depth)+"ptr_"+relName+
    //                 " = lowerptr_"+relName+"["+depthString+"];\n"+
    //                 offset(3+depth)+"while(ptr_"+relName+
    //                 " <= upperptr_"+relName+"["+depthString+"])\n"+
    //                 offset(3+depth)+"{\n"+
    //                 offset(4+depth)+relName+"_tuple &"+relName+"Pointer = "+
    //                 relName+"[ptr_"+relName+"];\n";;

    //             closeLoopString = offset(4+depth)+"++ptr_"+relName+";\n"+
    //                 offset(3+depth)+"}\n";

    //             loopConstructed[_qc->numberOfViews()] = 1;
    //             ++loopCounter;
    //         }
    //         for (size_t viewID=0; viewID < _qc->numberOfViews(); ++viewID)
    //         {
    //             if (loopCondition.first[viewID])
    //             {
    //                 // Add loop for view
    //                 returnString += offset(3+depth+loopCounter)+"ptr_"+viewName[viewID]+
    //                     " = lowerptr_"+viewName[viewID]+"["+depthString+"];\n"+
    //                     offset(3+depth+loopCounter)+
    //                     "while(ptr_"+viewName[viewID]+
    //                     " <= upperptr_"+viewName[viewID]+"["+depthString+"])\n"+
    //                     offset(3+depth+loopCounter)+"{\n"+
    //                     offset(4+depth+loopCounter)+"double *aggregates_"+
    //                     viewName[viewID]+" = "+viewName[viewID]+"[ptr_"+
    //                     viewName[viewID]+"].aggregates;\n";

    //                 closeLoopString = offset(4+depth+loopCounter)+
    //                     "++ptr_"+viewName[viewID]+";\n"+
    //                     offset(3+depth+loopCounter)+"}\n"+closeLoopString;

    //                 loopConstructed[viewID] = 1;
    //             }
    //         }
    //         returnString += loopCondition.second + closeLoopString;
    //     }
        
    //     returnString += offset(3+depth)+
    //         "/****************AggregateComputation*******************/\n";
    // }
    
    // for (const size_t& viewID : viewGroups[group_id])
    // {
    //     View* view = _qc->getView(viewID);
        
    //     if (!_requireHashing[viewID] &&
    //         depth+1 == (view->_fVars & groupVariableOrderBitset[group_id]).count())
    //     {
    //         size_t setAggOffset;

    //         if (addTuple)
    //         {
    //             returnString += offset(3+depth)+"if(addTuple_"+viewName[viewID]+")\n"+
    //                 offset(3+depth)+"{\n"+offset(4+depth)+viewName[viewID]+".push_back({";
    //             setAggOffset = 4+depth;
    //         }
    //         else
    //         {
    //             returnString += offset(3+depth)+viewName[viewID]+".push_back({";
    //             setAggOffset = 3+depth;
    //         }
    //         // for each free variable we find find the relation where they are
    //         // from - and then fill the tuple / struct
    //         // size_t varCounter = 0;
    //         for (size_t v = 0; v < NUM_OF_VARIABLES; ++v)
    //         {
    //             if (view->_fVars[v])
    //             {
    //                 // if var is in relation - use relation 
    //                 if (node._bag[v])
    //                 {
    //                     returnString += relName+"[lowerptr_"+relName+
    //                         "["+depthString+"]]."+_td->getAttribute(v)->_name+",";
    //                 }
    //                 else // else find corresponding view 
    //                 {
    //                     for (const size_t& incViewID : incViews) // why not use
    //                                                              // the incViews
    //                                                              // for this view?
    //                     {
    //                         if (_qc->getView(incViewID)->_fVars[v])
    //                         {
    //                             returnString += viewName[incViewID]+
    //                                 "[lowerptr_"+viewName[incViewID]+
    //                                 "["+depthString+"]]."+
    //                                 _td->getAttribute(v)->_name+",";
    //                         }
    //                     }
    //                 }                    
    //             }
    //         }
    //         returnString.pop_back();
    //         returnString += "});\n"+
    //             offset(setAggOffset)+"for (size_t agg = 0; agg < "+
    //             std::to_string(view->_aggregates.size())+"; ++agg)\n"+
    //             offset(setAggOffset+1)+viewName[viewID]+".back().aggregates[agg]"+
    //             " = aggregates_"+viewName[viewID]+"[agg];\n";

    //         if (addTuple)
    //             returnString += offset(3+depth)+"};\n";
    //     }
    // }
#endif 

    // Set lower to upper pointer 
    off = 1;
    if (viewsPerVar[idx+1] == _qc->numberOfViews())
    {
        // Set upper = lower and update ranges
        returnString += offset(3+depth)+
            "lowerptr_"+relName+"["+depthString+"] = "+
            "upperptr_"+relName+"["+depthString+"];\n";
        off = 2;
    }
    for (; off <= numberContributing; ++off)
    {
        size_t viewID = viewsPerVar[idx+off];
        returnString += offset(3+depth)+
            "lowerptr_"+viewName[viewID]+"["+depthString+"] = "+
            "upperptr_"+viewName[viewID]+"["+depthString+"];\n";
    }

    if (numberContributing > 1)
    {        
        // Switch to update the max value and relation pointer
        returnString += offset(3+depth)+
            "switch(order_"+attrName+"[rel["+depthString+"]].second)\n";
        returnString += offset(3+depth)+"{\n";

        // Add switch cases to the switch 
        off = 1;
        if (viewsPerVar[idx+1] == _qc->numberOfViews())
        {            
            returnString += updateMaxCase(depth,relName,attrName,
                                          _parallelizeGroup[group_id]);
            off = 2;
        }

        for (; off <= numberContributing; ++off)
        {
            size_t viewID = viewsPerVar[idx+off];
            returnString += updateMaxCase(depth,viewName[viewID],attrName,false);
        }
        returnString += offset(3+depth)+"}\n";
    }
    else
    {
        std::string factor;
        bool parallel;
        
        if (viewsPerVar[idx+1] == _qc->numberOfViews())
        {
            factor = relName;
            parallel = _parallelizeGroup[group_id];
        }
        else
        {
            factor = viewName[viewsPerVar[idx+1]];
            parallel = false;
        }
        
        returnString += offset(3+depth)+"lowerptr_"+factor+"["+depthString+"] += 1;\n"+
            // if statement
            offset(3+depth)+"if(lowerptr_"+factor+"["+depthString+"] > "+
            getUpperPointer(factor,depth,parallel)+")\n"+
            // body
            offset(4+depth)+"atEnd["+depthString+"] = true;\n"+
            //else
            offset(3+depth)+"else\n"+
            offset(4+depth)+"max_"+attrName+" = "+factor+"[lowerptr_"+factor+"["+
            depthString+"]]."+attrName+";\n";
    }

    // Closing the while loop 
    return returnString + offset(2+depth)+"}\n";
}









std::string CppGenerator::genGroupRelationOrdering(
    const std::string& rel_name, const size_t& depth, const size_t& group_id)
{
    // TODO: FIXME: add the case when there is only two or one reations
    std::string res = offset(2+depth)+
        "/*********** ORDERING RELATIONS *************/\n";

    Attribute* attr = _td->getAttribute(groupVariableOrder[group_id][depth]);

    const std::string& attr_name = attr->_name;
    const std::string attrType = typeToStr(attr->_type);
    const std::string depthString = std::to_string(depth);
    
    size_t idx = depth * (_qc->numberOfViews() + 2);
    size_t numberContributing = groupViewsPerVarInfo[group_id][idx];

    if (numberContributing == 2)
    {
        std::string first, firstID;
        if (groupViewsPerVarInfo[group_id][idx+1] == _qc->numberOfViews())
        {    
            first = rel_name+"[lowerptr_"+rel_name+"["+depthString+"]]."+attr_name;
            firstID = rel_name+"_ID";
        }
        else
        {
            size_t viewID = groupViewsPerVarInfo[group_id][idx+1];
            first = viewName[viewID]+"[lowerptr_"+viewName[viewID]+
                "["+depthString+"]]."+attr_name;
            firstID = viewName[viewID]+"_ID";
        }
        size_t viewID = groupViewsPerVarInfo[group_id][idx+2];
        std::string second = viewName[viewID]+"[lowerptr_"+viewName[viewID]+
                "["+depthString+"]]."+attr_name;
        std::string secondID = viewName[viewID]+"_ID";

        res += offset(2+depth)+"std::pair<"+attrType+", int> order_"+attr_name+"["+
            std::to_string(numberContributing)+"];\n"+
            offset(2+depth)+"if("+first+" < "+second+")\n"+offset(2+depth)+"{\n"+
            offset(3+depth)+"order_"+attr_name+"[0] = std::make_pair("+first+", "+
            firstID+");\n"+
            offset(3+depth)+"order_"+attr_name+"[1] = std::make_pair("+second+", "+
            secondID+");\n"+
            offset(2+depth)+"}\n"+
            offset(2+depth)+"else\n"+offset(2+depth)+"{\n"+
            offset(3+depth)+"order_"+attr_name+"[0] = std::make_pair("+second+", "+
            secondID+");\n"+
            offset(3+depth)+"order_"+attr_name+"[1] = std::make_pair("+first+", "+
            firstID+");\n"+
            offset(2+depth)+"}\n";
    }
    else
    {
        res += offset(2+depth)+"std::pair<"+attrType+", int> order_"+attr_name+"["+
            std::to_string(numberContributing)+"] = \n"+
            offset(3+depth)+"{";
        
        size_t off = 1;
        if (groupViewsPerVarInfo[group_id][idx+1] == _qc->numberOfViews())
        {
            res += "\n"+offset(4+depth)+"std::make_pair("+rel_name+
                "[lowerptr_"+rel_name+"["+depthString+"]]."+attr_name+", "+
                rel_name+"_ID),";
            off = 2;
        }
        
        for (; off <= numberContributing; ++off)
        {
            size_t viewID = groupViewsPerVarInfo[group_id][idx+off];
            res += "\n"+offset(4+depth)+"std::make_pair("+viewName[viewID]+
                "[lowerptr_"+viewName[viewID]+"["+depthString+"]]."+
                attr_name+", "+viewName[viewID]+"_ID),";
        }
        res.pop_back();
        res += "\n"+offset(3+depth)+"};\n";
    
        res += offset(2+depth)+"std::sort(order_"+attr_name+", order_"+attr_name+
            " + "+std::to_string(numberContributing)+",[](const std::pair<"+
            attrType+",int> &lhs, const std::pair<"+attrType+",int> &rhs)\n"+
            offset(3+depth)+"{\n"+offset(4+depth)+"return lhs.first < rhs.first;\n"+
            offset(3+depth)+"});\n";
    }
    

    res += offset(2+depth)+"min_"+attr_name+" = order_"+attr_name+"[0].first;\n";
    res += offset(2+depth)+"max_"+attr_name+" = order_"+attr_name+"["+
        std::to_string(numberContributing-1)+"].first;\n";

    res += offset(2+depth)+"/*********** ORDERING RELATIONS *************/\n";
    return res;
}





bool CppGenerator::requireHash(const size_t& view_id, const size_t& rel_id)
{
    View* view = _qc->getView(view_id);
    size_t orderIdx = 0;
    for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
    {
        if (view->_fVars[var])
        {
            if (sortOrders[rel_id][orderIdx] != var)
                return true;
            ++orderIdx;
        }
    }
    
    return false;
}


bool CppGenerator::resortRelation(const size_t& rel_id, const size_t& view_id)
{
    TDNode* rel = _td->getRelation(rel_id);
    
    size_t orderIdx = 0;
    for (const size_t& var : variableOrder[view_id])
    {
        if (rel->_bag[var])
        {
            if (sortOrders[rel_id][orderIdx] != var)
                return true;
            ++orderIdx;
        }
    }
    
    return false;
}


bool CppGenerator::resortView(const size_t& inc_view_id, const size_t& view_id)
{
    View* view = _qc->getView(inc_view_id);
    size_t orderIdx = 0;
    for (const size_t& var : variableOrder[view_id])
    {
        if (view->_fVars[var])
        {
            if (sortOrders[inc_view_id+_td->numberOfRelations()][orderIdx] != var)
                return true;
            ++orderIdx;
        }
    }

    return false;
}

std::string CppGenerator::genMaxMinValues(const std::vector<size_t>& varOrder)
{
    std::string returnString = "";
    for (size_t var = 0; var < varOrder.size(); ++var)
    {
        Attribute* attr = _td->getAttribute(varOrder[var]);
        returnString += offset(2)+typeToStr(attr->_type)+" min_"+attr->_name+", "+
            "max_"+attr->_name+";\n";
    }
    return returnString;
}

std::string CppGenerator::genPointerArrays(const std::string& rel,
                                           std::string& numOfJoinVars,
                                           bool parallelizeRelation)
{
    if (parallelizeRelation)
        return offset(2)+"size_t lowerptr_"+rel+"["+numOfJoinVars+"] = {}, "+
            "upperptr_"+rel+"["+numOfJoinVars+"] = {}; \n"+
            offset(2)+"upperptr_"+rel+"[0] = upperptr;\n"+
            offset(2)+"lowerptr_"+rel+"[0] = lowerptr;\n";
    else
        return offset(2)+"size_t lowerptr_"+rel+"["+numOfJoinVars+"] = {}, "+
            "upperptr_"+rel+"["+numOfJoinVars+"] = {}; \n"+
            offset(2)+"upperptr_"+rel+"[0] = "+rel+".size()-1;\n";
}


// One Generic Case for Seek Value 
std::string CppGenerator::seekValueCase(size_t depth, const std::string& rel_name,
                                        const std::string& attr_name,
                                        bool parallel)
{
    return offset(4+depth)+"case "+rel_name+"_ID:\n"+
        //if
        offset(5+depth)+"if("+rel_name+"[lowerptr_"+rel_name+"["+
        std::to_string(depth)+"]]."+attr_name+" == max_"+attr_name+")\n"+
        offset(6+depth)+"found["+std::to_string(depth)+"] = true;\n"+
        // else if 
        offset(5+depth)+"else if(max_"+attr_name+" > "+rel_name+"["+
        getUpperPointer(rel_name, depth, parallel)+"]."+attr_name+")\n"+
        offset(6+depth)+"atEnd["+std::to_string(depth)+"] = true;\n"+
        //else 
        offset(5+depth)+"else\n"+offset(5+depth)+"{\n"+
        genFindUpperBound(rel_name,attr_name,depth, parallel)+
        // offset(6+depth)+"findUpperBound("+rel_name+",&"+rel_name+"_tuple::"+
        // attr_name+", max_"+attr_name+",lowerptr_"+rel_name+"["+
        // std::to_string(depth)+"],"+getUpperPointer(rel_name,depth,parallel)+");\n"+
        offset(6+depth)+"max_"+attr_name+" = "+rel_name+"[lowerptr_"+
        rel_name+"["+std::to_string(depth)+"]]."+attr_name+";\n"+
        offset(6+depth)+"rel["+std::to_string(depth)+"] = (rel["+
        std::to_string(depth)+"]+1) % numOfRel["+std::to_string(depth)+"];\n"+
        offset(5+depth)+"}\n"+
        //break
        offset(5+depth)+"break;\n";
}

std::string CppGenerator::updateMaxCase(size_t depth, const std::string& rel_name,
                                        const std::string& attr_name,
                                        bool parallel)
{
    return offset(3+depth)+"case "+rel_name+"_ID:\n"+
        // update pointer
        offset(4+depth)+"lowerptr_"+rel_name+"["+
        std::to_string(depth)+"] += 1;\n"+
        // if statement
        offset(4+depth)+"if(lowerptr_"+rel_name+"["+
        std::to_string(depth)+"] > "+getUpperPointer(rel_name,depth,parallel)+")\n"+
        // body
        offset(5+depth)+"atEnd["+std::to_string(depth)+"] = true;\n"+
        //else
        offset(4+depth)+"else\n"+offset(4+depth)+"{\n"+
        offset(5+depth)+"max_"+attr_name+" = "+rel_name+"[lowerptr_"+
        rel_name+"["+std::to_string(depth)+"]]."+attr_name+";\n"+
        offset(5+depth)+"rel["+std::to_string(depth)+"] = (rel["+
        std::to_string(depth)+"]+ 1) % numOfRel["+std::to_string(depth)+"];\n"+
        offset(4+depth)+"}\n"+offset(4+depth)+"break;\n";
}


std::string CppGenerator::getUpperPointer(const std::string rel_name, size_t depth,
    bool parallelize)
{
    if (depth == 0 && parallelize)
        return "upperptr";
    if (depth == 0)
        return rel_name+".size()-1";
    return "upperptr_"+rel_name+"["+std::to_string(depth-1)+"]";
}

std::string CppGenerator::getLowerPointer(const std::string rel_name, size_t depth)
{
    if (depth == 0)
        return "0";
    return "lowerptr_"+rel_name+"["+std::to_string(depth-1)+"]";
}
    
std::string CppGenerator::updateRanges(size_t depth, const std::string& rel_name,
                                       const std::string& attr_name, bool parallel)
{
    std::string depthString = std::to_string(depth);
    return offset(3+depth)+
        //while statementnd
        "while(upperptr_"+rel_name+"["+depthString+"]<"+
        getUpperPointer(rel_name, depth, parallel)+" && "+
        rel_name+"[upperptr_"+rel_name+"["+depthString+"]+1]."+
        attr_name+" == max_"+attr_name+")\n"+
        //body
        offset(3+depth)+"{\n"+
        offset(4+depth)+"++upperptr_"+rel_name+"["+depthString+"];\n"+
        offset(3+depth)+"}\n";
}

std::string CppGenerator::getFunctionString(Function* f, std::string& fvars)
{
    switch (f->_operation)
    {
        // case Operation::count :
        //     return "f_"+to_string(fid);
    case Operation::sum :
        return fvars;
    case Operation::linear_sum :
        return fvars;
    case Operation::quadratic_sum :
        return fvars+"*"+fvars;
    case Operation::cubic_sum :
        return fvars+"*"+fvars+"*"+fvars;
    case Operation::quartic_sum :
        return fvars+"*"+fvars+"*"+fvars+"*"+fvars;
    case Operation::lr_cont_parameter :
        return "(0.15*"+fvars+")";
    case Operation::lr_cat_parameter :
        return "(0.15)";  // "_param["+fvars+"]"; // // TODO: THIS NEEDS TO BE A
                          // CATEGORICAL PARAMETER - Index?!
    case Operation::indicator_eq : 
        return "("+fvars+" == "+std::to_string(f->_parameter[0])+" ? 1 : 0)";
    case Operation::indicator_lt : 
        return "("+fvars+" <= "+std::to_string(f->_parameter[0])+" ? 1 : 0)";
    case Operation::indicator_neq : 
        return "("+fvars+" != "+std::to_string(f->_parameter[0])+" ? 1 : 0)";
    case Operation::indicator_gt : 
        return "("+fvars+" > "+std::to_string(f->_parameter[0])+" ? 1 : 0)";
    case Operation::dynamic : 
        return  f->_name+"("+fvars+")";
    default : return "f("+fvars+")";
    }
}

std::string CppGenerator::genRunFunction(bool parallelize)
{
    std::string returnString = offset(1)+"void run()\n"+
        offset(1)+"{\n";
    
    returnString += offset(2)+"int64_t startLoading = duration_cast<milliseconds>("+
        "system_clock::now().time_since_epoch()).count();\n\n"+
        offset(2)+"loadRelations();\n\n"+
        offset(2)+"int64_t loadTime = duration_cast<milliseconds>("+
        "system_clock::now().time_since_epoch()).count()-startLoading;\n"+
        offset(2)+"std::cout << \"Data loading: \"+"+
        "std::to_string(loadTime)+\"ms.\\n\";\n"+
        offset(2)+"std::ofstream ofs(\"times.txt\",std::ofstream::out | "+
        "std::ofstream::app);\n"+
        offset(2)+"ofs << loadTime;\n"+
        offset(2)+"ofs.close();\n\n";
    
    returnString += offset(2)+"int64_t startSorting = duration_cast<milliseconds>("+
        "system_clock::now().time_since_epoch()).count();\n\n";

    if (!parallelize)
    {
        for (size_t rel = 0; rel < _td->numberOfRelations(); ++rel)
        {
            TDNode* node = _td->getRelation(rel);
            returnString += offset(2)+"sort"+node->_name+"();\n";
        }
    }
    else
    {
        for (size_t rel = 0; rel < _td->numberOfRelations(); ++rel)
        {
            TDNode* node = _td->getRelation(rel);
            returnString += offset(2)+"std::thread sort"+node->_name+
                "Thread(sort"+node->_name+");\n";
        }

        for (size_t rel = 0; rel < _td->numberOfRelations(); ++rel)
        {
            TDNode* node = _td->getRelation(rel);
            returnString += offset(2)+"sort"+node->_name+"Thread.join();\n";
        }
    }
    
    returnString += "\n"+offset(2)+"int64_t sortingTime = duration_cast<milliseconds>("+
        "system_clock::now().time_since_epoch()).count()-startSorting;\n"+
        offset(2)+"std::cout << \"Data sorting: \" + "+
        "std::to_string(sortingTime)+\"ms.\\n\";\n\n"+
        offset(2)+"ofs.open(\"times.txt\",std::ofstream::out | "+
        "std::ofstream::app);\n"+
        offset(2)+"ofs << \"\\t\" << sortingTime;\n"+
        offset(2)+"ofs.close();\n\n";

    returnString += offset(2)+"int64_t startProcess = duration_cast<milliseconds>("+
        "system_clock::now().time_since_epoch()).count();\n\n";

// #ifndef OPTIMIZED    
//     for (size_t view = 0; view < _qc->numberOfViews(); ++view)
//         returnString += offset(2)+"computeView"+std::to_string(view)+"();\n";
// #else
    if (!parallelize)
    {
        for (size_t group = 0; group < viewGroups.size(); ++group)
        {
            // if (_parallelizeGroup[group])
            //     returnString += offset(2)+"computeGroup"+std::to_string(group)+
            //         "Parallelized();\n";
            // else
            //     returnString +=
            //     offset(2)+"computeGroup"+std::to_string(group)+"();\n";
            
            returnString += offset(2)+"computeGroup"+std::to_string(group)+"();\n";
        }
    }
    else
    {
        std::vector<bool> joinedGroups(viewGroups.size());
    
        for (size_t group = 0; group < viewGroups.size(); ++group)
        {
            for (const size_t& viewID : groupIncomingViews[group])
            {
                // find group that contains this view!
                size_t otherGroup = viewToGroupMapping[viewID];
                if (!joinedGroups[otherGroup])
                {
                    returnString += offset(2)+"group"+std::to_string(otherGroup)+
                        "Thread.join();\n";
                    joinedGroups[otherGroup] = 1;
                }
            }

            // if (_parallelizeGroup[group])            
            //     returnString += offset(2)+"std::thread group"+std::to_string(group)+
            //         "Thread(computeGroup"+std::to_string(group)+"Parallelized);\n";
            // else
            //     returnString += offset(2)+"std::thread group"+std::to_string(group)+
            //         "Thread(computeGroup"+std::to_string(group)+");\n";

            returnString += offset(2)+"std::thread group"+std::to_string(group)+
                    "Thread(computeGroup"+std::to_string(group)+");\n";            
        }

        for (size_t group = 0; group < viewGroups.size(); ++group)
        {
            if (!joinedGroups[group])
                returnString += offset(2)+"group"+std::to_string(group)+
                    "Thread.join();\n";
        }
    }
// #endif
    // returnString += "\n"+offset(2)+"std::cout << \"Data process: \"+"+
    //     "std::to_string(duration_cast<milliseconds>("+
    //     "system_clock::now().time_since_epoch()).count()-startProcess)+\"ms.\\n\";\n\n";
    
    returnString += "\n"+offset(2)+"int64_t processTime = duration_cast<milliseconds>("+
        "system_clock::now().time_since_epoch()).count()-startProcess;\n"+
        offset(2)+"std::cout << \"Data process: \"+"+
        "std::to_string(processTime)+\"ms.\\n\";\n"+
        offset(2)+"ofs.open(\"times.txt\",std::ofstream::out | std::ofstream::app);\n"+
        offset(2)+"ofs << \"\\t\" << processTime;\n";
    
    
#ifdef BENCH_INDIVIDUAL
    for (size_t view = 0; view < _qc->numberOfViews(); ++view)
    {
        returnString += offset(2)+"std::cout << \""+viewName[view]+": \" << "+
            viewName[view]+".size() << std::endl;\n";
    }
#endif
    if (_hasApplicationHandler)
    {
        returnString += offset(2)+"ofs.close();\n\n"+
            offset(2)+"int64_t appProcess = duration_cast<milliseconds>("+
            "system_clock::now().time_since_epoch()).count();\n\n";

#ifdef BENCH_INDIVIDUAL
        returnString += offset(2)+"std::cout << \"run Application:\\n\";\n";
#endif
        returnString += offset(2)+"runApplication();\n"; 
    }
    else {        
        returnString += offset(2)+"ofs << std::endl;\n"+
            offset(2)+"ofs.close();\n\n";
    }
    
    return returnString+offset(1)+"}\n";
}

// std::string CppGenerator::genRunMultithreadedFunction()
// {
//     std::string returnString = "#ifdef MULTITHREAD\n"+
//         offset(1)+"void runMultithreaded()\n"+
//         offset(1)+"{\n";

//     returnString += offset(2)+
//         "int64_t startLoading = duration_cast<milliseconds>("+
//         "system_clock::now().time_since_epoch()).count();\n\n"+
//         offset(2)+"loadRelations();\n\n"+
//         offset(2)+"std::cout << \"Data loading: \" + std::to_string("+
//         "duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()-"+
//         "startLoading)+\"ms.\\n\";\n\n";
    
//     returnString += offset(2)+"int64_t startSorting = duration_cast<milliseconds>("+
//         "system_clock::now().time_since_epoch()).count();\n\n";
    
//     for (size_t rel = 0; rel < _td->numberOfRelations(); ++rel)
//     {
//         TDNode* node = _td->getRelation(rel);
//         returnString += offset(2)+"std::thread sort"+node->_name+
//             "Thread(sort"+node->_name+");\n";
//     }

//     for (size_t rel = 0; rel < _td->numberOfRelations(); ++rel)
//     {
//         TDNode* node = _td->getRelation(rel);
//         returnString += offset(2)+"sort"+node->_name+"Thread.join();\n";
//     }

//     returnString += offset(2)+"std::cout << \"Data sorting: \" + std::to_string("+
//         "duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()-"+
//         "startSorting)+\"ms.\\n\";\n\n";

//     returnString += offset(2)+"int64_t startProcess = duration_cast<milliseconds>("+
//         "system_clock::now().time_since_epoch()).count();\n\n";

// #ifndef OPTIMIZED
//     std::vector<bool> joinedViews(_qc->numberOfViews());
//     for (size_t view = 0; view < _qc->numberOfViews(); ++view)
//     {
//         // TDNode* relation = _td->getRelation(_qc->getView(view)->_origin);
//         // if (!joinedRelations[relation->_id])
//         // {
//         //     returnString += offset(2)+"sort"+relation->_name+"Thread.join();\n";
//         //     joinedRelations[relation->_id] = 1;
//         // }

//         for (const size_t& otherView : incomingViews[view])
//         {
//             // find group that contains this view!
//             if (!joinedViews[otherView])
//             {
//                 returnString += offset(2)+"view"+std::to_string(otherView)+
//                     "Thread.join();\n";
//                 joinedViews[otherView] = 1;
//             }
//         }
//         returnString += offset(2)+"std::thread view"+std::to_string(view)+
//             "Thread(computeView"+std::to_string(view)+");\n";
//     }

//     for (size_t view = 0; view < _qc->numberOfViews(); ++view)
//     {
//         if(!joinedViews[view])
//             returnString += offset(2)+"view"+std::to_string(view)+"Thread.join();\n";
//     }
    
// #else
//     std::vector<bool> joinedGroups(viewGroups.size());
    
//     for (size_t group = 0; group < viewGroups.size(); ++group)
//     {
//         for (const size_t& viewID : groupIncomingViews[group])
//         {
//             // find group that contains this view!
//             size_t otherGroup = viewToGroupMapping[viewID];
//             if (!joinedGroups[otherGroup])
//             {
//                 returnString += offset(2)+"group"+std::to_string(otherGroup)+
//                     "Thread.join();\n";
//                 joinedGroups[otherGroup] = 1;
//             }
//         }

//         if (_parallelizeGroup[group])            
//             returnString += offset(2)+"std::thread group"+std::to_string(group)+
//                 "Thread(computeGroup"+std::to_string(group)+"Parallelized);\n";
//         else
//          returnString += offset(2)+"std::thread group"+std::to_string(group)+
//                 "Thread(computeGroup"+std::to_string(group)+");\n";
//     }

//     for (size_t group = 0; group < viewGroups.size(); ++group)
//     {
//         if (!joinedGroups[group])
//             returnString += offset(2)+"group"+std::to_string(group)+"Thread.join();\n";
//     }
    
// #endif

//     returnString += "\n"+offset(2)+"std::cout << \"Data process: \"+"+
//         "std::to_string(duration_cast<milliseconds>("+
//         "system_clock::now().time_since_epoch()).count()-startProcess)+\"ms.\\n\";\n\n";

//     for (size_t view = 0; view < _qc->numberOfViews(); ++view)
//     {
//         returnString += offset(2)+"std::cout << \""+viewName[view]+": \" << "+
//             viewName[view]+".size() << std::endl;\n";
//     }
    
//     return returnString+offset(1)+"}\n#endif\n";
// }


std::string CppGenerator::genFindUpperBound(const std::string& rel_name,
                                            const std::string& attrName,
                                            size_t depth, bool parallel)
{
    const std::string depthString = std::to_string(depth);
    const std::string upperPointer = getUpperPointer(rel_name,depth,parallel);
    
    std::string returnString = offset(6+depth)+"min_"+attrName+" = "+
        rel_name+"[lowerptr_"+rel_name+"["+depthString+"]]."+attrName +";\n"+
        offset(6+depth)+"size_t leap = 1;\n"+
        offset(6+depth)+"while (min_"+attrName+" < max_"+attrName+" && "+
        "lowerptr_"+rel_name+"["+depthString+"] <= "+upperPointer+")\n"+
        offset(6+depth)+"{\n"+
        offset(7+depth)+"lowerptr_"+rel_name+"["+depthString+"] += leap;\n"+
        offset(7+depth)+"leap *= 2;\n"+
        offset(7+depth)+"if(lowerptr_"+rel_name+"["+depthString+
        "] < "+upperPointer+")\n"+offset(7+depth)+"{\n"+
        offset(8+depth)+"min_"+attrName+" = "+rel_name+"[lowerptr_"+rel_name+"["+
        depthString+"]]."+attrName+";\n"+
        offset(7+depth)+"}\n"+offset(7+depth)+"else\n"+offset(7+depth)+"{\n"+
        offset(8+depth)+"lowerptr_"+rel_name+"["+depthString+"] = "+upperPointer+";\n"+
        offset(8+depth)+"break;\n"+
        offset(7+depth)+"}\n"+
        offset(6+depth)+"}\n";

    /*
     * When we found an upper bound; to find the least upper bound;
     * use binary search to backtrack.
     */ 
    returnString += offset(6+depth)+"/* Backtrack with binary search */\n"+
        offset(6+depth)+"if (leap > 2 && max_"+attrName+" <= "+rel_name+
        "[lowerptr_"+rel_name+"["+depthString+"]]."+attrName+")\n"+offset(6+depth)+"{\n"+
        offset(7+depth)+"leap /= 2;\n"+
        offset(7+depth)+"size_t high = lowerptr_"+rel_name+"["+depthString+"], low = "+
        "lowerptr_"+rel_name+"["+depthString+"] - leap, mid = 0;\n"+
        offset(7+depth)+"while (high > low && high != low)\n"+offset(7+depth)+"{\n"+
        offset(8+depth)+"mid = (high + low) / 2;\n"+
        offset(8+depth)+"if (max_"+attrName+" > "+rel_name+"[mid - 1]."+attrName+" && "+
        "max_"+attrName+" <= "+rel_name+"[mid]."+attrName+")\n"+offset(8+depth)+"{\n"+
        offset(9+depth)+"lowerptr_"+rel_name+"["+depthString+"] = mid;\n"+
        offset(9+depth)+"break;\n"+offset(8+depth)+"}\n"+
        offset(8+depth)+"else if (max_"+attrName+" <= "+rel_name+"[mid]."+attrName+")\n"+
        offset(9+depth)+"high = mid - 1;\n"+
        offset(8+depth)+"else \n"+offset(9+depth)+"low = mid + 1;\n"+
        offset(7+depth)+"}\n"+
        offset(7+depth)+"mid = (high + low) / 2;\n"+
        offset(7+depth)+"if ("+rel_name+"[mid-1]."+attrName+" >= max_"+attrName+")\n"+
        offset(8+depth)+"mid -= 1;\n"+
        offset(7+depth)+"lowerptr_"+rel_name+"["+depthString+"] = mid;\n"+
        offset(6+depth)+"}\n";

    return returnString;
}

std::string CppGenerator::genTestFunction()
{
    std::string returnString = "#ifdef TESTING\n"+
        offset(1)+"void ouputViewsForTesting()\n"+
        offset(1)+"{\n"+offset(2)+"std::ofstream ofs(\"test.out\");\n"+
        offset(2)+"ofs << std::fixed << std::setprecision(2);\n";
    
    for (size_t viewID = 0; viewID < _qc->numberOfViews(); ++viewID)
    {
        View* view = _qc->getView(viewID);

        if (view->_origin != view->_destination)
            continue;

        std::string fields = "";
        for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
        {
            if (view->_fVars[var])
            {
                Attribute* attr = _td->getAttribute(var);
                fields += " << tuple."+attr->_name+" <<\"|\"";
            }
        }

        for (size_t agg = 0; agg < view->_aggregates.size(); ++agg)
            fields += " << tuple.aggregates["+std::to_string(agg)+"] << \"|\"";

        fields.pop_back();
        fields.pop_back();
        fields.pop_back();
        
        returnString += offset(2)+"for (size_t i=0; i < "+viewName[viewID]+
            ".size(); ++i)\n"+offset(2)+"{\n"+offset(3)+
            viewName[viewID]+"_tuple& tuple = "+viewName[viewID]+"[i];\n"+
            offset(3)+"ofs "+fields+"\"\\n\";\n"+offset(2)+"}\n";
    }

    returnString += offset(2)+"ofs.close();\n"+offset(1)+"}\n"+"#endif\n";
    return returnString;
}


std::string CppGenerator::genDumpFunction()
{
    std::string returnString = "#ifdef DUMP_OUTPUT\n"+
        offset(1)+"void dumpOutputViews()\n"+offset(1)+"{\n"+
        offset(2)+"std::ofstream ofs;\n";
    
    for (size_t viewID = 0; viewID < _qc->numberOfViews(); ++viewID)
    {
        View* view = _qc->getView(viewID);

        if (view->_origin != view->_destination)
            continue;

        returnString += offset(2)+"ofs.open(\"output/"+
            viewName[viewID]+".tbl\");\n"+
            offset(2)+"ofs << \""+std::to_string(view->_fVars.count())+" "+
            std::to_string(view->_aggregates.size())+"\\n\";\n";
            
        std::string fields = "";
        for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
        {
            if (view->_fVars[var])
            {
                Attribute* attr = _td->getAttribute(var);
                fields += " << tuple."+attr->_name+" <<\"|\"";
            }
        }

        for (size_t agg = 0; agg < view->_aggregates.size(); ++agg)
            fields += " << tuple.aggregates["+std::to_string(agg)+"] << \"|\"";

        fields.pop_back();
        fields.pop_back();
        fields.pop_back();
        
        returnString += offset(2)+"for (size_t i=0; i < "+viewName[viewID]+
            ".size(); ++i)\n"+offset(2)+"{\n"+offset(3)+
            viewName[viewID]+"_tuple& tuple = "+viewName[viewID]+"[i];\n"+
            offset(3)+"ofs "+fields+"\"\\n\";\n"+offset(2)+"}\n";
    }

    returnString += offset(2)+"ofs.close();\n"+offset(1)+"}\n"+"#endif\n";
    return returnString;
}




#ifdef OLD
std::string CppGenerator::genComputeViewFunction(size_t view_id)
{
    // for the entire Aggregate - collect all incoming views and join on the
    // join aggregates from the relation -- let's first create the join
    // output - then we go from there
    View* view = _qc->getView(view_id);
    TDNode* baseRelation =_td->getRelation(view->_origin);
    const std::string& relName = baseRelation->_name;

    std::vector<size_t>& varOrder = variableOrder[view_id];
    std::vector<size_t>& viewsPerVar = viewsPerVarInfo[view_id];
    std::vector<size_t>& incViews = incomingViews[view_id];

    /**************************/
    // Now create the actual join code for this view 
    // Compare with template code
    /**************************/

    std::string returnString = offset(1)+"void computeView"+
        std::to_string(view_id)+"()\n"+offset(1)+"{\n";

    // ************************************************
    // SORT THE RELATION AND VIEW IF NECESSARY!
    /* 
     * Compares viewSortOrder with the varOrder required - if orders are
     * different we resort 
     */ 
    if (resortRelation(view->_origin, view_id))
    {
        returnString += offset(2)+"std::sort("+relName+".begin(),"+
            relName+".end(),[ ](const "+relName+"_tuple& lhs, const "+relName+
            "_tuple& rhs)\n"+offset(2)+"{\n";

        size_t orderIdx = 0;
        for (const size_t& var : varOrder)
        {
            if (baseRelation->_bag[var])
            {
                const std::string& attrName = _td->getAttribute(var)->_name;
                returnString += offset(3)+"if(lhs."+attrName+" != rhs."+attrName+")\n"+
                    offset(4)+"return lhs."+attrName+" < rhs."+attrName+";\n";
                // Reset the relSortOrder array for future calls 
                sortOrders[view->_origin][orderIdx] = var;
                ++orderIdx;
            }
        }
        returnString += offset(3)+"return false;\n"+offset(2)+"});\n";
    }

    for (const size_t& incViewID : incViews)
    {
        /*
         * Compares viewSortOrder with the varOrder required - if orders are
         * different we resort
         */
        if (resortView(incViewID, view_id))
        {
            returnString += offset(2)+"std::sort("+viewName[incViewID]+".begin(),"+
                viewName[incViewID]+".end(),"+"[ ](const "+viewName[incViewID]+
                "_tuple& lhs, const "+viewName[incViewID]+"_tuple& rhs)\n"+
                offset(2)+"{\n";

            View* incView = _qc->getView(incViewID);
            size_t orderIdx = 0;
            for (const size_t& var : varOrder)
            {
                if (incView->_fVars[var])
                {
                    const std::string& attrName = _td->getAttribute(var)->_name;
                    returnString += offset(3);
                    if (orderIdx+1 < incView->_fVars.count())
                        returnString += "if(lhs."+attrName+" != rhs."+
                            attrName+")\n"+offset(4);
                    returnString += "return lhs."+attrName+" < rhs."+attrName+";\n";
                    // Reset the viewSortOrder array for future calls
                    sortOrders[incViewID + _td->numberOfRelations()][orderIdx] = var;
                    ++orderIdx;
                }
            }
            //returnString += offset(3)+"return 0;\n";
            
            returnString += offset(2)+"});\n";
        }
    }
    // ************************************************

    // Generate min and max values for the join varibales 
    // returnString += genMaxMinValues(view_id);
    returnString += genMaxMinValues(varOrder);

    std::string numOfJoinVarString = std::to_string(varOrder.size());

    // Creates the relation-ptr and other helper arrays for the join algo
    // Need to know how many relations per variable
    std::string arrays = "";
    for (size_t v = 0; v < varOrder.size(); ++v)
    {
        size_t idx = v * (_qc->numberOfViews() + 2);
        arrays += std::to_string(viewsPerVar[idx])+",";
    }   
    arrays.pop_back();

    returnString += offset(2) +"size_t rel["+numOfJoinVarString+"] = {}, "+
        "numOfRel["+numOfJoinVarString+"] = {"+arrays+"};\n" + 
        offset(2) + "bool found["+numOfJoinVarString+"] = {}, "+
        "atEnd["+numOfJoinVarString+"] = {};\n";

    // Lower and upper pointer for the base relation
    returnString += genPointerArrays(baseRelation->_name, numOfJoinVarString, false);
        
    // Lower and upper pointer for each incoming view
    for (const size_t& viewID : incViews)            
        returnString += genPointerArrays(viewName[viewID],numOfJoinVarString, false);
    
    if (view->_fVars.count() == 0)
    {
        returnString += offset(2)+"bool addTuple = false;\n";
        returnString += offset(2)+"double";
        for (size_t agg = 0; agg < view->_aggregates.size(); ++agg)
            returnString += " agg_"+std::to_string(agg)+" = 0.0,";
        returnString.pop_back();
        returnString += ";\n";
    }
    
    // for each join var --- create the string that does the join
    returnString += genLeapfrogJoinCode(view_id, 0);

    if (view->_fVars.count() == 0)
    {
        returnString += offset(2)+"if (addTuple)\n";
        returnString += offset(3)+viewName[view_id]+
            ".push_back({";
        for (size_t agg = 0; agg < view->_aggregates.size(); ++agg)
            returnString += "agg_"+std::to_string(agg)+",";
        returnString.pop_back();
        returnString += "});\n";
    }
    returnString += offset(1)+"}\n";

    size_t orderIdx = 0;
    for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
    {
        if (view->_fVars[var])
        {
            sortOrders[view_id+_td->numberOfRelations()][orderIdx] =
                varOrder[orderIdx];
            ++orderIdx;
        }
    }
    return returnString;
}


void CppGenerator::createVariableOrder()
{
    DINFO("Generate variableOrder \n");
    
    viewsPerNode = new std::vector<size_t>[_td->numberOfRelations()]();

    variableOrder = new std::vector<size_t>[_qc->numberOfViews()]();
    incomingViews = new std::vector<size_t>[_qc->numberOfViews()]();
    viewsPerVarInfo = new std::vector<size_t>[_qc->numberOfViews()];
    variableOrderBitset.resize(_qc->numberOfViews());

    for (size_t viewID = 0; viewID < _qc->numberOfViews(); ++viewID)
    {
        View* view = _qc->getView(viewID);
        TDNode* baseRelation = _td->getRelation(view->_origin);
        size_t numberIncomingViews =
            (view->_origin == view->_destination ? baseRelation->_numOfNeighbors :
             baseRelation->_numOfNeighbors - 1);
        
        // for free vars push them on the variableOrder
        for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
            if (view->_fVars[var])
                variableOrder[viewID].push_back(var);
        
        // Now we find which variables are joined on and which views
        // contribute to this view
        var_bitset joinVars;
        std::vector<bool> incViewBitset(_qc->numberOfViews());
        for (size_t aggNo = 0; aggNo < view->_aggregates.size(); ++aggNo)
        {            
            Aggregate* aggregate = view->_aggregates[aggNo];
            size_t incOffset = 0;
            
            // First find the all views that contribute to this Aggregate
            for (size_t i = 0; i < aggregate->_agg.size(); ++i)
            {
                for (size_t j = 0; j < numberIncomingViews; ++j)
                {
                    const size_t& incViewID = aggregate->_incoming[incOffset].first;
                    // Add the intersection of the view and the base
                    // relation to joinVars 
                    joinVars |= (_qc->getView(incViewID)->_fVars &
                                 baseRelation->_bag);
                    // Indicate that this view contributes to some aggregate
                    incViewBitset[incViewID] = 1;
                    ++incOffset;
                }
            }
        }

        // For all join vars that are not free vars, add them to the
        // variable order 
        var_bitset additionalVars = joinVars & ~view->_fVars;
        for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
            if (additionalVars[var])
                variableOrder[viewID].push_back(var);

        variableOrderBitset[viewID] = (view->_fVars | joinVars);
        viewsPerVarInfo[viewID].resize(variableOrder[viewID].size() *
                                       (_qc->numberOfViews() + 2), 0);

        // For each variable, check if the baseRelation contains this variable
        for (size_t var=0; var < variableOrder[viewID].size(); ++var)
        {
            if (baseRelation->_bag[variableOrder[viewID][var]])
            {
                size_t idx = var * (_qc->numberOfViews() + 2);
                size_t& off = viewsPerVarInfo[viewID][idx];
                viewsPerVarInfo[viewID][idx+1] = _qc->numberOfViews();
                ++off;
            }
        }
        
        // For each incoming view, check if it contains any vars from the
        // variable order and then add it to the corresponding info
        for (size_t incViewID=0; incViewID < _qc->numberOfViews(); ++incViewID)
        {
            if (incViewBitset[incViewID])
            {
                incomingViews[viewID].push_back(incViewID);
                const var_bitset& viewFVars = _qc->getView(incViewID)->_fVars;
                    
                for (size_t var=0; var < variableOrder[viewID].size(); ++var)
                {
                    if (viewFVars[variableOrder[viewID][var]])
                    {
                        size_t idx = var * (_qc->numberOfViews() + 2);
                        size_t& off = viewsPerVarInfo[viewID][idx];
                        viewsPerVarInfo[viewID][idx+off+1] = incViewID;
                        ++off;
                    }
                }
            }
        }

        // Keep track of the views that origin from this node 
        viewsPerNode[view->_origin].push_back(viewID);
        
/* PRINT OUT */
        // std::cout << viewID << " : ";
        // for(size_t& n : variableOrder[viewID]) std::cout << n <<" ";
        // std::cout << "\n";
        // size_t i = 0;
        // for (const size_t& n : viewsPerVarInfo[viewID])
        // {
        //     if (i % (_qc->numberOfViews() + 2) == 0) std::cout << "| ";
        //     std::cout << n <<" ";
        //     ++i;
        // }
        // std::cout << "\n";
/* PRINT OUT */
    }
}

std::string CppGenerator::genRelationOrdering(
    const std::string& rel_name, const size_t& depth, const size_t& view_id)
{
    std::string res = offset(2+depth)+
        "/*********** ORDERING RELATIONS *************/\n";

    Attribute* attr = _td->getAttribute(variableOrder[view_id][depth]);

    const std::string& attr_name = attr->_name;
    const std::string attrType = typeToStr(attr->_type);
    const std::string depthString = std::to_string(depth);
    
    size_t idx = depth * (_qc->numberOfViews() + 2);
    size_t numberContributing = viewsPerVarInfo[view_id][idx];
    if (numberContributing == 2)
    {
        std::string first, firstID;
        if (viewsPerVarInfo[view_id][idx+1] == _qc->numberOfViews())
        {    
            first = rel_name+"[lowerptr_"+rel_name+"["+depthString+"]]."+attr_name;
            firstID = rel_name+"_ID";
        }
        else
        {
            size_t viewID = viewsPerVarInfo[view_id][idx+1];
            first = viewName[viewID]+"[lowerptr_"+viewName[viewID]+
                "["+depthString+"]]."+attr_name;
            firstID = viewName[viewID]+"_ID";
        }
        size_t viewID = viewsPerVarInfo[view_id][idx+2];
        std::string second = viewName[viewID]+"[lowerptr_"+viewName[viewID]+
                "["+depthString+"]]."+attr_name;
        std::string secondID = viewName[viewID]+"_ID";

        res += offset(2+depth)+"std::pair<"+attrType+", int> order_"+attr_name+"["+
            std::to_string(numberContributing)+"];\n"+
            offset(2+depth)+"if("+first+" < "+second+")\n"+offset(2+depth)+"{\n"+
            offset(3+depth)+"order_"+attr_name+"[0] = std::make_pair("+first+", "+
            firstID+");\n"+
            offset(3+depth)+"order_"+attr_name+"[1] = std::make_pair("+second+", "+
            secondID+");\n"+
            offset(2+depth)+"}\n"+
            offset(2+depth)+"else\n"+offset(2+depth)+"{\n"+
            offset(3+depth)+"order_"+attr_name+"[0] = std::make_pair("+second+", "+
            secondID+");\n"+
            offset(3+depth)+"order_"+attr_name+"[1] = std::make_pair("+first+", "+
            firstID+");\n"+
            offset(2+depth)+"}\n";
    }
    else
    {    
        res += offset(2+depth)+"std::pair<"+attrType+", int> order_"+attr_name+"["+
            std::to_string(numberContributing)+"] = \n"+
            offset(3+depth)+"{";
        
        size_t off = 1;
        if (viewsPerVarInfo[view_id][idx+1] == _qc->numberOfViews())
        {
            res += "\n"+offset(4+depth)+"std::make_pair("+rel_name+
                "[lowerptr_"+rel_name+"["+depthString+"]]."+attr_name+", "+
                rel_name+"_ID),";
            off = 2;
        }
        
        for (; off <= numberContributing; ++off)
        {
            size_t viewID = viewsPerVarInfo[view_id][idx+off];
            res += "\n"+offset(4+depth)+"std::make_pair("+viewName[viewID]+
                "[lowerptr_"+viewName[viewID]+"["+depthString+"]]."+
                attr_name+", "+viewName[viewID]+"_ID),";
        }
        res.pop_back();
        res += "\n"+offset(3+depth)+"};\n";
    
        res += offset(2+depth)+"std::sort(order_"+attr_name+", order_"+attr_name+
            " + "+std::to_string(numberContributing)+",[](const std::pair<"+
            attrType+",int> &lhs, const std::pair<"+attrType+",int> &rhs)\n"+
            offset(3+depth)+"{\n"+offset(4+depth)+"return lhs.first < rhs.first;\n"+
            offset(3+depth)+"});\n";
    }
    
    res += offset(2+depth)+"min_"+attr_name+" = order_"+attr_name+"[0].first;\n";
    res += offset(2+depth)+"max_"+attr_name+" = order_"+attr_name+"["+
        std::to_string(numberContributing-1)+"].first;\n";

    res += offset(2+depth)+"/*********** ORDERING RELATIONS *************/\n";
    return res;
}

std::string CppGenerator::genAggregateString(
    const std::vector<std::string>& aggRegister,const std::vector<dyn_bitset>& loopReg,
    dyn_bitset consideredLoops, dyn_bitset& includableAggs, size_t depth)
{
    std::string returnString = "", localAggregates = "";
    std::string depthString = std::to_string(depth);
    
    size_t numOfLoops = consideredLoops.count();
    dyn_bitset nextLoops(_qc->numberOfViews());
    
    for (size_t agg = 0; agg < aggRegister.size(); agg++)
    {
        if (includableAggs[agg])
        {
            if ((consideredLoops & loopReg[agg]) == consideredLoops)
            {
                dyn_bitset remainingLoops = loopReg[agg] & ~consideredLoops;
                if (remainingLoops.any())
                {
                    // need to continue with this the next loop
                    for (size_t view=0; view < _qc->numberOfViews(); ++view)
                    {
                        if (remainingLoops[view])
                        {
                            nextLoops.set(view);
                            break;
                        }
                    }
                }
                else
                {
                    // satisfyingAggs.set(agg);
                    localAggregates += offset(3+depth+numOfLoops)+
                        "aggregateRegister_"+depthString+
                        "["+std::to_string(agg)+"] += "+aggRegister[agg];
                    
                    includableAggs.reset(agg);
                }
            }
        }
    }

    if (nextLoops.any())
    {
        for (size_t viewID=0; viewID < _qc->numberOfViews(); ++viewID)
        {
            if (nextLoops[viewID])
            {
                returnString +=
                    // offset(3+depth+numOfLoops)+viewName[viewID]+
                    // "_tuple &"+viewName[viewID]+"Tuple = "+viewName[viewID]+
                    // "[lowerptr_"+viewName[viewID]+"["+depthString+"]];\n"+
                    offset(3+depth+numOfLoops)+"ptr_"+viewName[viewID]+
                    " = lowerptr_"+viewName[viewID]+"["+depthString+"];\n"+
                    offset(3+depth+numOfLoops)+"while(ptr_"+viewName[viewID]+
                    " <= upperptr_"+viewName[viewID]+"["+depthString+"])\n"+
                    offset(3+depth+numOfLoops)+"{\n"+
                    offset(4+depth+numOfLoops)+viewName[viewID]+
                    "_tuple &"+viewName[viewID]+"Tuple = "+viewName[viewID]+
                    "[ptr_"+viewName[viewID]+"];\n"+
                    offset(4+depth+numOfLoops)+"double *aggregates_"+viewName[viewID]+
                    " = "+viewName[viewID]+"Tuple.aggregates;\n";

                dyn_bitset nextConsideredLoops = consideredLoops;
                nextConsideredLoops.set(viewID);

                returnString += genAggregateString(aggRegister,loopReg,
                    nextConsideredLoops, includableAggs, depth);
            
                returnString += offset(4+depth+numOfLoops)+
                    "++ptr_"+viewName[viewID]+";\n"+
                    offset(3+depth+numOfLoops)+"}\n";
            }
        }
    }
    
    return returnString + localAggregates;
}

std::string CppGenerator::genFinalAggregateString(
    const std::vector<std::string>& aggRegister,const std::vector<dyn_bitset>& loopReg,
    dyn_bitset consideredLoops, dyn_bitset& includableAggs, size_t depth,
    std::vector<size_t>& includableViews, dyn_bitset& addedViews, size_t offDepth)
{
    size_t numOfLoops = consideredLoops.count();
    dyn_bitset nextLoops(_qc->numberOfViews());
    
    std::string depthString = std::to_string(depth);
    
    std::string aggregateString = "";
    for (size_t agg = 0; agg < aggRegister.size(); ++agg)
    {
        if (includableAggs[agg])
        {
            // TODO: TODO: TODO: IS THIS CORRECT ??  ?? ?? 
            if ((loopReg[agg] & consideredLoops) == consideredLoops)
            {
                // add the aggregate at this level
                aggregateString += offset(offDepth+numOfLoops)+
                    "aggregateRegister_"+depthString+
                    "["+std::to_string(agg)+"] = "+aggRegister[agg];
                includableAggs.reset(agg);
            }
        }
    }

    std::string viewString = "";
    std::string outputString = "";

    for (const size_t& viewID : includableViews)
    {
        // std::cout << "includableView: "<< viewID << std::endl;
        // check if this view has the consideredViews
        if ((viewLoopFactors[viewID] & consideredLoops) == consideredLoops)
        {
            // std::cout << "includableView: "<< viewID << std::endl;
            
            // should wrap it in the loop
            dyn_bitset remainingLoops = viewLoopFactors[viewID] & ~consideredLoops;

            // check if there are any aggregates that can be computed here
            // if so add it in
            
            // recurse to the next loop(s)
            // -- first check any next view loops
            // -- then check for any next agg loops

            if (remainingLoops.any())
            {
                // std::cout << "HERE remainingLoops.any()\n";
                
                // need to continue with this the next loop
                for (size_t view=0; view < _qc->numberOfViews(); ++view)
                {
                    if (remainingLoops[view])
                    {
                        nextLoops.set(view);
                        break;
                    }
                }
            }
            else
            {
                // check if any of the aggregates of this view need an
                // additional loop
                for (size_t& finalAgg : newFinalAggregates[viewID])
                {
                    // check if this aggregate can still be added
                    if (!includableAggs[finalAgg])
                        continue;

                    dyn_bitset remainingAggLoops = loopReg[finalAgg] & ~consideredLoops;

                    if (remainingAggLoops.any())
                    {
                        // need to continue with this the next loop
                        for (size_t view=0; view < _qc->numberOfViews(); ++view)
                        {
                            if (remainingAggLoops[view])
                            {
                                nextLoops.set(view);
                                break;
                            }
                        }
                    }
                }

                if (!addedViews[viewID])
                {
                    View* view = _qc->getView(viewID);
                    TDNode* node = _td->getRelation(view->_origin);
                    
                    std::string viewVars = "";            
                    for (size_t v = 0; v < NUM_OF_VARIABLES; ++v)
                    {
                        if (view->_fVars[v])
                        {
                            if (node->_bag[v])
                            {
                                viewVars += node->_name+"Tuple."+
                                    _td->getAttribute(v)->_name+",";
                            }
                            else
                            {
                                for (const size_t& incViewID : incomingViews[viewID])
                                {
                                    View* view = _qc->getView(incViewID);
                                    if (view->_fVars[v])
                                    {
                                        viewVars += viewName[incViewID]+"Tuple."+
                                            _td->getAttribute(v)->_name+",";
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    viewVars.pop_back();

                    if (_requireHashing[viewID])
                    {
                        outputString +=
                            //offset(3+depth+numOfLoops)+"size_t idx_"+viewName[viewID]+" = 0;\n"+
                            offset(offDepth+numOfLoops)+viewName[viewID]+"_iterator"+
                            " = "+viewName[viewID]+"_map.find({"+viewVars+"});\n"+
                            offset(offDepth+numOfLoops)+"if ("+viewName[viewID]+"_iterator"
                            " != "+viewName[viewID]+"_map.end())\n"+
                            offset(offDepth+numOfLoops)+"{\n"+
                            offset(offDepth+1+numOfLoops)+"aggregates_"+viewName[viewID]+
                            " = "+viewName[viewID]+"["+viewName[viewID]+"_iterator->second].aggregates;\n"+
                            // offset(4+depth+numOfLoops)+"idx_"+viewName[viewID]+
                            // " = it_"+viewName[viewID]+"->second;\n"+
                            offset(offDepth+numOfLoops)+"}\n"+
                            offset(offDepth+numOfLoops)+"else\n"+
                            offset(offDepth+numOfLoops)+"{\n"+
                            offset(offDepth+1+numOfLoops)+"size_t idx_"+viewName[viewID]+
                            " = "+viewName[viewID]+".size();\n"+
                            offset(offDepth+1+numOfLoops)+viewName[viewID]+
                            "_map[{"+viewVars+"}] = idx_"+viewName[viewID]+";\n"+
                            offset(offDepth+1+numOfLoops)+viewName[viewID]+
                            ".push_back({"+viewVars+"});\n"+
                            offset(offDepth+1+numOfLoops)+"aggregates_"+viewName[viewID]+
                            " = "+viewName[viewID]+"[idx_"+viewName[viewID]+"].aggregates;\n"+
                            offset(offDepth+numOfLoops)+"}\n";
                            // offset(3+depth+numOfLoops)+"aggregates_"+viewName[viewID]+
                            // " = "+viewName[viewID]+"[idx_"+viewName[viewID]+"].aggregates;\n";
                    }
                    else
                    {
                        // just add string to push to view
                        outputString += offset(offDepth+numOfLoops)+viewName[viewID]+
                            ".push_back({"+viewVars+"});\n"+offset(offDepth+numOfLoops)+
                            "aggregates_"+viewName[viewID]+" = "+viewName[viewID]+
                            ".back().aggregates;\n";
                        // if (viewLoopFactors[viewID].count() != 0)
                        // {
                        //     ERROR("numOfLoops > 0\n");
                        //     ERROR(outputString+"\n");
                        //     ERROR(viewLoopFactors[viewID] << "\n");
                        //     exit(1);
                        // }
                    }
                    
                    // add loop to add to the aggregate Array
                    size_t numOfIndexes = finalAggregateIndexes[viewID].size();

                    std::string indexes = "";
                    for (size_t idx = 0; idx < numOfIndexes; ++idx)
                        indexes += std::to_string(
                            finalAggregateIndexes[viewID][idx])+",";
                    indexes.pop_back();

                    // TODO: THE VIEW OUTPUT INDEXES SHOULD GO TO BEGINNING OF
                    // THE COMPUTE METHOD
                    aggregateHeader +=  offset(2)+"size_t "+viewName[viewID]+"Indexes["+
                        std::to_string(numOfIndexes)+"] = {"+indexes+"};\n";
                    
                    outputString += offset(offDepth+numOfLoops)+"for(size_t i = 0; i < "+
                        std::to_string(numOfIndexes)+"; i += "+
                        std::to_string(numAggregateIndexes[viewID])+")\n"+
                        offset(offDepth+1+numOfLoops)+"aggregates_"+
                        viewName[viewID]+"["+viewName[viewID]+"Indexes[i]] += "+
                        "aggregateRegister_"+std::to_string(depth)+"["+
                        viewName[viewID]+"Indexes[i+1]]";
        
                    if (numAggregateIndexes[viewID] == 3) 
                        outputString += "*aggregateRegister_"+std::to_string(depth)+"["+viewName[viewID]+"Indexes[i+2]];\n";
                    else if (numAggregateIndexes[viewID] == 2)
                        outputString += ";\n";
        
                    addedViews.set(viewID);
                }
            }
        }
    }

    std::string returnString = "";    
    if (nextLoops.any())
    {
        for (size_t viewID=0; viewID < _qc->numberOfViews(); ++viewID)
        {
            if (nextLoops[viewID])
            {
                returnString +=
                    // offset(offDepth+numOfLoops)+// viewName[viewID]+
                    // "_tuple &"+viewName[viewID]+"Pointer = "+viewName[viewID]+
                    // "[lowerptr_"+viewName[viewID]+"["+depthString+"]];\n"+
                    offset(offDepth+numOfLoops)+"ptr_"+viewName[viewID]+
                    " = lowerptr_"+viewName[viewID]+"["+depthString+"];\n"+
                    offset(offDepth+numOfLoops)+"while(ptr_"+viewName[viewID]+
                    " <= upperptr_"+viewName[viewID]+"["+depthString+"])\n"+
                    offset(offDepth+numOfLoops)+"{\n"+
                    offset(offDepth+1+numOfLoops)+viewName[viewID]+
                    "_tuple &"+viewName[viewID]+"Tuple = "+
                    viewName[viewID]+"[ptr_"+viewName[viewID]+"];\n"+
                    offset(offDepth+1+numOfLoops)+"double *aggregates_"+viewName[viewID]+
                    " = "+viewName[viewID]+"Tuple.aggregates;\n";

                dyn_bitset nextConsideredLoops = consideredLoops;
                nextConsideredLoops.set(viewID);

                returnString += genFinalAggregateString(
                    aggRegister, loopReg, nextConsideredLoops, includableAggs, depth,
                    includableViews, addedViews, offDepth);

                returnString += offset(offDepth+1+numOfLoops)+"++ptr_"+viewName[viewID]+";\n"+
                    offset(offDepth+numOfLoops)+"}\n";
            }
        }
    }
    
    return aggregateString + returnString + outputString;
}

#endif 


#ifdef OLD
std::string CppGenerator::genLeapfrogJoinCode(size_t view_id, size_t depth)
{
    std::string returnString = "";

#ifdef PREVIOUS
    
    View* view = _qc->getView(view_id);
    TDNode* node = _td->getRelation(view->_origin);
    
    const std::vector<size_t>& varOrder = variableOrder[view_id];
    const std::vector<size_t>& viewsPerVar = viewsPerVarInfo[view_id];
    const std::vector<size_t>& incViews = incomingViews[view_id];

    const std::string& attrName = _td->getAttribute(varOrder[depth])->_name;
    const std::string& relName = node->_name;
    
    size_t numberIncomingViews =
        (view->_origin == view->_destination ? node->_numOfNeighbors :
         node->_numOfNeighbors - 1);

    size_t idx = depth * (_qc->numberOfViews() + 2);
    size_t numberContributing = viewsPerVarInfo[view_id][idx];

    std::string depthString = std::to_string(depth);
    
    size_t off = 1;
    if (depth > 0)
    {
        returnString +=
            offset(2+depth)+"upperptr_"+relName+"["+depthString+"] = "+
            "upperptr_"+relName+"["+std::to_string(depth-1)+"];\n"+
            offset(2+depth)+"lowerptr_"+relName+"["+depthString+"] = "+
            "lowerptr_"+relName+"["+std::to_string(depth-1)+"];\n";

        for (const size_t& viewID : incViews)
        {   
            returnString +=
                offset(2+depth)+"upperptr_"+viewName[viewID]+"["+depthString+"] = "+
                "upperptr_"+viewName[viewID]+"["+std::to_string(depth-1)+"];\n"+
                offset(2+depth)+"lowerptr_"+viewName[viewID]+"["+depthString+"] = "+
                "lowerptr_"+viewName[viewID]+"["+std::to_string(depth-1)+"];\n";
        }
    }
        
    // Ordering Relations 
    returnString += genRelationOrdering(relName, depth, view_id);
            
    // Update rel pointers 
    returnString += offset(2+depth)+
        "rel["+depthString+"] = 0;\n";
    returnString += offset(2+depth)+
        "atEnd["+depthString+"] = false;\n";

    // Start while loop of the join 
    returnString += offset(2+depth)+"while(!atEnd["+
        depthString+"])\n"+
        offset(2+depth)+"{\n"+
        offset(3+depth)+"found["+depthString+"] = false;\n";

    // Seek Value
    returnString += offset(3+depth)+"// Seek Value\n" +
        offset(3+depth)+"do\n" +
        offset(3+depth)+"{\n" +
        offset(4+depth)+"switch(order_"+attrName+"[rel["+
        depthString+"]].second)\n" +
        offset(4+depth)+"{\n";
        
    off = 1;
    if (viewsPerVarInfo[view_id][idx+1] == _qc->numberOfViews())
    {
        // do base relation first
        returnString += seekValueCase(depth, relName, attrName,false);
        off = 2;
    }
        
    for (; off <= numberContributing; ++off)
    {
        const size_t& viewID = viewsPerVarInfo[view_id][idx+off];
        // case for this view
        returnString += seekValueCase(depth,viewName[viewID],attrName,false);
    }
    returnString += offset(4+depth)+"}\n";
        
    // Close the do loop
    returnString += offset(3+depth)+"} while (!found["+
        depthString+"] && !atEnd["+
        depthString+"]);\n" +
        offset(3+depth)+"// End Seek Value\n";
    // End seek value
        
    // check if atEnd
    returnString += offset(3+depth)+"// If atEnd break loop\n"+
        offset(3+depth)+"if(atEnd["+depthString+"])\n"+
        offset(4+depth)+"break;\n";

    off = 1;
    if (viewsPerVarInfo[view_id][idx+1] == _qc->numberOfViews())
    {            
        // Set upper = lower and update ranges
        returnString += offset(3+depth)+
            "upperptr_"+relName+"["+depthString+"] = "+
            "lowerptr_"+relName+"["+depthString+"];\n";

        // update range for base relation 
        returnString += updateRanges(depth,relName,attrName,false);
        off = 2;
    }

    for (; off <= numberContributing; ++off)
    {
        size_t viewID = viewsPerVarInfo[view_id][idx+off];

        returnString += offset(3+depth)+
            "upperptr_"+viewName[viewID]+
            "["+depthString+"] = "+
            "lowerptr_"+viewName[viewID]+
            "["+depthString+"];\n";
        
        returnString += updateRanges(depth,viewName[viewID],attrName,false);
    }
        
    if (depth + 1 == view->_fVars.count())
    {
        returnString += offset(3+depth)+"bool addTuple = false;\n";
        returnString += offset(3+depth)+"double";
        for (size_t agg = 0; agg < view->_aggregates.size(); ++agg)
            returnString += " agg_"+std::to_string(agg)+" = 0.0,";
        returnString.pop_back();
        returnString += ";\n";
    }
        
    // then you would need to go to next variable
    if (depth+1 < varOrder.size())
    {     
        // leapfrogJoin for next join variable 
        returnString += genLeapfrogJoinCode(view_id, depth+1);
    }
    else
    {
        returnString += offset(3+depth)+
            "/****************AggregateComputation*******************/\n";

        returnString += offset(3+depth)+"addTuple = true;\n";
        
        std::unordered_map<std::vector<bool>,std::string> aggregateSection;
        for (size_t aggID = 0; aggID < view->_aggregates.size(); ++aggID)
        {
            // get the aggregate pointer
            Aggregate* aggregate = view->_aggregates[aggID];
            
            // UPDATE THE AGGREGATE WITH THE AGG YOU WANT TO DO!
            std::string agg = "";
            size_t aggIdx = 0, incomingCounter = 0;
            for (size_t i = 0; i < aggregate->_agg.size(); ++i)
            {
                std::vector<bool> loopFactors(_qc->numberOfViews() + 1);
                size_t numLoopFactors = 0;
                    
                // This loop defines the local aggregates computed at this node
                std::string localAgg = "";
                while (aggIdx < aggregate->_m[i])
                {    
                    const prod_bitset& product = aggregate->_agg[aggIdx];           
                    for (size_t f = 0; f < NUM_OF_FUNCTIONS; ++f)
                    {
                        if (product.test(f))
                        {
                            Function* function = _qc->getFunction(f);
                            const var_bitset& functionVars = function->_fVars;

                            std::string freeVarString = "";
                            for (size_t v = 0; v < NUM_OF_VARIABLES; ++v)
                            {
                                if (functionVars[v])
                                {
                                    // CHECK IF IT IS IN THE BAG OF THE RELATION
                                    if (node->_bag[v])
                                    {
                                        if (variableOrderBitset[view_id][v])
                                        {
                                            freeVarString += relName+"[lowerptr_"+
                                                relName+"["+depthString+
                                                "]]."+ _td->getAttribute(v)->_name+",";
                                        }
                                        else
                                        {
                                            freeVarString += relName+"[ptr_"+relName+
                                                "]."+_td->getAttribute(v)->_name+",";

                                            if (!loopFactors[_qc->numberOfViews()])
                                                ++numLoopFactors;
                                                        
                                            loopFactors[_qc->numberOfViews()] = 1;
                                        }
                                    }
                                    else
                                    {
                                        // IF NOT WE NEED TO FIND THE CORRESPONDING VIEW
                                        size_t incCounter = incomingCounter;
                                        bool found = false;
                                        while(!found && incCounter < aggregate->_o[i])
                                        {
                                            for (size_t n = 0; n <
                                                     numberIncomingViews; ++n)
                                            {
                                                const size_t& viewID =
                                                    aggregate->_incoming[incCounter];

                                                View* incView = _qc->getView(viewID);

                                                if (incView->_fVars[v])
                                                {

                                                    /*variable is part of varOrder*/
                                                    if (variableOrderBitset[view_id][v])
                                                    {
                                                        // add V_ID.var_name to the fVarString
                                                        freeVarString +=
                                                            viewName[viewID]+
                                                            "[lowerptr_"+
                                                            viewName[viewID]+"["+
                                                            depthString+"]]."+
                                                            _td->getAttribute(v)->_name
                                                            +",";
                                                    }
                                                    else
                                                    {
                                                        // add V_ID.var_name to the fVarString
                                                        freeVarString +=
                                                            viewName[viewID]+
                                                            "[ptr_"+viewName[viewID]+
                                                            "]."+
                                                            _td->getAttribute(v)->_name
                                                            +",";

                                                        if (!loopFactors[viewID])
                                                            ++numLoopFactors;
                                                        loopFactors[viewID] = 1;
                                                    }
                                                        
                                                    found = true;
                                                    break; 
                                                }
                                                incCounter += 2;
                                            }
                                        }
                                    }
                                }
                            }

                            if (freeVarString.size() == 0)
                            {
                                ERROR("we have a problem \n");
                                exit(1);
                            }

                            if (freeVarString.size() != 0)
                                freeVarString.pop_back();

                            // This is just one function that we want to
                            // multiply with other functions and 
                            // turn function into a string of cpp-code
                            localAgg += getFunctionString(function,freeVarString)+"*";
                        }
                    }

                    if (localAgg.size() != 0)
                         localAgg.pop_back();
                    localAgg += "+";
                    ++aggIdx;
                }
                localAgg.pop_back();

                // If there is no local aggregate function then we simply
                // add the count for this group.
                if (localAgg.size() == 0)
                    localAgg = "upperptr_"+relName+"["+depthString+"]-"+
                        "lowerptr_"+relName+"["+depthString+"]+1";

                // This loop adds the aggregates that come from the views below
                std::string viewAgg = "";
                while(incomingCounter < aggregate->_o[i])
                {
                    for (size_t n = 0; n < numberIncomingViews; ++n)
                    {
                        size_t viewID = aggregate->_incoming[incomingCounter];
                        size_t aggID = aggregate->_incoming[incomingCounter+1];

                        if (loopFactors[viewID])
                        {
                            viewAgg += viewName[viewID]+
                                "[ptr_"+viewName[viewID]+"]"
                                ".agg_"+std::to_string(viewID)+"_"+
                                std::to_string(aggID)+"*";
                        }
                        else
                        {
                            viewAgg += viewName[viewID]+
                                "[lowerptr_"+viewName[viewID]+"["
                                +depthString+"]]"
                                ".agg_"+std::to_string(viewID)+"_"+
                                std::to_string(aggID)+"*";
                        }
                            
                        incomingCounter += 2;
                    }
                    viewAgg.pop_back();
                    viewAgg += "+";
                }
                if (viewAgg.size() != 0)
                    viewAgg.pop_back();
                    
                std::string aggregateString = offset(3+depth+numLoopFactors)+
                    "agg_"+std::to_string(aggID)+" += ";
                if (viewAgg.size() > 0 && localAgg.size() > 0)
                    aggregateString += "("+localAgg+")*("+viewAgg+");\n";
                else 
                    aggregateString += localAgg+viewAgg+";\n"; // One is empty
                    
                auto it = aggregateSection.find(loopFactors);
                if (it != aggregateSection.end())
                    it->second += aggregateString;
                else
                    aggregateSection[loopFactors] = aggregateString;                    
            } // end localAggregate (i.e. one product)
                
        } // end complete Aggregate
            
        for (const auto& loopCondition : aggregateSection)
        {
            size_t loopCounter = 0;
            std::string closeLoopString = "";
                
            if (loopCondition.first[_qc->numberOfViews()]) // Then loop over
                // relation
            {
                returnString += offset(3+depth)+"size_t ptr_"+relName+
                    " = lowerptr_"+relName+"["+depthString+"];\n";
                returnString += offset(3+depth)+"while(ptr_"+relName+
                    " <= upperptr_"+relName+"["+depthString+"])\n"+
                    offset(3+depth)+"{\n";

                closeLoopString = offset(3+depth+1)+"++ptr_"+relName+";\n"+
                    offset(3+depth)+"}\n";
                ++loopCounter;
            }
                
            for (size_t viewID=0; viewID < _qc->numberOfViews(); ++viewID)
            {
                if (loopCondition.first[viewID])
                {                        
                    returnString += offset(3+depth)+"size_t ptr_"+viewName[viewID]+
                        " = lowerptr_"+viewName[viewID]+"["+depthString+"]\n";

                    // Add loop for view
                    returnString += offset(3+depth+loopCounter)+
                        "while(ptr_"+viewName[viewID]+
                        " < upperptr_"+viewName[viewID]+"["+depthString+"])\n"+
                        offset(3+depth+loopCounter)+"{\n";

                    closeLoopString += offset(3+depth+loopCounter)+
                        "++ptr_"+viewName[viewID]+";\n"+
                        offset(3+depth+loopCounter)+"}\n"+closeLoopString;
                }
            }
            returnString += loopCondition.second + closeLoopString;
        }

        returnString += offset(3+depth)+
            "/****************AggregateComputation*******************/\n";
    }

    if (depth+1 == view->_fVars.count())
    {
        // TODO: TODO: ONLY ADD TUPLE IF WE FOUND A MATCHING JOIN TUPLE !

        returnString += offset(3+depth)+"if(addTuple)\n";
        returnString += offset(4+depth)+viewName[view_id]+".push_back({";
        // for each free variable we find find the relation where they are
        // from - and then fill the tuple / struct
        size_t varCounter = 0;
        for (size_t v = 0; v < NUM_OF_VARIABLES; ++v)
        {
            if (view->_fVars[v])
            {
                size_t idx = varCounter * (_qc->numberOfViews() + 2);
                if (viewsPerVar[idx+1] == _qc->numberOfViews())
                {
                    returnString += relName+"[lowerptr_"+relName+"["+depthString+"]]."+
                        _td->getAttribute(v)->_name+",";
                }
                else
                {
                    returnString += viewName[viewsPerVar[idx+1]]+
                        "[lowerptr_"+viewName[viewsPerVar[idx+1]]+
                        "["+depthString+"]]."+
                        _td->getAttribute(v)->_name+",";
                }
                ++varCounter;
            }
        }
        for (size_t agg = 0; agg < view->_aggregates.size(); ++agg)
            returnString += "agg_"+std::to_string(agg)+",";
        returnString.pop_back();
        returnString += "});\n";
    }

    // Set lower to upper pointer 
    off = 1;
    if (viewsPerVarInfo[view_id][idx+1] == _qc->numberOfViews())
    {            
        // Set upper = lower and update ranges
        returnString += offset(3+depth)+
            "lowerptr_"+relName+"["+depthString+"] = "+
            "upperptr_"+relName+"["+depthString+"];\n";
        off = 2;
    }

    for (; off <= numberContributing; ++off)
    {
        size_t viewID = viewsPerVarInfo[view_id][idx+off];
        returnString += offset(3+depth)+
            "lowerptr_"+viewName[viewID]+"["+depthString+"] = "+
            "upperptr_"+viewName[viewID]+"["+depthString+"];\n";
    }

    // Switch to update the max value and relation pointer
    returnString += offset(3+depth)+
        "switch(order_"+attrName+"[rel["+depthString+"]].second)\n";
    returnString += offset(3+depth)+"{\n";

    // Add switch cases to the switch 
    off = 1;
    if (viewsPerVarInfo[view_id][idx+1] == _qc->numberOfViews())
    {            
        returnString += updateMaxCase(depth,relName,attrName,false);
        off = 2;
    }

    for (; off <= numberContributing; ++off)
    {
        size_t viewID = viewsPerVarInfo[view_id][idx+off];
        returnString += updateMaxCase(depth,viewName[viewID],attrName,false);
    }
    returnString += offset(3+depth)+"}\n";

#endif    
    // Closing the while loop 
    return returnString + offset(2+depth)+"}\n";
}
#endif
