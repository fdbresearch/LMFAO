//--------------------------------------------------------------------
//
// DataHandlerCSV.h
//
// Created on: 11 May 2016
// Author: PY
//
//--------------------------------------------------------------------

#ifndef INCLUDE_DATAHANDLERCSV_H_
#define INCLUDE_DATAHANDLERCSV_H_

#include <DataHandler.hpp>
#include <Launcher.h>

#include<ExampleTemplate.hpp>

/**
 * Class that enables to build the in-memory representation of the database using CSV files, and to deal with the data to be processed by the engine.
 */
class DataHandlerCSV: public DataHandler
{

public:
    /**
     * Constructor; need to provide path to database files.
     */
    DataHandlerCSV(const std::string& pathToFiles, std::shared_ptr<Launcher> launcher);

    ~DataHandlerCSV();

    /**
     * Loads the tables from CSV files into memory.
     */
    void loadAllTables();
    
private:
    
    template <typename T>
    void readFromFile(std::vector<T>& data, const std::string& file,
                      size_t numOfAttributes);

};

#endif /* INCLUDE_DATAHANDLERCSV_H_ */
