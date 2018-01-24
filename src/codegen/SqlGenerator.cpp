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

#include <LinearRegression.h>
#include <SqlGenerator.h>

using namespace std;
using namespace multifaq::params;

SqlGenerator::SqlGenerator(const std::string path,
                           std::shared_ptr<Launcher> launcher) : _pathToData(path)
{
     DINFO("Constructing SQL - Generator \n");
   
    _td = launcher->getTreeDecomposition();
    _qc = launcher->getCompiler();
    _model = launcher->getModel();
    _app = launcher->getApplication();
    
    if (_pathToData.back() == '/')
        _pathToData.pop_back();
}

SqlGenerator::~SqlGenerator()
{
}

void SqlGenerator::generateCode()
{
    DINFO("Starting SQL - Generator \n");
    //  generateLoadQuery();

    generateLmfaoQuery();
    
    // generateNaiveQueries();
    
}

void SqlGenerator::generateLmfaoQuery()
{
    string returnString = "";
    
    int viewIdx = 0;
    for (size_t viewID = 0; viewID < _qc->numberOfViews(); ++viewID)
    {
        View* v = _qc->getView(viewID);
        TDNode* node = _td->getRelation(v->_origin);

        int numberIncomingViews =
            (v->_origin == v->_destination ? node->_numOfNeighbors :
             node->_numOfNeighbors - 1);
        
        // TODO: move down 
        vector<bool> viewBitset(_qc->numberOfViews());

        string fvars = ""; 
        for (size_t i = 0; i < NUM_OF_VARIABLES; ++i)
            if (v->_fVars.test(i))
                fvars +=  _td->getAttribute(i)->_name + ",";
        fvars.pop_back();

        string aggregateString = "";
        for (size_t aggNo = 0; aggNo < v->_aggregates.size(); ++aggNo)
        {
            Aggregate* aggregate = v->_aggregates[aggNo];

            
            string agg = "";
            size_t aggIdx = 0, incomingCounter = 0;
            for (size_t i = 0; i < aggregate->_n; ++i)
            {
                // TODO: move here 
                // vector<bool> viewBitset(_qc->numberOfViews());

                string localAgg = "";
                while (aggIdx < aggregate->_m[i])
                {
                    const prod_bitset& product = aggregate->_agg[aggIdx];

                    for (size_t f = 0; f < NUM_OF_FUNCTIONS; ++f)
                    {  
                        if (product.test(f))
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
                        incomingCounter += 2;
                    }
                
                    viewAgg.pop_back();
                    viewAgg += "+";
                }

                if (!viewAgg.empty())
                {          
                    viewAgg.pop_back();
           
                    if (localAgg.size() > 0)
                        agg += "("+localAgg+")*("+viewAgg+")+";
                    else
                        agg += viewAgg+"+";
                }
                else   
                    agg += localAgg+"+";


                // auto it = aggregateSection.find(viewBitset);
                // if (it != aggregateSection.end())
                //     it->second += aggregateString;
                // else
                //     aggregateSection[viewBitset] = aggregateString;
            }

            agg.pop_back();

            if (agg.empty()) agg = "1";

            aggregateString += "\nSUM("+agg+") AS agg_"+to_string(viewIdx)+
                "_"+to_string(aggNo)+",";
        }

        aggregateString.pop_back();

        if (fvars.size() > 0)
        {
            returnString += "CREATE VIEW view_"+to_string(viewIdx)+" AS\nSELECT "+fvars+
                ","+aggregateString+"\nFROM "+_td->getRelation(v->_origin)->_name+" ";
            for (size_t id = 0; id < _qc->numberOfViews(); ++id)
                if (viewBitset[id])
                    returnString += "NATURAL JOIN view_"+to_string(id)+" ";
            returnString += "\nGROUP BY "+fvars+";\n\n";
        }
        else
        {
            returnString += "CREATE VIEW view_"+to_string(viewIdx)+" AS\nSELECT "+
                aggregateString+"\nFROM "+_td->getRelation(v->_origin)->_name.c_str();
            for (size_t id = 0; id < _qc->numberOfViews(); ++id)
                if (viewBitset[id])
                    returnString += "NATURAL JOIN view_"+to_string(id)+" ";  
            returnString += ";\n\n";
        }
        ++viewIdx;
    }

    std::ofstream ofs("runtime/sql/lmfao.sql", std::ofstream::out);
    ofs << returnString;
    ofs.close();

    DINFO(returnString);   
}


