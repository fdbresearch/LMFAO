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

#include <Launcher.h>
#include <Logging.hpp>

// #include <CppGenerator.hpp>

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
       "operation to be computed: reg (default), covar, ctree, rtree, or perc")
      /* Option for code generator. */
      ("codegen", boost::program_options::value<std::string>()->default_value("cpp"),
       "open for code generation: cpp (default), or sql")
      /* Option for parallellization. */
      ("parallel", boost::program_options::value<std::string>()->default_value("none"),
       "options for parallelization: none, task, domain, or both");
   
      // /* Option for number of threads used for F cofactor calculation; default
      //  * set to number of hardware thread contexts. */
      // ("threads",
      //  boost::program_options::value<unsigned int>()->default_value(
      //     std::thread::hardware_concurrency()),
      //  "set number of threads; select 1 for single-threading; automatic multi-threading avail.")
      // /* Option for number of partitions used for F cofactor calculation. */
      // ("partitions", boost::program_options::value<unsigned int>(),
      //  "set number of partitions for cofactor calculation; default is one partition per thread.")
      // /* Option for IP address to current node. */
      // ("ip", boost::program_options::value<std::string>()->default_value(""),
      //  "set IP of the current node; do not set for automatic resolution of address.");

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
      std::cout << "DFDB - F Edition\n";
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
         ERROR(
             "Provided path is not a directory. \n");
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

   std::shared_ptr<Launcher> launcher(new Launcher(pathString));

#ifdef BENCH
   int64_t start = std::chrono::duration_cast<std::chrono::milliseconds>(
       std::chrono::system_clock::now().time_since_epoch()).count();
#endif

   /* Launch program. */
   int result = launcher->launch(vm["model"].as<std::string>(),
                                 vm["codegen"].as<std::string>(),
                                 vm["parallel"].as<std::string>());
   
#ifdef BENCH
   int64_t end = std::chrono::duration_cast<std::chrono::milliseconds>(
       std::chrono::system_clock::now().time_since_epoch()).count() - start;
#endif

   BINFO("MAIN - overall time: " + std::to_string(end) + "ms.\n");
   
   DINFO("Completed execution \n");
   
   return result;
};
