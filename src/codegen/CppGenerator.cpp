//--------------------------------------------------------------------
//
// CppGenerator.cpp
//
// Created on: 30 July 2019
// Author: Max
//
//--------------------------------------------------------------------

#include <fstream>
#include <string>

#include <CodegenUtils.hpp>
#include <CppGenerator.h>

const size_t LOOPIFY_THRESHOLD = 2;

using namespace multifaq::params;
using namespace multifaq::dir;
using namespace multifaq::cppgen;

struct AggregateIndexes
{
    std::bitset<7> present;
    size_t indexes[8];

    void reset()
    {
        present.reset();
        memset(&indexes[0], 0, 8*sizeof(size_t));
    }

    bool isIncrement(const AggregateIndexes& bench, const size_t& offset,
                     std::bitset<7>& increasing)
    {
        if (present != bench.present)
            return false;
        
        // If offset is zero we didn't compare any two tuples yet
        if (offset==0)
        {
            // compare this and bench
            increasing.reset();
            for (size_t i = 0; i < 7; i++)
            {
                if (present[i])
                {
                    if (indexes[i] == bench.indexes[i] + 1)
                    {
                        increasing.set(i);
                    }
                    else if (indexes[i] != bench.indexes[i])
                    {
                        return false;
                    }
                }
            }
            return true;
        }
        
        for (size_t i = 0; i < 7; i++)
        {
            if (present[i])
            {
                if (increasing[i])
                {
                    if (indexes[i] != bench.indexes[i] + offset + 1)
                        return false;
                }
                else
                {
                    if (indexes[i] != bench.indexes[i])
                        return false;
                }
            }
        }
        return true;
    }

    void setIndexes(const LoopRegister* loopReg, const LoopAggregate* loopAgg)
    {
        present.reset();
        memset(&indexes[0], 0, 8*sizeof(size_t));
        
        if (loopAgg->prevProduct != nullptr)
        {
            present.set(0);
            indexes[0] = loopAgg->prevProduct->aggregateIndex;
        }

        if (loopAgg->functionProduct > -1)
        {
            present.set(1);
            indexes[1] = loopReg->localProducts[loopAgg->functionProduct].aggregateIndex;
        }
            
        if (loopAgg->viewProduct > -1)
        {
            // TODO: check if this loop aggregate can be inlined!!
            // if (regTuple.singleViewAgg)
            // {
            //    returnString += "aggregates_"+viewName[regTuple.viewAgg.first]+"["+
            //         std::to_string(regTuple.viewAgg.second)+"]*";
            // }
            present.set(2);

            indexes[2] = loopReg->localViewAggregateProducts
                [loopAgg->viewProduct].aggregateIndex;
        }

        if (loopAgg->multiplyByCount)
        {
            present.set(5);
            indexes[5] = loopReg->localProducts[0].aggregateIndex;
        }
            
        // if (loopAgg->innerProduct != nullptr)
        // {
        //     present.set(6);
        //     indexes[6] = loopAgg->innerProduct->aggregateIndex;            
        // }

        indexes[7] = loopAgg->aggregateIndex;
    }

    void setIndexes(const RunningSumAggregate* aggregate)
    {
        present.reset();
        memset(&indexes[0], 0, 8*sizeof(size_t));

        std::cout << "++++++++ RunningSumAgg: " << aggregate->local << "  "
                  << aggregate->post << std::endl;
        
        if (aggregate->local != nullptr)
        {
            const FinalLoopAggregate* correspondingLoopAgg =
                aggregate->local->correspondingLoop.second;        
        
            present.set(0);
            indexes[0] = correspondingLoopAgg->aggregateIndex;
        }
        
        if (aggregate->post != nullptr)
        {
            present.set(1);
            indexes[1] = aggregate->post->aggregateIndex;
        }

        indexes[7] = aggregate->aggregateIndex;
    }


    void setIndexes(const FinalLoopAggregate* aggregate)
    {
        present.reset();
        memset(&indexes[0], 0, 8*sizeof(size_t));

        if (aggregate->loopAggregate != nullptr)
        {
            present.set(0);
            indexes[0] = aggregate->loopAggregate->aggregateIndex;
        }

        if (aggregate->previousProduct != nullptr)
        {
            present.set(1);
            indexes[1] = aggregate->previousProduct->aggregateIndex;
        }

        if (aggregate->innerProduct != nullptr)
        {
            present.set(2);
            indexes[2] = aggregate->innerProduct->aggregateIndex;
        }

        if (aggregate->previousRunSum != nullptr)
        {
            if (aggregate->previousRunSum->local != nullptr)
            {
                const FinalLoopAggregate* correspondingLoopAgg =
                    aggregate->previousRunSum->local->correspondingLoop.second;

                present.set(3);
                indexes[3] = correspondingLoopAgg->aggregateIndex;
            }

            if (aggregate->previousRunSum->post != nullptr)
            {
                present.set(4);
                indexes[4] = aggregate->previousRunSum->post->aggregateIndex;
            }
        }
        
        indexes[7] = aggregate->aggregateIndex;
    }
};




std::string genLoopAggregateString(
    AggregateIndexes& idx, size_t numOfAggs, const std::bitset<7>& increasing,
    size_t stringIndent)
{  
    std::string outString = "";
    
    if (numOfAggs+1 > LOOPIFY_THRESHOLD)
    {
        outString += indent(stringIndent)+
            "for (size_t i = 0; i < "+std::to_string(numOfAggs+1)+";++i)\n";

        outString += indent(stringIndent+1)+
            "aggregateRegister["+std::to_string(idx.indexes[7])+"+i] = ";

        if (idx.present[0])
        {
            std::string k = std::to_string(idx.indexes[0]);
            if (increasing[0])
                k += "+i";
            outString +="aggregateRegister["+k+"]*";
        }

        if (idx.present[1])
        {
            std::string k = std::to_string(idx.indexes[1]);
            if (increasing[1])
                k += "+i";
            outString +="localRegister["+k+"]*";
        }

        if (idx.present[2])
        {
            std::string k = std::to_string(idx.indexes[2]);
            if (increasing[2])
                k += "+i";
            outString +="localRegister["+k+"]*";
        }
        if (idx.present[3])
        {
            // TODO: CHECK THAT index 4 is also present
            if (increasing[3])
            {
                ERROR("VIEW ID SHOULD NEVER INCREASE!!! \n");
                exit(1);
            }
            
            std::string aggID = std::to_string(idx.indexes[4]);
            if (increasing[4])
                aggID += "+i";

            outString += "aggregates_V"+std::to_string(idx.indexes[3])+"["+aggID+"]*";
        }

        if (idx.present[5])
        {
            std::string k = std::to_string(idx.indexes[5]);
            outString += "localRegister["+k+"]*";
            // outString +=  "count*";
        }
        

        if (idx.present[6])
        {
            std::string k = std::to_string(idx.indexes[6]);            
            if (increasing[6])
                k += "+i";
            outString += "aggregateRegister["+k+"]*";
        }

        outString.pop_back();
        outString += ";\n";
    }
    else
    {
        // Print out all aggregates individually
        for (size_t i = 0; i < numOfAggs+1; ++i)
        {
            // We use a mapping to the correct index
            outString += indent(stringIndent)+
                "aggregateRegister["+std::to_string(idx.indexes[7]+i)+"] = ";

            if (idx.present[0])
            {
                size_t k = idx.indexes[0];
                if (increasing[0])
                    k += i;
                outString +="aggregateRegister["+std::to_string(k)+"]*";
            }

            if (idx.present[1])
            {
                size_t k = idx.indexes[1];
                if (increasing[1])
                    k += i;
                outString +="localRegister["+std::to_string(k)+"]*";
            }

            if (idx.present[2])
            {
                size_t k = idx.indexes[2];
                if (increasing[2])
                    k += i;
                outString +="localRegister["+std::to_string(k)+"]*";
            }
            if (idx.present[3])
            {
                // TODO: CHECK THAT index 4 is also present
                if (increasing[3])
                {
                    ERROR("VIEW ID SHOULD NEVER INCREASE!!! \n");
                    exit(1);
                }

                size_t aggID = idx.indexes[4];
                if (increasing[4])
                    aggID += i;

                outString += "aggregates_V"+std::to_string(idx.indexes[3])+
                    "["+std::to_string(aggID)+"]*";
            }

            if (idx.present[5])
            {
                std::string k = std::to_string(idx.indexes[5]);
                outString += "localRegister["+k+"]*";
                // outString +=  "count*";
            }

        
            if (idx.present[6])
            {
                size_t k = idx.indexes[6];
                if (increasing[6])
                    k += i;
                outString += "aggregateRegister["+std::to_string(k)+"]*";
            }

            outString.pop_back();
            outString += ";\n";
        }
    }

    return outString;
}


std::string genLoopRunSumString(
    AggregateIndexes& idx, size_t numOfAggs, const std::bitset<7>& increasing,
    size_t stringIndent, std::string outputArray = "aggregateRegister")
{  
    std::string outString = "";
    
    if (numOfAggs+1 > LOOPIFY_THRESHOLD)
    {
        outString += indent(stringIndent)+
            "for (size_t i = 0; i < "+std::to_string(numOfAggs+1)+";++i)\n";

        outString += indent(stringIndent+1)+outputArray+
            "["+std::to_string(idx.indexes[7])+"+i] += ";

        if (idx.present[0])
        {
            std::string k = std::to_string(idx.indexes[0]);
            if (increasing[0])
                k += "+i";
            outString +="aggregateRegister["+k+"]*";
        }

        if (idx.present[1])
        {
            std::string k = std::to_string(idx.indexes[1]);
            if (increasing[1])
                k += "+i";
            outString +="aggregateRegister["+k+"]*";
        }

        if (idx.present[2])
        {
            std::string k = std::to_string(idx.indexes[2]);
            if (increasing[2])
                k += "+i";
            outString +="aggregateRegister["+k+"]*";
        }

        if (idx.present[3])
        {
            std::string k = std::to_string(idx.indexes[3]);
            if (increasing[3])
                k += "+i";
            outString +="aggregateRegister["+k+"]*";
        }

        if (idx.present[4])
        {
            std::string k = std::to_string(idx.indexes[4]);
            if (increasing[4])
                k += "+i";
            outString +="runningSum["+k+"]*";
        }

        outString.pop_back();
        outString += ";\n";
    }
    else
    {
        // Print out all aggregates individually
        for (size_t i = 0; i < numOfAggs+1; ++i)
        {
            // We use a mapping to the correct index
            outString += indent(stringIndent)+
                "aggregateRegister["+std::to_string(idx.indexes[7]+i)+"] += ";

            if (idx.present[0])
            {
                size_t k = idx.indexes[0];
                if (increasing[0])
                    k += i;
                outString +="aggregateRegister["+std::to_string(k)+"]*";
            }

            if (idx.present[1])
            {
                size_t k = idx.indexes[1];
                if (increasing[1])
                    k += i;
                outString +="aggregateRegister["+std::to_string(k)+"]*";
            }

            if (idx.present[2])
            {
                size_t k = idx.indexes[2];
                if (increasing[2])
                    k += i;
                outString +="aggregateRegister["+std::to_string(k)+"]*";
            }
            
            if (idx.present[3])
            {
                size_t k = idx.indexes[3];
                if (increasing[3])
                    k += i;
                outString +="aggregateRegister["+std::to_string(k)+"]*";
            }
            
            if (idx.present[4])
            {
                size_t k = idx.indexes[4];
                if (increasing[4])
                    k += i;
                outString += "runningSum["+std::to_string(k)+"]*";
            }

            outString.pop_back();
            outString += ";\n";
        }
    }

    return outString;
}





std::string genRunSumAggregateString(
    AggregateIndexes& idx, size_t numOfAggs, const std::bitset<7>& increasing,
    size_t stringIndent)
{  
    std::string outString = "";

    if (numOfAggs+1 > LOOPIFY_THRESHOLD)
    {
        outString += indent(stringIndent)+
            "for (size_t i = 0; i < "+std::to_string(numOfAggs+1)+";++i)\n";
        outString += indent(stringIndent+1)+
            "runningSum["+std::to_string(idx.indexes[7])+"+i] += ";

        if (idx.present[0])
        {
            std::string k = std::to_string(idx.indexes[0]);
            if (increasing[0])
                k += "+i";
            outString +="aggregateRegister["+k+"]*";
        }

        if (idx.present[1])
        {
            std::string k = std::to_string(idx.indexes[1]);
            if (increasing[1])
                k += "+i";
            outString +="runningSum["+k+"]*";
        }
        
        outString.pop_back();
        outString += ";\n";

        std::cout << outString << "  " << idx.present << std::endl;
    }
    else
    {
        // Print out all aggregates individually
        for (size_t i = 0; i < numOfAggs+1; ++i)
        {
            // We use a mapping to the correct index
            outString += indent(stringIndent)+
                "runningSum["+std::to_string(idx.indexes[7]+i)+"] += ";
            
            if (idx.present[0])
            {
                size_t k = idx.indexes[0];
                if (increasing[0])
                    k += i;
                outString +="aggregateRegister["+std::to_string(k)+"]*";
            }
            
            if (idx.present[1])
            {
                size_t k = idx.indexes[1];
                if (increasing[1])
                    k += i;
                outString +="runningSum["+std::to_string(k)+"]*";
            }
            
            outString.pop_back();
            outString += ";\n";
        }
    }

    return outString;
}

// TODO: we should perhaps keep the function string locally for each Function! 
std::string getFunctionString(Function* f, std::string& fvars)
{
    switch (f->_operation)
    {
        // case Operation::count :
        //     return "f_"+to_string(fid);
    case Operation::sum :
        return fvars;
    case Operation::linear_sum :
        return fvars;
    case Operation::quadratic_sum :
        return fvars+"*"+fvars;
    case Operation::cubic_sum :
        return fvars+"*"+fvars+"*"+fvars;
    case Operation::quartic_sum :
        return fvars+"*"+fvars+"*"+fvars+"*"+fvars;
    case Operation::lr_cont_parameter :
        return "(0.15*"+fvars+")";
    case Operation::lr_cat_parameter :
        return "(0.15)";  // "_param["+fvars+"]"; // // TODO: THIS NEEDS TO BE A
                          // CATEGORICAL PARAMETER - Index?!
    case Operation::indicator_eq : 
        return "("+fvars+" == "+std::to_string(f->_parameter[0])+")"; // " ? 1 : 0)";
    case Operation::indicator_lt : 
        return "("+fvars+" <= "+std::to_string(f->_parameter[0])+")"; // " ? 1 : 0)";
    case Operation::indicator_neq : 
        return "("+fvars+" != "+std::to_string(f->_parameter[0])+")"; // " ? 1 : 0)";
    case Operation::indicator_gt : 
        return "("+fvars+" > "+std::to_string(f->_parameter[0])+")";  // " ? 1 : 0)";
    case Operation::dynamic : 
        return  f->_name+"("+fvars+")";
    default : return "f("+fvars+")";
    }
}

