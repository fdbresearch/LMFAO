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

#include <CppGenerator.h>
#include <TDNode.hpp>

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

    for (size_t rel = 0; rel < _td->numberOfRelations(); ++rel)
        delete[] sortOrders[rel];
    for (size_t view = 0; view < _qc->numberOfViews(); ++view)
        delete[] sortOrders[view + _td->numberOfRelations()];
    delete[] sortOrders;
};

void CppGenerator::generateCode()
{
    sortOrders = new size_t*[_td->numberOfRelations() + _qc->numberOfViews()];
    viewName = new std::string[_qc->numberOfViews()];
    
    for (size_t rel = 0; rel < _td->numberOfRelations(); ++rel)
        sortOrders[rel] = new size_t[
            _td->getRelation(rel)->_bag.count()]();
    for (size_t view = 0; view < _qc->numberOfViews(); ++view)
    {   
        sortOrders[view + _td->numberOfRelations()] = new size_t[
            _qc->getView(view)->_fVars.count()]();
        viewName[view] = "V"+std::to_string(view);
    }
    
    createVariableOrder();

    std::ofstream ofs("runtime/inmemory/Engine.hpp", std::ofstream::out);
    
    std::cout << genHeader() << std::endl;
    ofs << genHeader();
        
    for (size_t rel = 0; rel < _td->numberOfRelations(); ++rel)
    {
        std::cout << genRelationTupleStructs(_td->getRelation(rel));
        ofs <<  genRelationTupleStructs(_td->getRelation(rel));
    }

    for (size_t view = 0; view < _qc->numberOfViews(); ++view)
    {
        std::cout << genViewTupleStructs(_qc->getView(view), view);
        ofs << genViewTupleStructs(_qc->getView(view), view);
    }
        
    for (size_t relID = 0; relID < _td->numberOfRelations(); ++relID)
    {
        TDNode* rel = _td->getRelation(relID);        
        std::cout <<  offset(1)+"std::vector<"+rel->_name+"_tuple> "+rel->_name+";\n";
        ofs <<  offset(1)+"std::vector<"+rel->_name+"_tuple> "+rel->_name+";\n";
    }

    for (size_t view = 0; view < _qc->numberOfViews(); ++view)
    {         
        std::cout << offset(1)+"std::vector<"+viewName[view]+"_tuple> "+
            viewName[view]+";\n";
        ofs << offset(1)+"std::vector<"+viewName[view]+"_tuple> "+
            viewName[view]+";\n";
    }
    ofs << std::endl;
        
    std::cout << genLoadRelationFunction() << std::endl;
    ofs << genLoadRelationFunction() << std::endl;

    std::cout << genCaseIdentifiers() << std::endl;
    ofs << genCaseIdentifiers() << std::endl;

    for (size_t view = 0; view < _qc->numberOfViews(); ++view)
    {
        std::string s = genComputeViewFunction(view);
        
        std::cout << s << std::endl;
        ofs << s << std::endl;
    }
        
    std::cout << genRunAll() << std::endl;
    ofs << genRunAll() << std::endl;
        
    std::cout << genFooter() << std::endl;
    ofs << genFooter() << std::endl;
    ofs.close();
}


/* TODO: Technically there is no need to pre-materialise this! */
void CppGenerator::createVariableOrder()
{
    variableOrder = new std::vector<size_t>[_qc->numberOfViews()]();
    incomingViews = new std::vector<size_t>[_qc->numberOfViews()]();
    viewsPerVarInfo = new std::vector<size_t>[_qc->numberOfViews()];

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

/* PRINT OUT */
        std::cout << viewID << " : ";
        for(size_t& n : variableOrder[viewID]) std::cout << n <<" ";
        std::cout << "\n";
        size_t i = 0;
        for (const size_t& n : viewsPerVarInfo[viewID])
        {
            if (i % (_qc->numberOfViews() + 2) == 0) std::cout << "| ";
            std::cout << n <<" ";
            ++i;
        }
        std::cout << "\n";
/* PRINT OUT */
    }
}


    
std::string CppGenerator::offset(size_t off)
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

