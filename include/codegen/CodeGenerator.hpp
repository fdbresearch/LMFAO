//--------------------------------------------------------------------
//
// CodeGenerator.hpp
//
// Created on: 9 January 2018
// Author: Max
//
//--------------------------------------------------------------------


#ifndef INCLUDE_CODEGEN_CODEGENERATOR_HPP_
#define INCLUDE_CODEGEN_CODEGENERATOR_HPP_

// enum ParallelizationType 
// {
//     NO_PARALLELIZATION,
//     TASK_PARALLELIZATION,
//     DOMAIN_PARALLELIZATION,
//     BOTH_PARALLELIZATION
// };

/**
 * Abstract class to provide a uniform interface used to process the received data.
 */
class CodeGenerator
{

public:

    virtual ~CodeGenerator()
    {
    }
    
    /**
     * Runs the data processing task.
     */
    // virtual void generateCode() = 0;
    virtual void generateCode(bool, bool) = 0;

    // virtual size_t numberOfGroups() = 0;
    
};

#endif /* INCLUDE_CODEGEN_CODEGENERATOR_HPP_ */