std::string genPointerArrays(const std::string& rel,
                                           std::string& numOfJoinVars,
                                           bool parallelizeRelation)
{
    if (parallelizeRelation)
        return indent(2)+"size_t lowerptr_"+rel+"["+numOfJoinVars+"] = {}, "+
            "upperptr_"+rel+"["+numOfJoinVars+"] = {}; \n"+
            indent(2)+"upperptr_"+rel+"[0] = upperptr;\n"+
            indent(2)+"lowerptr_"+rel+"[0] = lowerptr;\n";
    else
        return indent(2)+"size_t lowerptr_"+rel+"["+numOfJoinVars+"] = {}, "+
            "upperptr_"+rel+"["+numOfJoinVars+"] = {}; \n"+
            indent(2)+"upperptr_"+rel+"[0] = "+rel+".size()-1;\n";
}








CppGenerator::CppGenerator(std::shared_ptr<Launcher> launcher)
{
    _db = launcher->getDatabase();
    _td = launcher->getTreeDecomposition();
    _qc = launcher->getCompiler();
}

CppGenerator::~CppGenerator()
{
    delete[] viewName;
}


void CppGenerator::generateCode(bool hasApplicationHandler,
                                bool hasDynamicFunctions)
{
    DINFO("Start generate C++ code. \n");
    
    viewName = new std::string[_qc->numberOfViews()];
    
    for (size_t view = 0; view < _qc->numberOfViews(); ++view)
        viewName[view] = "V"+std::to_string(view);
    
    generateDataHandler();

    generateMainFile(hasApplicationHandler);
    
    generateMakeFile(hasApplicationHandler, hasDynamicFunctions);
            
    for (size_t group = 0; group < _qc->numberOfViewGroups(); ++group)
    {
        std::cout << "GROUP : " <<  group << std::endl;
        generateViewGroupFiles(group, hasDynamicFunctions);
    }
    

}


void CppGenerator::generateMainFile(bool hasApplicationHandler)
{
    bool parallelize = PARALLEL_TYPE == TASK_PARALLELIZATION ||
        PARALLEL_TYPE == BOTH_PARALLELIZATION;

    std::ofstream ofs(OUTPUT_DIRECTORY+"main.cpp",std::ofstream::out);

    ofs << "#ifdef TESTING \n"+indent(1)+"#include <iomanip>\n"+"#endif\n";
    
    ofs << "#include \"DataHandler.h\"\n";
    for (size_t group = 0; group < _qc->numberOfViewGroups(); ++group)
        ofs << "#include \"ComputeGroup"+std::to_string(group)+".h\"\n";

    if (hasApplicationHandler)
        ofs << "#include \"ApplicationHandler.h\"\n";
    
    ofs << "\nnamespace lmfao\n{\n";    
    // indent(1)+"//const std::std::ring PATH_TO_DATA = \"/Users/Maximilian/Documents/"+
    // "Oxford/LMFAO/"+_pathToData+"\";\n"+
    // indent(1)+"const std::string PATH_TO_DATA = \"../../"+_pathToData+"\";\n\n";

    for (size_t relID = 0; relID < _db->numberOfRelations(); ++relID)
    {
        const std::string& relName = _db->getRelation(relID)->_name;
        ofs << indent(1)+"std::vector<"+relName+"_tuple> "+relName+";" << std::endl;
    }
    
    for (size_t viewID = 0; viewID < _qc->numberOfViews(); ++viewID)
    {
        ofs << indent(1)+"std::vector<"+viewName[viewID]+"_tuple> "+
            viewName[viewID]+";" << std::endl;
    }
    
    ofs << genLoadRelationFunction() << std::endl;

    for (size_t relID = 0; relID < _db->numberOfRelations(); ++relID)
        ofs << genSortFunction(relID) << std::endl;
    
    ofs << genRunFunction(parallelize, hasApplicationHandler) << std::endl;
    ofs << genTestFunction() << genDumpFunction() << "}\n\n";
 
    ofs << "int main(int argc, char *argv[])\n{\n"+
        indent(1)+"std::cout << \"run lmfao\" << std::endl;\n"+
        indent(1)+"lmfao::run();\n"+
        "#ifdef TESTING\n"+indent(1)+"lmfao::ouputViewsForTesting();\n#endif\n"+
        "#ifdef DUMP_OUTPUT\n"+indent(1)+"lmfao::dumpOutputViews();\n#endif\n"+
        indent(1)+"return 0;\n};\n";
    ofs.close();   
}


void CppGenerator::generateMakeFile(bool hasApplicationHandler, bool hasDynamicFunctions)
{
    std::string objectList = "main.o datahandler.o ";
    if (hasApplicationHandler)
        objectList += "application.o ";
    if (hasDynamicFunctions)
        objectList += "dynamic.o ";
    
    std::string objectConstruct = "";
    for (size_t group = 0; group < _qc->numberOfViewGroups(); ++group)
    {
        std::string groupString = std::to_string(group);
        objectList += "computegroup"+groupString+".o ";
        objectConstruct += "computegroup"+groupString+".o : ComputeGroup"+
            groupString+".cpp\n\t$(CXX) $(FLAG) $(CXXFLAG) -c ComputeGroup"+
            groupString+".cpp -o computegroup"+groupString+".o\n\n";
    }
    
    std::ofstream ofs(multifaq::dir::OUTPUT_DIRECTORY+"Makefile",std::ofstream::out);

    ofs << "## You can set costum flags by adding: FLAG=-D... \n\n";
    ofs << "CXXFLAG  += -std=c++11 -O3 -pthread -mtune=native -ftree-vectorize";

#if defined(__GNUC__) && defined(NDEBUG) && !defined(__clang__)
    ofs << " -fopenmp";
#endif

    ofs << "\n# -pthread -O2 -mtune=native -fassociative-math -freciprocal-math "
        << "-fno-signed-zeros -v -ftime-report -fno-stack-protector\n\n";

    ofs << "lmfao : "+objectList+"\n\t$(CXX) $(CXXFLAG) "+objectList+"-o lmfao\n\n";

    ofs << "main.o : main.cpp\n"
        << "\t$(CXX) $(FLAG) $(CXXFLAG) -c main.cpp -o main.o\n\n"
        << "datahandler.o : DataHandler.cpp\n"
        << "\t$(CXX) $(FLAG) $(CXXFLAG) -c DataHandler.cpp -o datahandler.o\n\n";
    if (hasApplicationHandler)
        ofs << "application.o : ApplicationHandler.cpp\n"
            << "\t$(CXX) $(FLAG) $(CXXFLAG) -c ApplicationHandler.cpp "
            << "-o application.o\n\n";
    if (hasDynamicFunctions)
        ofs << "dynamic.o : DynamicFunctions.cpp\n"
            << "\t$(CXX) $(FLAG) $(CXXFLAG) -c DynamicFunctions.cpp "
            << "-o dynamic.o\n\n";

    ofs << objectConstruct;

    ofs << ".PHONY : multithread\n"
        << "multi : FLAG = -DMULTITHREAD\n"
        << "multi : lmfao\n\n"
        << ".PHONY : precompilation\n"
        << "precomp : main.o\n"
        << "precomp : datahandler.o\n";
    if (hasApplicationHandler)
         ofs << "precomp : application.o\n";
    ofs << "\n.PHONY : test\n"
        << "test : FLAG = -DTESTING\n"
        << "test : lmfao\n\n"
        << ".PHONY : dump\n"
        << "dump : FLAG = -DDUMP_OUTPUT\n"
        << "dump : lmfao\n\n"
        << ".PHONY : debug\n"
        << "debug : CXXFLAG = -std=c++11 -g";

#if defined(__GNUC__) && defined(NDEBUG) && !defined(__clang__)
    ofs << " -fopenmp";
#endif
    
    ofs << "\ndebug : lmfao\n\n"
        << ".PHONY : clean\n"
        << "clean :\n"
	<< "\trm *.o lmfao";

    ofs.close();
}


void CppGenerator::generateDataHandler()
{
    std::string header = std::string("#ifndef INCLUDE_DATAHANDLER_HPP_\n")+
        "#define INCLUDE_DATAHANDLER_HPP_\n\n"+
        "#include <algorithm>\n"+
        "#include <chrono>\n"+
        "#include <cstring>\n"+
        "#include <fstream>\n"+
        "#include <iostream>\n" +
        "#include <thread>\n" +
        "#include <unordered_map>\n" +
        "#include <vector>\n\n";

#if defined(__GNUC__) && defined(NDEBUG) && !defined(__clang__)
    header += "#include <parallel/algorithm>\n";
#endif

    if (MICRO_BENCH)
    {
        header += "#define INIT_MICRO_BENCH(timer)\\\n"+indent(1)+
            "auto begin_##timer = std::chrono::high_resolution_clock::now();\\\n"+
            indent(1)+
            "auto finish_##timer = std::chrono::high_resolution_clock::now();\\\n"+
            indent(1)+"std::chrono::duration<double>  elapsed_##timer = "+
            "finish_##timer - begin_##timer;\\\n"+
            indent(1)+"double time_spent_##timer = elapsed_##timer.count();\\\n"+
            indent(1)+"double global_time_##timer = 0;\n";

        header += std::string("#define BEGIN_MICRO_BENCH(timer) begin_##timer = ")+
            "std::chrono::high_resolution_clock::now();\n";
        header += std::string("#define END_MICRO_BENCH(timer) finish_##timer = ")+
            "std::chrono::high_resolution_clock::now();\\\n"+
            indent(1)+"elapsed_##timer = finish_##timer - begin_##timer;\\\n"+
            indent(1)+"time_spent_##timer = elapsed_##timer.count();\\\n"+
            indent(1)+"global_time_##timer += time_spent_##timer;\n";

        header += std::string("#define PRINT_MICRO_BENCH(timer) std::cout << ")+
            "#timer << \": \" << std::to_string(global_time_##timer) << std::endl;\n\n";
    }
    
    header += "using namespace std::chrono;\n\nnamespace lmfao\n{\n"+
        indent(1)+"const std::string PATH_TO_DATA = \""+PATH_TO_DATA+"\";\n\n";

    std::ofstream ofs(multifaq::dir::OUTPUT_DIRECTORY+"DataHandler.h", std::ofstream::out);

    ofs << header << genTupleStructs() << genCaseIdentifiers() << std::endl;

    for (size_t relID = 0; relID < _db->numberOfRelations(); ++relID)
        ofs << indent(1)+"extern void sort"+_db->getRelation(relID)->_name+"();\n";
    
    ofs <<  "}\n\n#endif /* INCLUDE_DATAHANDLER_HPP_*/\n";    
    ofs.close();

    ofs.open(multifaq::dir::OUTPUT_DIRECTORY+"DataHandler.cpp", std::ofstream::out);
    ofs << genTupleStructConstructors() << std::endl;
    ofs.close();
}


void CppGenerator::generateViewGroupFiles(size_t group, bool hasDynamicFunctions)
{
    std::ofstream ofs(multifaq::dir::OUTPUT_DIRECTORY+"ComputeGroup"+std::to_string(group)+".h",
                      std::ofstream::out);

    ofs << "#ifndef INCLUDE_COMPUTEGROUP"+std::to_string(group)+"_HPP_\n"+
        "#define INCLUDE_COMPUTEGROUP"+std::to_string(group)+"_HPP_\n\n"+
        "#include \"DataHandler.h\"\n\nnamespace lmfao\n{\n"+
        indent(1)+"void computeGroup"+std::to_string(group)+"();\n"; 
    
    ofs << "}\n\n#endif /* INCLUDE_COMPUTEGROUP"+std::to_string(group)+"_HPP_*/\n";
    ofs.close();
        
    ofs.open(multifaq::dir::OUTPUT_DIRECTORY+"ComputeGroup"+std::to_string(group)+".cpp",
             std::ofstream::out);

    ofs << "#include \"ComputeGroup"+std::to_string(group)+".h\"\n";
    if (hasDynamicFunctions)
        ofs << "#include \"DynamicFunctions.h\"\n";
    
    ofs << "namespace lmfao\n{\n";
    ofs << genComputeGroupFunction(group) << std::endl;
    ofs << "}\n";
    ofs.close();
}

std::string CppGenerator::genLoadRelationFunction()
{
    std::string returnString = "\n"+indent(1)+"void loadRelations()\n"+indent(1)+"{\n"+
        indent(2)+"std::ifstream input;\n"+indent(2)+"std::string line;\n";
    
    for (size_t rel = 0; rel < _db->numberOfRelations(); ++rel)
    {
        std::string rel_name = _db->getRelation(rel)->_name;
        
        returnString += "\n"+indent(2)+rel_name+".clear();\n";

        returnString += indent(2)+"input.open(PATH_TO_DATA + \"/"+rel_name+".tbl\");\n"+
            indent(2)+"if (!input)\n"+indent(2)+"{\n"+
            indent(3)+"std::cerr << PATH_TO_DATA + \""+rel_name+
            ".tbl does is not exist. \\n\";\n"+
            indent(3)+"exit(1);\n"+indent(2)+"}\n"+
            indent(2)+"while(getline(input, line))\n"+
            indent(3)+rel_name+".push_back("+rel_name+"_tuple(line));\n"+
            indent(2)+"input.close();\n";            
    }

    // Reserving space for the views to speed up push tuples to them.
    returnString += "\n";
    for (size_t viewID = 0; viewID < _qc->numberOfViews(); ++viewID)
        returnString += indent(2)+viewName[viewID]+".reserve(1000000);\n";

    return returnString+indent(1)+"}\n";
}



