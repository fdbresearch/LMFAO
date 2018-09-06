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
#include <unordered_map>

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

void SqlGenerator::generateCode(const ParallelizationType parallelization_type)
{
    DINFO("Starting SQL - Generator \n");

    generateLoadQuery();
    generateJoinQuery();
    generateLmfaoQuery();
    generateAggregateQueries();
    generateOutputQueries();
}


void SqlGenerator::generateLmfaoQuery()
{
    string returnString = "";
    
    for (size_t viewID = 0; viewID < _qc->numberOfViews(); ++viewID)
    {
        View* v = _qc->getView(viewID);
        TDNode* node = _td->getRelation(v->_origin);

        int numberIncomingViews =
            (v->_origin == v->_destination ? node->_numOfNeighbors :
             node->_numOfNeighbors - 1);
        
        vector<bool> viewBitset(_qc->numberOfViews());

        string fvars = ""; 
        for (size_t i = 0; i < NUM_OF_VARIABLES; ++i)
            if (v->_fVars.test(i))
                fvars +=  _td->getAttribute(i)->_name + ",";
       
        string aggregateString = "";
        for (size_t aggNo = 0; aggNo < v->_aggregates.size(); ++aggNo)
        {
            Aggregate* aggregate = v->_aggregates[aggNo];

            string agg = "";
            size_t incomingCounter = 0;
            for (size_t i = 0; i < aggregate->_agg.size(); ++i)
            {
                string localAgg = "";
                const prod_bitset& product = aggregate->_agg[i];
                for (size_t f = 0; f < NUM_OF_FUNCTIONS; ++f)
                {  
                    if (product.test(f))
                        localAgg += getFunctionString(f)+"*";
                }

                agg += localAgg;
                
                string viewAgg = "";
                for (int n = 0; n < numberIncomingViews; ++n)
                {
                    std::pair<size_t,size_t> viewAggID =
                        aggregate->_incoming[incomingCounter];
                    
                    viewAgg += "agg_"+to_string(viewAggID.first)+"_"+
                        to_string(viewAggID.second)+"*";
                    viewBitset[viewAggID.first] = 1;
                    ++incomingCounter;
                }

                agg += viewAgg;

                if (!agg.empty())
                {
                    agg.pop_back();
                    agg += "+";
                }              
            }

            if (agg.empty())
                agg = "1";
            else
                agg.pop_back();

            aggregateString += "\nSUM("+agg+") AS agg_"+to_string(viewID)+
                "_"+to_string(aggNo)+",";
        }

        aggregateString.pop_back();

        
        returnString += "CREATE TABLE view_"+to_string(viewID)+" AS\nSELECT "+fvars+
            aggregateString+"\nFROM "+_td->getRelation(v->_origin)->_name+" ";
        for (size_t id = 0; id < _qc->numberOfViews(); ++id)
            if (viewBitset[id])
                returnString += "NATURAL JOIN view_"+to_string(id)+" ";

        if (!fvars.empty())
        {
            fvars.pop_back();
            returnString += "\nGROUP BY "+fvars;
        }
        
        returnString += ";\n\n";
    }

    std::ofstream ofs("runtime/sql/lmfao.sql", std::ofstream::out);
    ofs << returnString;
    ofs.close();
    // DINFO(returnString);   
}

void SqlGenerator::generateJoinQuery()
{
    string joinString = "", attributeString  = "";
    
    for (size_t rel = 0; rel < _td->numberOfRelations(); ++rel)
    {    
        joinString += _td->getRelation(rel)->_name;
        if (rel + 1 < _td->numberOfRelations())
            joinString += " NATURAL JOIN ";
    }
    
    for (size_t var = 0; var < _td->numberOfAttributes(); ++var)
        attributeString += _td->getAttribute(var)->_name + ",";
    attributeString.pop_back();

    std::ofstream ofs("runtime/sql/join.sql", std::ofstream::out);
    ofs << "SELECT "+attributeString+"\nFROM "+joinString+";\n";
    ofs.close();
}

