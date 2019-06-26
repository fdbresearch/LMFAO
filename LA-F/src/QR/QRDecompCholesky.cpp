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
        if (!m_onlyR)
        {
            mC.resize(N * N);

            for (uint32_t row = 0; row < N; row++)
            {
                mC[expIdx(row, row, N)] = 1;
            }

            omp_set_num_threads(8);
            for (uint32_t k = 1; k < N; k++)
            {
                uint32_t idxRCol = N * k;
                #pragma omp parallel for
                for (uint32_t j = 0; j <= k - 1; j++)
                {
                    uint32_t rowIdx = N * j;

                    for (uint32_t i = j; i <= k - 1; i++)
                    {
                        mC[rowIdx + k] -= mR[idxRCol + i] * mC[rowIdx + i] / mR[expIdx(i, i, N)];
                    }
                }
            }

            for (uint32_t col = 0; col < N; col++)
            {
                double norm = mR[col * N + col];
                // In this loop we normalize columns by dividing all of them with the norm.
                for (uint32_t row = 0; row <= col; row++)
                {
                    mC[expIdx(row, col, N)] /= norm;
                    LMFAO_LOG_DBG("NORM", row, col,norm);
                    LMFAO_LOG_DBG("c:", row, col, N, mC[expIdx(row, col, N)]);
                }
            }
        }
    }
}