std::string CppGenerator::typeToStringConverter(Type t)
{
    switch(t)
    {
    case Type::Integer : return "std::stoi";
    case Type::Double : return "std::stod";
    case Type::Short : return "std::stoi";
    case Type::U_Integer : return "std::stoul";
    default :
        ERROR("This type does not exist \n");
        exit(1);
    }
}
    
std::string CppGenerator::genHeader()
{   
    return "#ifndef INCLUDE_TEMPLATE_"+_datasetName+"_HPP_\n"+
        "#define INCLUDE_TEMPLATE_"+_datasetName+"_HPP_\n\n"+
        "#include <algorithm>\n" +
        "#include <chrono>\n" +
        "#include <iostream>\n" +
        "#include <vector>\n\n" +
        "#include \"CppHelper.hpp\"\n\n" +
        "using namespace std::chrono;\n\n"+
        "const std::string PATH_TO_DATA = \"../../"+_pathToData+"\";\n\n"+
        "namespace lmfao\n{\n";
}

std::string CppGenerator::genFooter()
{
    return "}\n\n#endif /* INCLUDE_TEMPLATE_"+_datasetName+"_HPP_*/";
}

std::string CppGenerator::genRelationTupleStructs(TDNode* rel)
{
    std::string tupleStruct = "", attrConversion = "", attrConstruct = "",
        attrAssign = "";
    size_t field = 0;
    std::string relName = rel->_name;
        
    tupleStruct += offset(1)+"struct "+relName+"_tuple\n"+offset(1)+"{\n"+offset(2);

    for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
    {
        if (rel->_bag[var])
        {
            Attribute* att = _td->getAttribute(var);
            tupleStruct += typeToStr(att->_type)+" "+att->_name+";\n"+offset(2);
            attrConversion += offset(3)+att->_name+" = "+
                typeToStringConverter(att->_type)+
                "(fields["+std::to_string(field)+"]);\n";
            attrConstruct += typeToStr(att->_type)+" "+att->_name+",";
            attrAssign += offset(3)+"this->"+ att->_name + " = "+att->_name+";\n";
            ++field;
        }
    }
    attrConstruct.pop_back();
        
    tupleStruct += relName+"_tuple() {} \n"+offset(2) +
        relName+"_tuple(std::vector<std::string>& fields)\n"+offset(2)+
        "{\n"+ attrConversion +offset(2)+"}\n"+offset(2)+
        relName+"_tuple("+attrConstruct+")\n"+offset(2)+
        "{\n"+attrAssign+offset(2)+"}\n"+offset(1)+"};\n\n";
    return tupleStruct;
}

std::string CppGenerator::genViewTupleStructs(View* view, size_t view_id)
{
    std::string tupleStruct = "", attrConstruct = "", attrAssign = "";
        
    tupleStruct += offset(1)+"struct "+viewName[view_id]+"_tuple\n"+
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
        agg_name = "agg_"+std::to_string(view_id)+"_"+std::to_string(agg);
            
        tupleStruct += "double "+agg_name+";\n"+offset(2);
        attrConstruct += "double "+agg_name+",";
        attrAssign += offset(3)+"this->"+ agg_name + " = "+agg_name+";\n";
    }
                
    attrConstruct.pop_back();
        
    tupleStruct += viewName[view_id]+"_tuple() {} \n"+offset(2) +
        viewName[view_id]+"_tuple("+attrConstruct+")\n"+offset(2)+
        "{\n"+attrAssign+offset(2)+"}\n"+offset(1)+"};\n\n";

    return tupleStruct;
}

// std::string CppGenerator::genRelationVectors(TDNode* rel)
// {
//     return offset(1)+"std::vector<"+rel->_name+"_tuple> "+rel->_name+";\n";
// }

