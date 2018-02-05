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
};

void CppGenerator::generateCode()
{
    sortOrders = new size_t*[_td->numberOfRelations() + _qc->numberOfViews()];
    viewName = new std::string[_qc->numberOfViews()];
    
    for (size_t rel = 0; rel < _td->numberOfRelations(); ++rel)
        sortOrders[rel] = new size_t[_td->getRelation(rel)->_bag.count()]();
    
    for (size_t view = 0; view < _qc->numberOfViews(); ++view)
    {   
        sortOrders[view + _td->numberOfRelations()] =
            new size_t[_qc->getView(view)->_fVars.count()]();
        viewName[view] = "V"+std::to_string(view);
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
        // TDNode* node = _td->getRelation(_qc->getView(viewGroups[gid][0])->_origin);
        // if (node->_name == "Inventory")
        // {
        //     _parallelizeGroup[gid] = true;
        //     _threadsPerGroup[gid] = 8;
        // }
        // else
        // {
        //     _parallelizeGroup[gid] = false;
        //     _threadsPerGroup[gid] = 0;
        // }
        _parallelizeGroup[gid] = false;
    }
    
    groupVariableOrderBitset.resize(viewGroups.size());
    
    for (size_t group = 0; group < viewGroups.size(); ++group)
    {
        var_bitset joinVars;
        std::vector<bool> incViewBitset(_qc->numberOfViews());

        TDNode* baseRelation = _td->getRelation(
            _qc->getView(viewGroups[group][0])->_origin);
        
        TDNode* destRelation = _td->getRelation(
            _qc->getView(viewGroups[group][0])->_destination);

        if (baseRelation->_id != destRelation->_id)
            joinVars |= destRelation->_bag & baseRelation->_bag;
        
        for (const size_t& viewID : viewGroups[group])
        {
            View* view = _qc->getView(viewID);

            int numberIncomingViews =
                (view->_origin == view->_destination ? baseRelation->_numOfNeighbors :
                 baseRelation->_numOfNeighbors - 1);

            // if ther are not inc views we use the fVars as free variable
            if (numberIncomingViews == 0)
                joinVars |= view->_fVars;
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
            offset(1)+"{\n"+offset(2);

        for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
        {
            if (view->_fVars[var])
            {
                Attribute* att = _td->getAttribute(var);
                tupleStruct += typeToStr(att->_type)+" "+att->_name+";\n"+offset(2);
                attrConstruct += typeToStr(att->_type)+" "+att->_name+",";
                attrAssign += offset(3)+"this->"+ att->_name + " = "+att->_name+";\n";
            }
        }

        std::string agg_name;
        for (size_t agg = 0; agg < view->_aggregates.size(); ++agg)
        {
            agg_name = "agg_"+std::to_string(viewID)+"_"+std::to_string(agg);
            tupleStruct += "double "+agg_name+";\n"+offset(2);
            attrConstruct += "double "+agg_name+",";
            attrAssign += offset(3)+"this->"+ agg_name + " = "+agg_name+";\n";
        }
        attrConstruct.pop_back();
        
        tupleStruct += viewName[viewID]+"_tuple() {} \n"+offset(2) +
            viewName[viewID]+"_tuple("+attrConstruct+")\n"+offset(2)+
            "{\n"+attrAssign+offset(2)+"}\n"+offset(1)+"};\n\n"+
            offset(1)+"std::vector<"+viewName[viewID]+"_tuple> "+
            viewName[viewID]+";\n\n";
    }

    return tupleStruct;
}

// std::string CppGenerator::genRelationTupleStructs(TDNode* rel)
// {
//     std::string tupleStruct = "", attrConversion = "", attrConstruct = "",
//         attrAssign = "";
//     size_t field = 0;
//     std::string relName = rel->_name;
        
//     tupleStruct += offset(1)+"struct "+relName+"_tuple\n"+offset(1)+"{\n"+offset(2);

//     for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
//     {
//         if (rel->_bag[var])
//         {
//             Attribute* att = _td->getAttribute(var);
//             tupleStruct += typeToStr(att->_type)+" "+att->_name+";\n"+offset(2);
//             attrConversion += offset(3)+att->_name+" = "+
//                 typeToStringConverter(att->_type)+
//                 "(fields["+std::to_string(field)+"]);\n";
//             attrConstruct += typeToStr(att->_type)+" "+att->_name+",";
//             attrAssign += offset(3)+"this->"+ att->_name + " = "+att->_name+";\n";
//             ++field;
//         }
//     }
//     attrConstruct.pop_back();
        
//     tupleStruct += relName+"_tuple() {} \n"+offset(2) +
//         relName+"_tuple(std::vector<std::string>& fields)\n"+offset(2)+
//         "{\n"+ attrConversion +offset(2)+"}\n"+offset(2)+
//         relName+"_tuple("+attrConstruct+")\n"+offset(2)+
//         "{\n"+attrAssign+offset(2)+"}\n"+offset(1)+"};\n\n";
//     return tupleStruct;
// }

// std::string CppGenerator::genViewTupleStructs(View* view, size_t view_id)
// {
//     std::string tupleStruct = "", attrConstruct = "", attrAssign = "";
        
//     tupleStruct += offset(1)+"struct "+viewName[view_id]+"_tuple\n"+
//         offset(1)+"{\n"+offset(2);

//     for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
//     {
//         if (view->_fVars[var])
//         {
//             Attribute* att = _td->getAttribute(var);
//             tupleStruct += typeToStr(att->_type)+" "+att->_name+";\n"+offset(2);
//             attrConstruct += typeToStr(att->_type)+" "+att->_name+",";
//             attrAssign += offset(3)+"this->"+ att->_name + " = "+att->_name+";\n";
//         }
//     }

//     std::string agg_name;
//     for (size_t agg = 0; agg < view->_aggregates.size(); ++agg)
//     {
//         agg_name = "agg_"+std::to_string(view_id)+"_"+std::to_string(agg);
            
//         tupleStruct += "double "+agg_name+";\n"+offset(2);
//         attrConstruct += "double "+agg_name+",";
//         attrAssign += offset(3)+"this->"+ agg_name + " = "+agg_name+";\n";
//     }
                
//     attrConstruct.pop_back();
        
//     tupleStruct += viewName[view_id]+"_tuple() {} \n"+offset(2) +
//         viewName[view_id]+"_tuple("+attrConstruct+")\n"+offset(2)+
//         "{\n"+attrAssign+offset(2)+"}\n"+offset(1)+"};\n\n";

//     return tupleStruct;
// }

    
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
    std::vector<size_t>& varOrder = groupVariableOrder[group_id];
    std::vector<size_t>& viewsPerVar = groupViewsPerVarInfo[group_id];
    std::vector<size_t>& incViews = groupIncomingViews[group_id];

    // DO WE NEED THIS ?
    TDNode* baseRelation =_td->getRelation(
        _qc->getView(viewGroups[group_id][0])->_origin);

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
                returnString += offset(2)+"/*********** HASHING ***********/ \n" +
                    offset(2)+"std::unordered_map<"+viewName[viewID]+"_key,"+
                    "size_t, "+viewName[viewID]+"_keyhash> "+viewName[viewID]+"_map;\n"+
                    offset(2)+"/*********** HASHING ***********/ \n";
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
        "atEnd["+numOfJoinVarString+"] = {};\n";

    // Lower and upper pointer for the base relation
    returnString += genPointerArrays(
        baseRelation->_name, numOfJoinVarString, _parallelizeGroup[group_id]);

    // Lower and upper pointer for each incoming view
    for (const size_t& viewID : incViews)            
        returnString += genPointerArrays(
            viewName[viewID],numOfJoinVarString, false);

    std::string closeAddTuple = "";
    for (const size_t& viewID : viewGroups[group_id])
    {
        View* view = _qc->getView(viewID);

        if (view->_fVars.count() == 0)
        {
            returnString += offset(2)+"bool addTuple_"+viewName[viewID]+" = false;\n";
            returnString += offset(2)+"double";
            for (size_t agg = 0; agg < view->_aggregates.size(); ++agg)
                returnString += " agg_"+std::to_string(viewID)+"_"+
                    std::to_string(agg)+" = 0.0,";
            returnString.pop_back();
            returnString += ";\n";

            closeAddTuple += offset(2)+"if (addTuple_"+viewName[viewID]+")\n";
            closeAddTuple += offset(3)+viewName[viewID]+".push_back({";
            for (size_t agg = 0; agg < view->_aggregates.size(); ++agg)
                closeAddTuple += "agg_"+std::to_string(viewID)+"_"+
                    std::to_string(agg)+",";
            closeAddTuple.pop_back();
            closeAddTuple += "});\n";
        }   
    }
    
    // for each join var --- create the string that does the join
    returnString += genGroupLeapfrogJoinCode(group_id, *baseRelation, 0);
    
    returnString += closeAddTuple;


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
                    for (size_t agg = 0; agg <
                             _qc->getView(viewID)->_aggregates.size(); ++agg)
                    {
                        returnString += offset(4)+viewName[viewID]+"[it->second].agg_"+
                            std::to_string(viewID)+"_"+std::to_string(agg)+" += "+
                            viewName[viewID]+"_"+std::to_string(t)+"[key.second].agg_"+
                            std::to_string(viewID)+"_"+std::to_string(agg)+";\n";
                    }
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

                        for (size_t agg = 0; agg < view->_aggregates.size(); ++agg)
                        {    
                            returnString += offset(3)+viewName[viewID]+".back().agg_"+
                                std::to_string(viewID)+"_"+std::to_string(agg)+" += "+
                                viewName[viewID]+"_"+std::to_string(t)+"[0].agg_"+
                                std::to_string(viewID)+"_"+std::to_string(agg)+";\n";
                        }
                        
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

                    for (size_t agg = 0; agg < view->_aggregates.size(); ++agg)
                    {    
                        returnString += offset(2)+viewName[viewID]+"[0].agg_"+
                            std::to_string(viewID)+"_"+std::to_string(agg)+" += ";

                        for (size_t t = 1; t < _threadsPerGroup[group_id]; ++t)
                        {
                            returnString += viewName[viewID]+"_"+std::to_string(t)+
                                "[0].agg_"+std::to_string(viewID)+"_"+
                                std::to_string(agg)+"+";
                        }
                        returnString.pop_back();
                        returnString += ";\n";
                    }
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


std::string CppGenerator::genGroupLeapfrogJoinCode(
    size_t group_id, const TDNode& node, size_t depth)
{
    const std::vector<size_t>& varOrder = groupVariableOrder[group_id];
    const std::vector<size_t>& viewsPerVar = groupViewsPerVarInfo[group_id];
    const std::vector<size_t>& incViews = groupIncomingViews[group_id];

    const std::string& attrName = _td->getAttribute(varOrder[depth])->_name;
    const std::string& relName = node._name;
    
    // size_t numberIncomingViews =
    //     (view->_origin == view->_destination ? node._numOfNeighbors :
    //      node._numOfNeighbors - 1);

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

    for (const size_t& viewID : viewGroups[group_id])
    {
        View* view = _qc->getView(viewID);
        
        if (!_requireHashing[viewID] && depth + 1 == view->_fVars.count())
        {
            if (addTuple)
                returnString += offset(3+depth)+
                    "bool addTuple_"+viewName[viewID]+" = false;\n";
            returnString += offset(3+depth)+"double";
            for (size_t agg = 0; agg < view->_aggregates.size(); ++agg)
                returnString += " agg_"+std::to_string(viewID)+"_"+
                    std::to_string(agg)+" = 0.0,";
            returnString.pop_back();
            returnString += ";\n";
        }
    }

    // then you would need to go to next variable
    if (depth+1 < varOrder.size())
    {     
        // leapfrogJoin for next join variable 
        returnString += genGroupLeapfrogJoinCode(group_id, node, depth+1);
    }
    else
    {
        returnString += offset(3+depth)+
            "/****************AggregateComputation*******************/\n";

        std::unordered_map<std::vector<bool>,std::string> aggregateSection;

        /* For each view in this group ... */
        for (const size_t& viewID : viewGroups[group_id])
        {
            View* view = _qc->getView(viewID);

            if (!_requireHashing[viewID] && depth + 1 > view->_fVars.count()) 
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
                        if (!groupVariableOrderBitset[group_id][v]) // TODO::
                                                                    // THERE IS
                                                                    // A PROBLEM
                                                                    // HERE we
                                                                    // do not
                                                                    // take care
                                                                    // of
                                                                    // incoming views!
                        {

                            // Need to find the variable either in Relation Then
                            // do the following

                            // else find it in the views
                            if (node._bag[v])
                            {                                                            
                                if (!loopFactors[_qc->numberOfViews()])
                                    ++numLoopFactors;
                                                        
                                loopFactors[_qc->numberOfViews()] = 1;
                            }
                            else
                            {
                                for (const size_t& incViewID : incViews)
                                {
                                    if (_qc->getView(incViewID)->_fVars[v])
                                    {
                                        if (!loopFactors[incViewID])
                                            ++numLoopFactors;
                                                        
                                        loopFactors[incViewID] = 1;                               
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
                                                freeVarString += relName+"[lowerptr_"+
                                                    relName+"["+depthString+"]]."+
                                                    _td->getAttribute(v)->_name+",";
                                            }
                                            else
                                            {
                                                freeVarString +=
                                                    relName+"[ptr_"+relName+
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
                                                        /* variable is part of varOrder */
                                                        if (groupVariableOrderBitset[group_id][v])
                                                        {
                                                            // add V_ID.var_name to the fVarString
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
                    
                    if (viewAgg.size() > 0 && localAgg.size() > 0)
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
                
                aggregateString += offset(3+depth+numLoopFactors)+
                        "agg_"+std::to_string(viewID)+"_"+std::to_string(aggID)+" += "+agg+";\n";
            } // end complete Aggregate

            std::string findTuple = "";
            if (_requireHashing[viewID])
            {
                size_t varCounter = 0;
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
                for (size_t agg = 0; agg < view->_aggregates.size(); ++agg)
                {
                    mapAggs += "0.0,";
                    aggRef += "&agg_"+std::to_string(viewID)+"_"+std::to_string(agg)+" = "+
                        viewName[viewID]+"[idx_"+viewName[viewID]+"].agg_"+
                        std::to_string(viewID)+"_"+std::to_string(agg)+",";
                }
                mapAggs.pop_back();
                aggRef.pop_back();
       
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
                    offset(3+depth+numLoopFactors)+"}\n"+
                    offset(3+depth+numLoopFactors)+"double "+aggRef+";\n";
            }
            
            auto it = aggregateSection.find(loopFactors);
            if (it != aggregateSection.end())
                it->second += findTuple+aggregateString;
            else
                aggregateSection[loopFactors] = findTuple+aggregateString; 
        } // end viewID

        // We are now printing out the aggregates in their respective loops 
        std::vector<bool> loopConstructed(_qc->numberOfViews() + 1);        
        for (const auto& loopCondition : aggregateSection)
        {
            size_t loopCounter = 0;
            std::string closeLoopString = "";

            // Loop over relation
            if (loopCondition.first[_qc->numberOfViews()]) 
            {   
                if (!loopConstructed[_qc->numberOfViews()]) 
                    returnString += offset(3+depth)+"size_t ptr_"+relName+
                        " = lowerptr_"+relName+"["+depthString+"];\n";
                else
                    returnString += offset(3+depth)+"ptr_"+relName+
                        " = lowerptr_"+relName+"["+depthString+"];\n";     

                returnString += offset(3+depth)+"while(ptr_"+relName+
                    " <= upperptr_"+relName+"["+depthString+"])\n"+
                    offset(3+depth)+"{\n";

                closeLoopString = offset(4+depth)+"++ptr_"+relName+";\n"+
                    offset(3+depth)+"}\n";

                loopConstructed[_qc->numberOfViews()] = 1;
                ++loopCounter;
            }
            for (size_t viewID=0; viewID < _qc->numberOfViews(); ++viewID)
            {
                if (loopCondition.first[viewID])
                {
                    if (! loopConstructed[viewID]) 
                        returnString += offset(3+depth+loopCounter)+"size_t ptr_"+viewName[viewID]+
                            " = lowerptr_"+viewName[viewID]+"["+depthString+"];\n";
                    else
                        returnString += offset(3+depth+loopCounter)+"ptr_"+viewName[viewID]+
                            " = lowerptr_"+viewName[viewID]+"["+depthString+"];\n";

                    // Add loop for view
                    returnString += offset(3+depth+loopCounter)+
                        "while(ptr_"+viewName[viewID]+
                        " <= upperptr_"+viewName[viewID]+"["+depthString+"])\n"+
                        offset(3+depth+loopCounter)+"{\n";

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
        
        if (!_requireHashing[viewID] && depth+1 == view->_fVars.count())
        {
            if (addTuple)
            {
                returnString += offset(3+depth)+"if(addTuple_"+viewName[viewID]+")\n";
                returnString += offset(4+depth)+viewName[viewID]+".push_back({";
            }
            else
                returnString += offset(3+depth)+viewName[viewID]+".push_back({";

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
                        returnString += relName+"[lowerptr_"+relName+
                            "["+depthString+"]]."+_td->getAttribute(v)->_name+",";
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
                returnString += "agg_"+std::to_string(viewID)+"_"+
                    std::to_string(agg)+",";
            returnString.pop_back();
            returnString += "});\n";
        }
    }
    
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
    case Operation::lr_cont_parameter :
        return "(0.15*"+fvars+")";
    case Operation::lr_cat_parameter :
        return "(0.15)";                 // TODO: THIS NEEDS TO BE A
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
        offset(6+depth)+"if (leap > 1 && max_"+attrName+" <= "+rel_name+
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

        
