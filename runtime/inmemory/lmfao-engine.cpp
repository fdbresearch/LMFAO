//--------------------------------------------------------------------
//
// lmfao-engine.cpp
//
// Created on: 9 Jan 2018
// Author: Max
//
//--------------------------------------------------------------------

#include "Engine.hpp"

int main(int argc, char *argv[])
{

#ifndef MULTITHREAD
    std::cout << "run lmfao\n";
    lmfao::run();
#else
    std::cout << "run multithreaded lmfao\n";
    lmfao::runMultithreaded();
#endif
    
    return 0;
};
