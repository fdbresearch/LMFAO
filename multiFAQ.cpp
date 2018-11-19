//--------------------------------------------------------------------
//
// multiFAQ.cpp
//
// Created on: 19 Sept 2017
// Author: Max
//
//--------------------------------------------------------------------

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <thread>
#include <typeinfo>

#include <GlobalParams.hpp>
#include <Launcher.h>
#include <Logging.hpp>


int main(int argc, char *argv[])
{
   /* Object containing a description of all the options available in the
      program. */
   boost::program_options::options_description desc("multiFAQ - allowed options");
   desc.add_options()
      /* Option to show help. */
      ("help", "produce help message.")
      /* Option to show some info. */
      ("info", "show some information about the program.")
      /* Option for path to data and configuration files. */
      ("path", boost::program_options::value<std::string>(),
       "set path for data and configuration files - required.")
      /* Option for machine learning model. */
      ("model", boost::program_options::value<std::string>()->default_value("reg"),
       "model to be computed: reg (default), covar, ctree, rtree, cube, mi or perc")
      /* Option for code generator. */
      ("codegen", boost::program_options::value<std::string>()->default_value("cpp"),
       "open for code generation: cpp (default), or sql")
      /* Option for directory of generated code. */
      ("out",boost::program_options::value<std::string>()->default_value("runtime/cpp/"),
       "output directory for the generated code, default: runtime/{codegen}/")
      /* Option for parallellization. */
      ("parallel", boost::program_options::value<std::string>()->default_value("none"),
       "options for parallelization: none (default), task, domain, or both")
      /* Option for feature config file. */
      ("feat", boost::program_options::value<std::string>()->
       default_value("features.conf"),
       "features file for this model, assumed to be in {pathToFiles}/config/")
      /* Option for feature config file. */
      ("td", boost::program_options::value<std::string>()->
       default_value("treedecomposition.conf"),
       "tree decomposition config file, assumed to be in {pathToFiles}/config/")
       /* Option to turn off mutlti output operator. */
      ("mo", boost::program_options::value<bool>()->default_value("1"),
       "turn multioutput operator on (default)/off")
      /* Option to turn off mutlti output operator. */
      ("resort", "enables resorting of views / relations, requires multiout off.")
      /* Option to turn off mutlti output operator. */
      ("microbench", "enables micro benchmarking.");

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

   std::string pathString;

   /* Retrieve compulsory path. */
   if (vm.count("path"))
   {
      pathString = vm["path"].as<std::string>();

      /*If provided path is not a directory, return failure. */
      if (!boost::filesystem::is_directory(pathString))
      {
         ERROR("Provided path is not a directory. \n");
         return EXIT_FAILURE;   
      }
   }
   else
   {
      ERROR(
         "You must specify a path containing the database and " <<
         "configuration files.\n");
      ERROR(
         "Run program with --help for more information about " <<
         "command line options.\n");
      
      return EXIT_FAILURE;
   }


   /* Check the code generator is supported */

   std::string codeGenerator = vm["codegen"].as<std::string>();

   if (codeGenerator.compare("cpp") != 0 && codeGenerator.compare("sql") != 0)
   {
       ERROR("The code generator "+codeGenerator+" is not supported. \n");
       return EXIT_FAILURE;
   }

   std::string outputDirectory;

   /* Set the path for outDirectory */
   if (vm.count("out"))
       outputDirectory = vm["out"].as<std::string>();
   else
       outputDirectory = "runtime/"+codeGenerator+"/";

   /*If provided path is not a directory, return failure. */
   if (boost::filesystem::create_directories(outputDirectory))
   {
       DINFO("Output directory " << outputDirectory << " created." << std::endl);
   }
   else
   {    
       DINFO("Output directory " << outputDirectory << " exists." << std::endl);
   }
   
   std::shared_ptr<Launcher> launcher(new Launcher(pathString));

#ifdef BENCH
   int64_t start = std::chrono::duration_cast<std::chrono::milliseconds>(
       std::chrono::system_clock::now().time_since_epoch()).count();
#endif

   /* Launch program. */
   int result = launcher->launch(vm["model"].as<std::string>(),
                                 vm["codegen"].as<std::string>(),
                                 vm["parallel"].as<std::string>(),
                                 vm["feat"].as<std::string>(),
                                 vm["td"].as<std::string>(),
                                 outputDirectory,
                                 vm["mo"].as<bool>(),
                                 vm.count("resort"),
                                 vm.count("microbench")
       );
   
#ifdef BENCH
   int64_t end = std::chrono::duration_cast<std::chrono::milliseconds>(
       std::chrono::system_clock::now().time_since_epoch()).count() - start;
#endif

   BINFO("MAIN - overall time: " + std::to_string(end) + "ms.\n");
   
   DINFO("Completed execution \n");
   
   return result;
};
