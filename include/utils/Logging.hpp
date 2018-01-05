//--------------------------------------------------------------------
//
// Logging.hpp
//
// Created on: 23 May 2016
// Author: PY
//
//--------------------------------------------------------------------

#ifndef INCLUDE_LOGGING_LOGGING_HPP_
#define INCLUDE_LOGGING_LOGGING_HPP_

#include <chrono>
#include <iostream>

/**
 * Normal logging macros.
 */
#ifdef BENCH
#define INFO(x)
#else
#define INFO(x) \
   std::cout << x;
#endif
#define ERROR(x) \
   std::cerr << x;

/**
 * Debug logging macros. If release or benchmark, the compiler will just get rid
 * of the empty functions.
 */
#ifdef NDEBUG
#define DINFO(x)
#define DERROR(x)
#elif defined BENCH
#define DINFO(x)
#define DERROR(x)
#else
#define DINFO(x) \
   std::cout << x;
#define DERROR(x) \
   std::cout << x;
#endif

/**
 * Benchmarking logging macros. If release, the compiler will just get rid of
 * the empty functions.
 */
#ifdef BENCH
#define BINFO(x) \
   std::cout << x;
#else
#define BINFO(x)
#endif

#endif /* INCLUDE_LOGGING_LOGGING_HPP_ */
