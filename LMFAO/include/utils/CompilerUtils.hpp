//--------------------------------------------------------------------
//
// CompilerUtils.hpp
//
// Created on: 25 Jun 2016
// Author: PY
//
//--------------------------------------------------------------------

#ifndef INCLUDE_UTILS_COMPILERUTILS_HPP_
#define INCLUDE_UTILS_COMPILERUTILS_HPP_

/**
 * Used to define functions as hot: optimised more aggressively and placed into a special subsection of the text section.
 */
#ifdef __GNUC__
#define HOT __attribute__((hot))
#else
#define HOT
#endif

/**
 * Sorting algorithms: if GCC and non debug, use a parallel implementation, else use standard std algorithm.
 */
#if defined(__GNUC__) && defined(NDEBUG) && !defined(__clang__)
#include <parallel/algorithm>
#define sortingAlgorithm(begin, end, order) __gnu_parallel::sort(begin, end, order);
#else
#define sortingAlgorithm(begin, end, order) std::sort(begin, end, order);
#endif

/**
 * Used to help the compiler make branching predictions.
 */
#ifdef __GNUC__
#define likely(x) __builtin_expect((x),1)
#define unlikely(x) __builtin_expect((x),0)
#else
#define likely(x) x
#define unlikely(x) x
#endif

#endif /* INCLUDE_UTILS_COMPILERUTILS_HPP_ */
