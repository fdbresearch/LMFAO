#include <fstream>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <limits>
#include <thread>
#include "QRDecompMultiThread.h"

namespace LMFAO::LinearAlgebra
{
    void QRDecompMultiThread::processCofactors(void)
    {
        if (mNumFeatsCat < 1)
            return;

        mCofactorPerFeature.resize(mNumFeatsExp - mNumFeatsCont);
        for (const Triple &triple : mCatVals)
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


    void QRDecompMultiThread::calculateCR(uint32_t threadId)
    {
        // R(0,0) = Cofactor[1,1] (Note that the first row and column contain the label's aggregates)
        uint32_t T = mNumFeatsCont;
        uint32_t N = mNumFeatsExp;
        uint32_t step = mNumThreads;
        uint32_t start = threadId;
        std::vector<double> sigmaExpanded(T * T);

        expandSigma(sigmaExpanded, false /*isNaive*/);

        mR[0] = sigmaExpanded[0];
        //std::cout << "TID " << threadId << std::endl;
        //auto begin_timer = std::chrono::high_resolution_clock::now();
        //auto end_timer = std::chrono::high_resolution_clock::now();
        //std::chrono::duration<double> elapsed_time = end_timer - begin_timer;
        //double time_spent = elapsed_time.count();

        // We skip k=0 since the ineer loops don't iterate over it.
        for (uint32_t k = 1; k < N; k++)
        {
            uint32_t idxRCol = N * k;


            if (k < T)
            {
                for (uint32_t i = start; i <= k - 1; i += step)
                {
                    for (uint32_t l = 0; l <= i; l++)
                    {
                        // Cache coherence is better in this case because:
                        // 1) mR column is cached between cores
                        // 2) mC row is cached
                        mR[idxRCol + i] += mC[expIdx(l, i, N)] * sigmaExpanded[expIdx(l, k, T)];
                        // R(i,k) += mC(l, i) * Cofactor(l, k);
                    }
                }
            }
            else
            {
                for (uint32_t i = start; i <= k - 1; i += step)
                {
                    for (const Pair tl : mCofactorPerFeature[k - T])
                    {
                        uint32_t l = std::get<0>(tl);

                        if (unlikely(l > i))
                            break;

                        mR[idxRCol + i] += mC[expIdx(l, i, N)] * std::get<1>(tl);
                        // R(i,k) += mC(l, i) * Cofactor(l, k);
                    }
                }
            }

            mBarrier.wait();
            //begin_timer = std::chrono::high_resolution_clock::now();
            for (uint32_t j = start; j <= k - 1; j += step)
            {
                uint32_t rowIdx = N * j;

                // note that $i in \{ j, ..., k-1 \} -- i.e. mC is upper triangular
                for (uint32_t i = j; i <= k - 1; i++)
                {
                    if (!mIsLinDepAllowed || fabs((mR[expIdx(i, i, N)]) >= mcPrecisionError))
                    {
                        mC[rowIdx + k] -= mR[idxRCol + i] * mC[rowIdx + i] / mR[expIdx(i, i, N)];
                    }
                    //mR[idxRCol + i] * mC[rowIdx + i] / mR[expIdx(i, i, N)];
                    // mC[j,k] -= R(i,k) * mC(j, i) / R(i,i);
                }
            }
            //end_timer = std::chrono::high_resolution_clock::now();
            //elapsed_time = end_timer - begin_timer;
            //time_spent +=  elapsed_time.count();
            mBarrier.wait();

            // Sum of sigma submatrix for continuous values.
            double D_k = 0; // stores R'(k,k)
            for (uint32_t l = start; l <= std::min(k, T - 1); l += step)
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

                //for (Triple tl : mCofactorList)
                for (uint32_t idx = threadId; idx < mCofactorList.size(); idx += step)
                {
                    const Triple tl = mCofactorList[idx];
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
                mMutex.lock();
                mR[idxRCol + k] += D_k;
                mMutex.unlock();
            }
        }
        //std::cout << "Time spent: " << time_spent << std::endl;
    }

    void QRDecompMultiThread::decompose(void)
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
        std::thread threadsCR[mNumThreads];

        for (uint32_t idx = 0; idx < mNumThreads; idx++)
        {
            threadsCR[idx] =  std::thread(&QRDecompMultiThread::calculateCR,
                                          this, idx);
        }

        for (uint32_t idx = 0; idx < mNumThreads; idx ++)
        {
            threadsCR[idx].join();
        }
        //std::cout << "Threads joined" << std::endl;

        //auto begin_timer = std::chrono::high_resolution_clock::now();
        // Normalise R' to obtain R
        for (uint32_t row = 0; row < N; row++)
        {
            //std::cout << "Norm" << mR[row * N + row] << std::endl;
            double norm = 1;;
            if (!mIsLinDepAllowed || (fabs(mR[row * N + row]) >= mcPrecisionError))
            {
                norm = sqrt(mR[row * N + row]);
            }
            //std::cout << "Norm" << "Preval" << mR[row * N + row] << " " <<  norm << std::endl;
            for (uint32_t col = row; col < N; col++)
            {
                mR[col * N + row] /= norm;
                //std::cout << row << " " << col << " " << mR[col * N + row] << std::endl;
            }
        }
        /*
        auto end_timer = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed_time = end_timer - begin_timer;
        auto time_spent = elapsed_time.count();
        */
        //std::cout << "Elapsed norm time is:" << time_spent << std::endl;

    }
}