std::string CppGenerator::genSortFunction(const size_t& rel_id)
{
    Relation* relation = _db->getRelation(rel_id);
    std::string relName = relation->_name;

#if defined(__GNUC__) && defined(NDEBUG) && !defined(__clang__)
    std::string sortAlgo = "__gnu_parallel::sort(";
#else
    std::string sortAlgo = "std::sort(";
#endif
    
    std::string returnString = indent(1)+"void sort"+relName+"()\n"+indent(1)+"{\n";

    if (BENCH_INDIVIDUAL)
        returnString += indent(2)+"int64_t startProcess = duration_cast<milliseconds>("+
            "system_clock::now().time_since_epoch()).count();\n\n";

    returnString += indent(2)+sortAlgo+relName+".begin(),"+
        relName+".end(),[ ](const "+relName+"_tuple& lhs, const "+relName+
        "_tuple& rhs)\n"+indent(2)+"{\n";

    // size_t orderIdx = 0;
    // for (const size_t& var : varOrder)
    // {
    //     if (baseRelation->_bag[var])
    //     {
    //         const std::string& attrName = _db->getAttribute(var)->_name;
    //         returnString += indent(3)+"if(lhs."+attrName+" != rhs."+attrName+")\n"+
    //             indent(4)+"return lhs."+attrName+" < rhs."+attrName+";\n";
    //         // Reset the relSortOrder array for future calls 
    //         sortOrders[view->_origin][orderIdx] = var;
    //         ++orderIdx;
    //     }
    // }
    var_bitset relSchema = relation->_schemaMask;
    
    for (const size_t var : _td->getJoinKeyOrder()) 
    {
        if (relSchema[var])
        {
            const std::string& attrName =
                _db->getAttribute(var)->_name;
            
            returnString += indent(3)+"if(lhs."+attrName+" != rhs."+attrName+")\n"+
                indent(4)+"return lhs."+attrName+" < rhs."+attrName+";\n";

            relSchema.reset(var);
        }
    }

    if (relSchema.any())
    {
        for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
        {
            if (relSchema[var])
            {
                const std::string& attrName =
                    _db->getAttribute(var)->_name;

                returnString += indent(3)+"return lhs."+attrName+" < rhs."+attrName+";\n"+
                    indent(2)+"});\n";
                break;
            }
        }
    }
    else
    {
        returnString += indent(3)+"return false;\n"+indent(2)+"});\n";
    }

    if (BENCH_INDIVIDUAL)
        returnString += "\n"+indent(2)+
            "int64_t endProcess = duration_cast<milliseconds>("+
            "system_clock::now().time_since_epoch()).count()-startProcess;\n"+
            indent(2)+"std::cout << \"Sort Relation "+relName+": \"+"+
            "std::to_string(endProcess)+\"ms.\\n\";\n\n";
        // indent(2)+"std::ofstream ofs(\"times.txt\",std::ofstream::out | " +
        // "std::ofstream::app);\n"+
        // indent(2)+"ofs << \"\\t\" << endProcess;\n"+
        // indent(2)+"ofs.close();\n";
    
    return returnString+indent(1)+"}\n";
}



std::string CppGenerator::genRunFunction(bool parallelize, bool hasApplicationHandler)
{
    std::string returnString = indent(1)+"void run()\n"+
        indent(1)+"{\n";
    
    returnString += indent(2)+"int64_t startLoading = duration_cast<milliseconds>("+
        "system_clock::now().time_since_epoch()).count();\n\n"+
        indent(2)+"loadRelations();\n\n"+
        indent(2)+"int64_t loadTime = duration_cast<milliseconds>("+
        "system_clock::now().time_since_epoch()).count()-startLoading;\n"+
        indent(2)+"std::cout << \"Data loading: \"+"+
        "std::to_string(loadTime)+\"ms.\\n\";\n"+
        indent(2)+"std::ofstream ofs(\"times.txt\",std::ofstream::out | "+
        "std::ofstream::app);\n"+
        indent(2)+"ofs << loadTime;\n"+
        indent(2)+"ofs.close();\n\n";
    
    returnString += indent(2)+"int64_t startSorting = duration_cast<milliseconds>("+
        "system_clock::now().time_since_epoch()).count();\n\n";

    if (!parallelize)
    {
        for (size_t rel = 0; rel < _db->numberOfRelations(); ++rel)
        {
            returnString += indent(2)+"sort"+_db->getRelation(rel)->_name+"();\n";
        }
    }
    else
    {
        for (size_t rel = 0; rel < _db->numberOfRelations(); ++rel)
        {
            Relation* node = _db->getRelation(rel);
            returnString += indent(2)+"std::thread sort"+node->_name+
                "Thread(sort"+node->_name+");\n";
        }

        for (size_t rel = 0; rel < _db->numberOfRelations(); ++rel)
        {
            Relation* node = _db->getRelation(rel);
            returnString += indent(2)+"sort"+node->_name+"Thread.join();\n";
        }
    }
    
    returnString += "\n"+indent(2)+"int64_t sortingTime = duration_cast<milliseconds>("+
        "system_clock::now().time_since_epoch()).count()-startSorting;\n"+
        indent(2)+"std::cout << \"Data sorting: \" + "+
        "std::to_string(sortingTime)+\"ms.\\n\";\n\n"+
        indent(2)+"ofs.open(\"times.txt\",std::ofstream::out | "+
        "std::ofstream::app);\n"+
        indent(2)+"ofs << \"\\t\" << sortingTime;\n"+
        indent(2)+"ofs.close();\n\n";

    returnString += indent(2)+"int64_t startProcess = duration_cast<milliseconds>("+
        "system_clock::now().time_since_epoch()).count();\n\n";

    if (!parallelize)
    {
        for (size_t group = 0; group < _qc->numberOfViewGroups(); ++group)
            returnString += indent(2)+"computeGroup"+std::to_string(group)+"();\n";
    }
    else
    {
        std::vector<bool> joinedGroups(_qc->numberOfViewGroups());
        std::vector<size_t> viewToGroupMapping(_qc->numberOfViews());
    
        for (size_t group = 0; group < _qc->numberOfViewGroups(); ++group)
        {
            ViewGroup& viewGroup = _qc->getViewGroup(group);
            
            for (const size_t& viewID : viewGroup._incomingViews)
                viewToGroupMapping[viewID] = group;
            
            for (const size_t& viewID : viewGroup._incomingViews)
            {
                // find group that contains this view!
                size_t otherGroup = viewToGroupMapping[viewID];
                
                if (!joinedGroups[otherGroup])
                {
                    returnString += indent(2)+"group"+std::to_string(otherGroup)+
                        "Thread.join();\n";
                    joinedGroups[otherGroup] = 1;
                }
            }

            returnString += indent(2)+"std::thread group"+std::to_string(group)+
                    "Thread(computeGroup"+std::to_string(group)+");\n";     
        }

        for (size_t group = 0; group < _qc->numberOfViewGroups(); ++group)
        {
            if (!joinedGroups[group])
                returnString += indent(2)+"group"+std::to_string(group)+
                    "Thread.join();\n";
        }
    }
    
    returnString += "\n"+indent(2)+"int64_t processTime = duration_cast<milliseconds>("+
        "system_clock::now().time_since_epoch()).count()-startProcess;\n"+
        indent(2)+"std::cout << \"Data process: \"+"+
        "std::to_string(processTime)+\"ms.\\n\";\n"+
        indent(2)+"ofs.open(\"times.txt\",std::ofstream::out | std::ofstream::app);\n"+
        indent(2)+"ofs << \"\\t\" << processTime;\n";
    
    // if (BENCH_INDIVIDUAL)
    for (size_t view = 0; view < _qc->numberOfViews(); ++view)
    {
        returnString += indent(2)+"std::cout << \""+viewName[view]+": \" << "+
            viewName[view]+".size() << std::endl;\n";
    }

    if (hasApplicationHandler)
    {
        returnString += indent(2)+"ofs.close();\n\n"+
            indent(2)+"int64_t appProcess = duration_cast<milliseconds>("+
            "system_clock::now().time_since_epoch()).count();\n\n";

        if (BENCH_INDIVIDUAL)
            returnString += indent(2)+"std::cout << \"run Application:\\n\";\n";

        returnString += indent(2)+"runApplication();\n"; 
    }
    else
    {
        returnString += indent(2)+"ofs << std::endl;\n"+
            indent(2)+"ofs.close();\n\n";
    }
    
    return returnString+indent(1)+"}\n";
}



std::string CppGenerator::genTestFunction()
{
    std::string returnString = "#ifdef TESTING\n"+
        indent(1)+"void ouputViewsForTesting()\n"+
        indent(1)+"{\n"+indent(2)+"std::ofstream ofs(\"test.out\");\n"+
        indent(2)+"ofs << std::fixed << std::setprecision(2);\n";
    
    for (size_t viewID = 0; viewID < _qc->numberOfViews(); ++viewID)
    {
        View* view = _qc->getView(viewID);

        if (view->_origin != view->_destination)
            continue;

        std::string fields = "";
        for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
        {
            if (view->_fVars[var])
            {
                Attribute* attr = _db->getAttribute(var);
                fields += " << tuple."+attr->_name+" <<\"|\"";
            }
        }

        for (size_t agg = 0; agg < view->_aggregates.size(); ++agg)
            fields += " << tuple.aggregates["+std::to_string(agg)+"] << \"|\"";

        fields.pop_back();
        fields.pop_back();
        fields.pop_back();
        
        returnString += indent(2)+"for (size_t i=0; i < "+viewName[viewID]+
            ".size(); ++i)\n"+indent(2)+"{\n"+indent(3)+
            viewName[viewID]+"_tuple& tuple = "+viewName[viewID]+"[i];\n"+
            indent(3)+"ofs "+fields+"\"\\n\";\n"+indent(2)+"}\n";
    }

    returnString += indent(2)+"ofs.close();\n"+indent(1)+"}\n"+"#endif\n";
    return returnString;
}


std::string CppGenerator::genDumpFunction()
{
    std::string returnString = "#ifdef DUMP_OUTPUT\n"+
        indent(1)+"void dumpOutputViews()\n"+indent(1)+"{\n"+
        indent(2)+"std::ofstream ofs;\n";
    
    for (size_t viewID = 0; viewID < _qc->numberOfViews(); ++viewID)
    {
        View* view = _qc->getView(viewID);

        // if (view->_origin != view->_destination)
        //     continue;

        returnString += indent(2)+"ofs.open(\"output/"+
            viewName[viewID]+".tbl\");\n"+
            indent(2)+"ofs << \""+std::to_string(view->_fVars.count())+" "+
            std::to_string(view->_aggregates.size())+"\\n\";\n";
            
        std::string fields = "";
        for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
        {
            if (view->_fVars[var])
            {
                Attribute* attr = _db->getAttribute(var);
                fields += " << tuple."+attr->_name+" <<\"|\"";
            }
        }

        for (size_t agg = 0; agg < view->_aggregates.size(); ++agg)
            fields += " << tuple.aggregates["+std::to_string(agg)+"] << \"|\"";

        fields.pop_back();
        fields.pop_back();
        fields.pop_back();
        
        returnString += indent(2)+"for (size_t i=0; i < "+viewName[viewID]+
            ".size(); ++i)\n"+indent(2)+"{\n"+indent(3)+
            viewName[viewID]+"_tuple& tuple = "+viewName[viewID]+"[i];\n"+
            indent(3)+"ofs "+fields+"\"\\n\";\n"+indent(2)+"}\n"+
            indent(2)+"ofs.close();\n";
    }

    returnString += indent(1)+"}\n"+"#endif\n";
    return returnString;
}


std::string CppGenerator::genTupleStructs()
{
    std::string tupleStruct = "", attrConversion = "", attrConstruct = "",
        attrAssign = "", attrParse = "";

    for (size_t relID = 0; relID < _db->numberOfRelations(); ++relID)
    {
        Relation* rel = _db->getRelation(relID);
        const std::string& relName = rel->_name;

        tupleStruct += indent(1)+"struct "+relName+"_tuple\n"+
            indent(1)+"{\n"+indent(2);

        for (const Attribute* att : rel->_schema)
        {
            tupleStruct += typeToStr(att->_type)+" "+att->_name;

            // Check if this attribute is a constant. 
            if (att->_isConstant)
                tupleStruct += " = ("+typeToStr(att->_type)+") "+
                    std::to_string(att->_default)+";\n"+indent(2);
            else
                tupleStruct += ";\n"+indent(2);
        }
        tupleStruct += relName+"_tuple(const std::string& tuple);\n"+indent(1)+"};\n\n"+
            indent(1)+"extern std::vector<"+rel->_name+"_tuple> "+rel->_name+";\n\n";
    }

    for (size_t viewID = 0; viewID < _qc->numberOfViews(); ++viewID)
    {
        View* view = _qc->getView(viewID);

        attrConstruct = "";
        
        tupleStruct += indent(1)+"struct "+viewName[viewID]+"_tuple\n"+
            indent(1)+"{\n";

        for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
        {
            if (view->_fVars[var])
            {
                Attribute* att = _db->getAttribute(var);
                tupleStruct += indent(2)+typeToStr(att->_type)+" "+att->_name+";\n";
                attrConstruct += "const "+typeToStr(att->_type)+"& "+att->_name+",";
            }
        }

        tupleStruct += indent(2)+"double aggregates["+
            std::to_string(view->_aggregates.size())+"] = {};\n";
        
        if (!attrConstruct.empty())
        {
            attrConstruct.pop_back();
            tupleStruct += indent(2)+viewName[viewID]+"_tuple("+
                attrConstruct+");\n"+indent(1)+"};\n\n"+
                indent(1)+"extern std::vector<"+viewName[viewID]+"_tuple> "+
                viewName[viewID]+";\n\n";
        }
        else
            tupleStruct += indent(2)+viewName[viewID]+"_tuple();\n"+indent(1)+
                "};\n\n"+indent(1)+"extern std::vector<"+viewName[viewID]+"_tuple> "+
                viewName[viewID]+";\n\n";
    }
    return tupleStruct;
}

