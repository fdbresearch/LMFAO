#include <fstream>
#include <iostream>
#include "QRDecomposition.h"

using namespace std;

namespace LMFAO::LinearAlgebra
{

    void QRDecomposition::readMatrix(const std::string& path)
    {
        std::ifstream f(path);
        int dim, row, col;
        float val;
        f >> dim;
        mSigma = Eigen::MatrixXd::Zero(dim, dim);
        while (f >> row >> col >> val) 
        {
            std::cout << row << " " <<  col << " " << val << std::endl;
            mSigma(row, col) = val;
        }
        mNumFeatsExp = mSigma.rows();
        mNumFeatsCat = mNumFeats - mNumFeatsCont;
    } 

    void QRDecomposition::expandSigma(vector <long double> &sigmaExpanded, bool isNaive) 
    {
        unsigned int num_rows = isNaive ?  mNumFeatsExp : mNumFeatsCont; 
        for (unsigned int row = 0; row < num_rows; row ++)
        {
            for (unsigned int col = 0; col < num_rows; col ++)
            {
                sigmaExpanded[expIdx(row, col, num_rows)] = mSigma(row, col);
            }
        }
    }


    void QRDecompositionNaive::calculateCR(void) {
        vector <long double> sigmaExpanded(mNumFeatsExp * mNumFeatsExp);
        unsigned int N = mNumFeatsExp;

        expandSigma(sigmaExpanded, true /*isNaive*/);
        // R(0,0) = Cofactor[1,1] (skips the intercept row because of linear dependency)
        mR[0] = sigmaExpanded[N+1];

        // We skip k=0 since it contains the labels (dependent variable)
        for (unsigned int k = 1; k < N; k++) {
            unsigned int idxR = (N - 1) * (k - 1);
            for (unsigned int i = 1; i < k; i++) {
                for (unsigned int l = 1; l <= i; l++) {
                    mR[idxR + i - 1] += mC[N * l + i] * sigmaExpanded[N * l + k];
                }
            }

            for (unsigned int j = 1; j < k; j++) {
                unsigned int rowIdx = N * j;

                for (unsigned int i = j; i < k; i++) {
                    mC[rowIdx + k] -= mR[idxR + i - 1] * mC[rowIdx + i] / mR[(i - 1) * (N - 1) + i - 1];
                }
            }

            if (k > 1) {
                for (unsigned int l = 1; l <= k; l++) {

                    double res = 0;
                    for (unsigned int p = 1; p <= k; p++) {
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
        for (unsigned int row = 0; row < N; row++) {
            mC[expIdx(row, row, N)] = 1;
        }

        // Initialises column-major matrix R
        mR.resize((N-1)*(N-1));
        calculateCR();

        mSigma.resize(N - 1, N - 1);
        mSigma =  Eigen::MatrixXd::Zero(mSigma.rows(), mSigma.cols());
        
        // Normalise R
        for (unsigned int row = 0; row < N-1; row++) {
            double norm = sqrt(mR[expIdx(row, row, N - 1)]);
            //std::cout << "NormBef " << mR[row * (N-1) + row]  << std::endl;
            for (unsigned int col = row; col < N-1; col++) {
                mR[col*(N-1) + row] /= norm;
                mSigma(row, col) = mR[col*(N-1) + row];
            }
        }


        Eigen::BDCSVD<Eigen::MatrixXd> svd(mSigma, Eigen::ComputeFullU | Eigen::ComputeFullV);
    
        cout << "Its singular values are:" << endl << svd.singularValues() << endl;
        cout << "Its left singular vectors are the columns of the thin U matrix:" << endl << svd.matrixU() << endl;
        cout << "Its right singular vectors are the columns of the thin V matrix:" << endl << svd.matrixV() << endl;
   
    }

    void QRDecompositionSingleThreaded::processCofactors(void)
    {

        if (mNumFeatsCat < 1) return;
        unsigned int aggNo = 0, aggIdx = 0;

        _cofactorPerFeature.resize(mNumFeatsExp - mNumFeatsCont);
        //_counts.resize(mNumFeatsExp - mNumFeatsCont);

        /*
        // Insert the (partially) categorical cofactors into Sigma and Phi
        while (aggNo < _parameterIndex.size()) {
            double aggregate = _categoricalAggregates[aggIdx];

            int combinations = _parameterIndex[aggNo];

            ++aggNo;

            int index1, index2, a, b;
            for (int i = 0; i < combinations; ++i) {
                index1 = _parameterIndex[aggNo];
                index2 = _parameterIndex[aggNo + 1];

                // skip1 if the "first" feature is categorical and the first category
                //auto skip1 = index1 >= mNumFeatsCon && _categoricalMap[index1 - mNumFeatsCon] < 0;
                //auto skip2 = index2 >= mNumFeatsCon && _categoricalMap[index2 - mNumFeatsCon] < 0;

                // skips aggregates of the first categorical feature of each categorical variable
                // to ensure full-rank
                //if (!skip1 && !skip2) {
                    if (index1 >= mNumFeatsCon)
                        index1 = _categoricalMap[index1 - mNumFeatsCon];
                    if (index2 >= mNumFeatsCon)
                        index2 = _categoricalMap[index2 - mNumFeatsCon];

                    a = max(index1, index2);
                    b = min(index1, index2);

                    //if (b != 0) {
                    _cofactorList.emplace_back(b, a, aggregate);
                    _cofactorPerFeature[a - mNumFeatsCon].emplace_back(b, aggregate);
                    //}
                    
                    //else {
                    //    _counts[a - mNumFeatsCon] = aggregate;
                    //}
                //}

                aggNo += 2;
            }

            ++aggIdx;
        }
        */
        // Sort cofactorList (Sigma) colexicographically
        sort(begin(_cofactorList), end(_cofactorList), [](const Triple& a, const Triple& b) -> bool
        {
            int a_max = max(get<0>(a), get<1>(a)), a_min = min(get<0>(a), get<1>(a));
            int b_max = max(get<0>(b), get<1>(b)), b_min = min(get<0>(b), get<1>(b));

            return tie(a_max, a_min) < tie(b_max, b_min);
        });

        // Sort the lists in Phi
        for (auto &_order : _cofactorPerFeature) {
            sort(begin(_order), end(_order));
        }
    }

    void QRDecompositionSingleThreaded::calculateCR(void)
    {
        // R(0,0) = Cofactor[1,1] (Note that the first row and column contain the label's aggregates)
        unsigned int T = mNumFeatsCont;
        unsigned int N = mNumFeatsExp;
        vector <long double> sigmaExpanded(T * T);
        
        expandSigma(sigmaExpanded, false /*isNaive*/ );

        mR[0] = sigmaExpanded[T+1];

        // We skip k=0 since it contains the labels (dependent variable)
        for (unsigned  int k = 1; k < N; k++) {
            unsigned int idxR = (N - 1) * (k - 1);

            if (k < T) {
                for (unsigned int  i = 1; i < k; i++) {
                    for (unsigned int l = 1; l <= i; l++) {
                        mR[idxR + i - 1] += mC[N * l + i] * sigmaExpanded[T * l + k];
                        // R(i,k) += mC(l, i) * Cofactor(l, k);
                    }
                }
            } else {
                for (unsigned int i = 1; i < k; i++) {
                    for (Pair tl : _cofactorPerFeature[k - T]) {
                        unsigned int l = get<0>(tl);

                        if (unlikely(l > i)) break;

                        mR[idxR + i - 1] += mC[N * l + i] * get<1>(tl);
                        // R(i,k) += mC(l, i) * Cofactor(l, k);
                    }
                }
            }

            for (unsigned int j = 1; j < k; j++) {
                unsigned int rowIdx = N * j;

                // note that $i in \{ j, ..., k-1 \} -- i.e. mC is upper triangular
                for (unsigned int i = j; i < k; i++) {
                    mC[rowIdx + k] -= mR[idxR + i - 1] * mC[rowIdx + i] / mR[(i - 1) * (N - 1) + i - 1];
                    // mC[j,k] -= R(i,k) * mC(j, i) / R(i,i);
                }
            }

            long double D_k = 0; // stores R'(k,k)
            for (unsigned int l = 1; l <= min(k, T - 1); l++) {

                long double res = 0;
                for (unsigned p = 1; p <= min(k, T - 1); p++) {
                    res += mC[p*N + k] * sigmaExpanded[l*T + p];
                    // res += mC(p, k) * Cofactor(l, p);
                }

                D_k += mC[l * N + k] * res;
                // D_K += mC(l,k) * SUM[ mC(p,k) * Cofactor(l,p) ]
            }

            if (k >= T) {

                for (Triple tl : _cofactorList) {
                    unsigned int p = get<0>(tl);
                    unsigned int l = get<1>(tl);

                    long double agg = get<2>(tl);

                    if (unlikely(p > k || l > k))
                        break;

                    unsigned int factor = (p != l) ? 2 : 1;

                    D_k += factor * mC[l * N + k] * mC[p * N + k] * agg;
                    // D_k += mC(l, k) * mC(p, k) * Cofactor(l, p);

                }
            }

            if (k > 1) {
                mR[idxR + k - 1] += D_k;
            }
        }
    }

    void QRDecompositionSingleThreaded::decompose(void)
    {
        unsigned int T = mNumFeatsCont;
        // We omit the first column of each categorical column matrix because they are linearly 
        // among themselves (for specific column). 
        unsigned int N = mNumFeatsExp;

        processCofactors();

        // Used to store constants (A = ACR), initialises mC = Identity[N,N]
        mC.resize(N * N);
        for (unsigned int row = 0; row < N; row++) {
            mC[expIdx(row, row, N)] = 1;
        }
        // R is stored column-major
        mR.resize((N-1)*(N-1));
        
        calculateCR();

        // Normalise R' to obtain R
        for (unsigned int row = 0; row < N-1; row++) {
            double norm = sqrt(mR[row * (N-1) + row]);
            for (unsigned int col = row; col < N-1; col++) {
                mR[col*(N-1) + row] /= norm;
            }
        }
    }

}
