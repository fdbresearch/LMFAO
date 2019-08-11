//--------------------------------------------------------------------
//
// CodegenUtils.hpp
//
// Created on: 5 Oct 2018
// Author: Max
//
//--------------------------------------------------------------------


#ifndef INCLUDE_UTILS_CODEGENUTILS_HPP_
#define INCLUDE_UTILS_CODEGENUTILS_HPP_

#include <QueryCompiler.h>

std::string typeToStr(Type t)
{
    switch(t)
    {
    case Type::Integer : return "int";
    case Type::Double : return "double";            
    case Type::Short : return "short";
    case Type::UInteger : return "size_t";
    default :
        ERROR("This type does not exist \n");
        exit(1);
    }
}

inline std::string offset(size_t off)
{
    return std::string(off*3,' ');
}

inline std::string indent(size_t off)
{
    return std::string(off*3,' ');
}

std::string getUpperPointer(
    const std::string rel_name, size_t depth,bool parallelize)
{
    if (depth == 0 && parallelize)
        return "upperptr";
    if (depth == 0)
        return rel_name+".size()-1";
    return "upperptr_"+rel_name+"["+std::to_string(depth-1)+"]";
}



#endif /* INCLUDE_UTILS_CODEGENUTILS_HPP_ */