std::string CppGenerator::genTupleStructConstructors()
{
    std::string tupleStruct = indent(0)+"#include \"DataHandler.h\"\n"+
        "#include <boost/spirit/include/qi.hpp>\n"+
        "#include <boost/spirit/include/phoenix_core.hpp>\n"+
        "#include <boost/spirit/include/phoenix_operator.hpp>\n\n"+
        "namespace qi = boost::spirit::qi;\n"+
        "namespace phoenix = boost::phoenix;\n\n"+
        "namespace lmfao\n{\n";
    
    std::string attrConstruct = "", attrAssign = "";
    
    for (size_t relID = 0; relID < _db->numberOfRelations(); ++relID)
    {
        Relation* rel = _db->getRelation(relID);
        const std::string& relName = rel->_name;

        attrConstruct = indent(2)+"qi::phrase_parse(tuple.begin(),tuple.end(),";

        for (const Attribute* att : rel->_schema)
        {           
            if (!att->_isConstant) 
                attrConstruct += "\n"+indent(3)+"qi::"+typeToStr(att->_type)+
                    "_[phoenix::ref("+att->_name+") = qi::_1]>>";
        }
        attrConstruct.pop_back();
        attrConstruct.pop_back();
        attrConstruct += ",\n"+indent(3)+"\'|\');\n"+indent(1);

        tupleStruct += indent(1)+relName+"_tuple::"+relName+
            "_tuple(const std::string& tuple)\n"+indent(1)+"{\n"+attrConstruct+"}\n\n";
    }

    for (size_t viewID = 0; viewID < _qc->numberOfViews(); ++viewID)
    {
        View* view = _qc->getView(viewID);

        attrConstruct = "";
        attrAssign = "";

        for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
        {
            if (view->_fVars[var])
            {
                Attribute* att = _db->getAttribute(var);
                attrConstruct += "const "+typeToStr(att->_type)+"& "+att->_name+",";
                attrAssign += att->_name+"("+att->_name+"),";
            }
        }        
        if (!attrConstruct.empty())
        {
            attrAssign.pop_back();
            attrConstruct.pop_back();
            
            tupleStruct +=  indent(1)+viewName[viewID]+"_tuple::"+
                viewName[viewID]+"_tuple("+attrConstruct+") : "+
                attrAssign+"{}\n\n";
        }
        else
            tupleStruct += indent(1)+viewName[viewID]+"_tuple::"+
                viewName[viewID]+"_tuple(){}\n\n";
    }
    return tupleStruct+"}\n";
}

    
std::string CppGenerator::genCaseIdentifiers()
{
    std::string returnString = indent(1)+"enum ID {";

    for (size_t rel = 0; rel < _db->numberOfRelations(); ++rel)
        returnString += "\n"+indent(2)+_db->getRelation(rel)->_name+"_ID,";

    for (size_t view = 0; view < _qc->numberOfViews(); ++view)
        returnString += "\n"+indent(2)+viewName[view]+"_ID,";
    returnString.pop_back();

    return returnString+"\n"+indent(1)+"};\n";
}











