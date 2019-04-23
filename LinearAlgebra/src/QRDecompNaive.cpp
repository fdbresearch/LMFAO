//#include <fstream>
//#include <iostream>
#include "QRDecomp.h"

namespace LMFAO::LinearAlgebra
{
    void QRDecompositionNaive::calculateCR(void)
    {
        std::vector<double> sigmaExpanded(mNumFeatsExp * mNumFeatsExp);
        unsigned int N = mNumFeatsExp;
        expandSigma(sigmaExpanded, true /*isNaive*/);
        mR[0] = sigmaExpanded[0];
        
        // We skip k=0 since the ineer loops don't iterate over it. 
        for (unsigned int k = 1; k < N; k++)
        {
            unsigned int idxRCol = N * k;
            for (unsigned int i = 0; i <= k - 1; i++)
            {
                for (unsigned int l = 0; l <= i; l++)
                {
                    mR[idxRCol + i] += mC[N * l + i] * sigmaExpanded[N * l + k];
                }
            }

            for (unsigned int j = 0; j <= k - 1; j++)
            {
                unsigned int rowIdx = N * j;

                for (unsigned int i = j; i <= k - 1; i++)
                {
                    if (!mIsLinDepAllowed || (fabs(mR[i * N + i] >= mcPrecisionError)))
                    {
                        mC[rowIdx + k] -= mR[idxRCol + i] * mC[rowIdx + i] / mR[expIdx(i, i, N)];
                    }
                }
            }

            for (unsigned int l = 0; l <= k; l++)
            {
                double res = 0;
                for (unsigned int p = 0; p <= k; p++)
                {
                    res += mC[p * N + k] * sigmaExpanded[l * N + p];
                }
                mR[idxRCol + k] += mC[l * N + k] * res;
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
            //std::cout << "Norm: " << row << " " <<  mR[expIdx(row, row, N)] <<  std::endl;
            double norm = 1;;
            if (!mIsLinDepAllowed || (fabs(mR[expIdx(row, row, N)]) >= mcPrecisionError))
            {
                norm = sqrt(mR[expIdx(row, row, N)]);;
            }

            for (unsigned int col = row; col < N; col++)
            {
                //std::cout << row << " " << col << " " << mR[col * N + row] << std::endl;
                mR[col * N + row] /= norm;
            }
        }
    }
}
