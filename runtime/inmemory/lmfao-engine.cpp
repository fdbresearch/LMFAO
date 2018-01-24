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
    lmfao::run();
#else
     lmfao::runMultithreaded();
#endif
    
    return 0;
};
