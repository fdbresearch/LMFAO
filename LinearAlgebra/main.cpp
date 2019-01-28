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
    auto begin_timer = std::chrono::high_resolution_clock::now();
    qrDecomposition.decompose();
    auto end_timer = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed_time = end_timer - begin_timer;
    double time_spent = elapsed_time.count();
    std::cout << "Elapsed time time is:" << time_spent << std::endl;
}
/*
void unitTestSingleThreaded()
{
    vector<double> C(N * N);
    for (int r = 0; r < N; r++) {
        C[r*N + r] = 1;
    }

    // R is stored column-major
    _R.resize((N-1)*(N-1));

    // R(0,0) = Cofactor[1,1] (Note that the first row and column contain the label's aggregates)
    _R[0] = _cofactorMatrix[T+1];

    doWork(C);

    // Normalise R' to obtain R
    for (size_t row = 0; row < N-1; row++) {
        double norm = sqrt(_R[row * (N-1) + row]);
        for (size_t col = row; col < N-1; col++) {
            _R[col*(N-1) + row] /= norm;
        }
    }
} 
*/

int main()
{
    unitTestNaive();

    return 0;
}
