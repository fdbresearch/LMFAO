#ifndef _LMFAO_LA_LOGGER_H_


// 0 - Debug
// 1 - Info
// 2 - Bench
// 3 - Warning
// 4 - Error

#define LMFAO_LOG_LEVEL 0

#include <iostream>

// TODO:
// 1) __FUNCTION__, __FILE__, __LINE__ flag for debugging
// 2) Add module definition for benchmark

template<typename Arg1, typename Arg2, typename ...Args>
void lmfaoLogStd(Arg1&& severity, Arg2&& arg, Args&& ...args)
{
    std::cout << severity << " ";
    std::cout << arg;
    ((std::cout << " " << std::forward<Args>(args)), ...) << std::endl;
}

template<typename Arg1, typename Arg2>
void lmfaoLogStd(Arg1&& severity, Arg2&& arg)
{
    std::cout << severity << " ";
    std::cout << arg << std::endl;
}

template<typename Arg, typename ...Args>
void lmfaoLogBench(Arg&& module, Args&& ...args)
{
    std::cout << "##" << module << "##";
    ((std::cout << "##" << std::forward<Args>(args)), ...) << std::endl;
}

#define LMFAO_LOG_MSG_INTERNAL(SEVERITY, ...) \
do                                            \
{                                             \
    lmfaoLogStd(#SEVERITY, __VA_ARGS__);      \
} while(0);

#define LMFAO_LOG_MSG_BENCH_INTERNAL(...) \
do                                        \
{                                         \
    lmfaoLogStd(__VA_ARGS__);             \
} while(0);

#define LMFAO_LOG_MSG_DBG(...)
#define LMFAO_LOG_MSG_INFO(...)
#define LMFAO_LOG_MSG_BENCH(...)
#define LMFAO_LOG_MSG_WARNING(...)
#define LMFAO_LOG_MSG_ERROR(...)


#if LMFAO_LOG_LEVEL < 1
#undef LMFAO_LOG_MSG_DBG
//#define LMFAO_LOG_MSG_DBG(...) LMFAO_LOG_MSG_INTERNAL(DEBUG, MSG)
#define LMFAO_LOG_MSG_DBG(...) LMFAO_LOG_MSG_INTERNAL(DEBUG, __VA_ARGS__)
#endif

#if LMFAO_LOG_LEVEL < 2
#undef LMFAO_LOG_MSG_INFO
#define LMFAO_LOG_MSG_INFO(...) LMFAO_LOG_MSG_INTERNAL(INFO, __VA_ARGS__)
#endif

#if LMFAO_LOG_LEVEL < 3
#undef LMFAO_LOG_MSG_BENCH
#define LMFAO_LOG_MSG_BENCH(...) LMFAO_LOG_MSG_BENCH_INTERNAL(__VA_ARGS__)
#endif

#if LMFAO_LOG_LEVEL < 4
#undef LMFAO_LOG_MSG_WARNING
#define LMFAO_LOG_MSG_WARNING(...) LMFAO_LOG_MSG_INTERNAL(WARNING, __VA_ARGS__)
#endif

#if LMFAO_LOG_LEVEL < 5
#undef LMFAO_LOG_MSG_ERROR
#define LMFAO_LOG_MSG_ERROR(...) LMFAO_LOG_MSG_INTERNAL(ERRROR, __VA_ARGS__)
#endif

#endif
