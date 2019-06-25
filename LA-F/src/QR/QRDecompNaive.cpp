//#include <fstream>
#include <iostream>
#include "QR/QRDecompNaive.h"

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

        // mR.resize(N * N);
        mR = Eigen::VectorXd::Zero(N * N);
        calculateCR();

        normalizeRC();
    }
}