void SqlGenerator::generateNaiveQueries()
{
    string joinString = "", aggregateString = "", fVarString = "",
        attributeString  = "";
    
    for (size_t rel = 0; rel < _td->numberOfRelations(); ++rel)
    {    
        joinString += _td->getRelation(rel)->_name;
        if (rel + 1 < _td->numberOfRelations())
            joinString += " NATURAL JOIN ";
    }
    
    for (size_t var = 0; var < _td->numberOfAttributes(); ++var)
        attributeString += _td->getAttribute(var)->_name + ",";
    attributeString.pop_back();

    std::ofstream ofs("runtime/sql/flat_join.sql", std::ofstream::out);
    ofs << "SELECT "+attributeString+"\nFROM "+joinString+";\n";
    ofs.close();
    DINFO("SELECT "+attributeString+"\nFROM "+joinString+";\n\n");
    
    if (_model == LinearRegressionModel)
    {
        // Add parameters to the join string !
        var_bitset features = _app->getFeatures();

        const var_bitset categoricalFeatures =
            static_pointer_cast<LinearRegression>(_app)->
            getCategoricalFeatures();
        
        std::string catParams = "", contParams = "";
        for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
        {
            if (features[var])
            {
                Attribute* attr = _td->getAttribute(var);

                if (categoricalFeatures[var])
                    catParams += " NATURAL JOIN "+attr->_name+"_param";
                else
                    contParams += ","+attr->_name+"_param";
            }
        }
        
        joinString += catParams + contParams;
    }

    ofs.open("runtime/sql/aggjoin_naive.sql", std::ofstream::out);
    
    for (size_t queryID = 0; queryID < _qc->numberOfQueries(); ++queryID)
    {
        Query* query = _qc->getQuery(queryID);
        aggregateString = "", fVarString = "";

        for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
        {
            if (query->_fVars[var])
                fVarString += _td->getAttribute(var)->_name + ",";
        }
        // if (!fVarString.empty())
        //     fVarString.pop_back();

        std::string aggregateString = "";
        for (size_t agg = 0; agg < query->_aggregates.size(); ++agg)
        {
            Aggregate* aggregate = query->_aggregates[agg];
            
            size_t aggIdx = 0;
            for (size_t i = 0; i < aggregate->_n; i++)
            {
                while (aggIdx < aggregate->_m[i])
                {
                    const prod_bitset prod = aggregate->_agg[aggIdx];
                    
                    for (size_t f = 0; f < NUM_OF_FUNCTIONS; f++)
                    {
                        if (prod.test(f))
                        {
                            aggregateString += getFunctionString(f)+"*";
                        }
                    }

                    aggregateString.pop_back();
                    aggregateString += "+";
                    ++aggIdx;
                }
            }
            aggregateString.pop_back();
            aggregateString += ",";
        }
        aggregateString.pop_back();
        
        ofs << "SELECT "+fVarString+aggregateString+"\nFROM "+joinString+";\n";
        DINFO("SELECT "+fVarString+aggregateString+"\nFROM "+joinString+";\n");
    }
    ofs.close();
}

void SqlGenerator::generateLoadQuery()
{
    string load = "", drop = "";
    
    for (size_t relID = 0; relID < _td->numberOfRelations(); ++relID)
    {
        TDNode* rel = _td->getRelation(relID);
        const std::string& relName = rel->_name;

        drop += "DROP TABLE IF EXISTS "+relName+";\n";
        
        load += "CREATE TABLE "+relName+ "(";
        for (size_t var = 0; var < NUM_OF_VARIABLES; var++)
        {
            if (rel->_bag[var])
            {
                Attribute* att = _td->getAttribute(var);
                load += att->_name+" "+typeToStr(att->_type)+",";
            }
        }
        load.pop_back();
        load += ");\n";

        load += "COPY "+relName+" FROM '"+_pathToData+"/"+relName+".tbl\' "+
                "DELIMITER \'|\' CSV;\n";
    }

    std::ofstream ofs("runtime/sql/load_data.sql", std::ofstream::out);
    ofs << drop + load;
    ofs.close();
    DINFO(drop + load);
    
    for (size_t viewID = 0; viewID < _qc->numberOfViews(); ++viewID)
        drop +=  "DROP VIEW IF EXISTS V"+std::to_string(viewID)+";\n";
    
    ofs.open("runtime/sql/drop_data.sql", std::ofstream::out);
    ofs << drop;
    ofs.close();
    DINFO(drop);
}

inline string SqlGenerator::typeToStr(Type type)
{
    switch (type)
    {
    case Type::Integer : return "int";
    case Type::Double : return "numeric";            
    case Type::Short : return "int";
    case Type::U_Integer : return "int";
    default :
        ERROR("This type does not exist  (SqlGenerator - typeToString)\n");
        exit(1);
    }
}

inline string SqlGenerator::getFunctionString(size_t fid)
{
    Function* f = _qc->getFunction(fid);

    string fvars = "";
    for (size_t i = 0; i < NUM_OF_VARIABLES; ++i)
    {
        if (f->_fVars.test(i))
            fvars +=  _td->getAttribute(i)->_name + ",";
    }
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

