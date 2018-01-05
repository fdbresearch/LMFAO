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

namespace multifaq
{
    template <typename T, typename V>
    inline void findUpperBound(std::vector<T>& relation, const V T::*element,
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
                std::cout << min << std::endl;
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
    template <typename T>
    struct sort_pred
    {
        bool operator()(const std::pair<T, int> &left,
                        const std::pair<T, int> &right)
        {
            return left.first < right.first;
        }
    };
}

#endif /*INCLUDE_CPPHELPER_HPP_*/
