//--------------------------------------------------------------------
//
// DataHandlerCSV.cpp
//
// Created on: 11 May 2016
// Author: PY
//
//--------------------------------------------------------------------

#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <cassert>
#include <fstream>

#include <DataHandlerCSV.h>
#include <GlobalParams.hpp>
#include <Logging.hpp>

/* Constant chars used for parsing the config and data files. */
static const char ATTRIBUTE_SEPARATOR_CHAR = ',';
static const char COMMENT_CHAR = '#';
static const char TABLE_NAME_CHAR = ':';
static const char VALUE_SEPARATOR_CHAR = '|';

static const std::string SCHEMA_CONF = "/schema.conf";

namespace phoenix = boost::phoenix;
using namespace boost::spirit;
using namespace multifaq::params;
using namespace multifaq::types;
using namespace std;

DataHandlerCSV::DataHandlerCSV(const string& pathToFiles,
                               std::shared_ptr<Launcher> launcher) :
    DataHandler(pathToFiles)
{

    _treeDecomposition = launcher->getTreeDecomposition();
    
//     /* Load the database schema file into an input stream. */
//     ifstream input(_pathToFiles + SCHEMA_CONF);
//     if (!input)
//     {
//         ERROR (_pathToFiles + SCHEMA_CONF + " does is not exist. \n");
//         exit(1);
//     }
//     /* String and associated stream to receive lines from the file. */
//     string line;
//     stringstream ssLine;
//     size_t numOfTables = 0;
//     /* Count the number of tables in the database. */
//     while (getline(input, line))
//     {
//         if (line[0] == COMMENT_CHAR || line == "")
//             continue;
//         ++numOfTables;
//     }
//     /* Check consistency between compile time flag and runtime value issued from
//      * configuration file. */
// #ifdef TABLES
//     if(TABLES != numOfTables)
//     {
//         ERROR("Value of compiler flag -DTABLES and number of tables specified in schema.conf inconsistent. Aborting.\n")
//             exit(1);
//     }
//     /* Assign value to extern variable in GlobalParams.hpp. */
// #endif
//     /* Initialise arrays; each element will correspond to one database table. */
//     _tableNames = new string[NUM_OF_TABLES];
//     _attrIDs = new vector<uint_fast16_t> [NUM_OF_TABLES];
//     /* Reset input stream to beginning of file. */
//     input.clear();
//     input.seekg(0, ios::beg);
//     /* Variable defining the ID of each attribute. */
//     uint_fast16_t attributeID = 0;
//     size_t table = 0;
//     /* Scan through the input stream line by line. */
//     while (getline(input, line))
//     {
//         if (line[0] == COMMENT_CHAR || line == "")
//             continue;
//         ssLine << line;
//         string tableName;
//         /* Extract the name of the table in the current line. */
//         getline(ssLine, tableName, TABLE_NAME_CHAR);
//         _tableNames[table] = tableName;

//         string attrName;
//         /* Scan through the attributes in the current line. */
//         while (getline(ssLine, attrName, ATTRIBUTE_SEPARATOR_CHAR))
//         {
//             /* Attribute encountered for the first time in the schema: give it a
//              * new ID. */
//             if (_namesMapping.count(attrName) == 0)
//             {
//                 _namesMapping[attrName] = attributeID;
//                 _attrIDs[table].push_back(attributeID);
//                 ++attributeID;
//             }
//             /* Attribute already encountered: just add its ID to the vector of
//              * IDs for the current table. */
//             else
//             {
//                 _attrIDs[table].push_back(_namesMapping[attrName]);
//             }
//         }
//         ++table;
//         /* Clear string stream. */
//         ssLine.clear();
//     }
//     assert(
//         table == NUM_OF_TABLES
//         && "The same number of lines must be processed during the first and
//         the second pass over the schema file.");
}

DataHandlerCSV::~DataHandlerCSV()
{
    /* Clean the database tuples. We must not clean the _localDataToProcess
     * structure, as it contains tuples from the other structures. */
    // for (size_t table = 0; table < NUM_OF_TABLES; ++table)
    // {
    //     for (Tuple tuple : _database[table])
    //     {
    //         delete[] tuple;
    //     }

    //     for (size_t worker = 0; worker < NUM_OF_WORKERS; ++worker)
    //     {
    //         for (Tuple tuple : _receivedDataToProcess[worker][table])
    //         {
    //             delete[] tuple;
    //         }
    //     }
    // }

    // for (size_t worker = 0; worker < NUM_OF_WORKERS; ++worker)
    // {
    //     delete[] _receivedDataToProcess[worker];
    // }

    // /* Clean the database tables. */
    // delete[] _database;
    // delete[] _localDataToProcess;
    // delete[] _receivedDataToProcess;

    // /* Clean other structures. */
    // delete[] _attrIDs;
    // delete[] _tableNames;

    INFO("MAIN: database structures cleaned up; ready to shutdown.\n");
}

template <typename T>
void DataHandlerCSV::readFromFile(std::vector<T>& data, const std::string& path,
                                  size_t numOfAttributes)
{            
    data.clear();
    
    std::ifstream input(path);
    
    if (!input) {
        ERROR (path + " does is not exist. \n");
        exit(1);
    }

    /* String to receive lines (ie. tuples) from the file. */
    string line;

    vector<string> tmpData;
    tmpData.resize(numOfAttributes);
    
    std::stringstream lineStream;
    std::string cell;
    
    /* Scan through the tuples in the current table. */
    while (getline(input, line))
    {
        lineStream << line;

        // TODO: Push the below into the constructor of the Struct and use
        // create with a string (line); --> can be done with boost::spirit 
        
        while (std::getline(lineStream, cell, VALUE_SEPARATOR_CHAR))
        {
            tmpData.push_back(cell);
        }

        data.push_back(T(tmpData));

        tmpData.clear();
        lineStream.clear();   
    }
 
    input.close();
}


void DataHandlerCSV::loadAllTables()
{
    for (size_t table = 0; table < NUM_OF_TABLES; ++table)
    {
        // TODO : CALL readFromFile for EACH RELATION
        
        
    }
    
    INFO("MAIN: " + to_string(NUM_OF_TABLES) + " tables loaded into memory.\n");
}
