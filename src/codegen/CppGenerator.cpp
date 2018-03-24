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

#include <CppGenerator.h>
#include <TDNode.hpp>

#define OPTIMIZED
#define BENCH_INDIVIDUAL

using namespace multifaq::params;

typedef boost::dynamic_bitset<> dyn_bitset;

CppGenerator::CppGenerator(const std::string path,
                           std::shared_ptr<Launcher> launcher) : _pathToData(path)
{
    _td = launcher->getTreeDecomposition();
    _qc = launcher->getCompiler();
        
    std::string dataset = path;

    if (dataset.back() == '/')
        dataset.pop_back();

    _datasetName = dataset.substr(dataset.rfind('/')).substr(1);
    std::transform(_datasetName.begin(), _datasetName.end(), _datasetName.begin(),
                   [](unsigned char c) -> unsigned char { return std::toupper(c); });
}

CppGenerator::~CppGenerator()
{
    delete[] viewName;
    for (size_t rel = 0; rel < _td->numberOfRelations() + _qc->numberOfViews(); ++rel)
        delete[] sortOrders[rel];
    delete[] sortOrders;
    delete[] viewLevelRegister;
};

void CppGenerator::generateCode()
{
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
    
    createVariableOrder();

#ifdef OPTIMIZED
    createGroupVariableOrder();
#endif
    
    std::ofstream ofs("runtime/inmemory/Engine.hpp", std::ofstream::out);
    
    // DINFO(genHeader() << std::endl);
    ofs << genHeader();

    // DINFO(genTupleStructs());
    ofs << genTupleStructs();

    // DINFO(genLoadRelationFunction() << std::endl);
    ofs << genLoadRelationFunction() << std::endl;

    // DINFO(genCaseIdentifiers() << std::endl);
    ofs << genCaseIdentifiers() << std::endl;

    for (size_t rel = 0; rel < _td->numberOfRelations(); ++rel)
    {
        // DINFO( genSortFunction(rel) << std::endl);
        ofs << genSortFunction(rel) << std::endl;
    }
    
#ifndef OPTIMIZED
    for (size_t view = 0; view < _qc->numberOfViews(); ++view)
    {
        std::string s = genComputeViewFunction(view);
        // DINFO( s << std::endl);
        ofs << s << std::endl;
    }
#else
    for (size_t group = 0; group < viewGroups.size(); ++group)
    {
        std::string s = genComputeGroupFunction(group);
        
        // DINFO( s << std::endl);
        ofs << s << std::endl;
    }
#endif
    
    // DINFO(genRunFunction() << std::endl);
    ofs << genRunFunction() << std::endl;

    // DINFO(genRunMultithreadedFunction() << std::endl);
    ofs << genRunMultithreadedFunction() << std::endl;

    // DINFO( "}\n\n#endif /* INCLUDE_TEMPLATE_"+_datasetName+"_HPP_*/\n");
    ofs <<  "}\n\n#endif /* INCLUDE_TEMPLATE_"+_datasetName+"_HPP_*/\n";
    
    // ofs << genFooter() << std::endl;
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
    returnString += "\n"+offset(2)+"std::cout << \"Sort Relation "+relName+": \"+"+
        "std::to_string(duration_cast<milliseconds>("+
        "system_clock::now().time_since_epoch()).count()-startProcess)+\"ms.\\n\";\n\n";
#endif
    
    return returnString+offset(1)+"}\n";
}


