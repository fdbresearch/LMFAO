//--------------------------------------------------------------------
//
// DataTypes.hpp
//
// Created on: 12 Dec 2017
// Author: Max
//
//--------------------------------------------------------------------

#ifndef INCLUDE_DATATYPES_HPP_
#define INCLUDE_DATATYPES_HPP_

#include <vector>

namespace multifaq
{
/**
 * Contains type definitions for the data structures used in multiFAQ.
 */
    namespace types
    {
        //! Data type used for most operations.
        typedef double DataType;
    
        //! Data type used for database tuples.
        typedef DataType* Tuple;

        /** 
         * Data type used for database tables; size of the tables not known in
         * advance and can vary depending on tuples received by other nodes. 
         */
        typedef std::vector<Tuple> Table;

        //! Data type used to store all the tables.
        typedef Table* Database;
    }
}

#endif /* INCLUDE_DATATYPES_HPP_ */