std::string CppGenerator::genComputeGroupFunction(size_t group_id)
{
    const AttributeOrder& varOrder = _qc->getAttributeOrder(group_id);

    const ViewGroup& viewGroup = _qc->getViewGroup(group_id);
    
    // const var_bitset& varOrderBitset = groupVariableOrderBitset[group_id];
    // const std::vector<size_t>& viewsPerVar = groupViewsPerVarInfo[group_id];
    // const std::vector<size_t>& incViews = groupIncomingViews[group_id];

    TDNode* baseRelation = viewGroup._tdNode;
    
    std::string relName = baseRelation->_relation->_name;

    std::string groupString = "Group"+std::to_string(group_id);
    
    std::string returnString = genHashStructAndFunction(viewGroup);
    
    if (!viewGroup._parallelize)
    {
        returnString += indent(1)+"void computeGroup"+std::to_string(group_id)+"()\n"+
            indent(1)+"{\n";

        for (const size_t& viewID : viewGroup._views)
        {
            if (_qc->requireHashing(viewID))
            {
                returnString += indent(2)+"std::unordered_map<"+viewName[viewID]+"_key,"+
                    viewName[viewID]+"_value,"+viewName[viewID]+"_keyhash> "+
                    viewName[viewID]+"_map;\n"+
                    indent(2)+"std::unordered_map<"+viewName[viewID]+"_key,"+
                    viewName[viewID]+"_value,"+viewName[viewID]+"_keyhash>::iterator "+
                    viewName[viewID]+"_iterator;\n"+
                    indent(2)+"std::pair<std::unordered_map<"+viewName[viewID]+"_key,"+
                    viewName[viewID]+"_value,"+
                    viewName[viewID]+"_keyhash>::iterator,bool> "+
                    viewName[viewID]+"_pair;\n"+
                    indent(2)+viewName[viewID]+"_map.reserve(500000);\n";
            }
        }
    }
    else
    {
        std::string numThreads = std::to_string(viewGroup._threads);
        
        for (const size_t& viewID : viewGroup._views)
        {
            if (!_qc->requireHashing(viewID))
            {
                returnString += indent(1)+"std::vector<"+viewName[viewID]+"_tuple> "+
                    "local_"+viewName[viewID]+"["+numThreads+"];\n";
            }
            else
            {
                returnString += indent(1)+"std::unordered_map<"+viewName[viewID]+"_key,"+
                    viewName[viewID]+"_value,"+viewName[viewID]+"_keyhash> "+
                    "local_"+viewName[viewID]+"_map["+numThreads+"];\n";
            }
        }
        
        returnString += "\n"+indent(1)+"void computeGroup"+std::to_string(group_id)+
            "Parallelized(const size_t thread, const size_t lowerptr, "+
            "const size_t upperptr)\n"+indent(1)+"{\n";

        for (const size_t& viewID : viewGroup._views)
        {
            if (!_qc->requireHashing(viewID))
            {
                returnString += indent(2)+"std::vector<"+viewName[viewID]+"_tuple>& "+
                viewName[viewID]+" = local_"+viewName[viewID]+"[thread];\n";
            }
            else
            {
                returnString += indent(2)+"std::unordered_map<"+viewName[viewID]+"_key,"+
                    viewName[viewID]+"_value,"+viewName[viewID]+"_keyhash>& "+
                    viewName[viewID]+"_map = local_"+viewName[viewID]+"_map[thread];\n"+
                    indent(2)+"std::unordered_map<"+viewName[viewID]+"_key,"+
                    viewName[viewID]+"_value,"+viewName[viewID]+"_keyhash>::iterator "+
                    viewName[viewID]+"_iterator;\n"+
                    indent(2)+viewName[viewID]+"_map.reserve(500000);\n";
            }
        }
    }

    // TODO: HERE WAS THE CODE FOR RESORT RELATIONS THIS HAS BEEN REMOVED

    if(MICRO_BENCH)
        returnString += indent(2)+"INIT_MICRO_BENCH("+groupString+"_timer_aggregate);\n"+
            indent(2)+"INIT_MICRO_BENCH("+groupString+"_timer_post_aggregate);\n"+
            indent(2)+"INIT_MICRO_BENCH("+groupString+"_timer_seek);\n"+
            indent(2)+"INIT_MICRO_BENCH("+groupString+"_timer_while);\n";

    if (BENCH_INDIVIDUAL)
        returnString += indent(2)+"int64_t startProcess = duration_cast<milliseconds>("+
            "system_clock::now().time_since_epoch()).count();\n\n";

    // Generate max values for the join varibales 
    for (size_t i = 1; i < varOrder.size(); ++i)
    {
        Attribute* attr = _db->getAttribute(varOrder[i]._attribute);
        returnString += indent(2)+typeToStr(attr->_type)+" max_"+attr->_name+";\n";
    }

    std::string numOfJoinVarString = std::to_string(varOrder.size() - 1);

    returnString += indent(2) + "bool found["+numOfJoinVarString+"] = {}, "+
        "atEnd["+numOfJoinVarString+"] = {}, addTuple["+numOfJoinVarString+"] = {};\n";

    // This would be used for LeapFroggingJoin / GenericJoin
    //     indent(2)+"size_t leap, low, mid, high;\n";

    // Lower and upper pointer for the base relation
    returnString += genPointerArrays(
        relName, numOfJoinVarString, viewGroup._parallelize);
    
    // Lower and upper pointer for each incoming view
    for (const size_t& viewID : viewGroup._incomingViews)            
        returnString += genPointerArrays(
            viewName[viewID],numOfJoinVarString, false);
    
    /******************* PRINT OUT *****************************/
    // if (group_id == 4)
    // {
    //     for (size_t& viewID : viewGroups[4])
    //     {
    //         std::cout << viewID << " " << viewLevelRegister[viewID] <<" : ";
    //         for (size_t incID : incomingViews[viewID])
    //             std::cout << incID << " ";
    //         std::cout << std::endl;
    //     }
    //     std::cout << baseRelation->_id  << "  " << relName << std::endl;
    // }
    /******************* PRINT OUT *****************************/


    returnString += indent(2)+"size_t ptr_"+relName+" = 0;\n";

    returnString += indent(2)+"double";
    for (const size_t& viewID : viewGroup._views)
        returnString += " *aggregates_"+viewName[viewID]+" = nullptr,";
    
    for (const size_t& viewID : viewGroup._incomingViews)
        returnString += " *aggregates_"+viewName[viewID]+" = nullptr,";
    
    returnString.pop_back();
    returnString += ";\n";


    var_bitset varOrderBitset;
    for (const AttributeOrderNode& orderNode : varOrder)
    {
        varOrderBitset.set(orderNode._attribute);
    }
    
    for (const size_t& viewID : viewGroup._incomingViews)
    {
        View* view = _qc->getView(viewID);
        
        if ((view->_fVars & ~varOrderBitset).any())
            returnString += indent(2)+"size_t ptr_"+viewName[viewID]+" = 0;\n";
    }
    
    // Declare the register Arrrays
    if (viewGroup.numberOfLoopAggregates > 0)
        returnString += indent(2)+"double aggregateRegister["+
            std::to_string(viewGroup.numberOfLoopAggregates)+"] = {};\n";    
    if (viewGroup.numberOfLocalAggregates > 0)
        returnString += indent(2)+"double localRegister["+
            std::to_string(viewGroup.numberOfLocalAggregates)+"] = {};\n";
    if (viewGroup.numberOfRunningSums > 0)
        returnString += indent(2)+"double runningSum["+
            std::to_string(viewGroup.numberOfRunningSums)+"] = {};\n";   

    /// *************************************************
    ///
    ///   Code that does join and aggregates 
    ///
    /// *************************************************
    
    returnString += genJoinAggregatesCode(group_id, varOrder, *baseRelation, 0);    

    /// *************************************************
    ///
    ///   OUTPUT TUPLES TO VIEWS WITHOUT FREE VARS! TODO: 
    ///
    /// ************************************************

    

    /**********************************************/

    std::string postComputationString = "";
    const AttributeOrderNode& orderNode = varOrder[0];

    if (!orderNode._registeredOutgoingRunningSum.empty())
    {
        AggregateIndexes bench, aggIdx;
        std::bitset<7> increasingIndexes;

        bench.setIndexes(orderNode._registeredOutgoingRunningSum[0]);

        size_t offset = 0;
        for (size_t aggID = 1; aggID <
                 orderNode._registeredOutgoingRunningSum.size(); ++aggID)
        {   
            aggIdx.setIndexes(orderNode._registeredOutgoingRunningSum[aggID]);
            
            if (aggIdx.isIncrement(bench,offset,increasingIndexes))
            {
                ++offset;
            }
            else
            {
                // If not, output bench and all offset aggregates
                postComputationString += genRunSumAggregateString(
                    bench,offset,increasingIndexes,2);
                
                // make aggregate the bench and set offset to 0
                bench = aggIdx;
                offset = 0;
            }
        }

        // If not, output bench and all offset aggregates
        postComputationString += genRunSumAggregateString(
            bench,offset,increasingIndexes,2);
    }
    
    /************************************************************************/
    
    if (orderNode._outgoingLoopRegister != nullptr)
    {
        std::string resetString = "";
        postComputationString +=  genOutputLoopAggregateStringCompressed(
            orderNode._outgoingLoopRegister,*(baseRelation),0,orderNode._joinViews,
            varOrder.size(),resetString);
    }

    /************************************************************************/

    returnString += postComputationString;

    // std::string outputString = "";
    
    // // Here we add the code that outputs tuples to the view 
    // for (size_t viewID : viewGroup._views)
    // {
    //     if (viewLevelRegister[viewID] == varOrder.size())
    //     {
    //         // just add string to push to view
    //         outputString += indent(2)+viewName[viewID]+".emplace_back();\n"+
    //             indent(2)+"aggregates_"+viewName[viewID]+" = "+
    //             viewName[viewID]+".back().aggregates;\n";
    
    //         if (COMPRESS_AGGREGATES)
    //         {    
    //             AggregateIndexes bench, aggIdx;
    //             std::bitset<7> increasingIndexes;
    //             mapAggregateToIndexes(bench,aggregateComputation[viewID][0],
    //                                   viewID,varOrder.size());
    //             size_t off = 0;
    //             for (size_t aggID = 1;
    //                  aggID < aggregateComputation[viewID].size(); ++aggID)
    //             {
    //                 const AggregateTuple& tuple = aggregateComputation[viewID][aggID];

    //                 aggIdx.reset();
    //                 mapAggregateToIndexes(aggIdx,tuple,viewID,varOrder.size());

    //                 if (aggIdx.isIncrement(bench,off,increasingIndexes))
    //                 {
    //                     ++off;
    //                 }
    //                 else
    //                 {
    //                     // If not, output bench and all offset aggregates
    //                     outputString += outputFinalRegTupleString(
    //                         bench,off,increasingIndexes,2);
                
    //                     // make aggregate the bench and set offset to 0
    //                     bench = aggIdx;
    //                     off = 0;
    //                     increasingIndexes.reset();
    //                 }
    //             }

    //             // If not, output bench and all offset aggregateString
    //             outputString += outputFinalRegTupleString(bench,off,increasingIndexes,2);
    //         }
    //         else
    //         {
    //             for (const AggregateTuple& aggTuple : aggregateComputation[viewID]) 
    //             {
    //                 outputString += indent(2)+
    //                     "aggregates_"+viewName[aggTuple.viewID]+"["+
    //                     std::to_string(aggTuple.aggID)+"] += ";
                                                              
    //                 if (aggTuple.post.first < varOrder.size())
    //                 {
    //                     std::string post = std::to_string(
    //                         postRemapping[aggTuple.post.first][aggTuple.post.second]);
    //                     outputString += "runningSum["+post+"]*";
    //                 }
    //                 else
    //                 {
    //                     ERROR("THIS SHOULD NOT HAPPEN - no post aggregate?!");
    //                     exit(1);
    //                 }
    //                 outputString.pop_back();
    //                 outputString += ";\n";    
    //             }  
    //         }
    //     }
    // }
    // returnString += outputString; 


    /// *************************************************
    ///
    ///  Code to combine    
    ///
    /// *************************************************
    
    if (!viewGroup._parallelize)
    {
        for (const size_t& viewID : viewGroup._views)
        {
            if (_qc->requireHashing(viewID))
            {
                View* view = _qc->getView(viewID);

                std::string copyString = "";
                for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
                {
                    if (!view->_fVars[var])
                        continue;

                    Attribute* att = _db->getAttribute(var);
                    copyString += "kv_pair.first."+att->_name+",";
                }
                copyString.pop_back();

                returnString += indent(2)+viewName[viewID]+".reserve("+
                    viewName[viewID]+"_map.size());\n";
                
                returnString +=
                    indent(2)+"for (const auto& kv_pair : "+viewName[viewID]+"_map)\n"+
                    indent(2)+"{\n"+
                    indent(3)+viewName[viewID]+".emplace_back("+copyString+");\n"+
                    indent(3)+"memcpy(&"+viewName[viewID]+".back().aggregates[0],"+
                    "&kv_pair.second.aggregates[0],"+
                    std::to_string(sizeof(double)*view->_aggregates.size())+");\n"+
                    indent(2)+"}\n";
            
#if defined(__GNUC__) && defined(NDEBUG) && !defined(__clang__)
                std::string sortAlgo = "__gnu_parallel::sort(";
#else
                std::string sortAlgo = "std::sort(";
#endif
                returnString += indent(2)+sortAlgo+viewName[viewID]+".begin(),"+
                    viewName[viewID]+".end(),[ ](const "+viewName[viewID]+
                    "_tuple& lhs, const "+viewName[viewID]+"_tuple& rhs)\n"+
                    indent(2)+"{\n";

                // TODO : TODO : figure out what to do here ! 
                var_bitset intersection = view->_fVars &
                    _db->getRelation(view->_destination)->_schemaMask;
                
                for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
                {
                    if (intersection[var])
                    {   
                        const std::string& attrName = _db->getAttribute(var)->_name;
                    
                        returnString += indent(3)+
                            "if(lhs."+attrName+" != rhs."+attrName+")\n"+indent(4)+
                            "return lhs."+attrName+" < rhs."+attrName+";\n";
                    }
                }
                var_bitset additional = view->_fVars & ~intersection;
                for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
                {
                    if (additional[var])
                    {   
                        const std::string& attrName = _db->getAttribute(var)->_name;
                    
                        returnString += indent(3)+
                            "if(lhs."+attrName+" != rhs."+attrName+")\n"+indent(4)+
                            "return lhs."+attrName+" < rhs."+attrName+";\n";
                    }
                }
                
                returnString += indent(3)+"return false;\n"+indent(2)+"});\n";
            }
        }
    }

    if (BENCH_INDIVIDUAL)
    {
        returnString += "\n"+indent(2)+
            "int64_t endProcess = duration_cast<milliseconds>("+
            "system_clock::now().time_since_epoch()).count()-startProcess;\n";

        if (viewGroup._parallelize)
            returnString += indent(2)+"std::cout <<\"Compute Group "+
                std::to_string(group_id)+" thread \"+std::to_string(thread)+\" "+
                "process: \"+std::to_string(endProcess)+\"ms.\\n\";\n";
        else 
            returnString += indent(2)+"std::cout <<\"Compute Group "+
                std::to_string(group_id)+" process: \"+"+
                "std::to_string(endProcess)+\"ms.\\n\";\n";

        returnString +=    
            indent(2)+"std::ofstream ofs(\"times.txt\",std::ofstream::out| "+
            "std::ofstream::app);\n"+
            indent(2)+"ofs << \"\\t\" << endProcess;\n"+
            indent(2)+"ofs.close();\n\n";
    }
    
    if (MICRO_BENCH)
        returnString += indent(2)+
            "PRINT_MICRO_BENCH("+groupString+"_timer_aggregate);\n"+
            indent(2)+"PRINT_MICRO_BENCH("+groupString+"_timer_post_aggregate);\n"+
            indent(2)+"PRINT_MICRO_BENCH("+groupString+"_timer_seek);\n"+
            indent(2)+"PRINT_MICRO_BENCH("+groupString+"_timer_while);\n";
    
    if (viewGroup._parallelize)
    {
        returnString += indent(1)+"}\n";

        returnString += indent(1)+"void computeGroup"+std::to_string(group_id)+
            "()\n"+indent(1)+"{\n"+indent(2)+"size_t lower, upper;\n";

        // for (const size_t& viewID : viewGroups[group_id])
        // {
        //     // for (size_t t = 0; t < _threadsPerGroup[group_id]; ++t)
        //     // {
        //     //     returnString += indent(2)+"std::vector<"+viewName[viewID]+
        //     //         "_tuple> "+viewName[viewID]+"_"+std::to_string(t)+";\n";
        //     // }
            
        //     if (_qc->requireHashing(viewID))
        //     {    
        //         for (size_t t = 0; t < _threadsPerGroup[group_id]; ++t)
        //         {
        //             // returnString += indent(2)+"std::unordered_map<"+
        //             //     viewName[viewID]+"_key,"+"size_t,"+viewName[viewID]+
        //             //     "_keyhash> "+viewName[viewID]+"_"+std::to_string(t)+"_map;\n";
        //             returnString += indent(2)+"std::unordered_map<"+
        //                 viewName[viewID]+"_key,"+ viewName[viewID]+"_value,"+
        //                 viewName[viewID]+"_keyhash> "+
        //                 viewName[viewID]+"_"+std::to_string(t)+"_map;\n";
        //         }
        //     }
        //     else
        //     {
        //         for (size_t t = 0; t < _threadsPerGroup[group_id]; ++t)
        //         {
        //             returnString += indent(2)+"std::vector<"+viewName[viewID]+
        //                 "_tuple> "+viewName[viewID]+"_"+std::to_string(t)+";\n";
        //         }
        //     }
        // }

        std::string numThreads = std::to_string(viewGroup._threads);
        
        for (size_t t = 0; t < viewGroup._threads; ++t)
        {
            returnString += indent(2)+"lower = (size_t)("+relName+".size() * ((double)"+
                std::to_string(t)+"/"+numThreads+"));\n"+
                indent(2)+"upper = (size_t)("+relName+".size() * ((double)"+
                std::to_string(t+1)+"/"+numThreads+")) - 1;\n"+
                indent(2)+"std::thread thread"+std::to_string(t)+
                "(computeGroup"+std::to_string(group_id)+"Parallelized,"+
                std::to_string(t)+",lower,upper);\n\n";
        }

        for (size_t t = 0; t < viewGroup._threads; ++t)
            returnString += indent(2)+"thread"+std::to_string(t)+".join();\n";
        

        for (const size_t& viewID : viewGroup._views)
        {
            if (!_qc->requireHashing(viewID))
            {
                View* view = _qc->getView(viewID);

                if (view->_fVars.any())
                {
                    returnString += indent(2)+viewName[viewID]+".reserve(";
                    for (size_t t = 0; t < viewGroup._threads; ++t)
                        returnString += "local_"+viewName[viewID]+"["+
                            std::to_string(t)+"].size()+";
                    returnString.pop_back();
                    returnString += ");\n";

                    returnString += indent(2)+viewName[viewID]+".insert("+
                        viewName[viewID]+".end(),"+
                        "local_"+viewName[viewID]+"[0].begin(), "+
                        "local_"+viewName[viewID]+"[0].end());\n";

                    returnString +=
                        indent(2)+"for (size_t t = 1; t < "+numThreads+"; ++t)\n"+
                        indent(2)+"{\n"+
                        indent(3)+"std::vector<"+viewName[viewID]+"_tuple>& "+
                         "this_"+viewName[viewID]+" = local_"+viewName[viewID]+"[t];\n"+
                        indent(3)+"if(";
                    
                        for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
                        {
                            if (view->_fVars[var])
                            {
                                Attribute* attr = _db->getAttribute(var);
                                returnString += viewName[viewID]+".back()."+
                                    attr->_name+"==this_"+viewName[viewID]+"[0]."+
                                    attr->_name+"&&";
                            }
                        }
                        returnString.pop_back();
                        returnString.pop_back();
                        
                        returnString += ")\n"+indent(3)+"{\n"+
                            indent(4)+"for (size_t agg = 0; agg < "+
                            std::to_string(view->_aggregates.size())+"; ++agg)\n"+
                            indent(5)+viewName[viewID]+".back().aggregates[agg] += "+
                            "this_"+viewName[viewID]+"[0].aggregates[agg];\n";
                        
                        returnString += indent(4)+viewName[viewID]+".insert("+
                            viewName[viewID]+".end(),"+
                            "this_"+viewName[viewID]+".begin()+1,"+
                            "this_"+viewName[viewID]+".end());\n"+
                            indent(3)+"}\n";
                    
                        returnString += indent(3)+"else\n"+indent(3)+"{\n"+
                            indent(4)+viewName[viewID]+".insert("+
                            viewName[viewID]+".end(), "+
                            "this_"+viewName[viewID]+".begin(), "+
                            "this_"+viewName[viewID]+".end());\n"+
                            indent(3)+"}\n"+indent(2)+"}\n";
                }
                else
                {
                    returnString += indent(2)+viewName[viewID]+".insert("+
                        viewName[viewID]+".end(), "+
                        "local_"+viewName[viewID]+"[0].begin(), "+
                        "local_"+viewName[viewID]+"[0].end());\n";

                    returnString += indent(2)+"for (size_t agg = 0; agg < "+
                        std::to_string(view->_aggregates.size())+"; ++agg)\n"+
                        indent(3)+viewName[viewID]+"[0].aggregates[agg] += ";
                        
                    for (size_t t = 1; t < viewGroup._threads; ++t)
                    {
                        returnString += "local_"+viewName[viewID]+"["+
                            std::to_string(t)+"][0].aggregates[agg]+";
                    }
                    returnString.pop_back();
                    returnString += ";\n";
                }
            }
        }

        for (const size_t& viewID : viewGroup._views)
        {
            if (_qc->requireHashing(viewID))
            {
                View* view = _qc->getView(viewID);

                returnString +=
                    indent(2)+"std::unordered_map<"+viewName[viewID]+"_key,"+
                    viewName[viewID]+"_value,"+viewName[viewID]+"_keyhash>& "+
                    viewName[viewID]+"_map = local_"+viewName[viewID]+"_map[0];\n"+
                    indent(2)+"for (size_t t = 1; t < "+numThreads+"; ++t)\n"+
                    indent(2)+"{\n"+
                    indent(3)+"for (const std::pair<"+viewName[viewID]+"_key,"+
                    viewName[viewID]+"_value>& key : "+
                    "local_"+viewName[viewID]+"_map[t])\n"+
                    indent(3)+"{\n"+
                    indent(4)+"auto it = "+viewName[viewID]+"_map.find(key.first);\n"+
                    indent(4)+"if (it != "+viewName[viewID]+"_map.end())\n"+
                    indent(4)+"{\n"+
                    // Loop over aggregates
                    indent(5)+"for (size_t agg = 0; agg < "+std::to_string(
                        _qc->getView(viewID)->_aggregates.size())+"; ++agg)\n"
                    +indent(6)+"it->second.aggregates[agg] += "+
                    "key.second.aggregates[agg];\n"+
                    indent(4)+"}\n"+
                    indent(4)+"else\n"+indent(4)+"{\n"+
                    indent(5)+viewName[viewID]+"_map[key.first] = key.second;\n"+
                    indent(4)+"}\n"+indent(3)+"}\n"+indent(2)+"}\n";
    
                std::string copyString = "";
                for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
                {
                    if (!view->_fVars[var])
                        continue;

                    Attribute* att = _db->getAttribute(var);
                    copyString += "kv_pair.first."+att->_name+",";
                }
                copyString.pop_back();               

                returnString += indent(2)+viewName[viewID]+".reserve("+
                    viewName[viewID]+"_map.size());\n";

                returnString +=
                    indent(2)+"for (const auto& kv_pair : "+viewName[viewID]+"_map)\n"+
                    indent(2)+"{\n"+
                    indent(3)+viewName[viewID]+".emplace_back("+copyString+");\n"+
                    indent(3)+"memcpy(&"+viewName[viewID]+".back().aggregates[0],"+
                    "&kv_pair.second.aggregates[0],"+
                    std::to_string(sizeof(double)*view->_aggregates.size())+");\n"+
                    indent(2)+"}\n";
                
#if defined(__GNUC__) && defined(NDEBUG) && !defined(__clang__)
                std::string sortAlgo = "__gnu_parallel::sort(";
#else
                std::string sortAlgo = "std::sort(";
#endif
                
                returnString += indent(2)+sortAlgo+viewName[viewID]+".begin(),"+
                    viewName[viewID]+".end(),[ ](const "+viewName[viewID]+
                    "_tuple& lhs, const "+viewName[viewID]+"_tuple& rhs)\n"+
                    indent(2)+"{\n";

                // TODO: What do we do here?!!
                var_bitset intersection = view->_fVars &
                    _db->getRelation(view->_destination)->_schemaMask;
                
                for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
                {
                    if (intersection[var])
                    {   
                        const std::string& attrName = _db->getAttribute(var)->_name;
                    
                        returnString += indent(3)+
                            "if(lhs."+attrName+" != rhs."+attrName+")\n"+indent(4)+
                            "return lhs."+attrName+" < rhs."+attrName+";\n";
                    }
                }
                var_bitset additional = view->_fVars & ~intersection;
                for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
                {
                    if (additional[var])
                    {   
                        const std::string& attrName = _db->getAttribute(var)->_name;
                    
                        returnString += indent(3)+
                            "if(lhs."+attrName+" != rhs."+attrName+")\n"+indent(4)+
                            "return lhs."+attrName+" < rhs."+attrName+";\n";
                    }
                }

                // for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
                // {
                //     if (view->_fVars[var])
                //     {   
                //         const std::string& attrName = _db->getAttribute(var)->_name;
                //         returnString += indent(3)+
                //             "if(lhs."+attrName+" != rhs."+attrName+")\n"+indent(4)+
                //             "return lhs."+attrName+" < rhs."+attrName+";\n";
                //     }
                // }
                returnString += indent(3)+"return false;\n"+indent(2)+"});\n";
            }
        }
    }

    return returnString + indent(1)+"}\n";
}