void CppGenerator::createGroupVariableOrder()
{
    // // FIRST WE CREATE THE VARIABLE ORDER FOR EACH RELATION
    // size_t orderIdx = 0;
    // for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
    // {
    //     if (_td->_root->_bag[var])
    //     {
    //         sortOrders[_td->_root->_id][orderIdx] = var;
    //         ++orderIdx;
    //     }
    // }

    // createRelationSortingOrder(_td->_root, _td->_root->_id);

    // for (size_t rel = 0; rel < _td->numberOfRelations(); ++rel)
    // {
    //     TDNode* node = _td->getRelation(rel);
    //     std::string relationOrder = node->_name+":";
    //     for (size_t i = 0; i < node->_bag.count(); ++i)
    //     {
    //         Attribute* att = _td->getAttribute(sortOrders[rel][i]);
    //         relationOrder += " "+att->_name+",";
    //     }
    //     relationOrder.pop_back();
    //     std::cout << relationOrder << std::endl;
    // }

    createTopologicalOrder();
    
    groupVariableOrder = new std::vector<size_t>[viewGroups.size()]();
    groupIncomingViews = new std::vector<size_t>[viewGroups.size()]();
    groupViewsPerVarInfo = new std::vector<size_t>[viewGroups.size()];

    _parallelizeGroup = new bool[viewGroups.size()];
    _threadsPerGroup = new size_t[viewGroups.size()]();

    for (size_t gid = 0; gid < viewGroups.size(); ++gid)
    {
        TDNode* node = _td->getRelation(_qc->getView(viewGroups[gid][0])->_origin);
        if (node->_name == "Inventory")
        {
            _parallelizeGroup[gid] = true;
            _threadsPerGroup[gid] = 8;
        }
        else
        {
            _parallelizeGroup[gid] = false;
            _threadsPerGroup[gid] = 0;
        }
        _parallelizeGroup[gid] = false;
    }
        
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

            int numberIncomingViews =
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
                    size_t incomingCounter = 0;
            
                    // First find the all views that contribute to this Aggregate
                    for (size_t i = 0; i < aggregate->_n; ++i)
                    {
                        while(incomingCounter < aggregate->_o[i])
                        {
                            for (int n = 0; n < numberIncomingViews; ++n)
                            {
                                const size_t& incViewID =
                                    aggregate->_incoming[incomingCounter];
                                // Add the intersection of the view and the base
                                // relation to joinVars 
                                joinVars |= (_qc->getView(incViewID)->_fVars &
                                             baseRelation->_bag);
                                // Indicate that this view contributes to some aggregate
                                incViewBitset[incViewID] = 1;
                                incomingCounter += 2;
                            }
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


/* TODO: Technically there is no need to pre-materialise this! */
void CppGenerator::createVariableOrder()
{
    viewsPerNode = new std::vector<size_t>[_td->numberOfRelations()]();

    variableOrder = new std::vector<size_t>[_qc->numberOfViews()]();
    incomingViews = new std::vector<size_t>[_qc->numberOfViews()]();
    viewsPerVarInfo = new std::vector<size_t>[_qc->numberOfViews()];

    std::cout << "before var order \n";

    variableOrderBitset.resize(_qc->numberOfViews());
        
    for (size_t viewID = 0; viewID < _qc->numberOfViews(); ++viewID)
    {
        View* view = _qc->getView(viewID);
        TDNode* baseRelation = _td->getRelation(view->_origin);
        int numberIncomingViews =
            (view->_origin == view->_destination ? baseRelation->_numOfNeighbors :
             baseRelation->_numOfNeighbors - 1);
        
        // for free vars push them on the variableOrder
        for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
            if (view->_fVars[var])
                variableOrder[viewID].push_back(var);
        // Now we find which varaibles are joined on and which views
        // contribute to this view
        var_bitset joinVars;
        std::vector<bool> incViewBitset(_qc->numberOfViews());
        for (size_t aggNo = 0; aggNo < view->_aggregates.size(); ++aggNo)
        {            
            Aggregate* aggregate = view->_aggregates[aggNo];
            size_t incomingCounter = 0;
            
            // First find the all views that contribute to this Aggregate
            for (size_t i = 0; i < aggregate->_n; ++i)
            {
                while(incomingCounter < aggregate->_o[i])
                {
                    for (int n = 0; n < numberIncomingViews; ++n)
                    {
                        size_t incViewID =
                            aggregate->_incoming[incomingCounter];
                        // Add the intersection of the view and the base
                        // relation to joinVars 
                        joinVars |= (_qc->getView(incViewID)->_fVars &
                                     baseRelation->_bag);
                        // Indicate that this view contributes to some aggregate
                        incViewBitset[incViewID] = 1;
                        incomingCounter += 2;
                    }
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

// std::string CppGenerator::typeToStringConverter(Type t)
// {
//     switch(t)
//     {
//     case Type::Integer : return "std::stoi";
//     case Type::Double : return "std::stod";
//     case Type::Short : return "std::stoi";
//     case Type::U_Integer : return "std::stoul";
//     default :
//         ERROR("This type does not exist \n");
//         exit(1);
//     }
// }

std::string CppGenerator::genHeader()
{   
    return "#ifndef INCLUDE_TEMPLATE_"+_datasetName+"_HPP_\n"+
        "#define INCLUDE_TEMPLATE_"+_datasetName+"_HPP_\n\n"+
        "#include <algorithm>\n" +
        "#include <chrono>\n" +
        "#include <fstream>\n"+
        "#include <iostream>\n" +
        "#include <unordered_map>\n" +
        "#include <thread>\n" +
        "#include <vector>\n\n" +
        "#include <boost/spirit/include/qi.hpp>\n"+
        "#include <boost/spirit/include/phoenix_core.hpp>\n"+
        "#include <boost/spirit/include/phoenix_operator.hpp>\n\n"
        // "#include \"CppHelper.hpp\"\n\n" +
        "using namespace std::chrono;\n"+
        "namespace qi = boost::spirit::qi;\n"+
        "namespace phoenix = boost::phoenix;\n\n"+
        "//const std::string PATH_TO_DATA = \"/Users/Maximilian/Documents/"+
        "Oxford/LMFAO/"+_pathToData+"\";\n\n"+
        "const std::string PATH_TO_DATA = \"../../"+_pathToData+"\";\n\n"+
        "namespace lmfao\n{\n";
}

// std::string CppGenerator::genFooter()
// {
//     return "}\n\n#endif /* INCLUDE_TEMPLATE_"+_datasetName+"_HPP_*/";
// }

std::string CppGenerator::genTupleStructs()
{
    std::string tupleStruct = "", attrConversion = "", attrConstruct = "",
        attrAssign = "", attrParse = "";

    for (size_t relID = 0; relID < _td->numberOfRelations(); ++relID)
    {
        TDNode* rel = _td->getRelation(relID);
        const std::string& relName = rel->_name;
        const var_bitset& bag = rel->_bag;

        attrConversion = "", attrConstruct = "", attrAssign = "";
        tupleStruct += offset(1)+"struct "+relName+"_tuple\n"+
            offset(1)+"{\n"+offset(2);

        attrParse = offset(3)+"qi::phrase_parse(tuple.begin(),tuple.end(),";
        
        size_t field = 0;
        for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
        {
            if (bag[var])
            {
                Attribute* att = _td->getAttribute(var);
                tupleStruct += typeToStr(att->_type)+" "+att->_name+";\n"+offset(2);
                // attrConversion += offset(3)+att->_name+" = "+
                //     typeToStringConverter(att->_type)+
                //     "(fields["+std::to_string(field)+"]);\n";
                attrConstruct += typeToStr(att->_type)+" "+att->_name+",";
                attrAssign += offset(3)+"this->"+ att->_name + " = "+att->_name+";\n";

                attrParse += "\n"+offset(4)+"qi::"+typeToStr(att->_type)+
                    "_[phoenix::ref("+att->_name+") = qi::_1]>>";
                ++field;
            }
        }
        attrConstruct.pop_back();
        attrParse.pop_back();
        attrParse.pop_back();
        attrParse += ",\n"+offset(4)+"\'|\');\n";

        tupleStruct += relName+"_tuple() {} \n"+offset(2)+
            relName+"_tuple(const std::string& tuple)\n"+offset(2)+
            "{\n"+attrParse+offset(2)+"}\n"+offset(2)+
            // relName+"_tuple(std::vector<std::string>& fields)\n"+offset(2)+
            // "{\n"+attrConversion +offset(2)+"}\n"+offset(2)+
            relName+"_tuple("+attrConstruct+")\n"+offset(2)+
            "{\n"+attrAssign+offset(2)+"}\n"+offset(1)+"};\n\n"+
            offset(1)+"std::vector<"+rel->_name+"_tuple> "+rel->_name+";\n\n";
    }

    for (size_t viewID = 0; viewID < _qc->numberOfViews(); ++viewID)
    {
        View* view = _qc->getView(viewID);

        attrConstruct = "", attrAssign = "";
        tupleStruct += offset(1)+"struct "+viewName[viewID]+"_tuple\n"+
            offset(1)+"{\n";
        

        for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
        {
            if (view->_fVars[var])
            {
                Attribute* att = _td->getAttribute(var);
                tupleStruct += offset(2)+typeToStr(att->_type)+" "+att->_name+";\n";
                attrConstruct += typeToStr(att->_type)+" "+att->_name+",";
                attrAssign += offset(3)+"this->"+ att->_name + " = "+att->_name+";\n";
            }
        }


        tupleStruct += offset(2)+"double aggregates["+
            std::to_string(view->_aggregates.size())+"] = {};\n";
        
        // std::string agg_name;
        // for (size_t agg = 0; agg < view->_aggregates.size(); ++agg)
        // {
        //     agg_name = "agg_"+std::to_string(viewID)+"_"+std::to_string(agg);
        //     tupleStruct += "double "+agg_name+";\n"+offset(2);
        //     // attrConstruct += "double "+agg_name+",";
        //     // attrAssign += offset(3)+"this->"+ agg_name + " = "+agg_name+";\n";
        // }
        
        if (!attrConstruct.empty())
        {
            attrConstruct.pop_back();
            tupleStruct += offset(2)+viewName[viewID]+"_tuple() {} \n"+
                offset(2) +viewName[viewID]+"_tuple("+attrConstruct+")\n"+
                offset(2)+"{\n"+attrAssign+offset(2)+"}\n"+offset(1)+"};\n\n"+
                offset(1)+"std::vector<"+viewName[viewID]+"_tuple> "+
                viewName[viewID]+";\n\n";
        }
        else
            tupleStruct += offset(2)+viewName[viewID]+"_tuple() {} \n"+offset(1)+
                "};\n\n"+offset(1)+"std::vector<"+viewName[viewID]+"_tuple> "+
                viewName[viewID]+";\n\n";
    }
    return tupleStruct;
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

        returnString += offset(2)+"input.open(PATH_TO_DATA + \""+rel_name+".tbl\");\n"+
            offset(2)+"if (!input)\n"+offset(2)+"{\n"+
            offset(3)+"std::cerr << PATH_TO_DATA + \""+rel_name+
            ".tbl does is not exist. \\n\";\n"+
            offset(3)+"exit(1);\n"+offset(2)+"}\n"+
            offset(2)+"while(getline(input, line))\n"+
            offset(3)+rel_name+".push_back("+rel_name+"_tuple(line));\n"+
            offset(2)+"input.close();\n";            
            // offset(2)+"readFromFile("+rel_name+", PATH_TO_DATA + \""+rel_name+
            // ".tbl\", \'|\');\n";
    }   
    return returnString+offset(1)+"}\n";
}

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

    for (const size_t& viewID : viewGroups[group_id])
    {
        if (_requireHashing[viewID])
        {
            View* view = _qc->getView(viewID);

            std::string attrConstruct = "";
            std::string attrAssign = "";
            std::string equalString = "";
            
            // CREATE A STRUCT WITH THE RIGHT ATTRIBUTES AND EQUAL FUNCTION
            // THEN CREATE STRUCT WITH HASH --> use hash combine 

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
    
    std::string returnString = hashStruct+hashFunct+offset(1)+"void computeGroup"+
        std::to_string(group_id);

    if (!_parallelizeGroup[group_id])
    {
        returnString += "()\n"+offset(1)+"{\n";

        for (const size_t& viewID : viewGroups[group_id])
        {
            if (_requireHashing[viewID])
            {
                returnString += offset(2)+"std::unordered_map<"+viewName[viewID]+"_key,"+
                    "size_t,"+viewName[viewID]+"_keyhash> "+viewName[viewID]+"_map;\n"+
                    offset(2)+"std::unordered_map<"+viewName[viewID]+"_key,"  +
                    "size_t,"+viewName[viewID]+"_keyhash>::iterator "+
                    viewName[viewID]+"_iterator;\n";
            }
        }

    }
    else
    {
        returnString += "(";
        for (const size_t& viewID : viewGroups[group_id])
        {
            returnString += "std::vector<"+viewName[viewID]+"_tuple>& "+
                viewName[viewID]+",";
            if (_requireHashing[viewID])
            {
                returnString += "std::unordered_map<"+viewName[viewID]+"_key,"+
                    "size_t, "+viewName[viewID]+"_keyhash>& "+viewName[viewID]+"_map,";
            }
        }
        returnString += "size_t lowerptr, size_t upperptr)\n"+offset(1)+"{\n";

        for (const size_t& viewID : viewGroups[group_id])
        {
            if (_requireHashing[viewID])
            {
                returnString += 
                    offset(2)+"std::unordered_map<"+viewName[viewID]+"_key,"  +
                    "size_t,"+viewName[viewID]+"_keyhash>::iterator "+
                    viewName[viewID]+"_iterator;\n";
            }
        }
    }

#ifdef BENCH_INDIVIDUAL
    returnString += offset(2)+"int64_t startProcess = duration_cast<milliseconds>("+
        "system_clock::now().time_since_epoch()).count();\n\n";
#endif
    
    // Generate min and max values for the join varibales 
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
        "atEnd["+numOfJoinVarString+"] = {}, addTuple["+numOfJoinVarString+"] = {};\n";

    // Lower and upper pointer for the base relation
    returnString += genPointerArrays(
        baseRelation->_name, numOfJoinVarString, _parallelizeGroup[group_id]);

    // Lower and upper pointer for each incoming view
    for (const size_t& viewID : incViews)            
        returnString += genPointerArrays(
            viewName[viewID],numOfJoinVarString, false);

#ifdef PREVIOUS
    std::string closeAddTuple = "";
    for (const size_t& viewID : viewGroups[group_id])
    {
        View* view = _qc->getView(viewID);

        returnString += offset(2)+"double aggregates_"+viewName[viewID]+
            "["+std::to_string(view->_aggregates.size())+"] = {};\n";
        
        if (view->_fVars.count() == 0)
        {
            returnString += offset(2)+"bool addTuple_"+viewName[viewID]+" = false;\n";
            // returnString += offset(2)+"double";
            // for (size_t agg = 0; agg < view->_aggregates.size(); ++agg)
            //     returnString += " agg_"+std::to_string(viewID)+"_"+
            //         std::to_string(agg)+" = 0.0,";
            // returnString.pop_back();
            // returnString += ";\n";

            closeAddTuple += offset(2)+"if (addTuple_"+viewName[viewID]+")\n"+
                 offset(2)+"{\n"+offset(3)+viewName[viewID]+".push_back({});\n";
            // for (size_t agg = 0; agg < view->_aggregates.size(); ++agg)
            //     closeAddTuple += "agg_"+std::to_string(viewID)+"_"+
            //         std::to_string(agg)+",";
            // closeAddTuple.pop_back();
            // closeAddTuple += "});\n";

            closeAddTuple += offset(3)+"memcpy(&"+viewName[viewID]+"[0].aggregates[0],"+
                "&aggregates_"+viewName[viewID]+"[0], sizeof(double) * "+
                std::to_string(view->_aggregates.size())+");\n"+
                offset(2)+"}\n";
            // for (size_t agg = 0; agg < view->_aggregates.size(); ++agg)
            //     closeAddTuple += offset(3)+"for (size_t agg = 0; agg < "+
            //         std::to_string(view->_aggregates.size())+"; ++agg)"+
            //         viewName[viewID]+"[0].aggregates[agg]"+" = "+
            //         "aggregates_"+viewName[viewID]+"[agg];\n";
        }   
    }

        
    // for each join var --- create the string that does the join
    returnString += genGroupLeapfrogJoinCode(group_id, *baseRelation, 0);
    
    returnString += closeAddTuple;

#else
    // array that stores the indexes of the aggregates 
    finalAggregateIndexes.resize(_qc->numberOfViews());
    newFinalAggregates.resize(_qc->numberOfViews());
    numAggregateIndexes = new size_t[_qc->numberOfViews()]();
    viewLoopFactors.resize(_qc->numberOfViews());
    
    aggregateGenerator(group_id, *baseRelation);
    
    returnString += aggregateHeader;

    returnString += offset(2)+"size_t ptr_"+relName+" = 0;\n";

    returnString += offset(2)+"double";
    for (const size_t& viewID : viewGroups[group_id])
        returnString += " *aggregates_"+viewName[viewID]+" = nullptr,";
    for (const size_t& viewID : incViews)
        returnString += " *aggregates_"+viewName[viewID]+" = nullptr,";
    returnString.pop_back();
    returnString += ";\n";

    for (const size_t& viewID : incViews)
    {
        View* view = _qc->getView(viewID);
        if ((view->_fVars & ~varOrderBitset).any())
            returnString += offset(2)+"size_t ptr_"+viewName[viewID]+" = 0;\n";
    }


    std::string aggReg = "";
    for (size_t depth = 0; depth <= varOrder.size(); depth++)
    {
        if (!aggregateRegister[depth].empty())
        {
            aggReg += " aggregateRegister_"+std::to_string(depth)+
                "["+std::to_string(aggregateRegister[depth].size())+"],";
        }
    }
    if (!aggReg.empty())
    {
        aggReg.pop_back();
        returnString += offset(2)+"double"+aggReg+";\n";
    }
    
    // for each join var --- create the string that does the join
    returnString += genGroupLeapfrogJoinCode(group_id, *baseRelation, 0);

    for (const size_t& viewID : viewGroups[group_id])
    {
        if (viewLevelRegister[viewID] == varOrder.size())
            returnString += finalAggregateString[viewLevelRegister[viewID]];
    }
    
#endif
    
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
    returnString += "\n"+offset(2)+"std::cout << \"Compute Group "+
        std::to_string(group_id)+" process: \"+"+
        "std::to_string(duration_cast<milliseconds>("+
        "system_clock::now().time_since_epoch()).count()-startProcess)+\"ms.\\n\";\n\n";
#endif
    
    if (_parallelizeGroup[group_id])
    {
        returnString += offset(1)+"}\n";

        returnString += offset(1)+"void computeGroup"+std::to_string(group_id)+
            "Parallelized()\n"+offset(1)+"{\n";

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
                "(computeGroup"+std::to_string(group_id)+",";
            
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

                for (size_t t = 0; t < _threadsPerGroup[group_id]; ++t)
                {
                    returnString += offset(2)+"for (const std::pair<"+
                        viewName[viewID]+"_key,size_t>& key : "+viewName[viewID]+
                        "_"+std::to_string(t)+"_map)\n"+offset(2)+"{\n"+
                        offset(3)+"auto it = "+viewName[viewID]+"_0"
                        "_map.find(key.first);\n"+
                        offset(3)+"if (it != "+viewName[viewID]+"_0_map.end())\n"+
                        offset(3)+"{\n";

                    returnString += offset(4)+"for (size_t agg = 0; agg < "+std::to_string(
                        _qc->getView(viewID)->_aggregates.size())+"; ++agg)\n"+
                        offset(5)+viewName[viewID]+"[it->second].aggregates[agg] += "+
                        viewName[viewID]+"_"+std::to_string(t)+"[key.second].aggregates[agg];\n";
                        
                        // returnString += offset(4)+viewName[viewID]+"[it->second].agg_"+
                        //     std::to_string(viewID)+"_"+std::to_string(agg)+" += "+
                        //     viewName[viewID]+"_"+std::to_string(t)+"[key.second].agg_"+
                        // //     std::to_string(viewID)+"_"+std::to_string(agg)+";\n";
                        // returnString += offset(4)+viewName[viewID]+"[it->second].aggregates["+
                        //     std::to_string(agg)+"] += "+
                        //     viewName[viewID]+"_"+std::to_string(t)+"[key.second].aggregates["+
                        //     std::to_string(agg)+"];\n";
                    
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
    return returnString + offset(1)+"}\n";
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
                size_t aggIdx = 0;
                for (size_t i = 0; i < aggregate->_n; ++i)
                {
                    while(aggIdx < aggregate->_m[i])
                    {
                        const prod_bitset& product = aggregate->_agg[aggIdx];
                        prod_bitset considered;
                        
                        for (size_t f = 0; f < NUM_OF_FUNCTIONS; ++f)
                        {
                            if (product.test(f) && !considered[f])
                            {
                                Function* function = _qc->getFunction(f);
                                
                                // check if this function is covered (?!?!)

                                // TODO: This shouldn't only be the nonJoinVars
                                // but the groups of variables that are in the
                                // relation of th
                                // if ((function->_fVars & nonJoinVars).any())
                                if ((function->_fVars & dependentViewVars).any())
                                {
                                    // var_bitset nonVarOrder =
                                    //function->_fVars & ~varOrderBitset;

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
                        
                        ++aggIdx;
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

void CppGenerator::computeAggregateRegister(
    const size_t group_id, const size_t view_id, const size_t agg_id,
    std::vector<prod_bitset>& productSum, std::vector<size_t>& incoming,
    std::vector<std::pair<size_t, size_t>>& localAggReg,
    bool splitViewAggSummation,size_t depth)
{
    // std::cout << "starting Aggregate Register " << view_id << "   " << group_id << std::endl;
    
    const std::vector<size_t>& varOrder = groupVariableOrder[group_id];
    const var_bitset& varOrderBitset = groupVariableOrderBitset[group_id];

    View* view = _qc->getView(view_id);

    const TDNode& node = *_td->getRelation(view->_origin);
    const std::string& relName = node._name;

    size_t numberIncomingViews =
        (view->_origin == view->_destination ? node._numOfNeighbors :
         node._numOfNeighbors - 1);

    const size_t numViewProducts =
        (numberIncomingViews == 0 ? 0 : (incoming.size() / 2) / numberIncomingViews);

    const size_t numLocalProducts = productSum.size();
    
    // Compute dependent variables on the variables in the head of view:
    var_bitset dependentVariables;
    var_bitset nonVarOrder = view->_fVars & ~varOrderBitset;

    for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
    {
        if (nonVarOrder[var])
        {
            // find bag that contains this variable
            if (node._bag[var])
                dependentVariables |= node._bag & ~varOrderBitset;
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

    // TODO: rename & remove one!!!
    // boost::dynamic_bitset<> loopFactors(_qc->numberOfViews()+1);
    boost::dynamic_bitset<> loopFactors(_qc->numberOfViews()+1);

    std::vector<std::pair<size_t, size_t>>
        localComputation(numLocalProducts + numViewProducts, {varOrder.size(), 0});
    
    std::vector<std::pair<size_t, size_t>>
        dependentComputation(numLocalProducts + numViewProducts, {varOrder.size(), 0});

    std::vector<std::string> dependentString(numLocalProducts+numViewProducts,"");
    std::vector<boost::dynamic_bitset<>> dependentLoopFactors
        (numLocalProducts+numViewProducts, boost::dynamic_bitset<>(_qc->numberOfViews()+1));
    
    bool addedViewAggsHere = false;
    bool addedViewProdHere = false;
    
    // go over each product and chekc if parts of it can be computed at current depth
    for (size_t prodID = 0; prodID < numLocalProducts; ++prodID)
    {
        loopFactors.reset();
        dependentLoopFactors[prodID].reset();
        
        prod_bitset& product = productSum[prodID];
        prod_bitset considered, localFunctions, dependentFunctions;
        
        for (size_t f = 0; f < NUM_OF_FUNCTIONS; ++f)
        {
            if (product.test(f) && !considered[f])
            {
                Function* function = _qc->getFunction(f);                      

                // check if this function is covered
                if ((function->_fVars & coveredVariables[depth]) == function->_fVars)
                {   
                    // var_bitset nonVarOrder = function->_fVars & ~varOrderBitset;
                    var_bitset dependentFunctionVars;
                    for (size_t var=0; var<NUM_OF_VARIABLES;++var)
                    {
                        if (function->_fVars[var])
                            dependentFunctionVars |= variableDependency[var];
                    }
                    dependentFunctionVars &= ~varOrderBitset;

                    // look for functions that overlap / depend on this function
                    bool overlaps = true, computeHere = true;
                    var_bitset overlapVars;
                    prod_bitset overlapFunc;
                    overlapFunc.set(f);
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
                                // check if functions depend on each other
                                // if ((otherFunction->_fVars & nonVarOrder).any()) 
                                if ((otherFunction->_fVars & dependentFunctionVars).any())
                                {
                                    considered.set(f2);
                                    overlapFunc.set(f2);
                                    overlaps = true;
                                    overlapVars |= otherFunction->_fVars;
                                    // check if otherFunction is covered
                                    if ((otherFunction->_fVars & coveredVariables[depth]) !=
                                        otherFunction->_fVars)
                                    {
                                        // if it is not we need
                                        // to compute at later stage
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
                        // if (((overlapVars & ~varOrderBitset) & view->_fVars).any())
                        if (((overlapVars & ~varOrderBitset) & dependentVariables).any())
                        {
                            if (viewLevelRegister[view_id] != depth)
                                ERROR("ERROR: ERROR: in dependent computation:"
                                      <<" viewLevelRegister[viewID] != depth\n");
                            
                            dependentFunctions |= overlapFunc;
                            product &= ~overlapFunc;
                            // TODO: is this correct?
                        }
                        else
                        {
                            // add all functions that overlap together at this level!
                            localFunctions |= overlapFunc;
                            product &= ~overlapFunc;
                            // TODO: is this correct?                    
                        }
                    }
                }
            }
        }

        prod_bitset allLocalFunctions = localFunctions | dependentFunctions;

        std::string preLocalString = "";
        
        std::vector<std::vector<size_t>> preViewAgg(incoming.size());
        std::vector<std::vector<size_t>> postViewAgg(incoming.size());
        
        // compute string for localFunctions that can be computed at this level
        for (size_t f = 0; f < NUM_OF_FUNCTIONS; f++)
        {            
            if (allLocalFunctions[f])
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
                            freeVarString += relName+"Tuple."+
                                _td->getAttribute(v)->_name+",";
                            
                            if (!varOrderBitset[v] && localFunctions[f])
                                loopFactors.set(_qc->numberOfViews());

                            if (!varOrderBitset[v] && dependentFunctions[f])
                                dependentLoopFactors[prodID].set(_qc->numberOfViews());
                        }
                        else
                        {
                            // find the corresponding view
                            size_t incCounter = 0, incIdx = 0;
                            while(incCounter < incoming.size())
                            {
                                std::string viewAgg = "";
                                for (size_t n = 0; n < numberIncomingViews; ++n)
                                {
                                    size_t& incViewID = incoming[incCounter];
                                    const size_t& incAggID = incoming[incCounter+1];
                                                    
                                    View* incView = _qc->getView(incViewID);
                                    if (incView->_fVars[v])
                                    {
                                        freeVarString += viewName[incViewID]+"Tuple."+
                                            _td->getAttribute(v)->_name+",";

                                        if (localFunctions[f])
                                            loopFactors.set(incViewID);
                                        
                                        if (dependentFunctions[f])
                                            dependentLoopFactors[prodID].set(incViewID);

                                        if (varOrderBitset[v])
                                        {
                                            ERROR ("WHY DO WE HAVE TO USE A VIEW TO FIND VAR IN VAR ORDER? \n");
                                            exit(0);
                                        }

                                        if (!varOrderBitset[v] && depth != varOrder.size()-1)
                                        {
                                            splitViewAggSummation = true;
                                            addedViewAggsHere = true;
                                            
                                            if (localFunctions[f])
                                            {
                                                preViewAgg[incIdx].push_back(incViewID);
                                                preViewAgg[incIdx].push_back(incAggID);   
                                            }
                                            if (dependentFunctions[f])
                                            {
                                                postViewAgg[incIdx].push_back(incViewID);
                                                postViewAgg[incIdx].push_back(incAggID);
                                            }                                            
                                        }
                                        else
                                        {
                                            // TODO: Consider this case again?!
                                            break;
                                        }
                                    }
                                    incCounter += 2;
                                }
                                ++incIdx;
                            }
                        }
                    }
                }

                freeVarString.pop_back();

                if (localFunctions[f])
                    preLocalString += getFunctionString(function,freeVarString)+"*";

                if (dependentFunctions[f])
                    dependentString[prodID] += getFunctionString(function,freeVarString)+"*";
            }
        }

        // TODO: never verified that this actuall works!?
        if (addedViewAggsHere)
        {
            boost::dynamic_bitset<> includedViews(_qc->numberOfViews());
            
            for (size_t incIdx = 0; incIdx * numberIncomingViews < incoming.size(); ++incIdx)
            {
                for (size_t preIdx = 0; preIdx < preViewAgg[incIdx].size(); preIdx += 2)
                {
                    size_t& incViewID = preViewAgg[incIdx][preIdx];
                    const size_t& incAggID = preViewAgg[incIdx][preIdx+1];

                    if (!includedViews[incViewID])
                    {
                        // add the string to preAgg
                        preLocalString += "aggregates_"+viewName[incViewID]+
                            "["+std::to_string(incAggID)+"]*";
                        incViewID = _qc->numberOfViews();
                        includedViews.set(incViewID);
                    }
                }
                
                for (size_t postIdx = 0; postIdx < postViewAgg[incIdx].size(); postIdx += 2)
                {
                    size_t& incViewID = postViewAgg[incIdx][postIdx];
                    const size_t& incAggID = postViewAgg[incIdx][postIdx+1];

                    if (!includedViews[incViewID])
                    {
                        // add the string to postAgg
                        dependentString[prodID] += "aggregates_"+viewName[incViewID]+
                            "["+std::to_string(incAggID)+"]*";
                        incViewID = _qc->numberOfViews();
                        includedViews.set(incViewID);
                    }
                }
            }
            splitViewAggSummation = true;
        }
        
        if (!preLocalString.empty())
        {
            // add part of product that has been computed before
            if (depth <= viewLevelRegister[view_id] ||
                viewLevelRegister[view_id] != varOrder.size())
            {
                // add previous Aggregate (if there is one)
                auto prevAgg = localAggReg[prodID];
                if (prevAgg.first < varOrder.size()) 
                {
                    // // do somehting
                    // std::cout << "adding to preLocalString:  aggregateRegister_"+
                    //     std::to_string(prevAgg.first)+
                    //     "["+std::to_string(prevAgg.second)+"]* \n";                
                    preLocalString += "aggregateRegister_"+
                        std::to_string(prevAgg.first)+
                        "["+std::to_string(prevAgg.second)+"]*";
                }
                // if this aggregate needs a loop perhaps we should only
                // multiply by the previous Aggregate after the loop?
                // TODO: (CONSIDER) think about keeping this separate 
            }
            
            preLocalString.pop_back();
            preLocalString += ";\n";

            updateAggregateRegister(localAggReg[prodID],preLocalString,depth,loopFactors,true);
            localComputation[prodID] = localAggReg[prodID];
        }
        else if (depth == viewLevelRegister[view_id])
        {
            // add previous Aggregate (if there is one)   
            std::pair<size_t, size_t>& prevAgg = localAggReg[prodID];
            if (prevAgg.first < varOrder.size()) 
            {
                // do somehting
                std::cout << "adding to dependentString:  aggregateRegister_"+
                    std::to_string(prevAgg.first)+
                    "["+std::to_string(prevAgg.second)+"]* \n";
                
                dependentString[prodID] += "aggregateRegister_"+
                    std::to_string(prevAgg.first)+
                    "["+std::to_string(prevAgg.second)+"]*";
            }
            // if this aggregate needs a loop perhaps we should only
            // multiply by the previous Aggregate after the loop?
            // TODO: (CONSIDER) think about keeping this separate           
        }
        if (preLocalString.empty() && depth == varOrder.size()-1)
        {
            // Add the COUNT to the aggregates!
            loopFactors.reset();
            
            if ((view->_fVars & ~varOrderBitset & node._bag).none())
            {
                // if (view_id == 15)
                // {
                //     std::cout << "WE GET HERE FOR VIEW 15!!!!------------------------------------------------------------\n";
                //     // exit(0);
                // }
 
                loopFactors.set(_qc->numberOfViews());
                updateAggregateRegister(localAggReg[prodID],"1;\n",depth,loopFactors,true);
                localComputation[prodID] = localAggReg[prodID];
            }
            else
            {
                if (dependentString[prodID].empty())
                    dependentString[prodID] += "1*";
            }
        }    
    }
    

    //**********************************// 
    /* We are now computing the viewAggregates from below for this Aggregate */
    size_t incCounter = 0, viewAggProd = 0;
    while(incCounter < incoming.size())
    {
        // go over each level and check if for each function of this
        // product that can be computed at this level register it at
        // this level!       
        loopFactors.reset();
        dependentLoopFactors[numLocalProducts+viewAggProd].reset(); 
        
        std::string preViewAggregate = "";

        for (size_t n = 0; n < numberIncomingViews; ++n)
        {
            size_t& incViewID = incoming[incCounter];
            const size_t& incAggID = incoming[incCounter+1];

            // std::cout << "here "<< addableViews[incViewID + (depth * (_qc->numberOfViews()+1))] << "\n";

            if (addableViews[incViewID + (depth * (_qc->numberOfViews()+1))])
            {
                
                View* incView = _qc->getView(incViewID);

                // TODO: I think the below statement is fine but should we use
                // dependent variables instead?! - here we just check if any of
                // the vars from incView are present in the vars for view
                if (((incView->_fVars & ~varOrderBitset) & view->_fVars).none())
                {
                    preViewAggregate += "aggregates_"+viewName[incViewID]+
                        "["+std::to_string(incAggID)+"]*";
                    
                    incoming[incCounter] = _qc->numberOfViews();
                }
                else
                {
                    if (depth == viewLevelRegister[view_id])
                    {
                        std::cout << "This view has an additional variable: "+
                            viewName[incViewID]+"\nviewAgg: aggregates_"+
                            viewName[incViewID]+"["+std::to_string(incAggID)+"]\n"+
                            "viewLevelReg: " <<viewLevelRegister[view_id]<<
                            " viewLevelRegInc: " <<viewLevelRegister[incViewID]<<
                            " depth: " << depth << std::endl;

                        dependentString[numLocalProducts+viewAggProd] +=
                            "aggregates_"+viewName[incViewID]+
                            "["+std::to_string(incAggID)+"]*";

                        incoming[incCounter] = _qc->numberOfViews();
                        dependentLoopFactors[numLocalProducts+viewAggProd].set(
                            incViewID);

                        addedViewProdHere = true;
                        // TODO: loopFactors should also consider pre and post
                    }
                }
            }
            incCounter += 2;
        }

        // std::cout << "preViewAggregate: " << preViewAggregate << "  " << incoming.size() << std::endl;
        
        // add view product at this depth here and update the prevAgg
        if (!preViewAggregate.empty())
        {
            // Add a previous Aggregate - if there is one?
            if (depth <= viewLevelRegister[view_id] ||
                viewLevelRegister[view_id] == varOrder.size())
            {
                // get the previous Aggregate
                // and add it to the preLocalString                    
                auto prevAgg = localAggReg[numLocalProducts + viewAggProd];
                if (prevAgg.first < varOrder.size()) 
                {
                    // // do somehting
                    // std::cout << "adding to preViewAggregate:  aggregateRegister_"+
                    //     std::to_string(prevAgg.first)+
                    //     "["+std::to_string(prevAgg.second)+"]*";

                    // do somehting
                    preViewAggregate += "aggregateRegister_"+
                        std::to_string(prevAgg.first)+
                        "["+std::to_string(prevAgg.second)+"]*";
                        // +
                        // std::to_string(viewAggProd)+"*"+
                        // std::to_string(numViewProducts)+"*";
                }
                // if this aggregate needs a loop perhaps we should only
                // multiply by the previous Aggregate after the loop?
                // TODO: (CONSIDER) think about keeping this separate 
            }

            preViewAggregate.pop_back();
            preViewAggregate += ";\n";
            
            updateAggregateRegister(
                localAggReg[numLocalProducts+viewAggProd],preViewAggregate,depth,loopFactors,true);
            
            localComputation[numLocalProducts+viewAggProd] = localAggReg[numLocalProducts+viewAggProd];
        }
        else if (depth == viewLevelRegister[view_id])
        {
            // add previous Aggregate (if there is one)
            std::pair<size_t, size_t>& prevAgg = localAggReg[numLocalProducts + viewAggProd];
            if (prevAgg.first < varOrder.size()) 
            {
                // do somehting
                // std::cout << "adding to dependentString:  aggregateRegister_"+
                //     std::to_string(prevAgg.first)+
                //     "["+std::to_string(prevAgg.second)+"]* \n";
                
                dependentString[numLocalProducts+viewAggProd] += "aggregateRegister_"+
                    std::to_string(prevAgg.first)+"["+std::to_string(prevAgg.second)+"]*";
            }
            // if this aggregate needs a loop perhaps we should only
            // multiply by the previous Aggregate after the loop?
            // TODO: (CONSIDER) think about keeping this separate 
        }
        ++viewAggProd;
    }
    
    if (depth != varOrder.size()-1)
    {
        computeAggregateRegister(
            group_id, view_id, agg_id, productSum, incoming, localAggReg,
            splitViewAggSummation,  depth+1);
    }
    else
    {        
        // resetting aggRegisters !
        for (size_t i = 0; i < numLocalProducts+numViewProducts; ++i)
            localAggReg[i] = {varOrder.size(),0};
    }
    
    if (depth > viewLevelRegister[view_id] ||
        viewLevelRegister[view_id] == varOrder.size())
    {
        // now adding aggregates coming from below to running sum
        for (size_t prodID = 0; prodID < numLocalProducts + numViewProducts; ++prodID)
        {
            // std::cout << "prodID: "<< prodID << "  " << numLocalProducts <<"\n";

            // if (prodID < numLocalProducts) 
            //     std::cout << productSum[prodID] <<"\n";

            // This is simply a running sum, so loopFactor will be empty
            loopFactors.reset();
            
            bool local = false, propup = false;
            size_t prevAgg = 0;
            
            std::string postAggregate = "";

            auto& localComp = localComputation[prodID];            

            // Adding local aggregate
            if (localComp.first < varOrder.size())
            {
                // std::cout << "here "<< prodID <<"\n";
                
                postAggregate += "aggregateRegister_"+
                    std::to_string(localComp.first)+"["+std::to_string(localComp.second)+"]*";
                local = true;
            }

            // Adding aggregate from below
            if (localAggReg[prodID].first < varOrder.size())
            {
                // std::cout << "there "<< prodID <<"\n";
                
                if (localAggReg[prodID].first != depth)
                {    
                    ERROR("ERROR ERROR ERROR - we expect localAggReg[prodID].first == depth \n");
                    ERROR(localAggReg[prodID].first << "vs." <<  depth << "\n");
                    exit(1);
                }
                
                postAggregate += "aggregateRegister_"+std::to_string(localAggReg[prodID].first)+
                    "["+std::to_string(localAggReg[prodID].second)+"];";
                propup = true;
                prevAgg = localAggReg[prodID].second;
            }
            
            if (!postAggregate.empty())
            {
                postAggregate.pop_back();
                postAggregate += "--------- POST AGGREGATE;\n";

                size_t prevDepth = (depth > 0 ? depth-1 : varOrder.size());
                size_t previousSize = aggregateRegister[prevDepth].size();
                
                updateAggregateRegister(localAggReg[prodID],postAggregate,prevDepth,loopFactors,false);

                // std::cout << postAggregate << std::endl;x
                // add to postAggregateIndexes the id of the aggregate!
                if (aggregateRegister[prevDepth].size() > previousSize)
                {
                    if (local && propup)
                    {
                        runningSumIndexesComplete[depth].push_back(localAggReg[prodID].second);
                        runningSumIndexesComplete[depth].push_back(localComp.second);
                        runningSumIndexesComplete[depth].push_back(prevAgg);

                        // std::cout << localAggReg[prodID].second << "  " << localComp.second << "  " << prevAgg << std::endl;
                    }
                    else
                    {
                        runningSumIndexesComplete[varOrder.size()+depth].push_back(localAggReg[prodID].second);

                        // std::cout << postAggregate << std::endl;
                        // std::cout << localAggReg[prodID].second << "  ";
                        if (local)
                        {    
                            runningSumIndexesComplete[varOrder.size()+depth].push_back(localComp.second);
                            // std::cout << localComp.second << "  ";                        
                        }
                        
                        if (propup)
                        {
                            runningSumIndexesComplete[varOrder.size()+depth].push_back(prevAgg);
                            // std::cout << prevAgg;
                        }

                        // std::cout << std::endl;             
                    }
                }
            }
            else
            {
                ERROR(depth << ": " << varOrder.size()<< "  " << prodID << "\n");
                ERROR("ERROR ERROR ERROR - postAggregate is empty - we would expect it to contain something \n");
                exit(1);
            }
        }
    }

    // TODO: splitViewAggSummation does not necessarily have to occur when depth
    // = viewLevelRegister
    // TODO: TODO: consider the case where the outgoing view has no freeVars
    
    if (depth == viewLevelRegister[view_id] ||
        (depth == 0 && viewLevelRegister[view_id] == varOrder.size()))
    {
        std::vector<std::string> productString(numLocalProducts+numViewProducts, "");
        bool addToRegister[numLocalProducts+numViewProducts];
        memset(addToRegister, 0, sizeof(addToRegister));
        
        std::vector<std::pair<size_t, size_t>> simpleProducts(numLocalProducts+numViewProducts);

        // get all loops for the view 
        dyn_bitset viewLoopFactors(_qc->numberOfViews()+1);

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
                        viewLoopFactors.set(_qc->numberOfViews());
                    }
                    else // else find corresponding view 
                    {
                        for (const size_t& incViewID : incomingViews[view_id])
                        {
                            if (_qc->getView(incViewID)->_fVars[var])
                                viewLoopFactors.set(incViewID);
                        }
                    }
                }
            }
        }

        // TODO :: NEED LOOP FACTORS FOR EACH PRODUCT!! 
        for (size_t prodID = 0; prodID < numLocalProducts+numViewProducts; ++prodID)
        {
            // TODO: TODO: Set LOOPFACTOR this should be
            // TODO: TODO: the loops for the view output + potential dependent
            // views
            // TODO: NOT SURE IF WE NEED TO ADD THE VIEW LOOPS 
            dependentLoopFactors[prodID] |= viewLoopFactors;

            // Add all dependentVariables 
            productString[prodID] = dependentString[prodID];

            if (!dependentString[prodID].empty())
                addToRegister[prodID] = true;
            
            const std::pair<size_t,size_t>& localComp = localComputation[prodID];
            if (localComp.first < varOrder.size())
            {
                productString[prodID] += "aggregateRegister_"+
                    std::to_string(localComp.first)+"["+std::to_string(localComp.second)+"]*";
                simpleProducts[prodID] = localComp;
            }
            
            if (localAggReg[prodID].first < varOrder.size() ||
                (depth == 0 && viewLevelRegister[view_id] == varOrder.size()))
            {
                productString[prodID] += "aggregateRegister_"+std::to_string(localAggReg[prodID].first)+
                    "["+std::to_string(localAggReg[prodID].second)+"]*";

                // if both localComp and localAggReg are valid then we multiply
                // and add to aggregateRegister
                if (localComp.first < varOrder.size())
                    addToRegister[prodID] = true;
                else 
                    simpleProducts[prodID] = localAggReg[prodID];
                // std::cout << "add below \n";
            }

            if (!productString[prodID].empty())
                productString[prodID].pop_back();

            // std::cout << "LocalComp["<< prodID <<"] ="<<productString[prodID]<<"  addToRegister "<< addToRegister[prodID] << std::endl;
        }

        // TODO: TODO: TODO: TODO:  If the product is not in the aggregateRegister for
        // the depth that we need then we need to add it there?!
        // TODO: CAN THIS BE POSSIBLE?!? (CONSIDER!!!)
        
        if (splitViewAggSummation)
        {    
            for (size_t prodID = 0; prodID < numLocalProducts + numViewProducts; ++prodID)
            {
                if (addToRegister[prodID])
                {
                    // TODO: TODO: TODO: set the postLoopFactor - this should be
                    // the loops for the view output + potential dependent views
                    loopFactors.reset();
                    productString[prodID] += ";\n";

                    updateAggregateRegister(simpleProducts[prodID], productString[prodID],
                                            viewLevelRegister[view_id], dependentLoopFactors[prodID],false);

                    newFinalAggregates[view_id].push_back(simpleProducts[prodID].second);
                    // TODO: STORE THE INDEX OF THE AGGREGATE SO I CAN USE IT
                    // FOR THE AGGREGATE STRING FUNCTION - will have to be
                    // bitset, but normal one is also fine ?!
                }
            }
            
            for (size_t prodID = 0; prodID < numLocalProducts; ++prodID)
            {   
                for (size_t viewAgg = 0; viewAgg < numViewProducts; ++viewAgg)
                {
                    // FINAL OUTPUT = LOCALAGG * VIEW AGG
                    finalAggregateIndexes[view_id].push_back(agg_id);
                    finalAggregateIndexes[view_id].push_back(
                        simpleProducts[prodID].second);
                    finalAggregateIndexes[view_id].push_back(
                        simpleProducts[numLocalProducts+viewAgg].second);
                    // add to the aggregatesIndexArray : aggID,id,id,id
                }
            }
            
            // keep track of how many id's we add to it for each agg
            numAggregateIndexes[view_id] = 3;
            // It is not possible to have no incoming views, because then we
            // would never split the viewAggComputation!
        }
        else
        {
            // TODO: // TODO: // TODO: // TODO: // TODO: // TODO: // TODO: 
            // WHAT WOULD BE THE LOOP FACTOR IN THIS CASE!?
            // this should be the loops for the view output + potential dependent views
            // TODO: // TODO: // TODO: // TODO: // TODO: // TODO: // TODO: 

            std::string aggString = "";
            
            std::pair<size_t,size_t> localAggID = simpleProducts[0];
            if (numLocalProducts > 1 || addToRegister[0])
            {
                loopFactors = viewLoopFactors;
                
                for (size_t prodID = 0; prodID < numLocalProducts; ++prodID)
                {
                    aggString += productString[prodID]+"+";

                    if ((dependentLoopFactors[prodID] & ~loopFactors).any())
                    {
                        ERROR("ERROR ERROR - dependentLoop has more loops than loopFactors\n");
                        exit(0);
                    }
                }
                aggString.pop_back();
                aggString += ";\n";
                
                updateAggregateRegister(
                    localAggID,aggString,viewLevelRegister[view_id],loopFactors,false);

                newFinalAggregates[view_id].push_back(localAggID.second);
            }
            else
            {
                // THIS IS ONLY USED TO PRINT OUT BELOW - COULD REMOVE!
                aggString = productString[0];
            }
            
            // push localAggID to finalAggregateIndexes
            finalAggregateIndexes[view_id].push_back(agg_id);
            finalAggregateIndexes[view_id].push_back(localAggID.second);
            
            if (numberIncomingViews > 0)
            {                
                std::string viewAggString = "";
                size_t prodID = numLocalProducts;
                
                std::pair<size_t,size_t> viewAggID = simpleProducts[prodID];
                
                if (numViewProducts > 1 || addToRegister[prodID])
                {
                    for (size_t viewAgg = numLocalProducts; viewAgg < numLocalProducts+numViewProducts; ++viewAgg)
                    {
                        viewAggString += productString[prodID]+"+";
                        ++prodID;
                    }
                    viewAggString.pop_back();
                    viewAggString += ";\n";

                    updateAggregateRegister(
                        viewAggID,viewAggString,viewLevelRegister[view_id],loopFactors,false);

                    newFinalAggregates[view_id].push_back(viewAggID.second);
                }
                // else
                // {
                //     // THIS IS ONLY USED TO PRINT OUT BELOW - COULD REMOVE! 
                //     viewAggString = productString[prodID];
                // }

                finalAggregateIndexes[view_id].push_back(viewAggID.second);
                
                // FINAL OUTPUT = LOCALAGG * VIEW AGG (if there is one)
                // // push viewAggID to finalAggregateIndexes
                // std::cout << "aggregates_"+viewName[view_id]+"["+std::to_string(agg_id)+"] = ("+
                //     aggString+")("+viewAggString+")\n";
                // if (aggString.empty())
                //     exit(0);
                
            }
            // else
            // {
            //     // TODO: get rid of this else statement
            //     std::cout << "aggregates_"+viewName[view_id]+"["+std::to_string(agg_id)+"] = ("+
            //         aggString+")\n";x
            //     if (aggString.empty())
            //         exit(0);
            // }            
        }

        if (numberIncomingViews > 0)
            numAggregateIndexes[view_id] = 3;
        else
            numAggregateIndexes[view_id] = 2;
    }    
    // TODO: WHEN WE SPLIT VIEW AGG SUMMATION WE ADD MORE PRODUCTS TO THE
    // LIST - HOW CAN WE HANDLE THAT!? // FIXME: // FIXME: // FIXME:
}


void CppGenerator::aggregateGenerator(size_t group_id, const TDNode& node)
{
    const std::vector<size_t>& varOrder = groupVariableOrder[group_id];
    const var_bitset& varOrderBitset = groupVariableOrderBitset[group_id];
    const std::string& relName = node._name;

    aggregateMap.clear();
    aggregateRegister.clear();
    loopRegister.clear();

    aggregateRegister.resize(varOrder.size() + 1);
    loopRegister.resize(varOrder.size() + 1);

    preAggregateIndexes.clear();
    preAggregateIndexes.resize(varOrder.size() + 1);

    dependentAggregateIndexes.clear();
    dependentAggregateIndexes.resize(varOrder.size() + 1);
    
    runningSumIndexesComplete.clear();
    //runningSumIndexesPartial.clear();
    
    runningSumIndexesComplete.resize(varOrder.size() * 2);
    //runningSumIndexesPartial.resize(varOrder.size());
    
    finalAggregateString.clear();
    finalAggregateString.resize(varOrder.size()+1, "");

    localAggregateString.clear();
    localAggregateString.resize(varOrder.size(), "");

    aggregateHeader.clear();
    
    computeViewLevel(group_id,node);    
    
    for (const size_t& viewID : viewGroups[group_id])
    {
        std::cout << "View: "<< viewID << " LevelRegister: " << viewLevelRegister[viewID] << std::endl;
        
        View* view = _qc->getView(viewID);
        
        size_t numberIncomingViews =
            (view->_origin == view->_destination ? node._numOfNeighbors :
             node._numOfNeighbors - 1);

        for (size_t aggNo = 0; aggNo < view->_aggregates.size(); ++aggNo)
        {
            Aggregate* aggregate = view->_aggregates[aggNo];
            
            // for each product in the aggregate
            size_t aggIdx = 0, incomingCounter = 0;
            for (size_t i = 0; i < aggregate->_n; ++i)
            {
                std::vector<prod_bitset> productSum(aggregate->_agg.begin()+aggIdx,
                                                    aggregate->_agg.begin()+aggregate->_m[i]);

                std::vector<size_t> incoming(aggregate->_incoming.begin()+incomingCounter,
                                             aggregate->_incoming.begin()+aggregate->_o[i]);

                size_t numAggProds = (numberIncomingViews == 0 ? 0 : (incoming.size()/2) / numberIncomingViews);

                std::vector<std::pair<size_t, size_t>> localAggReg
                    (productSum.size()+numAggProds,{varOrder.size(), 0});
                // std::vector<std::pair<size_t, size_t>> viewAggReg(numAggProds, {varOrder.size(), 0});

                computeAggregateRegister(
                    group_id, viewID, aggNo, productSum, incoming, localAggReg, false, 0);

                // std::cout << "after computeAggregateRegister " << viewID << "  " << group_id << std::endl;

                aggIdx = aggregate->_m[i];
                incomingCounter = aggregate->_o[i];   
            }
        }
    }
    
    /*********** PRINT OUT *******/ 
    // std::cout << "group_id : " << group_id << std::endl; 
    // for (size_t depth = 0; depth < varOrder.size(); ++depth)
    // {
    //     std::cout<<"depth: " << depth << std::endl;
    //     for (auto p : loopRegister[depth])
    //     {
    //         for (size_t s : p.second)
    //             std::cout << s << " - " << aggregateRegister[depth][s];
    //         for (size_t l = 0; l < _qc->numberOfViews() + 1; ++l)
    //             std::cout << p.first[l] << ", ";
    //         std::cout << "------------" << std::endl;
    //     }
    // }
    std::cout << "group_id : " << group_id << std::endl; 
    for (size_t depth = 0; depth < varOrder.size(); ++depth)
    {
        std::cout<<"depth: " << depth << std::endl;
        for (size_t s = 0; s < aggregateRegister[depth].size(); ++s)
        {
            std::cout << s << " - " << aggregateRegister[depth][s];
            // for (size_t l = 0; l < _qc->numberOfViews() + 1; ++l)
            //     std::cout << loopRegister[depth][s][l] << ", ";
            // std::cout << std::endl;x
        }
    }
    /*********** PRINT OUT *******/
    
    // Create the actual aggregate strings with loops!
    for (size_t depth = 0; depth < varOrder.size(); ++depth)
    {
        // get the aggregate Register index for depth
        const std::vector<std::string>& aggRegister = aggregateRegister[depth];
        const std::vector<dyn_bitset>& loopReg = loopRegister[depth];

        size_t numOfAggs = aggRegister.size();

        if (numOfAggs != loopReg.size())
        {
            ERROR("aggRegister.size() != loopReg.size() - this is a problem!\n");
            exit(0);
        }

        if (numOfAggs == 0)
            continue;
        
        dyn_bitset consideredLoops(_qc->numberOfViews()+1);

        // TODO: declare the Pointer References and aggregate Pointers for
        // views!
        std::string depthString = std::to_string(depth);

        localAggregateString[depth] += offset(3+depth)+relName+"_tuple &"+relName+"Tuple = "+relName+
            "[lowerptr_"+relName+"["+depthString+"]];\n";
        
        for (const size_t& viewID : groupIncomingViews[group_id])
            localAggregateString[depth] += offset(3+depth)+"aggregates_"+viewName[viewID]+" = "+
                viewName[viewID]+"[lowerptr_"+viewName[viewID]+"["+
                std::to_string(depth)+"]].aggregates;\n";
        
        if (depth + 1 < varOrder.size())
        {
            localAggregateString[depth] += genAggregateString(
                aggRegister, loopReg, consideredLoops, preAggregateIndexes[depth], depth);
        }
        else
        {
            // Case 1: loop over relation
            consideredLoops.set(_qc->numberOfViews());
            std::string returnString = genAggregateString(aggRegister, loopReg, consideredLoops, preAggregateIndexes[depth], depth);

            if (!returnString.empty())
            {
                returnString = 
                    offset(3+depth)+"ptr_"+relName+
                    " = lowerptr_"+relName+"["+depthString+"];\n"+
                    offset(3+depth)+"while(ptr_"+relName+
                    " <= upperptr_"+relName+"["+depthString+"])\n"+offset(3+depth)+
                    "{\n"+offset(4+depth)+relName+"_tuple &"+relName+"Tuple = "+
                    relName+"[ptr_"+relName+"];\n"+returnString+
                    offset(4+depth)+"++ptr_"+relName+";\n"+offset(3+depth)+"}\n";
            }
            
            // Case 2: do not loop over the relation
            consideredLoops.reset(_qc->numberOfViews());
            returnString += genAggregateString(aggRegister, loopReg, consideredLoops, preAggregateIndexes[depth], depth);

            localAggregateString[depth] += returnString;            
        }

        std::string indexes = "";
        std::string prevDepth = std::to_string(depth > 0 ? depth-1 : varOrder.size());

        // Now create array for postAggregates / Running Sum
        if (!runningSumIndexesComplete[depth].empty()) 
        {
            size_t numOfIndexes = runningSumIndexesComplete[depth].size();
            
            for (size_t agg = 0; agg < numOfIndexes; ++agg)
            {
                // TODO: verify that loopFactor is empty! (for testing)
                indexes += std::to_string(runningSumIndexesComplete[depth][agg])+",";
            }
        
            std::string forLoop =
                offset(3+depth)+"for(size_t i = 0; i < "+std::to_string(numOfIndexes)+"; i += 3)\n"+
                offset(4+depth)+"aggregateRegister_"+prevDepth+"[runSumIdx_"+depthString+"[i]] += "+
                "aggregateRegister_"+depthString+"[runSumIdx_"+depthString+"[i+1]] *"+
                "aggregateRegister_"+depthString+"[runSumIdx_"+depthString+"[i+2]];\n";

            finalAggregateString[depth] += forLoop;
        }

        if (!runningSumIndexesComplete[varOrder.size()+depth].empty()) 
        {
            // std::string indexes = "";
            size_t numOfIndexes = runningSumIndexesComplete[varOrder.size()+depth].size();
            
            for (size_t agg = 0; agg < numOfIndexes; ++agg)
            {
                // TODO: verify that loopFactor is empty! (for testing)
                indexes += std::to_string(runningSumIndexesComplete[varOrder.size()+depth][agg])+",";
            }
            
            std::string lowerBound = std::to_string(runningSumIndexesComplete[depth].size());
            std::string upperBound = std::to_string(numOfIndexes + runningSumIndexesComplete[depth].size());
            
            std::string forLoop = offset(3+depth)+"for(size_t i = "+lowerBound+"; i < "+
                upperBound+"; i += 2)\n"+offset(4+depth)+
                "aggregateRegister_"+prevDepth+"[runSumIdx_"+depthString+"[i]] += "+
                "aggregateRegister_"+depthString+"[runSumIdx_"+depthString+"[i+1]];\n";
            
            finalAggregateString[depth] += forLoop;
        }

        if (!indexes.empty())
        {
            indexes.pop_back();

            std::string numOfIndexes = std::to_string(
                runningSumIndexesComplete[depth+varOrder.size()].size()+runningSumIndexesComplete[depth].size());
            
            aggregateHeader += offset(2)+"size_t runSumIdx_"+depthString+"["+
                numOfIndexes+"] = {"+indexes+"};\n";
        }
        
    }

    std::vector<std::vector<size_t>> viewDepthLevels(varOrder.size()+1);

    // We need to do the same for each output view
    for (const size_t& viewID : viewGroups[group_id])
    {
        // TODO: TODO: TODO: we should combine all views that can be computed at
        // the same level and do loop fusion for all of them!!
        // TODO: This higly depends on the loops in the head of the view!
        // Perhaps just keep each view separate for now - merge later! 

        View* view = _qc->getView(viewID);
        viewLoopFactors[viewID].resize(_qc->numberOfViews()+1);

        // Compute the loops that are required by this view. 
        for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
        {
            if (view->_fVars[var])
            {
                if (!varOrderBitset[var])
                {
                    // if (viewID == 5)
                    // {
                    //     ERROR(_td->getRelation(view->_origin)->_name <<" - "<< _td->getRelation(view->_destination)->_name <<"\n");x
                    //     ERROR("we get here for 5 - "<< group_id << "\n" << view->_fVars  <<"\n");
                    //     ERROR(varOrderBitset << "\n");
                    //     ERROR(_td->getAttribute(4)->_name << "\n");
                    //     exit(1);
                    // }
                    
                    // if var is in relation - use relation 
                    if (node._bag[var])
                    {                                   
                        viewLoopFactors[viewID].set(_qc->numberOfViews());
                    }
                    else // else find corresponding view 
                    {
                        for (const size_t& incViewID : incomingViews[viewID])
                        {
                            if (_qc->getView(incViewID)->_fVars[var])
                                viewLoopFactors[viewID].set(incViewID);
                        }
                    }
                }
            }
        }
        
        // TODO: FIXME: I don't want to add this for each view! Can I combine them somehow?!
        // get the aggregate Register index for depth
        // TODO: Need to do loop Fusion for the views as well! 
        // const std::vector<std::string>& aggRegister = aggregateRegister[viewLevelRegister[viewID]];
        // const std::vector<dyn_bitset>& loopReg = loopRegister[viewLevelRegister[viewID]];
        
        // includableAggs[viewID].resize(aggRegister.size());        
        // for (size_t& finalAgg : newFinalAggregates[viewID])
        //     includableAggs[viewID].set(finalAgg);        
        viewDepthLevels[viewLevelRegister[viewID]].push_back(viewID);
    }
    
    for (size_t depth = 0; depth <= varOrder.size(); depth++)
    {
        size_t offDepth = (depth < varOrder.size() ?
                           (depth+1 < varOrder.size() ?
                            depth+4 : depth+3) : 2);
        
        std::string returnString = "";
        if (!viewDepthLevels[depth].empty())
        {
            // create a list of all aggregates that need to be computed here
            const std::vector<std::string>& aggRegister = aggregateRegister[depth];
            const std::vector<dyn_bitset>& loopReg = loopRegister[depth];

            dyn_bitset includableAggs(aggRegister.size());
            for (const size_t& viewID : viewDepthLevels[depth])
            {
                for (size_t& finalAgg : newFinalAggregates[viewID])
                    includableAggs.set(finalAgg);
            }
            
            dyn_bitset consideredLoops(_qc->numberOfViews()+1);
            dyn_bitset addedViews(_qc->numberOfViews());

            dyn_bitset includableViews(_qc->numberOfViews());
            for (const size_t& viewID : viewDepthLevels[depth])
                includableViews.set(viewID);
            
            // first check for relation loop
            consideredLoops.set(_qc->numberOfViews());
        
            std::string aggString = genFinalAggregateString(
                aggRegister, loopReg, consideredLoops, includableAggs, depth,
                viewDepthLevels[depth], addedViews, offDepth);

            if (!aggString.empty())
            {
                std::string depthString = std::to_string(depth);
                
                returnString = offset(offDepth)+// relName+"_tuple &"+relName+"Tuple = "+relName+
                    // "[lowerptr_"+relName+"["+depthString+"]];\n"+
                    offset(offDepth)+"ptr_"+relName+
                    " = lowerptr_"+relName+"["+depthString+"];\n"+
                    offset(offDepth)+"while(ptr_"+relName+
                    " <= upperptr_"+relName+"["+depthString+"])\n"+
                    offset(offDepth)+"{\n"+offset(offDepth+1)+
                    relName+"_tuple &"+relName+"Tuple = "+relName+"[ptr_"+relName+"];\n"+ aggString +
                    offset(offDepth+1)+"++ptr_"+relName+";\n"+offset(offDepth)+"}\n";
            }

            if ((includableViews & ~addedViews).any())
            {                
                // then without
                consideredLoops.reset(_qc->numberOfViews());

                returnString += genFinalAggregateString(
                    aggRegister, loopReg, consideredLoops, includableAggs, depth,
                    viewDepthLevels[depth], addedViews, offDepth);
            }


            if (depth+1 < varOrder.size())
            {
                // TODO: THIS HAS TO BE WRAPPED IN A IF STATEMENT

                returnString =  offset(depth+3)+
                    "if (addTuple["+std::to_string(depth)+"])\n"+
                    offset(depth+3)+"{\n"+returnString+offset(3+depth)+"}\n";
            }
            
            finalAggregateString[depth] += returnString;            
        }
    }


    for (size_t depth = 0; depth < varOrder.size(); ++depth)
    {
        std::cout << localAggregateString[depth] << std::endl;
        std::cout << finalAggregateString[depth] << std::endl;
    }
    
}

void CppGenerator::updateAggregateRegister(
    std::pair<size_t,size_t>& outputIndex, const std::string& aggString,
    size_t depth, const dyn_bitset& loopFactor, bool preAgg)
{
    auto it = aggregateMap.find(aggString);
    if (it != aggregateMap.end())
    {
        outputIndex = it->second;
        
        if (outputIndex.first != depth)
        {
            // TODO: consider changing this!?!
            ERROR("** We are re-using the wrong aggregate - "<<
                  "need to kepp aggregate Map for each depth!!\n");
            ERROR(aggString+" " << outputIndex.first <<"  "  << depth << "\n");
            exit(1);
        }
    }
    else
    {
        size_t numberOfAggs = aggregateRegister[depth].size();

        aggregateRegister[depth].push_back(aggString);
        aggregateMap[aggString] = {depth,numberOfAggs};
        loopRegister[depth].push_back(loopFactor);

        outputIndex = {depth,numberOfAggs};
        preAggregateIndexes[depth].push_back(preAgg);
    }
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
                    " = "+viewName[viewID]+"Tuple].aggregates;\n";

                dyn_bitset nextConsideredLoops = consideredLoops;
                nextConsideredLoops.set(viewID);

                returnString += genAggregateString(aggRegister,loopReg,
                    nextConsideredLoops, includableAggs, depth);
            
                returnString += offset(4+depth+numOfLoops)+"++ptr_"+viewName[viewID]+";\n"+
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
                        indexes += std::to_string(finalAggregateIndexes[viewID][idx])+",";
                    indexes.pop_back();

                    // TODO: THE VIEW OUTPUT INDEXES SHOULD GO TO BEGINNING OF
                    // THE COMPUTE METHOD
                    aggregateHeader +=  offset(2)+"size_t "+viewName[viewID]+"Indexes["+
                        std::to_string(numOfIndexes)+"] = {"+indexes+"};\n";
                    
                    outputString += offset(offDepth+numOfLoops)+"for(size_t i = 0; i < "+
                        std::to_string(numOfIndexes)+"; i += "+std::to_string(numAggregateIndexes[viewID])+")\n"+
                        offset(offDepth+1+numOfLoops)+"aggregates_"+viewName[viewID]+"["+viewName[viewID]+"Indexes[i]] += "+
                        "aggregateRegister_"+std::to_string(depth)+"["+viewName[viewID]+"Indexes[i+1]]";
        
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
    bool addTuple;

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

    if (depth + 1 != varOrder.size())
        addTuple = true;
    else
        addTuple = false;
    
    // Update rel pointers 
    returnString += offset(2+depth)+ "rel["+depthString+"] = 0;\n"+
        offset(2+depth)+ "atEnd["+depthString+"] = false;\n";

    // Start while loop of the join 
    returnString += offset(2+depth)+"while(!atEnd["+depthString+"])\n"+
        offset(2+depth)+"{\n";

    if (numberContributing > 1)
    {
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
    }
    
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

#ifndef PREVIOUS
    // setting the addTuple bool to false;
    returnString += offset(3+depth)+"addTuple["+depthString+"] = false;\n";

    // resetting the aggregateRegister for this level
    if (!aggregateRegister[depth].empty())
        returnString += offset(3+depth)+"memset(aggregateRegister_"+depthString+
            ", 0, sizeof(double) * "+std::to_string(aggregateRegister[depth].size())+");\n";

    // Insert Code for AggregateRegister HERE
    returnString += localAggregateString[depth];
#else        
    for (const size_t& viewID : viewGroups[group_id])
    {
        View* view = _qc->getView(viewID);
        
        if (!_requireHashing[viewID] && depth + 1 ==
            (view->_fVars & groupVariableOrderBitset[group_id]).count())
        {
            if (addTuple)
                returnString += offset(3+depth)+
                    "bool addTuple_"+viewName[viewID]+" = false;\n";
            returnString += offset(3+depth)+"memset(aggregates_"+viewName[viewID]+
                ", 0, sizeof(double) * "+std::to_string(view->_aggregates.size())+");\n";
        }
    }
#endif
    
    // TODO: FACTORIZED COMPUTATION OF THE AGGREGATES SHOULD PROBABLY BE DONE
    // HERE !
    
    // then you would need to go to next variable
    if (depth+1 < varOrder.size())
    {
        // leapfrogJoin for next join variable 
        returnString += genGroupLeapfrogJoinCode(group_id, node, depth+1);
    }
#ifndef PREVIOUS
    else
    {
        returnString += offset(3+depth)+"memset(addTuple, 1, sizeof(bool) * "+
            std::to_string(varOrder.size())+");\n";
    }

    returnString += finalAggregateString[depth];

    // TODO: // TODO: // TODO: // TODO: // TODO: // TODO: // TODO: // TODO:
    // INSERT CODE FOR FINAL AGGREGATES HERE !!
    // TODO: // TODO: // TODO: // TODO: // TODO: // TODO: // TODO: // TODO:

#else
    else
    {
        returnString += offset(3+depth)+
            "/****************AggregateComputation*******************/\n";

/******************** LOCAL COMPUTATION *********************/


        // generate an array that computes all local computation required
        // create map that gives index to this local aggregate - this is
        // used to create the array  

        // TODO: QUESTION : how do we handle loops?
        std::unordered_map<std::vector<prod_bitset>, size_t> localComputationAggregates;
        std::unordered_map<std::string, size_t> childAggregateComputation;

        std::vector<std::string> localAggregateIndexes;
        std::vector<std::string> childAggregateIndexes;
        
        std::vector<std::vector<size_t>> aggregateComputationIndexes(_qc->numberOfViews());

        // size_t cachedChildAggregate = 0;
        // size_t cachedLocalAggregate = 0;
        
        std::vector<bool> allLoopFactors(_qc->numberOfViews() + 1);
        std::unordered_map<std::vector<bool>,std::string> aggregateSection;
/*
        for (const size_t& viewID : viewGroups[group_id])
        {
            View* view = _qc->getView(viewID);

            size_t numberIncomingViews =
                (view->_origin == view->_destination ? node._numOfNeighbors :
                 node._numOfNeighbors - 1);

            
            std::vector<bool> viewLoopFactors(_qc->numberOfViews() + 1);
            size_t numLoopFactors = 0;

            std::string aggregateString = "";

            for (size_t v = 0; v < NUM_OF_VARIABLES; ++v)
            {
                if (view->_fVars[v])
                {
                    if (!groupVariableOrderBitset[group_id][v])
                    {
                        // if var is in relation - use relation 
                        if (node._bag[v])
                        {                                                            
                            if (!viewLoopFactors[_qc->numberOfViews()])
                                ++numLoopFactors;
                                                        
                            viewLoopFactors[_qc->numberOfViews()] = 1;
                            allLoopFactors[_qc->numberOfViews()] = 1;

                        }
                        else // else find corresponding view 
                        {
                            for (const size_t& incViewID : incomingViews[viewID])
                            {
                                if (_qc->getView(incViewID)->_fVars[v])
                                {
                                    if (!viewLoopFactors[incViewID])
                                        ++numLoopFactors;
                                                        
                                    viewLoopFactors[incViewID] = 1;
                                    allLoopFactors[incViewID] = 1;                               

                                }
                            }
                        }
                    }
                }
            }

            // TODO: FIXME: Check if this view requires any loops
            // i.e. if free variables of this view are not in the variable order
            // - then we need to loop over corresp. factor (for all aggregates that this
            // view contains )
            
            // TODO: For each aggregate - check if it needs to be in a loop or not 
            // if view requires it - entire aggregate is in loop
            // else if only local comptutation requires it -> we can aggregate
            // away!
            // but then the child aggregate also needs to be in the loop -->
            // thus we need to compute entire aggregate in loop!
            // Unless we have a more adaptive approach to it (i.e. do local *
            // child which needs to be in loop, then multiply by other children
            // that are not in loop? -> could enable more sharing!

            // TODO: can we use the ring computation here somehow?
            
            // TODO: if there is only one child - avoid the childAggregate
            // Computation! - use the aggregateVector instead!
            
            for (size_t aggID = 0; aggID < view->_aggregates.size(); ++aggID)
            {
                // std::cout << "View: " << viewID << " Aggregate: " << aggID << std::endl;

                // get the aggregate pointer
                Aggregate* aggregate = view->_aggregates[aggID];

                std::vector<bool> localLoopFactors (viewLoopFactors);
                std::vector<bool> childLoopFactors (viewLoopFactors);
                
                size_t localAggregateIndex, childAggregateIndex;
                size_t aggIdx = 0, incomingCounter = 0;
                for (size_t i = 0; i < aggregate->_n; ++i)
                {           
                    // this give us a vector of products
                    std::vector<prod_bitset> localProds
                        (aggregate->_agg.begin()+aggIdx, aggregate->_agg.begin()+aggregate->_m[i]);

                    // This loop defines the local aggregates computed at this node
                    std::string localAgg = "";

                    auto it = localComputationAggregates.find(localProds);
                    if (it != localComputationAggregates.end())
                    {
                        std::cout << "cached: localAggregates["+std::to_string(it->second)+"]" << std::endl;
                        
                        localAgg += "localAggregates["+std::to_string(it->second)+"]";
                        localAggregateIndex = it->second;
                        ++cachedLocalAggregate;
                    }
                    else
                    {
                        size_t numberOfProducts = localProds.size();
                        size_t numberOfFunctions = (numberOfProducts > 1 ? 2 : localProds[0].count());
                        
                        // do each product and check if it is in localProds 
                        for (const prod_bitset& p : localProds)
                        {
                            // TODO: I don't want to add several
                            // localAggregates together - need to keep them
                            // separated! 
                            std::vector<prod_bitset> prodBitset = {p};
                            auto it = localComputationAggregates.find(prodBitset);
                            if (it != localComputationAggregates.end())
                            {
                                // reuses localComputation component
                                localAgg += "localAggregates["+std::to_string(it->second)+"] ";
                                ++cachedLocalAggregate;
                                localAggregateIndex = it->second;
                            }
                            else
                            {
                                if (p.none())
                                {
                                    std::string count = "1 ";
                                    // if (localLoopFactors[_qc->numberOfViews()]) 
                                    //     count = "1 ";
                                    // else
                                    //     count = "upperptr_"+relName+"["+depthString+"]-"+
                                    //         "lowerptr_"+relName+"["+depthString+"]+1 ";

                                    localAgg += count;
                                    localAggregateIndexes.push_back(count);
                                    localComputationAggregates[{p}] = localAggregateIndexes.size()-1;
                                    localAggregateIndex = localAggregateIndexes.size()-1;
                                }                                
                                for (size_t f = 0; f < NUM_OF_FUNCTIONS; ++f)
                                {
                                    if (p.test(f))
                                    {
                                        // Test if this function has been computed
                                        // create a product with just one function
                                        std::vector<prod_bitset> func(1);
                                        func[0].set(f);

                                        auto it = localComputationAggregates.find(func);
                                        if (it != localComputationAggregates.end())
                                        {
                                            // function computed - so reuse
                                            localAgg += "localAggregates["+std::to_string(it->second)+"]";
                                            localAggregateIndex = it->second;
                                            ++cachedLocalAggregate;
                                        }
                                        else
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
                                                        freeVarString += relName+"Pointer."+
                                                            _td->getAttribute(v)->_name+",";
                                                            
                                                        if (!groupVariableOrderBitset[group_id][v])
                                                        {
                                                            if (!localLoopFactors[_qc->numberOfViews()])
                                                                ++numLoopFactors;
                                                            localLoopFactors[_qc->numberOfViews()] = 1;
                                                            allLoopFactors[_qc->numberOfViews()] = 1;
                                                        }
                                                    }
                                                    else
                                                    {
                                                        // IF NOT WE NEED TO FIND THE CORRESPONDING VIEW
                                                        size_t incCounter = incomingCounter;
                                                        bool found = false;
                                                        while(!found && incCounter < aggregate->_o[i])
                                                        {
                                                            for (size_t n = 0; n < numberIncomingViews; ++n)
                                                            {
                                                                const size_t& viewID =
                                                                    aggregate->_incoming[incCounter];

                                                                View* incView = _qc->getView(viewID);
                                                                if (incView->_fVars[v])
                                                                {
                                                                    // variable is part of varOrder 
                                                                    if (groupVariableOrderBitset[group_id][v])
                                                                    {
                                                                        freeVarString += viewName[viewID]+"[lowerptr_"+
                                                                            viewName[viewID]+"["+depthString+"]]."+
                                                                            _td->getAttribute(v)->_name+",";
                                                                    }
                                                                    else
                                                                    {
                                                                        // add V_ID.var_name to the fVarString
                                                                        freeVarString +=
                                                                            viewName[viewID]+"[ptr_"+viewName[viewID]+"]."+
                                                                            _td->getAttribute(v)->_name+",";
                                                                        
                                                                        if (!localLoopFactors[viewID])
                                                                            ++numLoopFactors;
                                                                        localLoopFactors[viewID] = 1;
                                                                        allLoopFactors[viewID] = 1;
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
                                            
                                            if (freeVarString.empty())
                                            {
                                                ERROR("We have a problem - function has no freeVars. \n");
                                                exit(1);
                                            }
                                            freeVarString.pop_back();

                                            // std::cout << "f("+getFunctionString(function,freeVarString)+")" << viewID << std::endl;
                                            localAggregateIndexes.push_back(getFunctionString(function,freeVarString));
                                            localComputationAggregates[func] = localAggregateIndexes.size()-1;
                                            localAggregateIndex = localAggregateIndexes.size()-1;


                                            // TODO: should add this to the map
                                            // of localVar computation! 

                                            // This is just one function that we want to
                                            // multiply with other functions and 
                                            // turn function into a string
                                            // of cpp-code
                                            localAgg += "localAggregates["+std::to_string(localAggregateIndexes.size()-1)+"]";   
                                            //getFunctionString(function,freeVarString);
                                        }
                                        localAgg += "*";
                                    }    
                                }
                            }
                            
                            localAgg.pop_back();
                            // if (!localAgg.empty())
                            // {
                            //     if (p.count() != 1)
                            //     {
                            //         localAggregateIndexes.push_back(localAgg);
                            //         localComputationAggregates[prodBitset] = localAggregateIndexes.size()-1;
                            //     }
                            // }
                            localAgg += "+";
                        }
                        
                        localAgg.pop_back();
                        // if (!localAgg.empty() && numberOfProducts > 1)
                        if (!localAgg.empty()  && numberOfFunctions > 1)
                        {
                            localAggregateIndexes.push_back(localAgg);
                            localComputationAggregates[localProds] = localAggregateIndexes.size()-1;
                            localAggregateIndex = localAggregateIndexes.size()-1;
                        }
                    }
                    aggIdx = aggregate->_m[i];
                    
                    // NOW DO CHILD AGGREGATE !!
                    // This loop adds the aggregates that come from the views
                    // below
                    size_t numberAdded = 0;
                    std::string viewAgg = " ";
                    while(incomingCounter < aggregate->_o[i])
                    {
                        std::string singleViewAgg = "";
                        for (size_t n = 0; n < numberIncomingViews; ++n)
                        {
                            size_t viewID = aggregate->_incoming[incomingCounter];
                            size_t aggID = aggregate->_incoming[incomingCounter+1];
                            
                            // if (loopFactors[viewID])
                            //     viewAgg += "aggregates_"+viewName[viewID]+
                            //         "["+std::to_string(aggID)+"]*";
                            // else
                            singleViewAgg += "aggregates_"+viewName[viewID]+
                                "["+std::to_string(aggID)+"]*";
                            
                            incomingCounter += 2;
                        }
                        singleViewAgg.pop_back();

                        // size_t singleChildAggIndex;
                        auto it2 = childAggregateComputation.find(singleViewAgg);
                        // check if viewAgg is in map --> if so reuse
                        if (it2 != childAggregateComputation.end())
                        {
                            childAggregateIndex = it2->second;
                            std::cout << "chached child Agg "+singleViewAgg+"\n";
                            cachedChildAggregate++;
                        }
                        else
                        {
                            childAggregateComputation[singleViewAgg] =
                                childAggregateIndexes.size();
                            childAggregateIndex = childAggregateIndexes.size();
                            childAggregateIndexes.push_back(singleViewAgg);
                        }
                        viewAgg += "childAggregates["+std::to_string(childAggregateIndex)+"]+";
                        ++numberAdded;
                    }
                    viewAgg.pop_back();

                    if (!viewAgg.empty() && numberAdded > 1)
                    {
                        auto it2 = childAggregateComputation.find(viewAgg);
                        // check if viewAgg is in map --> if so reuse
                        if (it2 != childAggregateComputation.end())
                        {
                            childAggregateIndex = it2->second;
                            cachedChildAggregate++;
                            std::cout << "chached child Agg "+viewAgg+"\n";
                        }
                        else
                        {
                            childAggregateComputation[viewAgg] =
                                childAggregateIndexes.size();
                            childAggregateIndex = childAggregateIndexes.size();
                            childAggregateIndexes.push_back(viewAgg);
                        }
                    }
                    
                    // TODO: add to array -> aggID, localAggPointer,
                    // childAggPointer

                    // When there is no childAggPointer then we can add directly
                    // to the aggregate array - no need for different pointer! 
                    aggregateComputationIndexes[viewID].push_back(aggID);
                    aggregateComputationIndexes[viewID].push_back(localAggregateIndex);
                    if (numberIncomingViews > 0) 
                        aggregateComputationIndexes[viewID].push_back(childAggregateIndex);
                }
            }
        }

        // CREATE THE ARRAY OF LOCAL COMPUTATIONS
        size_t idx = 0;
        for (std::string& s : localAggregateIndexes)
            std::cout << "localAggreagtes["+std::to_string(idx++) <<"] = "+s+";\n";

        idx = 0;
        for (std::string& s : childAggregateIndexes)
            std::cout << "childAggregates["+std::to_string(idx++) <<"] = "+s+";\n";

        for (size_t& viewID : viewGroups[group_id])
        {
            View* view = _qc->getView(viewID);
            
            size_t numberIncomingViews =
                (view->_origin == view->_destination ? node._numOfNeighbors :
                 node._numOfNeighbors - 1);

            if (numberIncomingViews > 0)
            {
                for (size_t s = 0; s < aggregateComputationIndexes[viewID].size(); s+=3)
                {
                    size_t t = aggregateComputationIndexes[viewID][s];
                    size_t t1 = aggregateComputationIndexes[viewID][s+1];
                    size_t t2 = aggregateComputationIndexes[viewID][s+2];
                
                    std::cout << "aggregate["+std::to_string(t)+"] = localAggregate["+std::to_string(t1)+"] * childAggregate["+std::to_string(t2)+"];\n";
                }
            }
            else
            {
                for (size_t s = 0; s < aggregateComputationIndexes[viewID].size(); s+=2)
                {
                    size_t& t = aggregateComputationIndexes[viewID][s];
                    size_t& t1 = aggregateComputationIndexes[viewID][s+1];
                
                    std::cout << "aggregate["+std::to_string(t)+"] = localAggregate["+std::to_string(t1)+"];\n";
                }
            }   
        }       

        for (size_t& viewID : viewGroups[group_id])
        {
            View* view = _qc->getView(viewID);
            
            size_t numberIncomingViews =
                (view->_origin == view->_destination ? node._numOfNeighbors :
                 node._numOfNeighbors - 1);


            size_t numberOfAggs = 0;
            std::string aggIndex = "size_t aggregateIndexes_"+viewName[viewID]+"[] = {";
            for (const size_t& s : aggregateComputationIndexes[viewID])
            {
                aggIndex += std::to_string(s) + ",";
                numberOfAggs++;
            }
            aggIndex.pop_back();
            aggIndex += "};\n";
            std::cout << aggIndex << std::endl;

            
            std::string aggLoop;
            
            if (numberIncomingViews > 0)
                aggLoop += "for (size_t off = 0; off < "+std::to_string(numberOfAggs)+
                    "; off += 3)\n"+offset(1)+"aggregates_"+viewName[viewID]+"["+
                    "aggregateIndexes_"+viewName[viewID]+"[off]] += localAggregate["+
                    "aggregateIndexes_"+viewName[viewID]+"[off+1]] * childAggregate["+
                    "aggregateIndexes_"+viewName[viewID]+"[off+2]];\n";
            else
                aggLoop += "for (size_t off = 0; off < "+std::to_string(numberOfAggs)+
                    "; off += 2)\n"+offset(1)+"aggregates_"+viewName[viewID]+"["+
                    "aggregateIndexes_"+viewName[viewID]+"[off]] += localAggregate["+
                    "aggregateIndexes_"+viewName[viewID]+"[off+1]];\n";
            std::cout << aggLoop << std::endl;
        }
        
        // TODO: How do we handle loops? Where do we place them? What do we
        // do with them?

        std::cout << cachedLocalAggregate << std::endl;
        std::cout << cachedChildAggregate << std::endl;

        // if (group_id == 4)
        //     exit(1);
*/ 
/**************************** LOCAL COMPUTATION *********/
            
        // std::unordered_map<std::vector<bool>,std::string> aggregateSection;

        //       std::vector<bool> allLoopFactors(_qc->numberOfViews() + 1);

        /* For each view in this group ... */
        for (const size_t& viewID : viewGroups[group_id])
        {
            View* view = _qc->getView(viewID);

            if (!_requireHashing[viewID] && depth + 1 > (view->_fVars & groupVariableOrderBitset[group_id]).count()) 
                returnString += offset(3+depth)+"addTuple_"+
                    viewName[viewID]+" = true;\n";

            size_t numberIncomingViews =
                (view->_origin == view->_destination ? node._numOfNeighbors :
                 node._numOfNeighbors - 1);

            std::vector<bool> loopFactors(_qc->numberOfViews() + 1);
            size_t numLoopFactors = 0;

            std::string aggregateString = "";

            if (_requireHashing[viewID])
            {
                for (size_t v = 0; v < NUM_OF_VARIABLES; ++v)
                {
                    if (view->_fVars[v])
                    {
                        if (!groupVariableOrderBitset[group_id][v])
                        {
                            // if var is in relation - use relation 
                            if (node._bag[v])
                            {                                                            
                                if (!loopFactors[_qc->numberOfViews()])
                                    ++numLoopFactors;
                                                        
                                loopFactors[_qc->numberOfViews()] = 1;
                                allLoopFactors[_qc->numberOfViews()] = 1;

                            }
                            else // else find corresponding view 
                            {
                                for (const size_t& incViewID : incViews)
                                {
                                    if (_qc->getView(incViewID)->_fVars[v])
                                    {
                                        if (!loopFactors[incViewID])
                                            ++numLoopFactors;
                                                        
                                        loopFactors[incViewID] = 1;
                                        allLoopFactors[incViewID] = 1;                               

                                    }
                                }
                            }
                        }
                    }
                }
            }           
            for (size_t aggID = 0; aggID < view->_aggregates.size(); ++aggID)
            {
                // get the aggregate pointer
                Aggregate* aggregate = view->_aggregates[aggID];
            
                std::string agg = "";
                size_t aggIdx = 0, incomingCounter = 0;
                for (size_t i = 0; i < aggregate->_n; ++i)
                {                   
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
                                        if (node._bag[v])
                                        {
                                            if (groupVariableOrderBitset[group_id][v])
                                            {
                                                // freeVarString += relName+"[lowerptr_"+
                                                //     relName+"["+depthString+"]]."+
                                                //     _td->getAttribute(v)->_name+",";

                                                freeVarString += relName+"Pointer."+
                                                    _td->getAttribute(v)->_name+",";
                                            }
                                            else
                                            {
                                                // freeVarString +=
                                                //     relName+"[ptr_"+relName+"]."+
                                                //     _td->getAttribute(v)->_name+",";

                                                freeVarString += relName+"Pointer."+
                                                    _td->getAttribute(v)->_name+",";

                                                if (!loopFactors[_qc->numberOfViews()])
                                                    ++numLoopFactors;
                                                        
                                                loopFactors[_qc->numberOfViews()] = 1;
                                                allLoopFactors[_qc->numberOfViews()] = 1;
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
                                                        /* variable is part of varOrder */
                                                        if (groupVariableOrderBitset[group_id][v])
                                                        {
                                                            // // add V_ID.var_name to the fVarString
                                                            // freeVarString += viewName[viewID]+"[lowerptr_"+
                                                            //     viewName[viewID]+"["+depthString+"]]."+
                                                            //     _td->getAttribute(v)->_name+",";
                                                            
                                                            freeVarString += viewName[viewID]+"[lowerptr_"+
                                                                viewName[viewID]+"["+depthString+"]]."+
                                                                _td->getAttribute(v)->_name+",";
                                                        }
                                                        else
                                                        {
                                                            // add V_ID.var_name to the fVarString
                                                            freeVarString +=
                                                                viewName[viewID]+"[ptr_"+viewName[viewID]+"]."+
                                                                _td->getAttribute(v)->_name+",";

                                                            if (!loopFactors[viewID])
                                                                ++numLoopFactors;
                                                            loopFactors[viewID] = 1;
                                                            allLoopFactors[viewID] = 1;
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
                                    ERROR("We have a problem - function has no freeVars. \n");
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
                    
                    // TODO: Could the if condition be only with loopFactors?
                    // If there is no local aggregate function then we simply
                    // add the count for this group.
                    if (localAgg.size() == 0 && !loopFactors[_qc->numberOfViews()]) 
                        localAgg = "upperptr_"+relName+"["+depthString+"]-"+
                            "lowerptr_"+relName+"["+depthString+"]+1";
                    
                    // if (localAgg.size() == 0 && loopFactors[_qc->numberOfViews()]) 
                    //     localAgg = "1";

                    // This loop adds the aggregates that come from the views below
                    std::string viewAgg = "";
                    while(incomingCounter < aggregate->_o[i])
                    {
                        for (size_t n = 0; n < numberIncomingViews; ++n)
                        {
                            size_t viewID = aggregate->_incoming[incomingCounter];
                            size_t aggID = aggregate->_incoming[incomingCounter+1];

                            if (loopFactors[viewID])
                                viewAgg += "aggregates_"+viewName[viewID]+"["+std::to_string(aggID)+"]*";
                            else
                                viewAgg += "aggregates_"+viewName[viewID]+"["+std::to_string(aggID)+"]*";                            
                            incomingCounter += 2;
                        }
                        viewAgg.pop_back();
                        viewAgg += "+";
                    }
                    if (!viewAgg.empty())
                        viewAgg.pop_back();
                    if (!viewAgg.empty() && !localAgg.empty())
                        agg += "("+localAgg+")*("+viewAgg+")+";
                    else 
                        agg += localAgg+viewAgg+"+"; // One is empty
                } // end localAggregate (i.e. one product
                agg.pop_back();

                if (agg.size() == 0 && !loopFactors[_qc->numberOfViews()]) 
                    agg = "upperptr_"+relName+"["+depthString+"]-"+
                        "lowerptr_"+relName+"["+depthString+"]+1";
                if (agg.size() == 0 && loopFactors[_qc->numberOfViews()]) 
                    agg = "1";

                if (!_requireHashing[viewID]) 
                    aggregateString += offset(3+depth+numLoopFactors)+
                        "aggregates_"+viewName[viewID]+"["+std::to_string(aggID)+"] += "+
                        agg+";\n";
                else
                    aggregateString += offset(3+depth+numLoopFactors)+
                        viewName[viewID]+"[idx_"+viewName[viewID]+"].aggregates["+
                        std::to_string(aggID)+"] += "+agg+";\n"; 
            } // end complete Aggregate

            std::string findTuple = "";
            if (_requireHashing[viewID])
            {
                // size_t varCounter = 0;
                std::string mapVars = "";            
                for (size_t v = 0; v < NUM_OF_VARIABLES; ++v)
                {
                    if (view->_fVars[v])
                    {
                        if (node._bag[v])
                        {
                            if (loopFactors[_qc->numberOfViews()]) 
                                mapVars += relName+"[ptr_"+relName+"]."+
                                    _td->getAttribute(v)->_name+",";
                            else
                                mapVars += relName+"[lowerptr_"+relName+"["+
                                    depthString+"]]."+_td->getAttribute(v)->_name+",";       
                        }
                        else
                        {
                            for (const size_t& incViewID : incViews)
                            {
                                View* view = _qc->getView(incViewID);
                                if (view->_fVars[v])
                                {
                                    if (loopFactors[incViewID])
                                        mapVars += viewName[incViewID]+"[ptr_"+
                                            viewName[incViewID]+"]."+
                                            _td->getAttribute(v)->_name+",";
                                    else 
                                        mapVars += viewName[incViewID]+"[lowerptr_"+
                                            viewName[incViewID]+"["+depthString+"]]."+
                                            _td->getAttribute(v)->_name+",";
                                    break;
                                }                                
                            }                            
                        }
                        
                        // size_t idx = varCounter * (_qc->numberOfViews() + 2);
                        // if (viewID == 16)
                        // {    
                        //     std::cout << viewsPerVar[idx] << " : " << viewsPerVar[idx+1] << std::endl;
                        //     std::cout << idx << std::endl;
                        //     std::cout << _td->getRelation(0)->_bag[33] << std::endl;
                        //     std::cout << v << ": " << _td->getAttribute(v)->_name << std::endl;
                        //     std::cout << _td->getAttribute(varOrder[varCounter])->_name << std::endl;
                        // }
                        // // TODO: FIXME: This needs to find the view that
                        // // actually contains this variable! 
                        // if (viewsPerVar[idx+1] == _qc->numberOfViews())
                        // {
                        // mapVars += relName+"[lowerptr_"+relName+"["+
                        //     depthString+"]]."+_td->getAttribute(v)->_name+",";
                        // }
                        // else
                        // {
                        //     mapVars += viewName[viewsPerVar[idx+1]]+"[lowerptr_"+
                        //         viewName[viewsPerVar[idx+1]]+"["+depthString+"]]."+
                        //         _td->getAttribute(v)->_name+",";
                        // }
                        // ++varCounter;
                    }
                }
                mapVars.pop_back();

                // if (viewID == 16)
                //     exit(0);
                

                std::string mapAggs = "", aggRef = "";
                aggRef += "&aggregates_"+viewName[viewID]+" = "+
                        viewName[viewID]+"[idx_"+viewName[viewID]+"].aggregates;\n";
                // for (size_t agg = 0; agg < view->_aggregates.size(); ++agg)
                // {
                //     // mapAggs += "0.0,";
                //     aggRef += "&agg_"+std::to_string(viewID)+"_"+std::to_string(agg)+" = "+
                //         viewName[viewID]+"[idx_"+viewName[viewID]+"].agg_"+
                //         std::to_string(viewID)+"_"+std::to_string(agg)+",";
                // }
                // mapAggs.pop_back();
                // aggRef.pop_back();

                findTuple += offset(3+depth+numLoopFactors)+"size_t idx_"+viewName[viewID]+" = 0;\n"+
                    offset(3+depth+numLoopFactors)+"auto it_"+viewName[viewID]+" = "+viewName[viewID]+"_map.find({"+mapVars+"});\n"+
                    offset(3+depth+numLoopFactors)+"if (it_"+viewName[viewID]+" != "+viewName[viewID]+"_map.end())\n"+
                    offset(3+depth+numLoopFactors)+"{\n"+
                    offset(4+depth+numLoopFactors)+"idx_"+viewName[viewID]+" = it_"+viewName[viewID]+"->second;\n"+
                    offset(3+depth+numLoopFactors)+"}\n"+offset(3+depth+numLoopFactors)+"else\n"+
                    offset(3+depth+numLoopFactors)+"{\n"+
                    offset(4+depth+numLoopFactors)+viewName[viewID]+"_map[{"+mapVars+"}] = "+viewName[viewID]+".size();\n"+
                    offset(4+depth+numLoopFactors)+"idx_"+viewName[viewID]+" = "+viewName[viewID]+".size();\n"+
                    offset(4+depth+numLoopFactors)+viewName[viewID]+".push_back({"+mapVars+","+mapAggs+"});\n"+
                    offset(3+depth+numLoopFactors)+"}\n";
                    // offset(3+depth+numLoopFactors)+"double "+aggRef+";\n";       
            }

            
            auto it = aggregateSection.find(loopFactors);
            if (it != aggregateSection.end())
                it->second += findTuple+aggregateString;
            else
                aggregateSection[loopFactors] = findTuple+aggregateString; 
        } // end viewID

        // We are now printing out the aggregates in their respective loops 
        std::vector<bool> loopConstructed(_qc->numberOfViews() + 1);
        
        std::string pointers = "";

        if (allLoopFactors[_qc->numberOfViews()])
            pointers += "ptr_"+relName+",";
        
        for (size_t viewID = 0; viewID < _qc->numberOfViews(); ++viewID)
        {
            if (allLoopFactors[viewID])
                pointers += "ptr_"+viewName[viewID]+",";
        }

        if (!pointers.empty())
        {
            pointers.pop_back();
            returnString += offset(3+depth)+"size_t "+pointers+";\n";
        }

        returnString += offset(3+depth)+relName+"_tuple &"+relName+"Pointer = "+
            relName+"[lowerptr_"+relName+"["+depthString+"]];\n";

        for (size_t incViewID : incViews)
            returnString += offset(3+depth)+"double *aggregates_"+viewName[incViewID]+
                " = "+viewName[incViewID]+"[lowerptr_"+viewName[incViewID]+"["+depthString+
                "]].aggregates;\n";
        
        for (const auto& loopCondition : aggregateSection)
        {
            size_t loopCounter = 0;
            std::string closeLoopString = "";

            // Loop over relation
            if (loopCondition.first[_qc->numberOfViews()]) 
            {   
                returnString += offset(3+depth)+"ptr_"+relName+
                    " = lowerptr_"+relName+"["+depthString+"];\n"+
                    offset(3+depth)+"while(ptr_"+relName+
                    " <= upperptr_"+relName+"["+depthString+"])\n"+
                    offset(3+depth)+"{\n"+
                    offset(4+depth)+relName+"_tuple &"+relName+"Pointer = "+
                    relName+"[ptr_"+relName+"];\n";;

                closeLoopString = offset(4+depth)+"++ptr_"+relName+";\n"+
                    offset(3+depth)+"}\n";

                loopConstructed[_qc->numberOfViews()] = 1;
                ++loopCounter;
            }
            for (size_t viewID=0; viewID < _qc->numberOfViews(); ++viewID)
            {
                if (loopCondition.first[viewID])
                {
                    // Add loop for view
                    returnString += offset(3+depth+loopCounter)+"ptr_"+viewName[viewID]+
                        " = lowerptr_"+viewName[viewID]+"["+depthString+"];\n"+
                        offset(3+depth+loopCounter)+
                        "while(ptr_"+viewName[viewID]+
                        " <= upperptr_"+viewName[viewID]+"["+depthString+"])\n"+
                        offset(3+depth+loopCounter)+"{\n"+
                        offset(4+depth+loopCounter)+"double *aggregates_"+viewName[viewID]+
                        " = "+viewName[viewID]+"[ptr_"+viewName[viewID]+"].aggregates;\n";

                    closeLoopString = offset(4+depth+loopCounter)+
                        "++ptr_"+viewName[viewID]+";\n"+
                        offset(3+depth+loopCounter)+"}\n"+closeLoopString;

                    loopConstructed[viewID] = 1;
                }
            }
            returnString += loopCondition.second + closeLoopString;
        }
        
        returnString += offset(3+depth)+
            "/****************AggregateComputation*******************/\n";
    }
    
    for (const size_t& viewID : viewGroups[group_id])
    {
        View* view = _qc->getView(viewID);
        
        if (!_requireHashing[viewID] && depth+1 == (view->_fVars & groupVariableOrderBitset[group_id]).count())
        {
            size_t setAggOffset;

            if (addTuple)
            {
                returnString += offset(3+depth)+"if(addTuple_"+viewName[viewID]+")\n"+
                    offset(3+depth)+"{\n"+offset(4+depth)+viewName[viewID]+".push_back({";
                setAggOffset = 4+depth;
            }
            else
            {
                returnString += offset(3+depth)+viewName[viewID]+".push_back({";
                setAggOffset = 3+depth;
            }
            // for each free variable we find find the relation where they are
            // from - and then fill the tuple / struct
            // size_t varCounter = 0;
            for (size_t v = 0; v < NUM_OF_VARIABLES; ++v)
            {
                if (view->_fVars[v])
                {
                    // if var is in relation - use relation 
                    if (node._bag[v])
                    {
                        returnString += relName+"[lowerptr_"+relName+
                            "["+depthString+"]]."+_td->getAttribute(v)->_name+",";
                    }
                    else // else find corresponding view 
                    {
                        for (const size_t& incViewID : incViews) // why not use
                                                                 // the incViews
                                                                 // for this view?
                        {
                            if (_qc->getView(incViewID)->_fVars[v])
                            {
                                returnString += viewName[incViewID]+
                                    "[lowerptr_"+viewName[incViewID]+
                                    "["+depthString+"]]."+
                                    _td->getAttribute(v)->_name+",";
                            }
                        }
                    }                    
                }
            }
            returnString.pop_back();
            returnString += "});\n"+
                offset(setAggOffset)+"for (size_t agg = 0; agg < "+
                std::to_string(view->_aggregates.size())+"; ++agg)\n"+
                offset(setAggOffset+1)+viewName[viewID]+".back().aggregates[agg]"+
                " = aggregates_"+viewName[viewID]+"[agg];\n";

            if (addTuple)
                returnString += offset(3+depth)+"};\n";
        }
    }

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


void CppGenerator::createTopologicalOrder()
{
    std::vector<bool> processedViews(_qc->numberOfViews());
    std::vector<bool> queuedViews(_qc->numberOfViews());
    
    // create orderedViewList -> which is empty
    std::vector<size_t> orderedViewList;
    std::queue<size_t> nodesToProcess;

    viewToGroupMapping = new size_t[_qc->numberOfViews()];
    
    for (const size_t& nodeID : _td->_leafNodes)
    {
        for (const size_t& viewID : viewsPerNode[nodeID])
        {
            if (incomingViews[viewID].size() == 0)
            {    
                nodesToProcess.push(viewID);
                queuedViews[viewID] = 1;
            }
        }
    }
    
    while (!nodesToProcess.empty()) // there is a node in the set
    {
        size_t viewID = nodesToProcess.front();
        nodesToProcess.pop();

        orderedViewList.push_back(viewID);
        processedViews[viewID] = 1;

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
                    queuedViews[destView] = 1;
                    nodesToProcess.push(destView);
                }       
            }
        }
    }

    size_t prevOrigin = _qc->getView(orderedViewList[0])->_origin;
    viewGroups.push_back({orderedViewList[0]});

    viewToGroupMapping[orderedViewList[0]] = 0;    
    size_t currentSet = 0;
    for (size_t viewID = 1; viewID < orderedViewList.size(); ++viewID)
    {
        size_t origin = _qc->getView(orderedViewList[viewID])->_origin;
        if (prevOrigin == origin)
        {
            // Combine the two into one set of
            viewGroups[currentSet].push_back(orderedViewList[viewID]);
            viewToGroupMapping[orderedViewList[viewID]] = currentSet;
        }
        else
        {
            // Add new set which contains origin ?!
            viewGroups.push_back({orderedViewList[viewID]});
            ++currentSet;
            viewToGroupMapping[orderedViewList[viewID]] = currentSet;
        }
        prevOrigin = origin;
    }

    
    for (size_t& i : orderedViewList)
        std::cout << i << ", ";
    std::cout << std::endl;

    
    for (auto& group : viewGroups)
    {
        for (auto& i : group)
        {
            std::cout << i << "  ";
        }
        std::cout << "|";
    }
    std::cout << std::endl;

    //  exit(0);
}

std::string CppGenerator::genRelationOrdering(
    const std::string& rel_name, const size_t& depth, const size_t& view_id)
{
    // TODO: FIXME: add the case when there is only two or one reations
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

std::string CppGenerator::genLeapfrogJoinCode(size_t view_id, size_t depth)
{
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
            for (size_t i = 0; i < aggregate->_n; ++i)
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

    // Closing the while loop 
    return returnString + offset(2+depth)+"}\n";
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
                                       const std::string& attr_name,
                                       bool parallel)
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
        return "(0.15*"+fvars+")"; // "_param["+fvars+"]"; // // TODO: THIS NEEDS TO BE A
                          // CATEGORICAL PARAMETER - Index?!
    default : return "f("+fvars+")";
    }
}

std::string CppGenerator::genRunFunction()
{    
    std::string returnString = offset(1)+"void run()\n"+
        offset(1)+"{\n";

    returnString += offset(2)+
        "int64_t startLoading = duration_cast<milliseconds>("+
        "system_clock::now().time_since_epoch()).count();\n\n"+
        offset(2)+"loadRelations();\n\n"+
        offset(2)+"std::cout << \"Data loading: \" + std::to_string("+
        "duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()-"+
        "startLoading)+\"ms.\\n\";\n\n";
    
    returnString += offset(2)+"int64_t startSorting = duration_cast<milliseconds>("+
        "system_clock::now().time_since_epoch()).count();\n\n";

    for (size_t rel = 0; rel < _td->numberOfRelations(); ++rel)
    {
        TDNode* node = _td->getRelation(rel);
        returnString += offset(2)+"sort"+node->_name+"();\n";
    }

    returnString += offset(2)+"std::cout << \"Data sorting: \" + std::to_string("+
        "duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()-"+
        "startSorting)+\"ms.\\n\";\n\n";

    returnString += offset(2)+"int64_t startProcess = duration_cast<milliseconds>("+
        "system_clock::now().time_since_epoch()).count();\n\n";

#ifndef OPTIMIZED    
    for (size_t view = 0; view < _qc->numberOfViews(); ++view)
        returnString += offset(2)+"computeView"+std::to_string(view)+"();\n";
#else
    for (size_t group = 0; group < viewGroups.size(); ++group)
    {
        if (_parallelizeGroup[group])
            returnString += offset(2)+"computeGroup"+std::to_string(group)+
                "Parallelized();\n";
        else
            returnString += offset(2)+"computeGroup"+std::to_string(group)+"();\n";
    }
#endif
    
    returnString += "\n"+offset(2)+"std::cout << \"Data process: \"+"+
        "std::to_string(duration_cast<milliseconds>("+
        "system_clock::now().time_since_epoch()).count()-startProcess)+\"ms.\\n\";\n\n";

    for (size_t view = 0; view < _qc->numberOfViews(); ++view)
    {
        returnString += offset(2)+"std::cout << \""+viewName[view]+": \" << "+
            viewName[view]+".size() << std::endl;\n";
    }
    
    return returnString+offset(1)+"}\n";
}

std::string CppGenerator::genRunMultithreadedFunction()
{    
    std::string returnString = offset(1)+"void runMultithreaded()\n"+
        offset(1)+"{\n";

    returnString += offset(2)+
        "int64_t startLoading = duration_cast<milliseconds>("+
        "system_clock::now().time_since_epoch()).count();\n\n"+
        offset(2)+"loadRelations();\n\n"+
        offset(2)+"std::cout << \"Data loading: \" + std::to_string("+
        "duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()-"+
        "startLoading)+\"ms.\\n\";\n\n";
    
    returnString += offset(2)+"int64_t startSorting = duration_cast<milliseconds>("+
        "system_clock::now().time_since_epoch()).count();\n\n";

    // std::vector<bool> joinedRelations(_td->numberOfRelations());

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

    returnString += offset(2)+"std::cout << \"Data sorting: \" + std::to_string("+
        "duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()-"+
        "startSorting)+\"ms.\\n\";\n\n";

    returnString += offset(2)+"int64_t startProcess = duration_cast<milliseconds>("+
        "system_clock::now().time_since_epoch()).count();\n\n";

#ifndef OPTIMIZED
    std::vector<bool> joinedViews(_qc->numberOfViews());
    for (size_t view = 0; view < _qc->numberOfViews(); ++view)
    {
        // TDNode* relation = _td->getRelation(_qc->getView(view)->_origin);
        // if (!joinedRelations[relation->_id])
        // {
        //     returnString += offset(2)+"sort"+relation->_name+"Thread.join();\n";
        //     joinedRelations[relation->_id] = 1;
        // }

        for (const size_t& otherView : incomingViews[view])
        {
            // find group that contains this view!
            if (!joinedViews[otherView])
            {
                returnString += offset(2)+"view"+std::to_string(otherView)+
                    "Thread.join();\n";
                joinedViews[otherView] = 1;
            }
            
        }
        returnString += offset(2)+"std::thread view"+std::to_string(view)+
            "Thread(computeView"+std::to_string(view)+");\n";
    }

    for (size_t view = 0; view < _qc->numberOfViews(); ++view)
    {
        if(!joinedViews[view])
            returnString += offset(2)+"view"+std::to_string(view)+"Thread.join();\n";
    }
    
#else

    std::vector<bool> joinedGroups(viewGroups.size());

    // for (size_t rel = 0; rel < _td->numberOfRelations(); ++rel)
    // {
    //     TDNode* node = _td->getRelation(rel);
    //     returnString += offset(2)+"std::thread sort"+node->_name+
    //         "Thread(sort"+node->_name+");\n";
    // }
    
    for (size_t group = 0; group < viewGroups.size(); ++group)
    {
        // TDNode* relation = _td->getRelation(_qc->getView(viewGroups[group][0])->_origin);
        // if (!joinedRelations[relation->_id])
        // {
        //     returnString += offset(2)+"sort"+relation->_name+"Thread.join();\n";
        //     joinedRelations[relation->_id] = 1;
        // }

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

        if (_parallelizeGroup[group])            
            returnString += offset(2)+"std::thread group"+std::to_string(group)+
                "Thread(computeGroup"+std::to_string(group)+"Parallelized);\n";
        else
         returnString += offset(2)+"std::thread group"+std::to_string(group)+
                "Thread(computeGroup"+std::to_string(group)+");\n";
    }

    for (size_t group = 0; group < viewGroups.size(); ++group)
    {
        if (!joinedGroups[group])
            returnString += offset(2)+"group"+std::to_string(group)+"Thread.join();\n";
    }
    
#endif

    returnString += "\n"+offset(2)+"std::cout << \"Data process: \"+"+
        "std::to_string(duration_cast<milliseconds>("+
        "system_clock::now().time_since_epoch()).count()-startProcess)+\"ms.\\n\";\n\n";

    for (size_t view = 0; view < _qc->numberOfViews(); ++view)
    {
        returnString += offset(2)+"std::cout << \""+viewName[view]+": \" << "+
            viewName[view]+".size() << std::endl;\n";
    }
    
    return returnString+offset(1)+"}\n";
}


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
//        offset(8+depth)+"leap *= 2;\n"+
        offset(7+depth)+"}\n"+offset(7+depth)+"else\n"+offset(7+depth)+"{\n"+
        offset(8+depth)+"lowerptr_"+rel_name+"["+depthString+"] = "+upperPointer+";\n"+
        offset(8+depth)+"break;\n"+
        offset(7+depth)+"}\n"+
        offset(6+depth)+"}\n";
//        offset(6+depth)+"leap /= 2;\n";

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

        
