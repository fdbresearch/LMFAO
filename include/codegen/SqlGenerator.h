//--------------------------------------------------------------------
//
// SqlGenerator.h
//
// Created on: 9 January 2018
// Author: Max
//
//--------------------------------------------------------------------

#ifndef INCLUDE_CODEGEN_SQLGENERATOR_H_
#define INCLUDE_CODEGEN_SQLGENERATOR_H_

#include <CodeGenerator.hpp>
#include <CompilerUtils.hpp>
#include <Launcher.h>
#include <Logging.hpp>
#include <QueryCompiler.h>
#include <TreeDecomposition.h>

class SqlGenerator: public CodeGenerator
{
public:

    SqlGenerator(const std::string path,
                 std::shared_ptr<Launcher> launcher);
    
    ~SqlGenerator();

    void generateCode();

private:

    std::string _pathToData;
    
    std::shared_ptr<TreeDecomposition> _td;

    std::shared_ptr<QueryCompiler> _qc;

    std::shared_ptr<Application> _app;

    Model _model;
    
    void generateLoadQuery();

    void generateLmfaoQuery();

    void generateNaiveQueries();
    
    inline std::string getFunctionString(size_t fid);

    inline std::string typeToStr(Type type);
};

#endif /* INCLUDE_SQLGENERATOR_CODEGENERATOR_H_ */