std::string CppGenerator::genHashStructAndFunction(const ViewGroup& viewGroup)
{    
    std::string hashStruct = "";
    std::string hashFunct = "";

    for (const size_t& viewID : viewGroup._views)
    {
        if (_qc->requireHashing(viewID))
        {
            View* view = _qc->getView(viewID);

            std::string attrConstruct = "";
            std::string attrAssign = "";
            std::string equalString = "";
            std::string initList = "";

            // Creates a struct which is used for hashing the key of the views 
            hashStruct += indent(1)+"struct "+viewName[viewID]+"_key\n"+
                indent(1)+"{\n"+indent(2);

            hashFunct += indent(1)+"struct "+viewName[viewID]+"_keyhash\n"+
                indent(1)+"{\n"+indent(2)+"size_t operator()(const "+viewName[viewID]+
                "_key &key ) const\n"+indent(2)+"{\n"+indent(3)+"size_t h = 0;\n";
            
            for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
            {
                if (view->_fVars[var])
                {
                    Attribute* att = _db->getAttribute(var);
                    std::string type = typeToStr(att->_type);

                    hashStruct += type+" "+att->_name+";\n"+indent(2);
                    attrConstruct += "const "+type+"& "+att->_name+",";
                    attrAssign += indent(3)+"this->"+ att->_name + " = "+
                        att->_name+";\n";
                    initList += att->_name+"("+att->_name+"),";
                    hashFunct += indent(3)+"h ^= std::hash<"+type+">()(key."+
                        att->_name+")+ 0x9e3779b9 + (h<<6) + (h>>2);\n";
                    // hashFunct += indent(3)+"hash_combine(h, key."+att->_name+");\n"; 
                    equalString += " this->"+att->_name+ " == other."+att->_name+" &&";
                }
            }
            attrConstruct.pop_back();
            equalString.pop_back();
            equalString.pop_back();
            initList.pop_back();

            hashStruct += viewName[viewID]+"_key("+attrConstruct+") : "+initList+"\n"+
                indent(2)+"{  }\n"+
                indent(2)+"bool operator==(const "+viewName[viewID]+
                "_key &other) const\n"+indent(2)+"{\n"+indent(3)+
                "return"+equalString+";\n"+indent(2)+"}\n"+indent(1)+"};\n\n";

            hashStruct += indent(1)+"struct "+viewName[viewID]+"_value\n"+
                indent(1)+"{\n"+indent(2)+"double aggregates["+
                std::to_string(view->_aggregates.size())+"];\n"+
                indent(1)+"};\n\n";

            hashFunct += indent(3)+"return h;\n"+indent(2)+"}\n"+indent(1)+"};\n\n";
        }
    }

    return hashStruct + hashFunct;
}




std::string CppGenerator::genJoinAggregatesCode(
    size_t group_id, const AttributeOrder& varOrder, const TDNode& node, size_t depth)
{
    const ViewGroup& viewGroup = _qc->getViewGroup(group_id);
    
    const AttributeOrderNode& orderNode = varOrder[depth+1];
    
    // TODO: this will fail due to the dummy node!
    const std::string& attrName = _db->getAttribute(orderNode._attribute)->_name;

    const std::string& relName = node._relation->_name;
    
    size_t numberContributing = orderNode._joinViews.size();
    
    std::string depthString = std::to_string(depth);
    std::string returnString = "";

    /* Setting upper and lower pointer for this level */ 
    if (depth > 0)
    {
        returnString +=
            indent(2+depth)+"upperptr_"+relName+"["+depthString+"] = "+
            "upperptr_"+relName+"["+std::to_string(depth-1)+"];\n"+
            indent(2+depth)+"lowerptr_"+relName+"["+depthString+"] = "+
            "lowerptr_"+relName+"["+std::to_string(depth-1)+"];\n";

        for (const size_t& viewID : viewGroup._incomingViews)
        {   
            returnString +=
                indent(2+depth)+"upperptr_"+viewName[viewID]+"["+depthString+"] = "+
                "upperptr_"+viewName[viewID]+"["+std::to_string(depth-1)+"];\n"+
                indent(2+depth)+"lowerptr_"+viewName[viewID]+"["+depthString+"] = "+
                "lowerptr_"+viewName[viewID]+"["+std::to_string(depth-1)+"];\n";
        }
    }
    
    returnString += indent(2+depth)+ "atEnd["+depthString+"] = false;\n";

    /* Resetting the runningSum before we start the loop */
    if (!orderNode._registeredRunningSum.empty())
    {        
        size_t firstidx = orderNode._registeredRunningSum[0]->aggregateIndex;
        size_t lastidx = orderNode._registeredRunningSum.back()->aggregateIndex;

        // TODO: varify that this is correct! (especially the lastidx)
        returnString += indent(2+depth)+"memset(&runningSum["+
            std::to_string(firstidx)+"], 0, sizeof(double) * "+
            std::to_string(lastidx - firstidx + 1)+");\n";
    }
    
    // Start while loop of the join 
    returnString += indent(2+depth)+"while(!atEnd["+depthString+"])\n"+
        indent(2+depth)+"{\n";

    /******************************************************************************/
    /******************************************************************************/
    /******************************************************************************/

    if (numberContributing > 0)
    {
        if (MICRO_BENCH)
            returnString += indent(3+depth)+
                "BEGIN_MICRO_BENCH(Group"+std::to_string(group_id)+"_timer_seek);\n";

        if (orderNode._registerdRelation == nullptr)
        {
            ERROR("This order node does not join on a relation, how can this be?\n");
            exit(1);
        }

        // Seek Value
        returnString += indent(3+depth)+"max_"+attrName+" = "+
            relName+"[lowerptr_"+relName+"["+depthString+"]]."+attrName +";\n"+
            indent(3+depth)+"// Seek Value\n" +
            indent(3+depth)+"do\n" +
            indent(3+depth)+"{\n"+
            indent(4+depth)+"found["+depthString+"] = true;\n";
        
            
        for (const size_t& viewID : orderNode._joinViews)
        {
            std::string upperPointer =
                getUpperPointer(viewName[viewID], depth, false);
            
            /* Check max value is in bounds */ 
            returnString += offset(4+depth)+"if(max_"+attrName+" > "+
                viewName[viewID]+"["+upperPointer+"]."+attrName+")\n"+
                offset(4+depth)+"{\n"+offset(5+depth)+"atEnd["+depthString+"] = true;\n"+
                offset(5+depth)+"break;\n"+offset(4+depth)+"}\n";

            /* Set lower pointer */
            returnString += offset(4+depth)+"while(lowerptr_"+
                viewName[viewID]+"["+depthString+"]<="+upperPointer+" && "+
                viewName[viewID]+"[lowerptr_"+viewName[viewID]+"["+depthString+"]]."+
                attrName+" < max_"+attrName+")\n"+
                offset(5+depth)+"++lowerptr_"+viewName[viewID]+"["+depthString+"];\n";
            
            /* Check that you got the same value */
            returnString += offset(4+depth)+"if ("+viewName[viewID]+"[lowerptr_"+
                viewName[viewID]+"["+depthString+"]]."+attrName+" > "+
                "max_"+attrName+")\n"+offset(4+depth)+"{\n"+
                offset(5+depth)+"max_"+attrName+" = "+viewName[viewID]+
                "[lowerptr_"+viewName[viewID]+"["+depthString+"]]."+attrName+";\n"+
                offset(5+depth)+"found["+depthString+"] = false;\n"+
                offset(5+depth)+"continue;\n"+offset(4+depth)+"}\n";
            
            /* Set upper pointer */
            returnString += offset(4+depth)+
                "upperptr_"+viewName[viewID]+"["+depthString+"] = "+
                "lowerptr_"+viewName[viewID]+"["+depthString+"];\n"+
                offset(4+depth)+"while(upperptr_"+viewName[viewID]+
                "["+depthString+"]<"+upperPointer+" && "+
                viewName[viewID]+"[upperptr_"+viewName[viewID]+"["+depthString+"]+1]."+
                attrName+" == max_"+attrName+")\n"+
                offset(5+depth)+"++upperptr_"+viewName[viewID]+"["+depthString+"];\n";
        }

        { // The join for the relation        
            std::string relationCase = "";
        
            std::string upperPointer =
                getUpperPointer(relName, depth, viewGroup._parallelize);
            
            /* Check max value is in bounds */ 
            relationCase += offset(4+depth)+"if(max_"+attrName+" > "+
                relName+"["+upperPointer+"]."+attrName+")\n"+
                offset(4+depth)+"{\n"+offset(5+depth)+"atEnd["+depthString+"] = true;\n"+
                offset(5+depth)+"break;\n"+offset(4+depth)+"}\n";

            /* Set the lowerPointer */
            relationCase += offset(4+depth)+//while statement
                "while(lowerptr_"+relName+"["+depthString+"]<="+upperPointer+" && "+
                relName+"[lowerptr_"+relName+"["+depthString+"]]."+
                attrName+" < max_"+attrName+")\n"+
                offset(5+depth)+"++lowerptr_"+relName+"["+depthString+"];\n";

            /* Check that you got the same value */
            relationCase += offset(4+depth)+
                "if ("+relName+"[lowerptr_"+relName+"["+depthString+"]]."+
                attrName+" > max_"+attrName+")\n"+offset(4+depth)+"{\n"+
                offset(5+depth)+"max_"+attrName+" = "+
                relName+"[lowerptr_"+relName+"["+depthString+"]]."+attrName+";\n"+
                offset(5+depth)+"found["+depthString+"] = false;\n"+
                offset(5+depth)+"continue;\n"+offset(4+depth)+"}\n";

            /* Set the upperPointer = lowerPointer */
            relationCase += offset(4+depth)+
                "upperptr_"+relName+"["+depthString+"] = "+
                "lowerptr_"+relName+"["+depthString+"];\n";

            /* Set upperPointer */
            relationCase += offset(4+depth)+//while statement
                "while(upperptr_"+relName+"["+depthString+"]<"+upperPointer+" && "+
                relName+"[upperptr_"+relName+"["+depthString+"]+1]."+
                attrName+" == max_"+attrName+")\n"+
                offset(5+depth)+"++upperptr_"+relName+"["+depthString+"];\n";
        
            returnString += relationCase;
        }
        
        
        // Close the do loop
        returnString += indent(3+depth)+"} while (!found["+depthString+"] && !atEnd["+
            depthString+"]);\n"+indent(3+depth)+"// End Seek Value\n";
        // End seek value

        // check if atEnd
        returnString += indent(3+depth)+"// If atEnd break loop\n"+
            indent(3+depth)+"if(atEnd["+depthString+"])\n"+indent(4+depth)+"break;\n";

        if (MICRO_BENCH)
            returnString += indent(3+depth)+"END_MICRO_BENCH(Group"+
                std::to_string(group_id)+"_timer_seek);\n";
    }
    else
    {    
        if (MICRO_BENCH)
            returnString += indent(3+depth)+
                "BEGIN_MICRO_BENCH(Group"+std::to_string(group_id)+"_timer_while);\n";

        assert(orderNode._registerdRelation != nullptr && orderNode._joinViews.empty());
        
        // Set upper = lower and update ranges
        returnString += indent(3+depth)+"max_"+attrName+" = "+
            relName+"[lowerptr_"+relName+"["+depthString+"]]."+attrName +";\n"+
            indent(3+depth)+"upperptr_"+relName+"["+depthString+"] = "+
            "lowerptr_"+relName+"["+depthString+"];\n";

        returnString += indent(3+depth)+ //while statement
            "while(upperptr_"+relName+"["+depthString+"]<"+
            getUpperPointer(relName, depth, viewGroup._parallelize)+" && "+
            relName+"[upperptr_"+relName+"["+depthString+"]+1]."+
            attrName+" == max_"+attrName+")\n"+
            indent(4+depth)+"++upperptr_"+relName+"["+depthString+"];\n";

        if (MICRO_BENCH)
            returnString += indent(3+depth)+
                "END_MICRO_BENCH(Group"+std::to_string(group_id)+"_timer_while);\n";
    }



    /******************************************************************************/
    /******************************************************************************/
    /******************************************************************************/

    
    // setting the addTuple bool to false;
    returnString += indent(3+depth)+"addTuple["+depthString+"] = false;\n";


    if (MICRO_BENCH)
        returnString += indent(3+depth)+
            "BEGIN_MICRO_BENCH(Group"+std::to_string(group_id)+"_timer_aggregate);\n";
    
    // if (orderNode._registeredProducts.size() > 0)
    // {
    //     // TODO:TODO:TODO: varify that this is correct !!
    //     size_t firstIndex = -1;
    //     // orderNode._loopProduct->loopAggregateList[0]->aggregateIndex;
    //     size_t secondIndex = -1;
    //     // orderNode._loopProduct->loopAggregateList.back()->aggregateIndex;
        
    //     returnString += indent(3 + depth)+"memset(&aggregateRegister["+
    //         std::to_string(firstIndex)+"], 0, sizeof(double) * "+
    //         std::to_string(secondIndex-firstIndex)+");\n";
    // }
   
    // We add the definition of the Tuple references and aggregate pointers
    returnString += indent(3+depth)+relName +"_tuple &"+relName+"Tuple = "+
        relName+"[lowerptr_"+relName+"["+depthString+"]];\n";

    // TODO:TODO:TODO: 
    // THIS IS TOO LOOSE: We can join on one attribute in the view but still not
    // require it for the agg 
    for (const size_t& viewID : orderNode._joinViews)
    {
        returnString += indent(3+depth)+viewName[viewID]+
            "_tuple &"+viewName[viewID]+"Tuple = "+viewName[viewID]+
            "[lowerptr_"+viewName[viewID]+"["+depthString+"]];\n"+
            indent(3+depth)+"double *aggregates_"+viewName[viewID]+
            " = "+viewName[viewID]+"Tuple.aggregates;\n";
    }

    // if (depth + 2 == varOrder.size())
    //     returnString += indent(3+depth)+"double count = upperptr_"+relName+
    //         "["+depthString+"] - lowerptr_"+relName+"["+depthString+"] + 1;\n";

    /**********************************************/
    /**********************************************/
    /**********************************************/
    /**********************************************/

    std::string resetString = "", loopString = "";
    loopString += genAggLoopStringCompressed(
        orderNode._loopProduct,node,depth,orderNode._joinViews,0,resetString);

    returnString += resetString + loopString;

    /**********************************************/
    /**********************************************/
    /**********************************************/
    
    if (MICRO_BENCH)
        returnString += indent(3+depth)+"END_MICRO_BENCH(Group"+
            std::to_string(group_id)+"_timer_aggregate);\n";
    
    if (depth+2 < varOrder.size())
    {
        // Join for next variable 
        returnString += genJoinAggregatesCode(group_id, varOrder, node, depth+1);
    }
    else
    {
        returnString += indent(3+depth)+"memset(addTuple, true, sizeof(bool) * "+
            std::to_string(varOrder.size())+");\n";
    }    
    

    /**********************************************/

    std::string postComputationString = "";

    size_t postOff = (depth+2 < varOrder.size() ? 4+depth : 3+depth);

    if (!orderNode._registeredOutgoingRunningSum.empty())
    {
        AggregateIndexes bench, aggIdx;
        std::bitset<7> increasingIndexes;

        bench.setIndexes(orderNode._registeredOutgoingRunningSum[0]);

        size_t offset = 0;
        for (size_t aggID = 1; aggID <
                 orderNode._registeredOutgoingRunningSum.size(); ++aggID)
        {   
            aggIdx.setIndexes(orderNode._registeredOutgoingRunningSum[aggID]);
            
            if (aggIdx.isIncrement(bench,offset,increasingIndexes))
            {
                ++offset;
            }
            else
            {
                // If not, output bench and all offset aggregates
                postComputationString += genRunSumAggregateString(
                    bench,offset,increasingIndexes,postOff);
                
                // make aggregate the bench and set offset to 0
                bench = aggIdx;
                offset = 0;
            }
        }

        // If not, output bench and all offset aggregates
        postComputationString += genRunSumAggregateString(
            bench,offset,increasingIndexes,postOff);
    }
    
    /************************************************************************/

    if (!orderNode._registeredRunningSum.empty())
    {
        AggregateIndexes bench, aggIdx;
        std::bitset<7> increasingIndexes;

        
        if (group_id == 2)
        {
            for (RunningSumAggregate* agg : orderNode._registeredRunningSum)
            {
                std::cout << " GROUP 2 --- RunningSUM AGG: " <<
                    agg->local << " --- " << agg->post  << " DEPTH "
                          << depth<< std::endl;
            }
        }

        bench.setIndexes(orderNode._registeredRunningSum.front());        

        size_t offset = 0;
        for (size_t aggID = 1; aggID <
                 orderNode._registeredRunningSum.size(); ++aggID)
        {
            aggIdx.setIndexes(orderNode._registeredRunningSum[aggID]);            

            if (aggIdx.isIncrement(bench,offset,increasingIndexes))
            {
                ++offset;
            }
            else
            {
                // If not, output bench and all offset aggregates
                postComputationString += genRunSumAggregateString(
                    bench,offset,increasingIndexes,postOff);
                        
                // make aggregate the bench and set offset to 0
                bench = aggIdx;
                offset = 0;
            }
        }

        // If not, output bench and all offset aggregates
        postComputationString += genRunSumAggregateString(
            bench,offset,increasingIndexes,postOff);
    }
    
    /************************************************************************/
    
    if (orderNode._outgoingLoopRegister != nullptr)
    {   
        resetString = "";
    
        loopString = genOutputLoopAggregateStringCompressed(
            orderNode._outgoingLoopRegister,node,depth,orderNode._joinViews,
            varOrder.size(),resetString);

        postComputationString += resetString+loopString;
    }

    /************************************************************************/

    // Adding postComputationString to returnString
    if (!postComputationString.empty())
    {
        if (MICRO_BENCH)
            returnString += indent(3+depth)+
                "BEGIN_MICRO_BENCH(Group"+std::to_string(group_id)+
                "_timer_post_aggregate);\n";

        // The computation below is only done if we have satisfying join values
        if (depth+2 < varOrder.size())
            returnString += indent(3+depth)+"if (addTuple["+depthString+"])\n"+
                indent(3+depth)+"{\n"+postComputationString+indent(3+depth)+"}\n";
        else
            returnString += postComputationString;

        if (MICRO_BENCH)
            returnString += indent(3+depth)+"END_MICRO_BENCH(Group"+
                std::to_string(group_id)+"_timer_post_aggregate);\n";
    }
    

    std::string atEndCondition = "";
    
    // Set lower to upper pointer 
    if (orderNode._registerdRelation != nullptr)
    {
        // Set upper = lower and update ranges
        returnString += indent(3+depth)+
            "lowerptr_"+relName+"["+depthString+"] = "+
            "upperptr_"+relName+"["+depthString+"] + 1;\n";

        atEndCondition += "lowerptr_"+relName+"["+depthString+"] > "+
            getUpperPointer(relName,depth,viewGroup._parallelize)+" ||";
    }

    for (size_t viewID : orderNode._joinViews)
    {
        returnString += indent(3+depth)+
            "lowerptr_"+viewName[viewID]+"["+depthString+"] = "+
            "upperptr_"+viewName[viewID]+"["+depthString+"] + 1;\n";

        atEndCondition += " lowerptr_"+viewName[viewID]+"["+depthString+"]>"+
            getUpperPointer(viewName[viewID],depth,viewGroup._parallelize)+" ||";
    }
    atEndCondition.pop_back();
    atEndCondition.pop_back();
    
    returnString += indent(3+depth)+"if ("+atEndCondition+")\n"+
        indent(4+depth)+"atEnd["+depthString+"] = true;\n";

    // Closing the while loop 
    return returnString + indent(2+depth)+"}\n";
}






