//#include <fstream>
//#include <iostream>
#include "QRDecomp.h"

namespace LMFAO::LinearAlgebra
{
    void QRDecompositionNaive::calculateCR(void)
    {
        std::vector<long double> sigmaExpanded(mNumFeatsExp * mNumFeatsExp);
        unsigned int N = mNumFeatsExp;
        expandSigma(sigmaExpanded, true /*isNaive*/);
        mR[0] = sigmaExpanded[0];
        // We skip k=0 since it contains necessary values
        for (unsigned int k = 1; k < N; k++)
        {
            unsigned int idxR = N * k;
            for (unsigned int i = 0; i <= k - 1; i++)
            {
                for (unsigned int l = 0; l <= i; l++)
                {
                    mR[idxR + i] += mC[N * l + i] * sigmaExpanded[N * l + k];
                }
            }

            for (unsigned int j = 0; j <= k - 1; j++)
            {
                unsigned int rowIdx = N * j;

                for (unsigned int i = j; i <= k - 1; i++)
                {
                    mC[rowIdx + k] -= mR[idxR + i] * mC[rowIdx + i] / mR[i * N + i];
                }
            }

            for (unsigned int l = 0; l <= k; l++)
            {
                double res = 0;
                for (unsigned int p = 0; p <= k; p++)
                {
                    res += mC[p * N + k] * sigmaExpanded[l * N + p];
                }
                mR[idxR + k] += mC[l * N + k] * res;
            }
        }
    }

    void QRDecompositionNaive::decompose(void)
    {
        unsigned int N = mNumFeatsExp;
        mC.resize(N * N);
        for (unsigned int row = 0; row < N; row++)
        {
            mC[expIdx(row, row, N)] = 1;
        }

        mR.resize(N * N);
        calculateCR();

        // Normalise R
        for (unsigned int row = 0; row < N; row++)
        {
            //std::cout << "Norm" << mR[expIdx(row, row, N)] <<  std::endl;
            double norm = sqrt(mR[expIdx(row, row, N)]);
            for (unsigned int col = row; col < N; col++)
            {
                //std::cout << row << " " << col << mR[col * N + row] << std::endl;
                mR[col * N + row] /= norm;
            }
        }
    }
}
