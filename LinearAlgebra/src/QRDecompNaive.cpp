#include <fstream>
#include <iostream>
#include "QRDecomposition.h"

using namespace std;

namespace LMFAO::LinearAlgebra
{
    void QRDecompositionNaive::calculateCR(void)
    {
        vector<long double> sigmaExpanded(mNumFeatsExp * mNumFeatsExp);
        unsigned int N = mNumFeatsExp;

        expandSigma(sigmaExpanded, true /*isNaive*/);
        // R(0,0) = Cofactor[1,1] (skips the intercept row because of linear dependency)
        mR[0] = sigmaExpanded[N + 1];

        // We skip k=0 since it contains the labels (dependent variable)
        for (unsigned int k = 1; k < N; k++)
        {
            unsigned int idxR = (N - 1) * (k - 1);
            for (unsigned int i = 1; i < k; i++)
            {
                for (unsigned int l = 1; l <= i; l++)
                {
                    mR[idxR + i - 1] += mC[N * l + i] * sigmaExpanded[N * l + k];
                }
            }

            for (unsigned int j = 1; j < k; j++)
            {
                unsigned int rowIdx = N * j;

                for (unsigned int i = j; i < k; i++)
                {
                    mC[rowIdx + k] -= mR[idxR + i - 1] * mC[rowIdx + i] / mR[(i - 1) * (N - 1) + i - 1];
                }
            }

            if (k > 1)
            {
                for (unsigned int l = 1; l <= k; l++)
                {

                    double res = 0;
                    for (unsigned int p = 1; p <= k; p++)
                    {
                        res += mC[p * N + k] * sigmaExpanded[l * N + p];
                    }
                    mR[idxR + k - 1] += mC[l * N + k] * res;
                }
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

        // Initialises column-major matrix R
        mR.resize((N - 1) * (N - 1));
        calculateCR();

        mSigma.resize(N - 1, N - 1);
        mSigma = Eigen::MatrixXd::Zero(mSigma.rows(), mSigma.cols());

        // Normalise R
        for (unsigned int row = 0; row < N - 1; row++)
        {
            double norm = sqrt(mR[expIdx(row, row, N - 1)]);
            cout << "NormBef " << mR[row * (N - 1) + row] << std::endl;
            for (unsigned int col = row; col < N - 1; col++)
            {
                mR[col * (N - 1) + row] /= norm;
                mSigma(row, col) = mR[col * (N - 1) + row];
            }
        }

        Eigen::BDCSVD<Eigen::MatrixXd> svd(mSigma, Eigen::ComputeFullU | Eigen::ComputeFullV);

        cout << "Its singular values are:" << endl
            << svd.singularValues() << endl;
        cout << "Its left singular vectors are the columns of the thin U matrix:" << endl
            << svd.matrixU() << endl;
        cout << "Its right singular vectors are the columns of the thin V matrix:" << endl
            << svd.matrixV() << endl;
    }
}
