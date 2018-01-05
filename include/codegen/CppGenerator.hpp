//--------------------------------------------------------------------
//
// CppGenerator.hpp
//
// Created on: 16 Dec 2017
// Author: Max
//
//--------------------------------------------------------------------

#ifndef INCLUDE_CPPGENERATOR_HPP_
#define INCLUDE_CPPGENERATOR_HPP_

#include <fstream>
#include <string>

#include <Launcher.h>
#include <QueryCompiler.h>
#include <TDNode.hpp>
#include <TreeDecomposition.h>

using namespace multifaq::params;

class CppGenerator
{
public:

    CppGenerator(const std::string path,
                 std::shared_ptr<Launcher> launcher) : _pathToData(path)
    {
        _td = launcher->getTreeDecomposition();
        _qc = launcher->getCompiler();
        
        std::string dataset = path;
        
        if (dataset.back() == '/')
            dataset.pop_back();
        
        _datasetName = dataset.substr(dataset.rfind('/')).substr(1);
    }

    ~CppGenerator();

    void generateCppCode()
    {
        createVariableOrder();

        std::ofstream ofs ("test.hpp", std::ofstream::out);
        
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
        
        for (size_t rel = 0; rel < _td->numberOfRelations(); ++rel)
        {
            std::cout << genRelationVectors(_td->getRelation(rel));
            ofs << genRelationVectors(_td->getRelation(rel));
        }

        for (size_t view = 0; view < _qc->numberOfViews(); ++view)
        {
            std::cout << genViewVectors(view);
            ofs << genViewVectors(view);

        }
        ofs << std::endl;

        
        std::cout << genLoadRelationFunction() << std::endl;
        ofs << genLoadRelationFunction() << std::endl;

        std::cout << genCaseIdentifiers() << std::endl;
        ofs << genCaseIdentifiers() << std::endl;
        
        for (size_t view = 0; view < _qc->numberOfViews(); ++view)
        {
            std::cout << genViewComputeFunction(view) << std::endl;
            ofs << genViewComputeFunction(view) << std::endl;
        }
        
        std::cout << genFooter() << std::endl;
        ofs << genFooter() << std::endl;

        ofs.close();
    };
    
private:
    
    std::string _pathToData;

    std::string _datasetName;

    std::shared_ptr<TreeDecomposition> _td;

    std::shared_ptr<QueryCompiler> _qc;

    // std::vector<std::vector<bool>> vbs;
    // std::vector<size_t> vbsCount;

    std::vector<size_t>* variableOrder = nullptr;
    std::vector<size_t>* viewsPerVarInfo = nullptr;
    std::vector<size_t>* incomingViews = nullptr; //TODO: this could be part of
                                                  //the view

