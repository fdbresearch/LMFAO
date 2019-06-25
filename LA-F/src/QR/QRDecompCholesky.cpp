//#include <fstream>
#include <iostream>
#include "QR/QRDecompCholesky.h"
#include "Utils/Logger.h"

#include <omp.h>

namespace LMFAO::LinearAlgebra
{
    void QRDecompCholesky::decompose(void)
    {
        Eigen::LLT<Eigen::MatrixXd> lltOfA(mSigma); // compute the Cholesky decomposition of A
        Eigen::MatrixXd L = lltOfA.matrixL();
        uint32_t N = mNumFeatsExp;
        //std::vector<double> sigmaExpanded(mNumFeatsExp * mNumFeatsExp);
        //expandSigma(sigmaExpanded, true /*isNaive*/);
        mR.resize(N, N);
        // TODO: Rethink about the order of storing operation (look at website)
        for (uint32_t row = 0; row < N; row++)
        {
            for (uint32_t col = 0; col < row; col++)
            {
                mR[col * N + row] = 0;
            }
            for (uint32_t col = row; col < N; col++)
            {
                mR[col * N + row] = L(col, row);
            }

        }
        /*
        mC.resize(N * N);

        for (uint32_t row = 0; row < N; row++)
        {
            mC[expIdx(row, row, N)] = 1;
        }


        omp_set_num_threads(8);
        for (uint32_t k = 1; k < N; k++)
        {
            uint32_t idxRCol = N * k;
            #pragma omp parallel
            #pragma omp for
            for (uint32_t j = 0; j <= k - 1; j++)
            {
                uint32_t rowIdx = N * j;

                for (uint32_t i = j; i <= k - 1; i++)
                {
                    //if (!mIsLinDepAllowed || (fabs(mR[i * N + i] >= mcPrecisionError)))
                    //{
                        mC[rowIdx + k] -= mR[idxRCol + i] * mC[rowIdx + i] / mR[expIdx(i, i, N)];
                    //}
                }
            }

        }
        */
    }
}
