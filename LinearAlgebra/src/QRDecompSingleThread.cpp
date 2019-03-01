#include <fstream>
#include <iostream>
#include "QRDecomp.h"

using namespace std;

namespace LMFAO::LinearAlgebra
{
    void QRDecompositionSingleThreaded::processCofactors(void)
    {
        if (mNumFeatsCat < 1)
            return;

        mCofactorPerFeature.resize(mNumFeatsExp - mNumFeatsCont);
        for (const Triple &triple : mCatVals)
        {
            unsigned int row = get<0>(triple);
            unsigned int col = get<1>(triple);
            unsigned int minIdx, maxIdx;
            double aggregate = get<2>(triple);
            minIdx = min(row, col);
            maxIdx = max(row, col);

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
            int a_max = max(get<0>(a), get<1>(a)), a_min = min(get<0>(a), get<1>(a));
            int b_max = max(get<0>(b), get<1>(b)), b_min = min(get<0>(b), get<1>(b));

            return tie(a_max, a_min) < tie(b_max, b_min);
        });

        // Sort the lists in Phi
        for (auto &_order : mCofactorPerFeature)
        {
            sort(begin(_order), end(_order));
        }
    }


    void QRDecompositionSingleThreaded::calculateCR(void)
    {
        // R(0,0) = Cofactor[1,1] (Note that the first row and column contain the label's aggregates)
        unsigned int T = mNumFeatsCont;
        unsigned int N = mNumFeatsExp;
        vector<long double> sigmaExpanded(T * T);

        expandSigma(sigmaExpanded, false /*isNaive*/);

        mR[0] = sigmaExpanded[0];

        // We skip k=0 since it contains the labels (dependent variable)
        for (unsigned int k = 1; k < N; k++)
        {
            unsigned int idxR = N * k;

            if (k < T)
            {
                for (unsigned int i = 0; i <= k - 1; i++)
                {
                    for (unsigned int l = 0; l <= i; l++)
                    {
                        mR[idxR + i] += mC[expIdx(l, i, N)] * sigmaExpanded[expIdx(l, k, T)];
                        // R(i,k) += mC(l, i) * Cofactor(l, k);
                    }
                }
            }
            else
            {
                for (unsigned int i = 0; i <= k - 1; i++)
                {
                    for (Pair tl : mCofactorPerFeature[k - T])
                    {
                        unsigned int l = get<0>(tl);

                        if (unlikely(l > i))
                            break;

                        mR[idxR + i] += mC[expIdx(l, i, N)] * get<1>(tl);
                        // R(i,k) += mC(l, i) * Cofactor(l, k);
                    }
                }
            }

            for (unsigned int j = 0; j <= k - 1; j++)
            {
                unsigned int rowIdx = N * j;

                // note that $i in \{ j, ..., k-1 \} -- i.e. mC is upper triangular
                for (unsigned int i = j; i <= k - 1; i++)
                {
                    mC[rowIdx + k] -= mR[idxR + i] * mC[rowIdx + i] / mR[expIdx(i, i, N)];
                    // mC[j,k] -= R(i,k) * mC(j, i) / R(i,i);
                }
            }
            // Sum of sigma submatrix for continuous values.
            long double D_k = 0; // stores R'(k,k)
            for (unsigned int l = 0; l <= min(k, T - 1); l++)
            {
                long double res = 0;
                for (unsigned p = 0; p <= min(k, T - 1); p++)
                {
                    res += mC[expIdx(p, k, N)] * sigmaExpanded[expIdx(l, p, T)];
                    // res += mC(p, k) * Cofactor(l, p);
                }

                D_k += mC[expIdx(l, k, N)] * res;
                // D_K += mC(l,k) * SUM[ mC(p,k) * Cofactor(l,p) ]
            }

            if (k >= T)
            {

                for (Triple tl : mCofactorList)
                {
                    unsigned int p = get<0>(tl);
                    unsigned int l = get<1>(tl);

                    long double agg = get<2>(tl);

                    if (unlikely(p > k || l > k))
                        break;

                    unsigned int factor = (p != l) ? 2 : 1;

                    D_k += factor * mC[expIdx(l, k, N)] * mC[expIdx(p, k, N)] * agg;
                    // D_k += mC(l, k) * mC(p, k) * Cofactor(l, p);
                }
            }

            if (k > 0)
            {
                mR[idxR + k] += D_k;
            }
        }
    }

    void QRDecompositionSingleThreaded::decompose(void)
    {
        // We omit the first column of each categorical column matrix because they are linearly
        // among themselves (for specific column).
        unsigned int N = mNumFeatsExp;

        processCofactors();

        // Used to store constants (A = ACR), initialises mC = Identity[N,N]
        mC.resize(N * N);
        for (unsigned int row = 0; row < N; row++)
        {
            mC[expIdx(row, row, N)] = 1;
        }
        // R is stored column-major
        mR.resize(N * N);
        calculateCR();

        // Normalise R' to obtain R
        for (unsigned int row = 0; row < N; row++)
        {
            //std::cout << "Norm" << mR[row * N + row] << std::endl;
            double norm = sqrt(mR[row * N + row]);
            for (unsigned int col = row; col < N; col++)
            {
                //std::cout << row << " " << col << mR[col * N + row] << std::endl;
                mR[col * N + row] /= norm;
            }
        }
    }
}