std::string CppGenerator::genAggLoopStringCompressed(
    const LoopRegister* loop, const TDNode& node, size_t depth,
    const std::vector<size_t>& contributingViews,const size_t numOfViewLoops,
    std::string& resetString)
{
    size_t numOfLoops =  1 + numOfViewLoops;

    const std::string relName = node._relation->_name;
    
    std::string returnString = "";
    std::string depthString = std::to_string(depth);

    const std::vector<LocalFunctionProduct>& localProducts = loop->localProducts;
    const std::vector<LocalViewAggregateProduct>& viewAggregateProducts =
        loop->localViewAggregateProducts;
        
    // Print all products for this considered loop
    for (size_t prodID = 0; prodID < localProducts.size(); ++prodID)
    {
        const prod_bitset& product = localProducts[prodID].product;
        
        bool decomposedProduct = false;
        std::string productString = "";

        // We now check if parts of the product have already been computed 
        if (product.count() > 1)
        {
            prod_bitset subProduct = product;
            prod_bitset removedFunctions;

            size_t subProductID;

            bool foundSubProduct = false;
            while (!foundSubProduct && subProduct.count() > 1)
            {
                size_t highestPosition = 0;
                // need to continue with this the next loop
                for (size_t f = NUM_OF_FUNCTIONS; f > 0; f--)
                {
                    if (subProduct[f])
                    {
                        highestPosition = f;
                        break;
                    }
                }
                
                subProduct.reset(highestPosition);
                removedFunctions.set(highestPosition);

                subProductID = loop->getProduct(subProduct);
                if (subProductID < localProducts.size())
                {
                    foundSubProduct = true;
                }
            }
            

            if (foundSubProduct)
            {
                productString += "localRegister["+std::to_string(
                    localProducts[subProductID].aggregateIndex)+"]*";

                subProductID = loop->getProduct(subProduct);
                if (subProductID < localProducts.size() &&
                    subProductID < prodID)
                {
                    productString += "localRegister["+std::to_string(
                        localProducts[subProductID].aggregateIndex)+"]";
                }
                else
                {
                    productString += genProductString(
                        node,contributingViews,removedFunctions);
                }

                decomposedProduct = true;
            }
        }
        
        // We turn the product into a string


        if (product.none())
            productString = "upperptr_"+relName+
                "["+depthString+"] - lowerptr_"+relName+"["+depthString+"] + 1";    
        else if (!decomposedProduct)
            productString += genProductString(node,contributingViews,product);
        
        std::cout << offset(3+depth+numOfLoops)+"localRegister["+
            std::to_string(localProducts[prodID].aggregateIndex)+"] = "+
            productString + ";\n\n";
        
        returnString += offset(2+depth+numOfLoops)+"localRegister["+
            std::to_string(localProducts[prodID].aggregateIndex)+"] = "+
            productString + ";\n";
    }

    for (size_t viewProd = 0; viewProd < viewAggregateProducts.size(); ++viewProd)
    {   
        std::string viewProduct = "";
        for (const ViewAggregateIndex& viewAgg :
                 viewAggregateProducts[viewProd].product)
        {
            viewProduct += "aggregates_"+viewName[viewAgg.first]+"["+
                std::to_string(viewAgg.second)+"]*";
        }   
        viewProduct.pop_back();

        returnString += offset(2+depth+numOfLoops)+"localRegister["+
            std::to_string(viewAggregateProducts[viewProd].aggregateIndex)+"] = "+
            viewProduct +";\n";
    }

       if (COMPRESS_AGGREGATES && !loop->loopAggregateList.empty())
    {
        // TODO: check if there are any aggregates at all 

        AggregateIndexes bench, aggIdx;
        bench.setIndexes(loop, loop->loopAggregateList.front());

        std::bitset<7> increasingIndexes;
        size_t rangeSize = 0;

        // Loop over all other aggregates 
        for (size_t aggID = 1; aggID < loop->loopAggregateList.size(); ++aggID)
        {   
            aggIdx.setIndexes(loop,loop->loopAggregateList[aggID]);

            if (aggIdx.isIncrement(bench,rangeSize,increasingIndexes))
            {
                ++rangeSize;
            }
            else
            {
                // If not, output bench and the whole range of aggregates
                returnString += genLoopAggregateString(
                    bench,rangeSize,increasingIndexes,2+depth+numOfLoops);
                
                // make aggregate the bench and set offset to 0
                bench = aggIdx;
                rangeSize = 0;
            }
        }

        // Output bench for the last range of aggregates 
        returnString += genLoopAggregateString(
            bench,rangeSize,increasingIndexes,2+depth+numOfLoops);
    }
    else
    {        
        for (const LoopAggregate* loopAgg : loop->loopAggregateList)
        {        
            // We use a mapping to the correct index
            returnString += offset(2+depth+numOfLoops)+
                "aggregateRegister["+std::to_string(loopAgg->aggregateIndex)+"] = ";

            if (loopAgg->prevProduct != nullptr)
            {
                std::string prev = std::to_string(loopAgg->prevProduct->aggregateIndex);

                std::cout << "SETTING PREV PRODID TO: " << prev << " "
                          << loopAgg->prevProduct << std::endl;

                returnString += "aggregateRegister["+prev+"]*";
            }
                        
            // if (loopAgg.multiplyByCount)
            //     returnString +=  "count*";

            if (loopAgg->functionProduct > -1)
            {
                std::string local = std::to_string(
                    localProducts[loopAgg->functionProduct].aggregateIndex);
                
                returnString += "localRegister["+local+"]*";
            }
            
            if (loopAgg->viewProduct > -1)
            {
                // TODO: check if this loop aggregate can be inlined!!
                // if (regTuple.singleViewAgg)
                // {
                //    returnString += "aggregates_"+viewName[regTuple.viewAgg.first]+"["+
                //         std::to_string(regTuple.viewAgg.second)+"]*";
                // }
                size_t view = viewAggregateProducts[loopAgg->viewProduct].aggregateIndex;
                returnString += "localRegister["+std::to_string(view)+"];";
            }
            
            // if (loopAgg->innerProduct != nullptr)
            // {
            //     std::string prev = std::to_string(loopAgg->innerProduct->aggregateIndex);
            //     std::cout << "SETTING INNER PRODID TO: " << prev << " "
            //               << loopAgg->innerProduct << std::endl;
                
            //     returnString += "aggregateRegister["+prev+"]*";
            // }
            
            returnString.pop_back();
            returnString += ";\n";            
        }
    }

     // Recurse to the next loop(s) if possible
    for (const LoopRegister* nextLoop : loop->innerLoops)
    {
        std::string loopString = "", closeLoopString = "";
        if (nextLoop->loopID < _qc->numberOfViews())
        {
            size_t viewID = nextLoop->loopID;
           
            loopString += "\n"+
                offset(2+depth+numOfLoops)+"ptr_"+viewName[viewID]+
                " = lowerptr_"+viewName[viewID]+"["+depthString+"];\n"+
                offset(2+depth+numOfLoops)+"while(ptr_"+viewName[viewID]+
                " <= upperptr_"+viewName[viewID]+"["+depthString+"])\n"+
                offset(2+depth+numOfLoops)+"{\n"+
                offset(3+depth+numOfLoops)+viewName[viewID]+
                "_tuple &"+viewName[viewID]+"Tuple = "+viewName[viewID]+
                "[ptr_"+viewName[viewID]+"];\n"+
                offset(3+depth+numOfLoops)+"double *aggregates_"+viewName[viewID]+
                " = "+viewName[viewID]+"Tuple.aggregates;\n";
                
            closeLoopString += offset(3+depth+numOfLoops)+"++ptr_"+viewName[viewID]+
                ";\n"+offset(2+depth+numOfLoops)+"}\n"+closeLoopString;
        }
        else 
        {
            loopString += "\n"+
                offset(2+depth+numOfLoops)+"ptr_"+relName+
                " = lowerptr_"+relName+"["+depthString+"];\n"+
                offset(2+depth+numOfLoops)+"while(ptr_"+relName+
                " <= upperptr_"+relName+"["+depthString+"])\n"+
                offset(2+depth+numOfLoops)+"{\n"+
                offset(3+depth+numOfLoops)+relName +
                "_tuple &"+relName+"Tuple = "+relName+
                "[ptr_"+relName+"];\n";
                
            closeLoopString = offset(3+depth+numOfLoops)+"++ptr_"+relName+";\n"+
                offset(2+depth+numOfLoops)+"}\n"+closeLoopString;
        }

        std::string resetAggregates = "";

        loopString += genAggLoopStringCompressed(
            nextLoop, node, depth, contributingViews, 0, resetAggregates);
        
        returnString += resetAggregates + loopString + closeLoopString;
    }


    if (COMPRESS_AGGREGATES && !loop->loopRunningSums.empty())
    {
        AggregateIndexes bench, aggIdx;
        bench.setIndexes(loop->loopRunningSums.front());

        std::bitset<7> increasingIndexes;
        size_t rangeSize = 0;

        // Loop over all other aggregates 
        for (size_t aggID = 1; aggID < loop->loopRunningSums.size(); ++aggID)
        {   
            aggIdx.setIndexes(loop->loopRunningSums[aggID]);

            if (aggIdx.isIncrement(bench,rangeSize,increasingIndexes))
            {
                ++rangeSize;
            }
            else
            {
                // If not, output bench and the whole range of aggregates
                returnString += genLoopRunSumString(
                    bench,rangeSize,increasingIndexes,2+depth+numOfLoops);
                
                // make aggregate the bench and set offset to 0
                bench = aggIdx;
                rangeSize = 0;
            }
        }

        // Output bench for the last range of aggregates 
        returnString += genLoopRunSumString(
            bench,rangeSize,increasingIndexes,2+depth+numOfLoops);
    }
    else
    {
        for (const FinalLoopAggregate* loopAgg : loop->loopRunningSums)
        {
            // We use a mapping to the correct index
            returnString += offset(2+depth+numOfLoops)+
                "aggregateRegister["+std::to_string(loopAgg->aggregateIndex)+"] += ";

            if (loopAgg->loopAggregate != nullptr)
            {
                std::string index = std::to_string(
                    loopAgg->loopAggregate->aggregateIndex);
                returnString += "aggregateRegister["+index+"]*";
            }

            if (loopAgg->previousProduct != nullptr)
            {
                std::string index = std::to_string(
                    loopAgg->previousProduct->aggregateIndex);
                returnString += "aggregateRegister["+index+"]*";
            }

            if (loopAgg->innerProduct != nullptr)
            {
                std::string index = std::to_string(
                    loopAgg->innerProduct->aggregateIndex);
                returnString += "aggregateRegister["+index+"]*";
            }

            if (loopAgg->previousRunSum != nullptr)
            {
                std::string index = std::to_string(
                    loopAgg->previousRunSum->aggregateIndex);
                returnString += "runningSum["+index+"]*";
            }

            returnString.pop_back();
            returnString += ";\n";                    
        }
    }
    
    
    
    if (!loop->loopRunningSums.empty())
    {
        size_t startingOffset =  loop->loopRunningSums[0]->aggregateIndex;
        size_t resetOffset = (numOfLoops == 1 ? 3+depth : 2+depth+numOfLoops);
        
        resetString += offset(resetOffset)+"memset(&aggregateRegister["+
            std::to_string(startingOffset)+"], 0, sizeof(double) * "+
            std::to_string(loop->loopRunningSums.size())+");\n";
    }    
    
    return returnString;
}






