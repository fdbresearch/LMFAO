//--------------------------------------------------------------------
//
// DataHandler.hpp
//
// Created on: 11 May 2016
// Author: PY
//
//--------------------------------------------------------------------

#ifndef INCLUDE_DATAHANDLER_HPP_
#define INCLUDE_DATAHANDLER_HPP_

#include <string>
#include <unordered_map>
#include <vector>

#include <DataTypes.hpp>
//#include <TreeDecomposition.h>

/**
 * Abstract class to provide a uniform interface used to load the data in main
 * memory and to deal with the data to be processed by the engine.
 */
≈‚// class DataHandler
// {
// public:

//     virtual ~DataHandler()
//     {
//     }

//     /**
//      * Loads the tables into memory.
//      */
//     virtual void loadAllTables() = 0;

//     /**
//      * Returns the local database (ie. data loaded from disk).
//      */
//     virtual multifaq::types::Database getDatabase()
//     {
//         return _database;
//     }
    
// protected:

//     /**
//      * Superclass constructor; used to set _pathToFiles variable.
//      */
//     DataHandler(const std::string& pathToFiles) :
//         _pathToFiles(pathToFiles)
//     {
//     }

//     // std::shared_ptr<TreeDecomposition> _treeDecomposition;
    
//     //! Physical path to the schema and table files.
//     std::string _pathToFiles;

//     /** Represents the database, in other words the tuples that are stored on
//      * the disk of the local node. */
//     multifaq::types::Database _database;

// };

#endif /* INCLUDE_DATAHANDLER_HPP_ */