// void SqlGenerator::generateNaiveQueries()
// {
// #ifdef PREVIOUS
//     string joinString = "", aggregateString = "", fVarString = "",
//         attributeString  = "";
//     for (size_t rel = 0; rel < _td->numberOfRelations(); ++rel)
//     {    
//         joinString += _td->getRelation(rel)->_name;
//         if (rel + 1 < _td->numberOfRelations())
//             joinString += " NATURAL JOIN ";
//     }
//     for (size_t var = 0; var < _td->numberOfAttributes(); ++var)
//         attributeString += _td->getAttribute(var)->_name + ",";
//     attributeString.pop_back();
//     std::ofstream ofs("runtime/sql/flat_join.sql", std::ofstream::out);
//     ofs << "SELECT "+attributeString+"\nFROM "+joinString+";\n";
//     ofs.close();
//     //  DINFO("SELECT "+attributeString+"\nFROM "+joinString+";\n\n");
//     if (_model == LinearRegressionModel)
//     {
//         // Add parameters to the join string !
//         var_bitset features = _app->getFeatures();
//         const var_bitset categoricalFeatures =
//             static_pointer_cast<LinearRegression>(_app)->
//             getCategoricalFeatures();
//         std::string catParams = "", contParams = "";
//         for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
//         {
//             if (features[var])
//             {
//                 Attribute* attr = _td->getAttribute(var);
//                 if (categoricalFeatures[var])
//                     catParams += " NATURAL JOIN "+attr->_name+"_param";
//                 else
//                     contParams += ","+attr->_name+"_param";
//             }
//         }
//         joinString += catParams + contParams;
//     }
//     ofs.open("runtime/sql/aggjoin_naive.sql", std::ofstream::out);
//     for (size_t queryID = 0; queryID < _qc->numberOfQueries(); ++queryID)
//     {
//         Query* query = _qc->getQuery(queryID);
//         aggregateString = "", fVarString = "";
//         for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
//         {
//             if (query->_fVars[var])
//                 fVarString += _td->getAttribute(var)->_name + ",";
//         }
//         std::string aggregateString = "";
//         for (size_t agg = 0; agg < query->_aggregates.size(); ++agg)
//         {
//             Aggregate* aggregate = query->_aggregates[agg];
//             size_t aggIdx = 0;
//             for (size_t i = 0; i < aggregate->_n; i++)
//             {
//                 while (aggIdx < aggregate->_m[i])
//                 {
//                     const prod_bitset prod = aggregate->_agg[aggIdx];
//                     for (size_t f = 0; f < NUM_OF_FUNCTIONS; f++)
//                     {
//                         if (prod.test(f))
//                         {
//                             aggregateString += getFunctionString(f)+"*";
//                         }
//                     }
//                     aggregateString.pop_back();
//                     aggregateString += "+";
//                     ++aggIdx;
//                 }
//             }
//             aggregateString.pop_back();
//             aggregateString += ",";
//         }
//         aggregateString.pop_back();
//         if (aggregateString.empty())
//             aggregateString = "1";
        
//         ofs << "SELECT "+fVarString+"SUM("+aggregateString+")\nFROM "+joinString;
//         if (!fVarString.empty())
//         {
//             fVarString.pop_back();
//             ofs << "\nGROUP BY "+fVarString;
//         }
//         ofs << ";\n";
        
//         //   DINFO("SELECT "+fVarString+aggregateString+"\nFROM "+joinString+";\n");
//     }
//     ofs.close();
// #endif
// }



