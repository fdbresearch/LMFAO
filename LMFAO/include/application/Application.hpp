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

#include <CompilerUtils.hpp>
#include <GlobalParams.hpp>
#include <Logging.hpp>
#include <QueryCompiler.h>
#include <TDNode.hpp>
#include <TreeDecomposition.h>

struct Feature
{
    var_bitset head;
    size_t body[multifaq::params::NUM_OF_VARIABLES] = {};
};


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

    virtual void generateCode(const std::string& outputDirectory) = 0;
    
protected:

    var_bitset _features;
    var_bitset _isFeature;
    var_bitset _isCategoricalFeature;

    std::vector<Feature> _listOfFeatures;

    inline std::string offset(size_t off)
    {
        return std::string(off*3, ' ');
    }

    std::string typeToStr(Type t)
    {
        switch(t)
        {
        case Type::Integer : return "int";
        case Type::Double : return "double";            
        case Type::Short : return "short";
        case Type::U_Integer : return "size_t";
        default :
            ERROR("This type does not exist \n");
            exit(1);
        }
    }
    
};

#endif /* INCLUDE_APPLICATION_APPLICATION_HPP_ */
