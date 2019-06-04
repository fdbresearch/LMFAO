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
#endif

/* Compile time constant: can be used directly to enable loop unrolling and
 * other compiler optimisations. */
#ifdef VARIABLES
        const size_t NUM_OF_VARIABLES = VARIABLES;
/* Runtime variable: can be used across different compilation units; must be
 * defined in a source file. */
#else
        const size_t NUM_OF_VARIABLES = 100;
#endif

/* Compile time constant: can be used directly to enable loop unrolling and
 * other compiler optimisations. */
#ifdef PRODUCTS
        const size_t NUM_OF_PRODUCTS = PRODUCTS;
/* Runtime variable: can be used across different compilation units; must be
 * defined in a source file. */
#else
        const size_t NUM_OF_PRODUCTS = 200;
#endif
    }

    namespace config
    {
        extern std::string FEATURE_CONF;
        
        extern std::string TREEDECOMP_CONF;

        extern std::string SCHEMA_CONF;
    }

    namespace dir
    {
        extern std::string PATH_TO_DATA;

        extern std::string DATASET_NAME;

        extern std::string OUTPUT_DIRECTORY;

        extern std::string PATH_TO_FILES;
    }
    

    namespace cppgen
    {
        extern bool MULTI_OUTPUT;
        
        extern bool RESORT_RELATIONS;

        extern bool MICRO_BENCH;

        extern bool COMPRESS_AGGREGATES;

        extern bool BENCH_INDIVIDUAL;

        extern bool COLUMNAR_LAYOUT;


        enum PARALLELIZATION_TYPE
        {
            NO_PARALLELIZATION,
            TASK_PARALLELIZATION,
            DOMAIN_PARALLELIZATION,
            BOTH_PARALLELIZATION
        };
        
        extern PARALLELIZATION_TYPE PARALLEL_TYPE;
    }


    namespace application
    {
        extern const size_t K;

        extern const size_t DEGREE;
    }
    
    
}

#endif /* GLOBALPARAMS_HPP_ */