void SqlGenerator::generateAggregateQueries()
{
    string joinString = "", attributeString  = "";
    
    for (size_t rel = 0; rel < _td->numberOfRelations(); ++rel)
    {    
        joinString += _td->getRelation(rel)->_name;
        if (rel + 1 < _td->numberOfRelations())
            joinString += " NATURAL JOIN ";
    }
    
    // for (size_t var = 0; var < _td->numberOfAttributes(); ++var)
    //     attributeString += _td->getAttribute(var)->_name + ",";
    // attributeString.pop_back();
    
    //********** TODO: TODO: ********** 
    // technically we need to join in the parameters
    // for now we just fix the params
    //********** TODO: TODO: ********** 
    if (false && _model == LinearRegressionModel)
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

    unordered_map<var_bitset,string> fVarAggregateMap;
    for (size_t queryID = 0; queryID < _qc->numberOfQueries(); ++queryID)
    {
        Query* query = _qc->getQuery(queryID);
        
        std::string aggregateString = "";
        for (size_t agg = 0; agg < query->_aggregates.size(); ++agg)
        {
            Aggregate* aggregate = query->_aggregates[agg];
            
            size_t aggIdx = 0;

            string sumString = "";
            for (size_t i = 0; i < aggregate->_agg.size(); i++)
            {
                const prod_bitset prod = aggregate->_agg[aggIdx];

                string prodString = "";
                for (size_t f = 0; f < NUM_OF_FUNCTIONS; f++)
                {
                    if (prod.test(f))
                    {
                        prodString += getFunctionString(f)+"*";
                    }
                }

                if (!prodString.empty())
                    prodString.pop_back();
                else
                    prodString += "1";
                
                sumString += prodString+"+";
                ++aggIdx;
            }
            sumString.pop_back();
            aggregateString += "SUM("+sumString+"),";
        }

        auto it_pair = fVarAggregateMap.insert({query->_fVars,""});
        it_pair.first->second += aggregateString;        
    }

    ofstream ofs("runtime/sql/aggregates.sql", std::ofstream::out);

    for (auto& fvarAggPair : fVarAggregateMap)
    {
        string fVarString = "";
        
        for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
        {
            if (fvarAggPair.first[var])
                fVarString += _td->getAttribute(var)->_name + ",";
        }

        fvarAggPair.second.pop_back();
        
        ofs << "SELECT "+fVarString+fvarAggPair.second+"\nFROM "+joinString;
        if (!fVarString.empty())
        {
            fVarString.pop_back();
            ofs << "\nGROUP BY "+fVarString;
        }
        ofs << ";\n\n";
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

        load += "\\COPY "+relName+" FROM \'../../"+_pathToData+"/"+relName+".tbl\' "+
                "DELIMITER \'|\' CSV;\n";
    }

    std::ofstream ofs("runtime/sql/load_data.sql", std::ofstream::out);
    ofs << drop + load;
    ofs.close();
    // DINFO(drop + load);

    string drop_views = "";

    for (size_t viewID = 0; viewID < _qc->numberOfViews(); ++viewID)
        drop_views +=  "DROP TABLE IF EXISTS view_"+std::to_string(viewID)+";\n";

    ofs.open("runtime/sql/lmfao_cleanup.sql", std::ofstream::out);
    ofs << drop_views;
    ofs.close();
    
    ofs.open("runtime/sql/drop_data.sql", std::ofstream::out);
    ofs << drop + drop_views;
    ofs.close();
    // DINFO(drop);
// #endif
}


void SqlGenerator::generateOutputQueries()
{
    ofstream ofs("runtime/sql/output.sql");
    
    for (size_t viewID = 0; viewID < _qc->numberOfViews(); ++viewID)
    {
        View* view = _qc->getView(viewID);
        std::string fields = "";
        for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
        {
            if (view->_fVars[var])
            {
                Attribute* attr = _td->getAttribute(var);
                fields += attr->_name+",";
            }
        }
        std::string orderby = "";
        if (!fields.empty())
        {
            orderby += " ORDER BY "+fields;
            orderby.pop_back();
        }

        for (size_t agg = 0; agg < view->_aggregates.size(); ++agg)
            fields += "CAST(agg_"+std::to_string(viewID)+"_"+std::to_string(agg)+
                " AS decimal(53,2)),";

        fields.pop_back();
        
        ofs << "\\COPY (SELECT "+fields+" FROM view_"+to_string(viewID)+orderby+
            ") TO PROGRAM \'cat - >> test.out\' CSV DELIMITER \'|\';\n\n";
    }
    ofs.close();
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
    if (!fvars.empty())
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
    // case Operation::lr_cont_parameter :
    //     return "("+fvars+"_param*"+fvars+")";
    // case Operation::lr_cat_parameter :
    //     return "("+fvars+"_param)";
    case Operation::lr_cont_parameter :
        return "(0.15*"+fvars+")";
    case Operation::lr_cat_parameter :
        return "(0.15)";
    default : return "f_"+to_string(fid);
    }

    return " ";
}



