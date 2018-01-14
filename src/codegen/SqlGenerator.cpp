//--------------------------------------------------------------------
//
// SqlGenerator.cpp
//
// Created on: 9 January 2018
// Author: Max
//
//--------------------------------------------------------------------

#include <fstream>
#include <string>

#include <SqlGenerator.h>

using namespace std;
using namespace multifaq::params;

SqlGenerator::SqlGenerator(const std::string path,
                           std::shared_ptr<Launcher> launcher) : _pathToData(path)
{
     DINFO("Constructing SQL - Generator \n");
   
    _td = launcher->getTreeDecomposition();
    _qc = launcher->getCompiler();
        
    std::string dataset = path;
        
    if (dataset.back() == '/')
        dataset.pop_back();
}

SqlGenerator::~SqlGenerator()
{
}

void SqlGenerator::generateCode()
{
    DINFO("Starting SQL - Generator \n");
    
    int viewIdx = 0;
    
    for (size_t viewID = 0; viewID < _qc->numberOfViews(); ++viewID)
    {
        View* v = _qc->getView(viewID);
        
        string fvars = "", fromClause = "";
        
        for (size_t i = 0; i < NUM_OF_VARIABLES; ++i)
            if (v->_fVars.test(i))
                fvars +=  _td->getAttribute(i)->_name + ",";
        fvars.pop_back();

        TDNode* node = _td->getRelation(v->_origin);
        int numberIncomingViews =
            (v->_origin == v->_destination ? node->_numOfNeighbors :
             node->_numOfNeighbors - 1);

        vector<bool> viewBitset(_qc->numberOfViews());
        
        string aggregateString = "";
        for (size_t aggNo = 0; aggNo < v->_aggregates.size(); ++aggNo)
        {
            string agg = "";
            
            Aggregate* aggregate = v->_aggregates[aggNo];
                
            size_t aggIdx = 0, incomingCounter = 0;

            for (size_t i = 0; i < aggregate->_n; ++i)
            {
                string localAgg = "";

                while (aggIdx < aggregate->_m[i])
                {
                    const prod_bitset& p = aggregate->_agg[aggIdx];

                    for (size_t f = 0; f < NUM_OF_FUNCTIONS; ++f)
                    {  
                        if (p.test(f))
                            localAgg += getFunctionString(f)+"*";
                    }

                    localAgg.pop_back();
                    localAgg += "+";
                    ++aggIdx;
                }

                localAgg.pop_back();

                string viewAgg = "";
                while(incomingCounter < aggregate->_o[i])
                {
                    for (int n = 0; n < numberIncomingViews; ++n)
                    {
                        size_t viewID = aggregate->_incoming[incomingCounter];
                        size_t aggID = aggregate->_incoming[incomingCounter+1]; 
                    
                        viewAgg += "agg_"+to_string(viewID)+"_"+to_string(aggID)+"*";
                        viewBitset[viewID] = 1;                    
                        fromClause += "NATURAL JOIN view_"+to_string(viewID)+" ";
                        incomingCounter += 2;
                    }
                
                    viewAgg.pop_back();
                    viewAgg += "+";
                } 
                viewAgg.pop_back();
           
                if (viewAgg.size() > 0)
                {                
                    if (localAgg.size() > 0)
                        agg += "("+localAgg+")*("+viewAgg+")+";
                    else
                        agg += viewAgg+"+";
                }
                else   
                    agg += localAgg+"+";
            }
            agg.pop_back();

            if (agg.size() == 0) agg = "1";

            aggregateString += "\nSUM("+agg+") AS agg_"+to_string(viewIdx)+
                "_"+to_string(aggNo)+",";
        }

        aggregateString.pop_back();

        if (fvars.size() > 0)
        {
            printf("CREATE VIEW view_%d AS\nSELECT %s,%s\nFROM %s ",
                   viewIdx, fvars.c_str(), aggregateString.c_str(),
                   _td->getRelation(v->_origin)->_name.c_str());
            for (size_t id = 0; id < _qc->numberOfViews(); ++id)
                if (viewBitset[id])
                    printf("NATURAL JOIN view_%lu ",id);
            printf("\nGROUP BY %s;\n\n", fvars.c_str());
        }
        else
        {
            printf("CREATE VIEW view_%d AS \nSELECT %s\nFROM %s ",
                   viewIdx, aggregateString.c_str(),
                   _td->getRelation(v->_origin)->_name.c_str());
            for (size_t id = 0; id < _qc->numberOfViews(); ++id)
                if (viewBitset[id])
                    printf("NATURAL JOIN view_%lu ", id);     
            printf(";\n\n");
        }
        ++viewIdx;
    }
}


string SqlGenerator::getFunctionString(size_t fid)
{
    Function* f = _qc->getFunction(fid);

    string fvars = "";
        
    for (size_t i = 0; i < NUM_OF_VARIABLES; ++i)
        if (f->_fVars.test(i))
            fvars +=  _td->getAttribute(i)->_name + ",";
    fvars.pop_back();

    switch (f->_operation)
    {
    case Operation::count :
        return "f_"+to_string(fid);
    case Operation::sum :
        return fvars;  
    case Operation::linear_sum :
        return fvars;
    case Operation::quadratic_sum :
        return fvars+"*"+fvars;
    case Operation::prod :
        return "f_"+to_string(fid);
    case Operation::indicator_eq :
        return "f_"+to_string(fid);
    case Operation::indicator_neq :
        return "f_"+to_string(fid);
    case Operation::indicator_lt :
        return "f_"+to_string(fid);
    case Operation::indicator_gt :
        return "f_"+to_string(fid);
    case Operation::exponential :
        return "f_"+to_string(fid);
    case Operation::lr_cont_parameter :
        return "("+fvars+"_param*"+fvars+")";
    case Operation::lr_cat_parameter :
        return "("+fvars+"_param)";
    default : return "f_"+to_string(fid);
    }

    return " ";        
};