std::string CppGenerator::genOutputLoopAggregateStringCompressed(
    const LoopRegister* loop, const TDNode& node, size_t depth,
    const std::vector<size_t>& contributingViews,const size_t numOfViewLoops,
    std::string& resetString)
{
    size_t numOfLoops =  1 + numOfViewLoops;

    const std::string relName = node._relation->_name;
    
    std::string returnString = "";
    std::string depthString = std::to_string(depth);

    const std::vector<LocalFunctionProduct>& localProducts = loop->localProducts;
    const std::vector<LocalViewAggregateProduct>& viewAggregateProducts =
        loop->localViewAggregateProducts;
        
    // Print all products for this considered loop
    for (const LocalFunctionProduct& localProduct : localProducts)
    {
        const prod_bitset& product = localProduct.product;

        // We turn the product into a string 
        std::string productString = "";

        if (product.any())
            productString = genProductString(node,contributingViews,product);
        else
            productString = "upperptr_"+relName+
                "["+depthString+"] - lowerptr_"+relName+"["+depthString+"] + 1";
        
        returnString += offset(3+depth+numOfLoops)+"localRegister["+
            std::to_string(localProduct.aggregateIndex)+"] = "+
            productString + ";\n";


        std::cout << offset(3+depth+numOfLoops)+"localRegister["+
            std::to_string(localProduct.aggregateIndex)+"] = "+
            productString + ";\n\n";
    }

    //size_t viewProd = 0; viewProd < viewAggregateProducts.size(); ++viewProd)
    for (const LocalViewAggregateProduct& viewAggProd : viewAggregateProducts)
    {   
        std::string viewProduct = "";
        for (const ViewAggregateIndex& viewAgg : viewAggProd.product)
        {
            viewProduct += "aggregates_"+viewName[viewAgg.first]+"["+
                std::to_string(viewAgg.second)+"]*";
        }
        viewProduct.pop_back();

        returnString += offset(3+depth+numOfLoops)+"localRegister["+
            std::to_string(viewAggProd.aggregateIndex)+"] = "+
            viewProduct +";\n";
    }    
    
    if (COMPRESS_AGGREGATES && !loop->loopAggregateList.empty())
    {
        // TODO: check if there are any aggregates at all
        AggregateIndexes bench, aggIdx;
        bench.setIndexes(loop, loop->loopAggregateList.front());

        std::bitset<7> increasingIndexes;
        size_t rangeSize = 0;

        // Loop over all other aggregates 
        for (size_t aggID = 1; aggID < loop->loopAggregateList.size(); ++aggID)
        {   
            aggIdx.setIndexes(loop,loop->loopAggregateList[aggID]);

            if (aggIdx.isIncrement(bench,rangeSize,increasingIndexes))
            {
                ++rangeSize;
            }
            else
            {
                // If not, output bench and the whole range of aggregates
                returnString += genLoopAggregateString(
                    bench,rangeSize,increasingIndexes,3+depth+numOfLoops);
                
                // make aggregate the bench and set offset to 0
                bench = aggIdx;
                rangeSize = 0;
            }
        }

        // Output bench for the last range of aggregates 
        returnString += genLoopAggregateString(
            bench,rangeSize,increasingIndexes,3+depth+numOfLoops);
    }
    else
    {        
        for (const LoopAggregate* loopAgg : loop->loopAggregateList)
        {        
            // We use a mapping to the correct index
            returnString += offset(3+depth+numOfLoops)+
                "aggregateRegister["+std::to_string(loopAgg->aggregateIndex)+"] = ";

            if (loopAgg->prevProduct != nullptr)
            {
                std::string prev = std::to_string(loopAgg->prevProduct->aggregateIndex);
                returnString += "aggregateRegister["+prev+"]*";
            }

            if (loopAgg->functionProduct > -1)
            {
                std::string local = std::to_string(
                    localProducts[loopAgg->functionProduct].aggregateIndex);
                
                returnString += "localRegister["+local+"]*";
            }
            
            if (loopAgg->viewProduct > -1)
            {
                // TODO: check if this loop aggregate can be inlined!!
                // if (regTuple.singleViewAgg)
                // {
                //    returnString += "aggregates_"+viewName[regTuple.viewAgg.first]+"["+
                //         std::to_string(regTuple.viewAgg.second)+"]*";
                // }
                size_t view = viewAggregateProducts[loopAgg->viewProduct].aggregateIndex;
                returnString += "localRegister["+std::to_string(view)+"];";
            }
            
            returnString.pop_back();
            returnString += ";\n";            
        }
    }

    size_t stringIndent = depth+numOfLoops-1;
    
    // Recurse to the next loop(s) if possible
    for (const LoopRegister* nextLoop : loop->innerLoops)
    {
        std::string loopString = "", closeLoopString = "";
        if (nextLoop->loopID < _qc->numberOfViews())
        {
            size_t viewID = nextLoop->loopID;
           
            loopString += "\n"+
                offset(stringIndent)+"ptr_"+viewName[viewID]+
                " = lowerptr_"+viewName[viewID]+"["+depthString+"];\n"+
                offset(stringIndent)+"while(ptr_"+viewName[viewID]+
                " <= upperptr_"+viewName[viewID]+"["+depthString+"])\n"+
                offset(stringIndent)+"{\n"+
                offset(1+stringIndent)+viewName[viewID]+
                "_tuple &"+viewName[viewID]+"Tuple = "+viewName[viewID]+
                "[ptr_"+viewName[viewID]+"];\n"+
                offset(1+stringIndent)+"double *aggregates_"+viewName[viewID]+
                " = "+viewName[viewID]+"Tuple.aggregates;\n";
                
            closeLoopString += offset(1+stringIndent)+"++ptr_"+viewName[viewID]+
                ";\n"+offset(stringIndent)+"}\n"+closeLoopString;
        }
        else 
        {
            loopString += "\n"+
                offset(stringIndent)+"ptr_"+relName+
                " = lowerptr_"+relName+"["+depthString+"];\n"+
                offset(stringIndent)+"while(ptr_"+relName+
                " <= upperptr_"+relName+"["+depthString+"])\n"+
                offset(stringIndent)+"{\n"+
                offset(1+stringIndent)+relName +
                "_tuple &"+relName+"Tuple = "+relName+
                "[ptr_"+relName+"];\n";
                
            closeLoopString = offset(1+stringIndent)+"++ptr_"+relName+";\n"+
                offset(stringIndent)+"}\n"+closeLoopString;
        }

        std::string resetAggregates = "";

        loopString += genOutputLoopAggregateStringCompressed(
            nextLoop, node, depth, contributingViews, numOfLoops, resetAggregates);
        
        returnString += resetAggregates + loopString + closeLoopString;
    }


    for (const OutgoingView& outView : loop->outgoingViews)
    {   
        View* view = _qc->getView(outView.viewID);
        
        // Add the code that outputs tuples for the views           
        std::string viewVars = "";            
        for (size_t v = 0; v < NUM_OF_VARIABLES; ++v)
        {
            if (view->_fVars[v])
            {
                if (node._relation->_schemaMask[v])
                {
                    viewVars += relName+"Tuple."+
                        _db->getAttribute(v)->_name+",";
                }
                else
                {
                    for (const size_t& incViewID : view->_incomingViews)
                    {
                        View* view = _qc->getView(incViewID);
                        if (view->_fVars[v])
                        {
                            viewVars += viewName[incViewID]+"Tuple."+
                                _db->getAttribute(v)->_name+",";
                            break;
                        }
                    }
                }
            }
        }
        if (!viewVars.empty())
            viewVars.pop_back();

        size_t offDepth = depth+numOfLoops-1;
            // (depth + 2 < varOrder.size() ?
            //                4+depth+numOfLoops : 3+depth+numOfLoops);

        if (_qc->requireHashing(outView.viewID))
        {
            returnString += "\n"+
                indent(offDepth)+viewName[outView.viewID]+"_iterator = "+
                viewName[outView.viewID]+"_map.find({"+viewVars+"});\n"+
                indent(offDepth)+"if ("+viewName[outView.viewID]+"_iterator != "+
                viewName[outView.viewID]+"_map.end())\n"+
                indent(offDepth+1)+"aggregates_"+viewName[outView.viewID]+" = &"+
                viewName[outView.viewID]+"_iterator->second.aggregates[0];\n"+
                indent(offDepth)+"else\n"+
                indent(offDepth+1)+"aggregates_"+viewName[outView.viewID]+" = &"+
                viewName[outView.viewID]+"_map.insert({{"+viewVars+
                "},{}}).first->second.aggregates[0];\n";
        }
        else
        {
            // just add string to push to view
            returnString += "\n"+indent(offDepth)+viewName[outView.viewID]+
                ".emplace_back("+viewVars+");\n"+indent(offDepth)+
                // ".push_back({"+viewVars+"});\n"+indent(offDepth)+
                "aggregates_"+viewName[outView.viewID]+" = "+viewName[outView.viewID]+
                ".back().aggregates;\n";
        }
                
        // TODO: set aggregates accordingly;


        if (COMPRESS_AGGREGATES)
        {
            AggregateIndexes bench, aggIdx;
            bench.setIndexes(outView.aggregates.front());

            std::bitset<7> increasingIndexes;
            size_t rangeSize = 0;

            // Loop over all other aggregates 
            for (size_t aggID = 1; aggID < outView.aggregates.size(); ++aggID)
            {   
                aggIdx.setIndexes(outView.aggregates[aggID]);

                if (aggIdx.isIncrement(bench,rangeSize,increasingIndexes))
                {
                    ++rangeSize;
                }
                else
                {
                    // If not, output bench and the whole range of aggregates
                    returnString += genLoopRunSumString(
                        bench,rangeSize,increasingIndexes,2+depth+numOfLoops,
                        "aggregates_"+viewName[outView.viewID]);
                
                    // make aggregate the bench and set offset to 0
                    bench = aggIdx;
                    rangeSize = 0;
                }
            }

            // Output bench for the last range of aggregates 
            returnString += genLoopRunSumString(
                bench,rangeSize,increasingIndexes,depth+numOfLoops-1,
                "aggregates_"+viewName[outView.viewID]);
        }
        else
        {
            for (const FinalLoopAggregate* loopAgg : outView.aggregates)
            {
                // We use a mapping to the correct index
                returnString += offset(depth+numOfLoops-1)+
                    "aggregates_"+viewName[outView.viewID]+
                    "["+std::to_string(loopAgg->aggregateIndex)+"] += ";

                if (loopAgg->loopAggregate != nullptr)
                {
                    std::string index = std::to_string(
                        loopAgg->loopAggregate->aggregateIndex);
                    returnString += "aggregateRegister["+index+"]*";
                }

                if (loopAgg->previousProduct != nullptr)
                {
                    std::string index = std::to_string(
                        loopAgg->previousProduct->aggregateIndex);
                    returnString += "aggregateRegister["+index+"]*";
                }

                if (loopAgg->innerProduct != nullptr)
                {
                    std::string index = std::to_string(
                        loopAgg->innerProduct->aggregateIndex);
                    returnString += "aggregateRegister["+index+"]*";
                }

                if (loopAgg->previousRunSum->local != nullptr)
                {
                    std::string index = std::to_string(
                        loopAgg->previousRunSum->local->
                        correspondingLoop.second->aggregateIndex);

                    returnString += "aggregateRegister["+index+"]*";
                }

                if (loopAgg->previousRunSum->post != nullptr)
                {
                    std::string index = std::to_string(
                        loopAgg->previousRunSum->aggregateIndex);
                    returnString += "runningSum["+index+"]*";
                }

                returnString.pop_back();
                returnString += ";\n";                    
            }
        }
    }
    returnString += "\n";                    


    if (!loop->loopAggregateList.empty())
    {
        size_t startingOffset =  loop->loopAggregateList[0]->aggregateIndex;

        size_t resetOffset = (numOfLoops == 0 ? 3+depth : 2+depth+numOfLoops);
        resetString += offset(resetOffset)+"memset(&aggregateRegister["+
            std::to_string(startingOffset)+"], 0, sizeof(double) * "+
            std::to_string(loop->loopAggregateList.size())+");\n";
    }
    else
    {
        ERROR("THERE IS A LOOP THAT DOESN'T COMPUTE ANY AGGREGATES!?! \n");
        // exit(1);
    }
    
    
    return returnString;
}







std::string CppGenerator::genProductString(
    const TDNode& node, const std::vector<size_t>& contributingViews,
    const prod_bitset& product)
{
    const std::string relName = node._relation->_name;
    
    std::string productString = "";
    
    // Turn each function into string
    for (size_t f = 0; f < NUM_OF_FUNCTIONS; f++)
    {
        if (product[f])
        {
            Function* function = _qc->getFunction(f);
            const var_bitset& functionVars = function->_fVars;

            std::string freeVarString = "";

            for (size_t v = 0; v < NUM_OF_VARIABLES; ++v)
            {                        
                if (functionVars[v])
                {
                    if (node._relation->_schemaMask[v])
                    {
                        freeVarString += relName+"Tuple."+
                            _db->getAttribute(v)->_name+",";                            
                    }
                    else
                    {
                        bool notFound = true;
                        for (const size_t &incViewID : contributingViews)
                        {
                            View* incView = _qc->getView(incViewID);

                            if (incView->_fVars[v])
                            {
                                freeVarString += viewName[incViewID]+"Tuple."+
                                    _db->getAttribute(v)->_name+",";
                                notFound = false;
                                break;
                            }                  
                        }

                        if (notFound)
                        {
                            ERROR("The loop hasn't been found - THIS IS A MISTAKE\n");
                            std::cout << relName << std::endl;
                            std::cout << product << std::endl;
                            std::cout << _db->getAttribute(v)->_name << std::endl;
                            exit(1);
                        }
                    }
                }
            }

            freeVarString.pop_back();
            productString += getFunctionString(function,freeVarString)+"*";
        }
    }
            
    if (!productString.empty())
        productString.pop_back();
    else
        productString += "1";
    
    return productString;
}
