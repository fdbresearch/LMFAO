#include <iostream>
#include <fstream>
#include <chrono>
#include <random>
#include <algorithm>
#include <vector>

#include "QRDecomposition.h"

using namespace std;

static const string FILE_INPUT = "test.in";
using namespace LMFAO::LinearAlgebra;

void unitTestNaive() 
{
    QRDecompositionNaive qrDecomposition(FILE_INPUT);
    //QRDecompositionSingleThreaded  qrDecomposition(FILE_INPUT);
    auto begin_timer = std::chrono::high_resolution_clock::now();
    qrDecomposition.decompose();
    auto end_timer = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed_time = end_timer - begin_timer;
    double time_spent = elapsed_time.count();
    std::cout << "Elapsed time time is:" << time_spent << std::endl;
}

int main()
{
    unitTestNaive();

    return 0;
}
