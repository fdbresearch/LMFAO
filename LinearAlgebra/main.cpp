#include <iostream>
#include <fstream>
#include <chrono>
#include <random>
#include <algorithm>
#include <vector>

#include "SVDecomp.h"
using namespace std;

static const string FILE_INPUT = "test.in";
using namespace LMFAO::LinearAlgebra;

void unitTest() 
{
    SVDecomp aSvdecomp[] = {
        SVDecomp(FILE_INPUT, SVDecomp::DecompType::NAIVE),
        SVDecomp(FILE_INPUT, SVDecomp::DecompType::SINGLE_THREAD)
        };
    for (SVDecomp& svdDecomp: aSvdecomp)
    {
        auto begin_timer = std::chrono::high_resolution_clock::now();
        svdDecomp.decompose();
        auto end_timer = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed_time = end_timer - begin_timer;
        double time_spent = elapsed_time.count();
        std::cout << "Elapsed time time is:" << time_spent << std::endl;
    }
}

int main()
{
    unitTest();

    return 0;
}
