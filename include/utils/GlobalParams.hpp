//--------------------------------------------------------------------
//
// GlobalParams.hpp
//
// Created on: 21 Nov 2017
// Author: Max
//
//--------------------------------------------------------------------

#ifndef GLOBALPARAMS_HPP_
#define GLOBALPARAMS_HPP_

namespace multifaq
{
    namespace params
    {

/* Compile time constant: can be used directly to enable loop unrolling and
 * other compiler optimisations. */
#ifdef FUNCTIONS
        const size_t NUM_OF_FUNCTIONS = FUNCTIONS;
/* Runtime variable: can be used across different compilation units; must be
 * defined in a source file. */
#else
        const size_t NUM_OF_FUNCTIONS = 1500;
        // const size_t NUM_OF_FUNCTIONS = 15;
#endif

/* Compile time constant: can be used directly to enable loop unrolling and
 * other compiler optimisations. */
#ifdef VARIABLES
        const size_t NUM_OF_VARIABLES = VARIABLES;
/* Runtime variable: can be used across different compilation units; must be
 * defined in a source file. */
#else
        const size_t NUM_OF_VARIABLES = 50;
#endif

/* Compile time constant: can be used directly to enable loop unrolling and
 * other compiler optimisations. */
#ifdef PRODUCTS
        const size_t NUM_OF_PRODUCTS = PRODUCTS;
/* Runtime variable: can be used across different compilation units; must be
 * defined in a source file. */
#else
        const size_t NUM_OF_PRODUCTS = 200;
        // const size_t NUM_OF_PRODUCTS = 20;
#endif

/* Compile time constant: can be used directly to enable loop unrolling and
 * other compiler optimisations. */
#ifdef TABLES
        const size_t NUM_OF_TABLES = TABLES;
/* Runtime variable: can be used across different compilation units; must be
 * defined in a source file. */
#else
        const size_t NUM_OF_TABLES = 10;
#endif

        extern std::string FEATURE_CONF;
        
        extern std::string TREEDECOMP_CONF;

    }
}

#endif /* GLOBALPARAMS_HPP_ */
