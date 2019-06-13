//#include <fstream>
#include <iostream>
#include "QRDecompCholesky.h"
#include "Logger.h"

namespace LMFAO::LinearAlgebra
{
    void QRDecompCholesky::decompose(void)
    {
        Eigen::LLT<Eigen::MatrixXd> lltOfA(mSigma); // compute the Cholesky decomposition of A
        Eigen::MatrixXd L = lltOfA.matrixL();
        uint32_t N = mNumFeatsExp;
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
    }
}
