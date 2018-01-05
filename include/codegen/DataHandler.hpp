//--------------------------------------------------------------------
//
// DataHandler.hpp
//
// Created on: 16 Dec 2017
// Author: Max
//
//--------------------------------------------------------------------

#ifndef INCLUDE_DATAHANDLER_HPP_
#define INCLUDE_DATAHANDLER_HPP_

#include <fstream>
#include <sstream>

namespace multifaq
{
    template <typename T>
    void readFromFile(std::vector<T>& data, const std::string& path,
                      char delimiter)
    {
        std::ifstream input(path);
    
        if (!input) {
            ERROR (path + " does is not exist. \n");
            exit(1);
        }
        
        /* String to receive lines (ie. tuples) from the file. */
        std::string line;

        std::vector<std::string> tmpData;
        std::stringstream lineStream;
        std::string cell;
    
        /* Scan through the tuples in the current table. */
        while (getline(input, line))
        {
            lineStream << line;

            // TODO: Push the below into the constructor of the Struct and use
            // create with a string (line); --> can be done with boost::spirit
            while (std::getline(lineStream, cell, delimiter))
            {
                tmpData.push_back(cell);
            }

            data.push_back(T(tmpData));

            tmpData.clear();
            lineStream.clear();
        }
 
        input.close();
    }
}

#endif /* INCLUDE_DATAHANDLER_HPP*/
