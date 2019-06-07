//#include <fstream>
#include <iostream>
#include "QRDecompNaive.h"

namespace LMFAO::LinearAlgebra
{
    void QRDecompNaive::calculateCR(void)
    {
        std::vector<double> sigmaExpanded(mNumFeatsExp * mNumFeatsExp);
        uint32_t N = mNumFeatsExp;
        expandSigma(sigmaExpanded, true /*isNaive*/);
        mR[0] = sigmaExpanded[0];

        // We skip k=0 since the ineer loops don't iterate over it.
        for (uint32_t k = 1; k < N; k++)
        {
            uint32_t idxRCol = N * k;
            for (uint32_t i = 0; i <= k - 1; i++)
            {
                for (uint32_t l = 0; l <= i; l++)
                {
                    mR[idxRCol + i] += mC[N * l + i] * sigmaExpanded[N * l + k];
                }
            }

            for (uint32_t j = 0; j <= k - 1; j++)
            {
                uint32_t rowIdx = N * j;

                for (uint32_t i = j; i <= k - 1; i++)
                {
                    if (!mIsLinDepAllowed || (fabs(mR[i * N + i] >= mcPrecisionError)))
                    {
                        mC[rowIdx + k] -= mR[idxRCol + i] * mC[rowIdx + i] / mR[expIdx(i, i, N)];
                    }
                }
            }

            for (uint32_t l = 0; l <= k; l++)
            {
                double res = 0;
                for (uint32_t p = 0; p <= k; p++)
                {
                    res += mC[p * N + k] * sigmaExpanded[l * N + p];
                }
                mR[idxRCol + k] += mC[l * N + k] * res;
            }

        }
    }

    void QRDecompNaive::decompose(void)
    {
        uint32_t N = mNumFeatsExp;
        mC.resize(N * N);
        for (uint32_t row = 0; row < N; row++)
        {
            mC[expIdx(row, row, N)] = 1;
        }

        mR.resize(N * N);
        calculateCR();

        // Normalise R
        for (uint32_t row = 0; row < N; row++)
        {
            //std::cout << "Norm: " << row << " " <<  mR[expIdx(row, row, N)] <<  std::endl;
            double norm = 1;;
            if (!mIsLinDepAllowed || (fabs(mR[expIdx(row, row, N)]) >= mcPrecisionError))
            {
                norm = sqrt(mR[expIdx(row, row, N)]);;
            }
            /*
            for (uint32_t col = 0; col < row; col++)
            {
                std::cout << "0.0 ";
            }
            */

            for (uint32_t col = row; col < N; col++)
            {
                //std::cout << row << " " << col << " " << mR[col * N + row] << std::endl;
                mR[col * N + row] /= norm;
                //std::cout << mR[col * N + row] << " ";
            }
            //std::cout << std::endl;
        }
    }
}
