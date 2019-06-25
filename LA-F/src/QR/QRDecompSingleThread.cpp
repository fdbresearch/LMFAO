//#include <fstream>
//#include <iostream>
#include "QR/QRDecompSingleThread.h"

namespace LMFAO::LinearAlgebra
{
    void QRDecompSingleThread::processCofactors(void)
    {
        if (mNumFeatsCat < 1)
            return;

        mCofactorPerFeature.resize(mNumFeatsExp - mNumFeatsCont);
        for (const Triple &triple : *m_pvCatVals)
        {
            uint32_t row = std::get<0>(triple);
            uint32_t col = std::get<1>(triple);
            uint32_t minIdx, maxIdx;
            double aggregate = std::get<2>(triple);
            minIdx = std::min(row, col);
            maxIdx = std::max(row, col);

            // Because matrix is symetric, we don't need need to insert two
            // times in aggregates for one feature, also we skip intercept row.
            if (row >= col)
            {
                mCofactorList.emplace_back(minIdx, maxIdx, aggregate);
                mCofactorPerFeature[maxIdx - mNumFeatsCont].emplace_back(minIdx, aggregate);
            }
        }

        // Sort cofactorList (Sigma) colexicographically
        sort(begin(mCofactorList), end(mCofactorList), [](const Triple &a, const Triple &b) -> bool {
            uint32_t a_max = std::max(std::get<0>(a), std::get<1>(a));
            uint32_t a_min = std::min(std::get<0>(a), std::get<1>(a));
            uint32_t b_max = std::max(std::get<0>(b), std::get<1>(b));
            uint32_t b_min = std::min(std::get<0>(b), std::get<1>(b));

            return std::tie(a_max, a_min) < std::tie(b_max, b_min);
        });

        // Sort the lists in Phi
        for (auto &_order : mCofactorPerFeature)
        {
            sort(begin(_order), end(_order));
        }
    }


    void QRDecompSingleThread::calculateCR(void)
    {
        // R(0,0) = Cofactor[1,1] (Note that the first row and column contain the label's aggregates)
        uint32_t T = mNumFeatsCont;
        uint32_t N = mNumFeatsExp;
        std::vector<double> sigmaExpanded(T * T);

        expandSigma(sigmaExpanded, false /*isNaive*/);

        mR[0] = sigmaExpanded[0];

        // We skip k=0 since the ineer loops don't iterate over it.
        for (uint32_t k = 1; k < N; k++)
        {
            uint32_t idxRCol = N * k;

            if (k < T)
            {
                for (uint32_t i = 0; i <= k - 1; i++)
                {
                    for (uint32_t l = 0; l <= i; l++)
                    {
                        mR[idxRCol + i] += mC[expIdx(l, i, N)] * sigmaExpanded[expIdx(l, k, T)];
                        // R(i,k) += mC(l, i) * Cofactor(l, k);
                    }
                }
            }
            else
            {
                for (uint32_t i = 0; i <= k - 1; i++)
                {
                    for (Pair tl : mCofactorPerFeature[k - T])
                    {
                        uint32_t l = std::get<0>(tl);

                        if (unlikely(l > i))
                            break;

                        mR[idxRCol + i] += mC[expIdx(l, i, N)] * std::get<1>(tl);
                        // R(i,k) += mC(l, i) * Cofactor(l, k);
                    }
                }
            }

            for (uint32_t j = 0; j <= k - 1; j++)
            {
                uint32_t rowIdx = N * j;

                // note that $i in \{ j, ..., k-1 \} -- i.e. mC is upper triangular
                for (uint32_t i = j; i <= k - 1; i++)
                {
                    if (!mIsLinDepAllowed || fabs((mR[expIdx(i, i, N)]) >= mcPrecisionError))
                    {
                        mC[rowIdx + k] -= mR[idxRCol + i] * mC[rowIdx + i] / mR[expIdx(i, i, N)];
                    }
                    // mC[j,k] -= R(i,k) * mC(j, i) / R(i,i);
                }
            }
            // Sum of sigma submatrix for continuous values.
            double D_k = 0; // stores R'(k,k)
            for (uint32_t l = 0; l <= std::min(k, T - 1); l++)
            {
                double res = 0;
                for (unsigned p = 0; p <= std::min(k, T - 1); p++)
                {
                    res += mC[expIdx(p, k, N)] * sigmaExpanded[expIdx(l, p, T)];
                    // res += mC(p, k) * Cofactor(l, p);
                }

                D_k += mC[expIdx(l, k, N)] * res;
                // D_K += mC(l,k) * SUM[ mC(p,k) * Cofactor(l,p) ]
            }

            if (k >= T)
            {

                for (const Triple& tl: mCofactorList)
                {
                    uint32_t p = std::get<0>(tl);
                    uint32_t l = std::get<1>(tl);

                    double agg = std::get<2>(tl);

                    if (unlikely(p > k || l > k))
                        break;

                    uint32_t factor = (p != l) ? 2 : 1;

                    D_k += factor * mC[expIdx(l, k, N)] * mC[expIdx(p, k, N)] * agg;
                    // D_k += mC(l, k) * mC(p, k) * Cofactor(l, p);
                }
            }

            if (k > 0)
            {
                mR[idxRCol + k] += D_k;
            }
        }
    }

    void QRDecompSingleThread::decompose(void)
    {
        // We omit the first column of each categorical column matrix because they are linearly
        // among themselves (for specific column).
        uint32_t N = mNumFeatsExp;

        processCofactors();

        // Used to store constants (A = ACR), initialises mC = Identity[N,N]
        mC.resize(N * N);
        for (uint32_t row = 0; row < N; row++)
        {
            mC[expIdx(row, row, N)] = 1;
        }
        // R is stored column-major
        //mR.resize(N * N);
        mR = Eigen::VectorXd::Zero(N * N);
        calculateCR();
        normalizeRC();
    }
}