    /* TODO: Technically there is no need to pre-materialise this! */
    void createVariableOrder()
    {
        variableOrder = new std::vector<size_t>[_qc->numberOfViews()]();
        incomingViews = new std::vector<size_t>[_qc->numberOfViews()]();
        viewsPerVarInfo = new std::vector<size_t>[_qc->numberOfViews()];
        
        for (size_t viewID = 0; viewID < _qc->numberOfViews(); ++viewID)
        {
            View* view = _qc->getView(viewID);
            TDNode* baseRelation = _td->getRelation(view->_origin);

            int numberIncomingViews =
                (view->_origin == view->_destination ? baseRelation->_numOfNeighbors :
                 baseRelation->_numOfNeighbors - 1);
            
            // for free vars push them on the variableOrder
            for (size_t var = 0; var < multifaq::params::NUM_OF_VARIABLES; ++var)
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
            for (size_t var = 0; var < multifaq::params::NUM_OF_VARIABLES; ++var)
                if (additionalVars[var])
                    variableOrder[viewID].push_back(var);
            
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
    
    std::string offset(size_t off)
    {
        return std::string(off*3, ' ');
    }
    
    std::string typeToStr(Type t)
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

    std::string typeToStringConverter(Type t)
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
    
    std::string genHeader()
    {
        std::string upperDBName;
        std::transform(_datasetName.begin(), _datasetName.end(),
                       upperDBName.begin(), ::toupper);
        
        return "#ifndef INCLUDE_TEMPLATE_"+upperDBName+"_HPP_\n"+
            "#define INCLUDE_TEMPLATE_"+upperDBName+"_HPP_\n\n"+
            "#include <iostream>\n" +
            "#include <vector>\n\n" +
            "#include <CppHelper.hpp>\n" +
            "#include <DataHandler.hpp>\n\n" +
            "const std::string PATH_TO_DATA = \""+_pathToData+"\";\n\n"+
            "namespace multifaq\n{\n";
    }

    std::string genFooter()
    {
        std::string upperDBName;
        std::transform(_datasetName.begin(), _datasetName.end(),
                       upperDBName.begin(), ::toupper);
        
        return "}\n\n#endif /* INCLUDE_TEMPLATE_"+upperDBName+"_HPP_*/";
    }
    
    std::string genRelationTupleStructs(TDNode* rel)
    {
        std::string tupleStruct = "", attrConversion = "",
            attrConstruct = "", attrAssign = "";
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

    std::string genViewTupleStructs(View* view, size_t view_id)
    {
        std::string tupleStruct = "", attrConstruct = "", attrAssign = "";
        std::string view_name = "V"+std::to_string(view_id);
        
        tupleStruct += offset(1)+"struct "+view_name+"_tuple\n"+
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
        
        tupleStruct += view_name+"_tuple() {} \n"+offset(2) +
            view_name+"_tuple("+attrConstruct+")\n"+offset(2)+
            "{\n"+attrAssign+offset(2)+"}\n"+offset(1)+"};\n\n";

        return tupleStruct;
    }

    std::string genRelationVectors(TDNode* rel)
    {
        return offset(1)+"std::vector<"+rel->_name+"_tuple> "+rel->_name+";\n";
    }

    std::string genViewVectors(size_t view_id)
    {
        return offset(1)+"std::vector<V"+std::to_string(view_id)+"_tuple> "+
            "V"+std::to_string(view_id)+";\n";
    }
    
    std::string genCaseIdentifiers()
    {
        std::string returnString = offset(1)+"enum ID {";

        for (size_t rel = 0; rel < _td->numberOfRelations(); ++rel)
        {
            returnString += "\n"+offset(2)+_td->getRelation(rel)->_name+"_ID,";
        }
        for (size_t view = 0; view < _qc->numberOfViews(); ++view)
        {
            returnString += "\n"+offset(2)+"V"+std::to_string(view)+"_ID,";
        }
        returnString.pop_back();

        return returnString+"\n"+offset(1)+"};\n";
    }

    std::string genLoadRelationFunction()
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
    
    std::string genViewComputeFunction(size_t view_id)
    {
        // for the entire Aggregate - collect all incoming views and join on the
        // join aggregates from the relation -- let's first create the join
        // output - then we go from there

        std::cout << "start genViewComputeFunction \n";
        
        View* view = _qc->getView(view_id);
        TDNode* baseRelation =_td->getRelation(view->_origin);
        
        int numberIncomingViews =
            (view->_origin == view->_destination ? baseRelation->_numOfNeighbors :
             baseRelation->_numOfNeighbors - 1);

        std::vector<size_t>& varOrder = variableOrder[view_id];
        std::vector<size_t>& viewsPerVar = viewsPerVarInfo[view_id];
        std::vector<size_t>& incViews = incomingViews[view_id];
        
        /**************************/
        // Now create the actual join code for this view 
        // Compare with template code
        /**************************/

        std::string returnString = offset(1)+"void computeView"+
            std::to_string(view_id)+"()\n"+offset(1)+"{\n";
        
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
        {            
            returnString += genPointerArrays(
                "V"+std::to_string(viewID), numOfJoinVarString);
        }
        
        // find first join attribute
        std::string attr_name = _td->getAttribute(varOrder[0])->_name;
        
        std::cout << "before join code \n";

        // for each join var --- create the string that does the join
        returnString += genLeapfrogJoinCode(view_id, 0,
                                            baseRelation->_name, attr_name);
        std::cout << "after join code \n";

        returnString += offset(1)+"}\n";

        return returnString;
    }
    
    std::string genMaxMinValues(const size_t& view_id)
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

    std::string genPointerArrays(const std::string& rel, std::string& numOfJoinVars)
    {
        return offset(2)+"size_t lower_pointer_"+rel+"["+numOfJoinVars+"] = {}, "+
            "upper_pointer_"+rel+"["+numOfJoinVars+"] = {}; \n" +
            offset(2)+"upper_pointer_"+rel+"[0] = "+rel+".size()-1;\n";
    }

    std::string genRelationOrdering(const std::string& rel_name, size_t join_idx,
                                    const std::string& attr_name, std::string depth,
                                    size_t view_id)
    {
        // TODO: FIXME: add the case when there is only two or one reations
        std::string res = offset(2+join_idx)+
            "/*********** ORDERING RELATIONS *************/\n";
        
        size_t idx = join_idx * (_qc->numberOfViews() + 2);
        size_t numberContributing = viewsPerVarInfo[view_id][idx];

        res += offset(2+join_idx)+"std::pair<int, int> order_"+attr_name+"["+
            std::to_string(numberContributing)+"] = \n"+
            offset(3+join_idx)+"{";
        
        size_t off = 1;
        if (viewsPerVarInfo[view_id][idx+1] == _qc->numberOfViews())
        {
            res += "\n"+offset(4+join_idx)+"std::make_pair("+rel_name+
                "[lower_pointer_"+rel_name+"["+depth+"]]."+attr_name+", "+
                rel_name+"_ID),";
            off = 2;
        }
        
        for (; off <= numberContributing; ++off)
        {
            size_t viewID = viewsPerVarInfo[view_id][idx+off];
            res += "\n"+offset(4+join_idx)+"std::make_pair(V"+std::to_string(viewID)+
                "[lower_pointer_V"+std::to_string(viewID)+"["+depth+"]]."+
                attr_name+", V"+std::to_string(viewID)+"_ID),";
        }
        res.pop_back();
        res += "\n"+offset(3+join_idx)+"};\n";
        
        res += offset(2+join_idx)+"std::sort(order_"+attr_name+", order_"+attr_name+
            " + "+std::to_string(numberContributing)+", sort_pred<int>());\n";

        res += offset(2+join_idx)+"min_"+attr_name+" = order_"+attr_name+"[0].first;\n";
        res += offset(2+join_idx)+"max_"+attr_name+" = order_"+attr_name+"["+
            std::to_string(numberContributing-1)+"].first;\n";
        
        res += offset(2+join_idx)+"/*********** ORDERING RELATIONS *************/\n";
        return res;
    }

    std::string genLeapfrogJoinCode(size_t view_id, size_t depth,
                                    const std::string& rel_name,
                                    const std::string& attr_name)
    {
        std::vector<size_t>& varOrder = variableOrder[view_id];
        std::vector<size_t>& viewsPerVar = viewsPerVarInfo[view_id];
        std::vector<size_t>& incViews = viewsPerVarInfo[view_id];
        
        size_t idx = depth * (_qc->numberOfViews() + 2);
        size_t numberContributing = viewsPerVarInfo[view_id][idx];

        std::string returnString = "";
        
        size_t off = 1;
        if (depth > 0)
        {
            if (viewsPerVarInfo[view_id][idx+1] == _qc->numberOfViews())
            {
                returnString += offset(2+depth)+
                    "upper_pointer_"+rel_name+"["+std::to_string(depth)+"] = "+
                    getUpperPointer(rel_name, depth)+";\n"+
                    offset(2+depth)+
                    "lower_pointer_"+rel_name+"["+std::to_string(depth)+"] = "+
                    getLowerPointer(rel_name, depth)+";\n";

                off = 2;
            }
        
            for (; off <= numberContributing; ++off)
            {
                size_t viewID = viewsPerVarInfo[view_id][idx+off];

                returnString += offset(2+depth)+
                    "upper_pointer_V"+std::to_string(viewID)+
                    "["+std::to_string(depth)+"] = "+
                    getUpperPointer("V"+std::to_string(viewID),depth)+";\n"+
                    offset(2+depth)+
                    "lower_pointer_V"+std::to_string(viewID)+
                    "["+std::to_string(depth)+"] = "+
                    getLowerPointer("V"+std::to_string(viewID),depth)+";\n";
            }
        }
        
        // Ordering Relations 
        returnString += genRelationOrdering(
            rel_name, depth, attr_name, std::to_string(depth), view_id);
            
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
            offset(4+depth)+"switch(order_"+attr_name+"[rel["+
            std::to_string(depth)+"]].second)\n" +
            offset(4+depth)+"{\n";
        
        off = 1;
        if (viewsPerVarInfo[view_id][idx+1] == _qc->numberOfViews())
        {
            // do base relation first
            returnString += seekValueCase(depth, rel_name, attr_name);
            off = 2;
        }
        
        for (; off <= numberContributing; ++off)
        {
            size_t viewID = viewsPerVarInfo[view_id][idx+off];
            // case for this view
            returnString += seekValueCase(
                depth,"V"+std::to_string(viewID),attr_name);
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
                "upper_pointer_"+rel_name+"["+std::to_string(depth)+"] = "+
                "lower_pointer_"+rel_name+"["+std::to_string(depth)+"];\n";

            // update range for base relation 
            returnString += updateRanges(depth, rel_name, attr_name);
            off = 2;
        }
        
        for (; off <= numberContributing; ++off)
        {
            size_t viewID = viewsPerVarInfo[view_id][idx+off];

            returnString += offset(3+depth)+
                "upper_pointer_V"+std::to_string(viewID)+
                "["+std::to_string(depth)+"] = "+
                "lower_pointer_V"+std::to_string(viewID)+
                "["+std::to_string(depth)+"];\n";
                
            returnString += updateRanges(depth,"V"+std::to_string(viewID),
                                         attr_name);
        }

        // then you would need to go to next variable
        if (depth+1 < varOrder.size())
        {
            // Find next join attribute
            std::string nextAttrName = _td->getAttribute(varOrder[depth+1])->_name;
            
            // leapfrogJoin for next join variable 
            returnString += genLeapfrogJoinCode(view_id, depth+1,
                                                rel_name, nextAttrName);
        }
        else
        {
            //* Do whatever operation is necessary *//

            // need to find the relation where they are from - and then fill the
            // tuple
            View* view = _qc->getView(view_id);         

            returnString += offset(3+depth)+"V"+std::to_string(view_id)+".push_back({";

            size_t varCounter = 0;
            for (size_t v = 0; v < multifaq::params::NUM_OF_VARIABLES; ++v)
            {
                if (view->_fVars[v])
                {
                    size_t idx = varCounter * (_qc->numberOfViews() + 2);
                    if (viewsPerVar[idx+1] == _qc->numberOfViews())
                    {
                        returnString += rel_name+"[lower_pointer_"+rel_name+
                            "["+std::to_string(depth)+"]]."+
                            _td->getAttribute(v)->_name+",";
                    }
                    else
                    {
                        returnString += "V"+std::to_string(viewsPerVar[idx+1])+
                            "[lower_pointer_V"+std::to_string(viewsPerVar[idx+1])+
                            "["+std::to_string(depth)+"]]."+
                            _td->getAttribute(v)->_name+",";
                    }
                    ++varCounter;
                }
            }
            for (size_t agg = 0; agg < view->_aggregates.size(); ++agg)
                 returnString += "0,";
            
            returnString.pop_back();
            returnString += "});\n";
        }

        // Set lower to upper pointer 
        off = 1;
        if (viewsPerVarInfo[view_id][idx+1] == _qc->numberOfViews())
        {            
            // Set upper = lower and update ranges
            returnString += offset(3+depth)+
                "lower_pointer_"+rel_name+"["+std::to_string(depth)+"] = "+
                "upper_pointer_"+rel_name+"["+std::to_string(depth)+"];\n";
            off = 2;
        }

        for (; off <= numberContributing; ++off)
        {
            size_t viewID = viewsPerVarInfo[view_id][idx+off];
            returnString += offset(3+depth)+
                "lower_pointer_V"+std::to_string(viewID)+
                "["+std::to_string(depth)+"] = "+
                "upper_pointer_V"+std::to_string(viewID)+
                "["+std::to_string(depth)+"];\n";
        }

        // Switch to update the max value and relation pointer
        returnString += offset(3+depth)+"switch(order_"+attr_name+"[rel[0]].second)\n";
        returnString += offset(3+depth)+"{\n";

        // Add switch cases to the switch 
        off = 1;
        if (viewsPerVarInfo[view_id][idx+1] == _qc->numberOfViews())
        {            
            returnString += updateMaxCase(depth,rel_name,attr_name);
            off = 2;
        }

        for (; off <= numberContributing; ++off)
        {
            size_t viewID = viewsPerVarInfo[view_id][idx+off];
            returnString += updateMaxCase(depth,"V"+std::to_string(viewID),attr_name);
        }
        returnString += offset(3+depth)+"}\n";

        // Closing the while loop 
        returnString += offset(2+depth)+"}\n";
        
        return returnString;
    }

    // One Generic Case for Seek Value 
    std::string seekValueCase(size_t depth, const std::string& rel_name,
                              const std::string& attr_name)
    {
        return offset(4+depth)+"case "+rel_name+"_ID:\n"+
            //if
            offset(5+depth)+"if("+rel_name+"[lower_pointer_"+rel_name+"["+
            std::to_string(depth)+"]]."+attr_name+" == max_"+attr_name+")\n"+
            offset(6+depth)+"found["+std::to_string(depth)+"] = true;\n"+
            // else if 
            offset(5+depth)+"else if(max_"+attr_name+" > "+rel_name+"["+
            getUpperPointer(rel_name, depth)+"]."+attr_name+")\n"+
            offset(6+depth)+"atEnd["+std::to_string(depth)+"] = true;\n"+
            //else 
            offset(5+depth)+"else\n"+offset(5+depth)+"{\n"+
            offset(6+depth)+"findUpperBound("+rel_name+",&"+rel_name+"_tuple::"+
            attr_name+", max_"+attr_name+",lower_pointer_"+rel_name+"["+
            std::to_string(depth)+"],"+getUpperPointer(rel_name,depth)+");\n"+
            offset(6+depth)+"max_"+attr_name+" = "+rel_name+"[lower_pointer_"+
            rel_name+"["+std::to_string(depth)+"]]."+attr_name+";\n"+
            offset(6+depth)+"rel["+std::to_string(depth)+"] = (rel["+
            std::to_string(depth)+"]+1) % numOfRel["+std::to_string(depth)+"];\n"+
            offset(5+depth)+"}\n"+
            //break
            offset(5+depth)+"break;\n";
    }

    std::string updateMaxCase(size_t depth, const std::string& rel_name,
                              const std::string& attr_name)
    {
        return offset(3+depth)+"case "+rel_name+"_ID:\n"+
            // update pointer
            offset(4+depth)+"lower_pointer_"+rel_name+"["+
            std::to_string(depth)+"] += 1;\n"+
            // if statement
            offset(4+depth)+"if(lower_pointer_"+rel_name+"["+
            std::to_string(depth)+"] > "+getUpperPointer(rel_name, depth)+")\n"+
            // body
            offset(5+depth)+"atEnd["+std::to_string(depth)+"] = true;\n"+
            //else
            offset(4+depth)+"else\n"+offset(4+depth)+"{\n"+
            offset(5+depth)+"max_"+attr_name+" = "+rel_name+"[lower_pointer_"+
            rel_name+"["+std::to_string(depth)+"]]."+attr_name+";\n"+
            offset(5+depth)+"rel["+std::to_string(depth)+"] = (rel["+
            std::to_string(depth)+"]+ 1) % numOfRel["+std::to_string(depth)+"];\n"+
            offset(4+depth)+"}\n"+offset(4+depth)+"break;\n";
    }

    std::string getUpperPointer(const std::string rel_name, size_t depth)
    {
        if (depth == 0)
            return rel_name+".size()-1";
        return "upper_pointer_"+rel_name+"["+std::to_string(depth-1)+"]";
    }

    std::string getLowerPointer(const std::string rel_name, size_t depth)
    {
        if (depth == 0)
            return "0";
        return "lower_pointer_"+rel_name+"["+std::to_string(depth-1)+"]";
    }
    
    std::string updateRanges(size_t depth, const std::string& rel_name,
                              const std::string& attr_name)
    {
        std::string depthString = std::to_string(depth);
        return offset(3+depth)+
            //while statement
            "while(upper_pointer_"+rel_name+"["+depthString+"]<"+
            getUpperPointer(rel_name, depth)+" && "+
            rel_name+"[upper_pointer_"+rel_name+"["+depthString+"]+1]."+
            attr_name+" == max_"+attr_name+")\n"+
            //body
            offset(3+depth)+"{\n"+
            offset(4+depth)+"++upper_pointer_"+rel_name+"["+depthString+"];\n"+
            offset(3+depth)+"}\n";
    }
    
};


#endif /* INCLUDE_CPPGENERATOR_HPP_ */
