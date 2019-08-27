//--------------------------------------------------------------------
//
// Count.cpp
//
// Created on: Feb 1, 2018
// Author: Max
//
//--------------------------------------------------------------------

#include <fstream>

#include <Launcher.h>
#include <Count.h>

using namespace std;

Count::Count(shared_ptr<Launcher> launcher) 
{
    _compiler = launcher->getCompiler();
    _td = launcher->getTreeDecomposition();
}

Count::~Count()
{
}

void Count::run()
{
    modelToQueries();
    _compiler->compile();
}

void Count::generateCode()
{
    std::string viewID = std::to_string(countQuery->_incoming[0].first);

    std::string runFunction = offset(1)+"void runApplication()\n"+
        offset(1)+"{\n"+offset(2)+"std::cout << std::fixed << \"The count is: \" << "+
        "long(V"+viewID+"[0].aggregates[0]) << std::endl;\n"+
        offset(1)+"}\n";

    std::ofstream ofs(multifaq::dir::OUTPUT_DIRECTORY+"ApplicationHandler.h", std::ofstream::out);
    ofs << "#ifndef INCLUDE_APPLICATIONHANDLER_HPP_\n"<<
        "#define INCLUDE_APPLICATIONHANDLER_HPP_\n\n"<<
        "#include \"DataHandler.h\"\n\n"<<
        "namespace lmfao\n{\n"<<
        offset(1)+"void runApplication();\n"<<
        "}\n\n#endif /* INCLUDE_APPLICATIONHANDLER_HPP_*/\n";    
    ofs.close();

    ofs.open(multifaq::dir::OUTPUT_DIRECTORY+"ApplicationHandler.cpp",
             std::ofstream::out);
    ofs << "#include \"ApplicationHandler.h\"\n"
        << "namespace lmfao\n{\n";
    ofs << runFunction << std::endl;
    ofs << "}\n";
    ofs.close();
}

void Count::modelToQueries()
{
    // Create a query & Aggregate
    countQuery = new Query();
    countQuery->_root = _td->_root;
    
    Aggregate* agg = new Aggregate();

    prod_bitset product;
    agg->_sum.push_back(product);
    
    countQuery->_aggregates.push_back(agg);
    _compiler->addQuery(countQuery);
}
