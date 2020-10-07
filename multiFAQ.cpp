//--------------------------------------------------------------------
//
// multiFAQ.cpp
//
// Created on: 19 Sept 2017
// Author: Max
//
//--------------------------------------------------------------------

#include <boost/filesystem.hpp>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <thread>
#include <typeinfo>

#include <GlobalParams.hpp>
#include <Launcher.h>
#include <Logging.hpp>

std::string multifaq::dir::PATH_TO_DATA;
std::string multifaq::dir::PATH_TO_FILES;
std::string multifaq::dir::DATASET_NAME;
std::string multifaq::dir::OUTPUT_DIRECTORY;

int main(int argc, char *argv[])
{
   /* Object containing a description of all the options available in the
      program. */
   boost::program_options::options_description desc("multiFAQ - allowed options");
   desc.add_options()
       /* Option to show help. */
       ("help,h", "produce help message.")
       /* Option to show some info. */
       ("info", "show some information about the program.")
       /* Option for path to data and configuration files. */
       ("path,p", boost::program_options::value<std::string>(),
        "set path for data and configuration files - required.")
       /* Option for path to data and configuration files. */
       ("files,f", boost::program_options::value<std::string>(),
        "set path for config files - if different from path to data.")     
       /* Option for feature config file. */
       ("feat", boost::program_options::value<std::string>()->
        default_value("features.conf"),
        "features file for this model, assumed to be in pathToFiles")
       /* Option for treedecomposition config file. */
       ("td", boost::program_options::value<std::string>()->
        default_value("treedecomposition.conf"),
        "tree decomposition config file, assumed to be in pathToFiles")
       /* Option for treedecomposition config file. */
       ("schema", boost::program_options::value<std::string>()->
        default_value("schema.conf"),
        "schmea config file, assumed to be in pathToFiles")
       /* Option for machine learning model. */
       ("model,m", boost::program_options::value<std::string>()->default_value("covar"),
        "model to be computed: reg, covar (default), ctree, rtree, cube, mi, or perc")
       /* Option for code generator. */
       ("codegen,g", boost::program_options::value<std::string>()->default_value("cpp"),
        "open for code generation: cpp (default), or sql")
       /* Option for directory of generated code. */
       ("out,o",boost::program_options::value<std::string>(),
        "output directory for the generated code, default: runtime/{codegen}/")
       /* Option for parallellization. */
       ("parallel", boost::program_options::value<std::string>()->default_value("none"),
        "options for parallelization: none (default), task, domain, or both")
       /* Option to turn off mutlti output operator. */
       ("mo", boost::program_options::value<bool>()->default_value("1"),
        "turn multioutput operator on (default)/off")
       /* Option to turn off compression of aggregates operator. */
       ("compress", boost::program_options::value<bool>()->default_value("1"),
        "turn compression of aggregates on (default)/off")
       /* Option to turn off mutlti output operator. */
       ("resort", "enables resorting of views / relations, requires multiout off.")
       /* Option to turn off mutlti output operator. */
       ("microbench", "enables micro benchmarking.")
       /* Option to turn off mutlti output operator. */
       ("bench_individual", "enables benchmarking for each group individually.")
       ("clusters,k", boost::program_options::value<size_t>()->default_value(3),
        "k for k-means algorithm. (Default = 3).")
       ("kappa", boost::program_options::value<size_t>(),
        "kappa for k-means algorithm. (Default = k).")
       /* Option for parallellization. */
       ("degree", boost::program_options::value<int>()->default_value(1),
        "Degree of interactions for regression models and FMs. (Default = 1).");


   /* Register previous options and do command line parsing. */
   boost::program_options::variables_map vm;
   boost::program_options::store(
       boost::program_options::parse_command_line(argc, argv, desc), vm);
   boost::program_options::notify(vm);

   /* Display help. */
   if (vm.count("help"))
   {
      std::cout << desc << "\n";
      return EXIT_SUCCESS;
   }

   /* Display info. */
   if (vm.count("info"))
   {
      std::cout << "LMFAO - 0.1 Edition\n";
      std::cout << "Compiled on " << __DATE__ << " at " << __TIME__ << ".\n";
#ifdef BENCH
      std::cout << "Build type: Benchmark.\n";
#elif defined NDEBUG
      std::cout << "Build type: Release.\n";
#else
      std::cout << "Build type: Debug.\n";
#endif
      return EXIT_SUCCESS;
   }

   
   boost::filesystem::path pathToData;
   
   /* Retrieve compulsory path. */
   if (vm.count("path"))
   {
       
       pathToData = boost::filesystem::canonical(vm["path"].as<std::string>());

       /*If provided path is not a directory, return failure. */
       if (!boost::filesystem::is_directory(pathToData))
       {
           ERROR("Provided path is not a directory. \n");
           return EXIT_FAILURE;
       }
   }
   else
   {
      ERROR(
         "You must specify a path containing the database and configuration files.\n" <<
         "Run program with --help for more information about command line options.\n");
      return EXIT_FAILURE;
   }   

   boost::filesystem::path pathToFiles;

   /* Retrieve path to files. */
   if (vm.count("files"))
   {
       pathToFiles = boost::filesystem::canonical(vm["files"].as<std::string>());

       /*If provided path is not a directory, return failure. */
       if (!boost::filesystem::is_directory(pathToFiles))
       {
           ERROR("Provided path to files is not a directory. \n");
           return EXIT_FAILURE;
       }
   }
   else
   {
       pathToFiles = pathToData;
   }   

   /* Check the code generator is supported */
   std::string codeGenerator = vm["codegen"].as<std::string>();

   if (codeGenerator.compare("cpp") != 0 && codeGenerator.compare("sql") != 0)
   {
       ERROR("The code generator "+codeGenerator+" is not supported. \n");
       return EXIT_FAILURE;
   }


   /* Set the path for outDirectory */
   std::string outputDirectory;
   if (vm.count("out"))
       outputDirectory = boost::filesystem::weakly_canonical(
           vm["out"].as<std::string>()).string()+"/";
   else
       outputDirectory = "runtime/"+codeGenerator+"/";
   
   /*If provided path is not a directory, create it. */
   if (boost::filesystem::create_directories(outputDirectory))
   {   
       DINFO("INFO: Output directory " << outputDirectory << " created." << std::endl);
   }
   else
   {    
       DINFO("INFO: Output directory " << outputDirectory << " exists." << std::endl);
   }
   
           
   /* Setting global parameters for directories */
   multifaq::dir::PATH_TO_DATA = pathToData.string();
   multifaq::dir::DATASET_NAME = pathToData.filename().string();
   multifaq::dir::PATH_TO_FILES = pathToFiles.string();
   multifaq::dir::OUTPUT_DIRECTORY = outputDirectory;


   /* Create and run Launcher */
   std::shared_ptr<Launcher> launcher(new Launcher());

#ifdef BENCH
   int64_t start = std::chrono::duration_cast<std::chrono::milliseconds>(
       std::chrono::system_clock::now().time_since_epoch()).count();
#endif

   /* Launch program. */
   int result = launcher->launch(vm);

#ifdef BENCH
   int64_t end = std::chrono::duration_cast<std::chrono::milliseconds>(
       std::chrono::system_clock::now().time_since_epoch()).count() - start;
#endif

   BINFO("BENCH - overall compilation time: " + std::to_string(end) + "ms.\n");
   DINFO("INFO: Completed execution \n");
   
   std::cout << "----------------------------------------" << std::endl;
   std::cout << "The generated code was output to: " << outputDirectory << std::endl;
   std::cout << "Run the following commands to execute code: "  << std::endl;
   std::cout << "   cd " << outputDirectory << std::endl;
   std::cout << "   make -j " << std::endl;
   std::cout << "   ./lmfao " << std::endl;
   
   return result;
};
