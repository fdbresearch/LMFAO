//--------------------------------------------------------------------
//
// Application.hpp
//
// Created on: 11 December 2017
// Author: Max
//
//--------------------------------------------------------------------

#ifndef INCLUDE_APPLICATION_APPLICATION_HPP_
#define INCLUDE_APPLICATION_APPLICATION_HPP_

#include <memory>

// #include <DataHandler.hpp>
#include <CompilerUtils.hpp>
#include <GlobalParams.hpp>
#include <Logging.hpp>
#include <QueryCompiler.h>
#include <TDNode.hpp>
#include <TreeDecomposition.h>

/**
 * Abstract class to provide a uniform interface used to process the received data.
 */
class Application
{

public:

    virtual ~Application()
    {
    }
    
    /**
     * Runs the data processing task.
     */
    virtual void run() = 0;

    var_bitset getFeatures()
    {
        return _features;
    }
    
protected:

    var_bitset _features;
    
};

#endif /* INCLUDE_APPLICATION_APPLICATION_HPP_ */
