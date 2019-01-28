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
    } 

    void QRDecomposition::expandSigma(vector <long double> &sigmaExpanded) 
    {
        for (unsigned int row = 0; row < mNumFeatsExp; row ++)
        {
            for (unsigned int col = 0; col < mNumFeatsExp; col ++)
            {
                sigmaExpanded[row * mNumFeatsExp + col] = mSigma(row, col);
            }
        }
    }


    void QRDecompositionNaive::calculateQR(void) {
        vector <long double> sigmaExpanded(mNumFeatsExp * mNumFeatsExp);
        unsigned int N = mNumFeatsExp;

        expandSigma(sigmaExpanded);
        // R(0,0) = Cofactor[1,1] (skips the intercept row because of linear dependency)
        mR[0] = sigmaExpanded[N+1];

        // We skip k=0 since it contains the labels (dependent variable)
        for (size_t k = 1; k < N; k++) {
            size_t idxR = (N - 1) * (k - 1);
            for (size_t i = 1; i < k; i++) {
                for (size_t l = 1; l <= i; l++) {
                    mR[idxR + i - 1] += mC[N * l + i] * sigmaExpanded[N * l + k];
                }
            }

            for (size_t j = 1; j < k; j++) {
                size_t rowIdx = N * j;

                for (size_t i = j; i < k; i++) {
                    mC[rowIdx + k] -= mR[idxR + i - 1] * mC[rowIdx + i] / mR[(i - 1) * (N - 1) + i - 1];
                }
            }

            if (k > 1) {
                for (size_t l = 1; l <= k; l++) {

                    double res = 0;
                    for (size_t p = 1; p <= k; p++) {
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
        for (int row = 0; row < N; r++) {
            mC[row * N + row] = 1;
        }

        // Initialises column-major matrix R
        mR.resize((N-1)*(N-1));
        calculateQR();

        mSigma.resize(N - 1, N - 1);
        mSigma =  Eigen::MatrixXd::Zero(mSigma.rows(), mSigma.cols());
        
        // Normalise R
        for (size_t row = 0; row < N-1; row++) {
            double norm = sqrt(mR[row * (N-1) + row]);
            //std::cout << "NormBef " << mR[row * (N-1) + row]  << std::endl;
            for (size_t col = row; col < N-1; col++) {
                mR[col*(N-1) + row] /= norm;
                mSigma(row, col) = mR[col*(N-1) + row];
            }
        }


        Eigen::BDCSVD<Eigen::MatrixXd> svd(mSigma, Eigen::ComputeFullU | Eigen::ComputeFullV);
    
        cout << "Its singular values are:" << endl << svd.singularValues() << endl;
        cout << "Its left singular vectors are the columns of the thin U matrix:" << endl << svd.matrixU() << endl;
        cout << "Its right singular vectors are the columns of the thin V matrix:" << endl << svd.matrixV() << endl;
   
    }

}