// std::string CppGenerator::genViewVectors(size_t view_id)
// {
//     return offset(1)+"std::vector<"viewName[view_id]+"_tuple> "+
//         viewName[view_id]+";\n";
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
    std::string returnString = offset(1)+"void loadRelations()\n"+offset(1)+"{";
    for (size_t rel = 0; rel < _td->numberOfRelations(); ++rel)
    {
        std::string rel_name = _td->getRelation(rel)->_name;
            
        returnString += "\n"+offset(2)+rel_name+".clear();\n"+
            offset(2)+"readFromFile("+rel_name+", PATH_TO_DATA + \""+rel_name+
            ".tbl\", \'|\');\n";
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
    returnString += genMaxMinValues(view_id);

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
    returnString += genPointerArrays(baseRelation->_name, numOfJoinVarString);
        
    // Lower and upper pointer for each incoming view
    for (const size_t& viewID : incViews)            
        returnString += genPointerArrays(viewName[viewID],numOfJoinVarString);
    
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

std::string CppGenerator::genMaxMinValues(const size_t& view_id)
{
    const std::vector<size_t>& varOrder = variableOrder[view_id];
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
                                           std::string& numOfJoinVars)
{
    return offset(2)+"size_t lowerptr_"+rel+"["+numOfJoinVars+"] = {}, "+
        "upperptr_"+rel+"["+numOfJoinVars+"] = {}; \n" +
        offset(2)+"upperptr_"+rel+"[0] = "+rel+".size()-1;\n";
}

std::string CppGenerator::genRelationOrdering(const std::string& rel_name,
                                              const size_t& depth,
                                              const size_t& view_id)
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
        
    // res += offset(2+depth)+"std::sort(order_"+attr_name+", order_"+attr_name+
    //     " + "+std::to_string(numberContributing)+", sortRelationOrder<"+attrType+">());\n";
    
    res += offset(2+depth)+"std::sort(order_"+attr_name+", order_"+attr_name+
        " + "+std::to_string(numberContributing)+
        ",[](const std::pair<"+attrType+",int> &lhs, const std::pair<"+attrType+",int> &rhs)\n"+
        offset(3+depth)+"{\n"+offset(4+depth)+"return lhs.first < rhs.first;\n"+
        offset(3+depth)+"});\n";

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

    std::string returnString = "";

    size_t off = 1;
    if (depth > 0)
    {
        // if (viewsPerVarInfo[view_id][idx+1] == _qc->numberOfViews())
        // {
        returnString += offset(2+depth)+
            "upperptr_"+relName+"["+std::to_string(depth)+"] = "+
            getUpperPointer(relName, depth)+";\n"+
            offset(2+depth)+
            "lowerptr_"+relName+"["+std::to_string(depth)+"] = "+
            getLowerPointer(relName, depth)+";\n";
        //     off = 2;
        // }

        for (const size_t& viewID : incViews)
        {   
            returnString += offset(2+depth)+
                "upperptr_"+viewName[viewID]+
                "["+std::to_string(depth)+"] = "+
                getUpperPointer(viewName[viewID],depth)+";\n"+
                offset(2+depth)+
                "lowerptr_"+viewName[viewID]+
                "["+std::to_string(depth)+"] = "+
                getLowerPointer(viewName[viewID],depth)+";\n";
        }
        
        // for (; off <= numberContributing; ++off)
        // {
        //     size_t viewID = viewsPerVarInfo[view_id][idx+off];

        //     returnString += offset(2+depth)+
        //         "upperptr_"+viewName[viewID]+
        //         "["+std::to_string(depth)+"] = "+
        //         getUpperPointer(viewName[viewID],depth)+";\n"+
        //         offset(2+depth)+
        //         "lowerptr_"+viewName[viewID]+
        //         "["+std::to_string(depth)+"] = "+
        //         getLowerPointer(viewName[viewID],depth)+";\n";
        // }
    }
        
    // Ordering Relations 
    returnString += genRelationOrdering(relName, depth, view_id);
            
    // Update rel pointers 
    returnString += offset(2+depth)+
        "rel["+std::to_string(depth)+"] = 0;\n";
    returnString += offset(2+depth)+
        "atEnd["+std::to_string(depth)+"] = false;\n";

    // Start while loop of the join 
    returnString += offset(2+depth)+"while(!atEnd["+
        std::to_string(depth)+"])\n"+
        offset(2+depth)+"{\n"+
        offset(3+depth)+"found["+std::to_string(depth)+"] = false;\n";

    // Seek Value
    returnString += offset(3+depth)+"// Seek Value\n" +
        offset(3+depth)+"do\n" +
        offset(3+depth)+"{\n" +
        offset(4+depth)+"switch(order_"+attrName+"[rel["+
        std::to_string(depth)+"]].second)\n" +
        offset(4+depth)+"{\n";
        
    off = 1;
    if (viewsPerVarInfo[view_id][idx+1] == _qc->numberOfViews())
    {
        // do base relation first
        returnString += seekValueCase(depth, relName, attrName);
        off = 2;
    }
        
    for (; off <= numberContributing; ++off)
    {
        const size_t& viewID = viewsPerVarInfo[view_id][idx+off];
        // case for this view
        returnString += seekValueCase(depth,viewName[viewID],attrName);
    }
    returnString += offset(4+depth)+"}\n";
        
    // Close the do loop
    returnString += offset(3+depth)+"} while (!found["+
        std::to_string(depth)+"] && !atEnd["+
        std::to_string(depth)+"]);\n" +
        offset(3+depth)+"// End Seek Value\n";
    // End seek value
        
    // check if atEnd
    returnString += offset(3+depth)+"// If atEnd break loop\n"+
        offset(3+depth)+"if(atEnd["+std::to_string(depth)+"])\n"+
        offset(4+depth)+"break;\n";

    off = 1;
    if (viewsPerVarInfo[view_id][idx+1] == _qc->numberOfViews())
    {            
        // Set upper = lower and update ranges
        returnString += offset(3+depth)+
            "upperptr_"+relName+"["+std::to_string(depth)+"] = "+
            "lowerptr_"+relName+"["+std::to_string(depth)+"];\n";

        // update range for base relation 
        returnString += updateRanges(depth, relName, attrName);
        off = 2;
    }

    for (; off <= numberContributing; ++off)
    {
        size_t viewID = viewsPerVarInfo[view_id][idx+off];

        returnString += offset(3+depth)+
            "upperptr_"+viewName[viewID]+
            "["+std::to_string(depth)+"] = "+
            "lowerptr_"+viewName[viewID]+
            "["+std::to_string(depth)+"];\n";
                
        returnString += updateRanges(depth,viewName[viewID],
                                     attrName);
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
                                                relName+"["+std::to_string(depth)+
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
                                            for (size_t n = 0; n < numberIncomingViews; ++n)
                                            {
                                                const size_t& viewID =
                                                    aggregate->_incoming[incCounter];

                                                View* incView = _qc->getView(viewID);

                                                if (incView->_fVars[v])
                                                {

                                                    /*this variable is part of varOrder*/
                                                    if (variableOrderBitset[view_id][v])
                                                    {
                                                        // add V_ID.var_name to the fVarString
                                                        freeVarString +=
                                                            viewName[viewID]+
                                                            "[lowerptr_"+
                                                            viewName[viewID]+"["+
                                                            std::to_string(depth)+"]]."+
                                                            _td->getAttribute(v)->_name+",";
                                                    }
                                                    else
                                                    {
                                                        // add V_ID.var_name to the fVarString
                                                        freeVarString += viewName[viewID]+
                                                            "[ptr_"+viewName[viewID]+"]."+
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
                    localAgg = "upperptr_"+relName+"["+std::to_string(depth)+"]-"+
                        "lowerptr_"+relName+"["+std::to_string(depth)+"]+1";
                    
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
                                +std::to_string(depth)+"]]"
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
                    " = lowerptr_"+relName+"["+std::to_string(depth)+"];\n";
                returnString += offset(3+depth)+"while(ptr_"+relName+
                    " <= upperptr_"+relName+"["+std::to_string(depth)+"])\n"+
                    offset(3+depth)+"{\n";

                closeLoopString = offset(3+depth+1)+"++ptr_"+relName+";\n"+
                    offset(3+depth)+"}\n";
                ++loopCounter;
            }
                
            for (size_t viewID=0; viewID < _qc->numberOfViews(); ++viewID)
            {
                if (loopCondition.first[viewID])
                {                        
                    returnString += offset(3+depth)+"size_t ptr_"+viewName[viewID]+"="+
                        " = lowerptr_"+viewName[viewID]+"["+std::to_string(depth)+"]\n";
                        
                    // Add loop for view
                    returnString += offset(3+depth+loopCounter)+
                        "while(ptr_"+viewName[viewID]+
                        " < upperptr_"+viewName[viewID]+"["+std::to_string(depth)+"])\n"+
                        offset(3+depth+loopCounter)+"{\n";

                    closeLoopString += offset(3+depth+loopCounter)+"++ptr_"+viewName[viewID]+";\n"+
                        offset(3+depth+loopCounter)+"}\n"+closeLoopString;
                }
            }

            returnString += loopCondition.second;

            returnString += closeLoopString;
                
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
                    returnString += relName+"[lowerptr_"+relName+
                        "["+std::to_string(depth)+"]]."+
                        _td->getAttribute(v)->_name+",";
                }
                else
                {
                    returnString += viewName[viewsPerVar[idx+1]]+
                        "[lowerptr_"+viewName[viewsPerVar[idx+1]]+
                        "["+std::to_string(depth)+"]]."+
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
            "lowerptr_"+relName+"["+std::to_string(depth)+"] = "+
            "upperptr_"+relName+"["+std::to_string(depth)+"];\n";
        off = 2;
    }

    for (; off <= numberContributing; ++off)
    {
        size_t viewID = viewsPerVarInfo[view_id][idx+off];
        returnString += offset(3+depth)+
            "lowerptr_"+viewName[viewID]+
            "["+std::to_string(depth)+"] = "+
            "upperptr_"+viewName[viewID]+
            "["+std::to_string(depth)+"];\n";
    }

    // Switch to update the max value and relation pointer
    returnString += offset(3+depth)+"switch(order_"+attrName+"[rel["+std::to_string(depth)+"]].second)\n";
    returnString += offset(3+depth)+"{\n";

    // Add switch cases to the switch 
    off = 1;
    if (viewsPerVarInfo[view_id][idx+1] == _qc->numberOfViews())
    {            
        returnString += updateMaxCase(depth,relName,attrName);
        off = 2;
    }

    for (; off <= numberContributing; ++off)
    {
        size_t viewID = viewsPerVarInfo[view_id][idx+off];
        returnString += updateMaxCase(depth,viewName[viewID],attrName);
    }
    returnString += offset(3+depth)+"}\n";

    // Closing the while loop 
    returnString += offset(2+depth)+"}\n";

    return returnString;
}

// One Generic Case for Seek Value 
std::string CppGenerator::seekValueCase(size_t depth, const std::string& rel_name,
                                        const std::string& attr_name)
{
    return offset(4+depth)+"case "+rel_name+"_ID:\n"+
        //if
        offset(5+depth)+"if("+rel_name+"[lowerptr_"+rel_name+"["+
        std::to_string(depth)+"]]."+attr_name+" == max_"+attr_name+")\n"+
        offset(6+depth)+"found["+std::to_string(depth)+"] = true;\n"+
        // else if 
        offset(5+depth)+"else if(max_"+attr_name+" > "+rel_name+"["+
        getUpperPointer(rel_name, depth)+"]."+attr_name+")\n"+
        offset(6+depth)+"atEnd["+std::to_string(depth)+"] = true;\n"+
        //else 
        offset(5+depth)+"else\n"+offset(5+depth)+"{\n"+
        offset(6+depth)+"findUpperBound("+rel_name+",&"+rel_name+"_tuple::"+
        attr_name+", max_"+attr_name+",lowerptr_"+rel_name+"["+
        std::to_string(depth)+"],"+getUpperPointer(rel_name,depth)+");\n"+
        offset(6+depth)+"max_"+attr_name+" = "+rel_name+"[lowerptr_"+
        rel_name+"["+std::to_string(depth)+"]]."+attr_name+";\n"+
        offset(6+depth)+"rel["+std::to_string(depth)+"] = (rel["+
        std::to_string(depth)+"]+1) % numOfRel["+std::to_string(depth)+"];\n"+
        offset(5+depth)+"}\n"+
        //break
        offset(5+depth)+"break;\n";
}

std::string CppGenerator::updateMaxCase(size_t depth, const std::string& rel_name,
                                        const std::string& attr_name)
{
    return offset(3+depth)+"case "+rel_name+"_ID:\n"+
        // update pointer
        offset(4+depth)+"lowerptr_"+rel_name+"["+
        std::to_string(depth)+"] += 1;\n"+
        // if statement
        offset(4+depth)+"if(lowerptr_"+rel_name+"["+
        std::to_string(depth)+"] > "+getUpperPointer(rel_name, depth)+")\n"+
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


std::string CppGenerator::getUpperPointer(const std::string rel_name, size_t depth)
{
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
                                       const std::string& attr_name)
{
    std::string depthString = std::to_string(depth);
    return offset(3+depth)+
        //while statementnd
        "while(upperptr_"+rel_name+"["+depthString+"]<"+
        getUpperPointer(rel_name, depth)+" && "+
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

std::string CppGenerator::genRunAll()
{
    
    std::string returnString = offset(1)+"void runAll()\n"+
        offset(1)+"{\n";

    returnString += offset(2)+
        "int64_t startLoading = duration_cast<milliseconds>("+
        "system_clock::now().time_since_epoch()).count();\n\n"+
        offset(2)+"loadRelations();\n\n"+
        offset(2)+"std::cout << \"Data loading: \" + std::to_string("+
        "duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()-"+
        "startLoading)+\"ms.\\n\";\n\n";
    
    returnString += offset(2)+"int64_t startProcess = duration_cast<milliseconds>("+
        "system_clock::now().time_since_epoch()).count();\n\n";

    for (size_t view = 0; view < _qc->numberOfViews(); ++view)
        returnString += offset(2)+"computeView"+std::to_string(view)+"();\n";
    
    returnString += "\n"+offset(2)+"std::cout << \"Data process: \"+"+
        "std::to_string(duration_cast<milliseconds>("+
        "system_clock::now().time_since_epoch()).count()-startProcess)+\"ms.\\n\";\n\n";
    
    // returnString += offset(2)+"for (V0_tuple& v : V0) std::cout << v.A << \",\" << "+
    //     "v.B <<\"|\"<< v.agg_0_0 << \",\" << v.agg_0_0 << \",\" << v.agg_0_0 << \",\""+
    //     " << v.agg_0_3 << std::endl;\n"+
    //     offset(2)+"std::cout << \"--------- \" << std::endl;\n";
    // returnString += offset(2)+"for (V1_tuple& v : V1) std::cout <<  v.E << \"|\" <<"+
    //     " v.agg_1_0 << \",\" << v.agg_1_1 << \",\" << v.agg_1_1 << \",\" << v.agg_1_3"+
    //     " << std::endl;\n"+offset(2)+"std::cout << \"--------- \" << std::endl;\n";
    // returnString += offset(2)+"for (V2_tuple& v : V2) std::cout <<  v.A << \"|\" <<"+
    //     " v.agg_2_0 << \",\" << v.agg_2_1 << \",\" << v.agg_2_2 << \",\" << v.agg_2_3"+
    //     " << \",\" << v.agg_2_4  << \",\" << v.agg_2_5 << std::endl;\n"+
    //     offset(2)+"std::cout << \"--------- \" << std::endl;\n";
    // returnString += offset(2)+"std::cout << V3[0].agg_3_0 << \",\" << V3[0].agg_3_1 "+
    //     "<< \",\" << V3[0].agg_3_2 << \",\" << V3[0].agg_3_3 << \",\" << V3[0].agg_3_4"+
    //     "<< \",\" << V3[0].agg_3_5 << std::endl;\n"+
    //     offset(2)+"std::cout << \"--------- \" << std::endl;\n";
    
    return returnString+offset(1)+"}\n";
}
