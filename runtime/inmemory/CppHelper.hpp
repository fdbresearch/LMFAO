//--------------------------------------------------------------------
//
// CppHelper.hpp
//
// Created on: 16 Dec 2017
// Author: Max
//
//--------------------------------------------------------------------


#ifndef INCLUDE_CPPHELPER_HPP_
#define INCLUDE_CPPHELPER_HPP_

#include <fstream>
#include <sstream>

#include "CompilerUtils.hpp"
//#include "Logging.hpp"

namespace lmfao
{
    template <typename T, typename V>
    HOT inline void findUpperBound(std::vector<T>& relation, const V T::*element,
                               const V max, size_t& lower_pointer,
                               const size_t& upper_pointer)
    {
        V min = relation[lower_pointer].*element;

        size_t leap = 1;
        while (lower_pointer <= upper_pointer && min < max)
        {
            lower_pointer += leap;
            
            if (lower_pointer < upper_pointer)
            {
                min = relation[lower_pointer].*element;
                leap *= 2;
            }
            else
            {
                lower_pointer = upper_pointer;
                break;
            }
        }

        /*
         * When we found an upper bound; to find the least upper bound;
         * use binary search to backtrack.
         */
        if (leap > 1 && max <= relation[lower_pointer].*element)
        {
            int high = lower_pointer, low = lower_pointer - leap, mid = 0;
            while (high > low && high != low)
            {
                mid = (high + low) / 2;
                if (max > relation[mid - 1].*element && max <= relation[mid].*element)
                {
                    lower_pointer = mid;
                    break;
                }
                else if (max <= relation[mid].*element)
                    high = mid - 1;
                else
                    low = mid + 1;
            }

            mid = (high + low) / 2;
            if (relation[mid - 1].*element >= max)
                mid -= 1;

            lower_pointer = mid;
        }
    }

    /**
     * Structure used for sorting relations (used in getRelationOrdering).
     */
/*    template <typename T>
    struct sortRelationOrder
    {
        inline bool operator()(const std::pair<T, int> &left,
                        const std::pair<T, int> &right)
        {
            return left.first < right.first;
        }
    }; */

    template <typename T>
    inline void readFromFile(std::vector<T>& data, const std::string& path,
                      char delimiter)
    {
        std::ifstream input(path);
    
        if (!input) {
            std::cerr << path + " does is not exist. \n";
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


    template <class T>
    HOT inline void hash_combine(std::size_t& seed, const T& v)
    {
        seed ^= std::hash<T>()(v) + 0x9e3779b9 + (seed<<6) + (seed>>2);
    }

}

#endif /*INCLUDE_CPPHELPER_HPP_*/
