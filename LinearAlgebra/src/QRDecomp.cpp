#include <fstream>
#include <iostream>
#include "QRDecomp.h"

using namespace std;

namespace LMFAO::LinearAlgebra
{

    void QRDecomposition::readMatrix(const std::string& path)
    {
        std::ifstream f(path);
        unsigned int row, col;
        bool isCategorical; 
        long double val;
        f >> mNumFeatsExp;
        f >> mNumFeats;
        f >> mNumFeatsCont;
        std::cout << mNumFeatsExp << mNumFeats << mNumFeatsCont << std::endl;
        mSigma = Eigen::MatrixXd::Zero(mNumFeatsExp, mNumFeatsExp);
        MatrixBool matIsCategorical = MatrixBool::Constant(mNumFeatsExp, mNumFeatsExp, false);

        mNumFeatsExp = mNumFeatsExp;
        mNumFeatsCat = mNumFeats - mNumFeatsCont;

        while (f >> row >> col >> val >> isCategorical) 
        {
            std::cout << row << " " <<  col << " " << val << " " 
                      << isCategorical << std::endl;
            mSigma(row, col) = val;
            matIsCategorical(row, col) = isCategorical;
        }
        rearrangeMatrix(matIsCategorical);
    }

    void QRDecomposition::formMatrix(const MapMatrixAggregate &matrixAggregate,
                                     unsigned int numFeatsExp, unsigned int numFeats, 
                                     unsigned int numFeatsCont)
    {
        unsigned int row, col;
        bool isCategorical;
        long double val;
        mNumFeatsExp = numFeatsExp;
        mNumFeats = numFeats;
        mNumFeatsCont = numFeatsCont;
        mNumFeatsCat = mNumFeats - mNumFeatsCont;
        mSigma = Eigen::MatrixXd::Zero(mNumFeatsExp, mNumFeatsExp);
        MatrixBool matIsCategorical = MatrixBool::Constant(mNumFeatsExp, mNumFeatsExp, false);

        for (auto& keyValue: matrixAggregate)
        {
            const auto& key = keyValue.first;
            const auto& value = keyValue.second;
            row = key.first; 
            col = key.second;
            val = value.first;
            isCategorical = value.second;
            mSigma(row, col) = val;
            matIsCategorical(row, col) = isCategorical;
        }
        rearrangeMatrix(matIsCategorical);
    }

    void QRDecomposition::rearrangeMatrix(const MatrixBool& mIsCategorical)
    {
        vector<unsigned int> dispCont(mNumFeatsExp, 0);
        vector<unsigned int> dispCat(mNumFeatsExp, 0);
        for (unsigned int idx = 1; idx < mNumFeatsExp; idx++)
        {
            dispCont[idx] = dispCont[idx - 1] + mIsCategorical(idx - 1, 0);
            dispCat[idx] = dispCat[idx - 1] + !mIsCategorical(idx - 1, 0);
        }

        for (unsigned int row = 0; row < mNumFeatsExp; row++)
        {
            for (unsigned int col = 0; col < mNumFeatsExp; col++)
            {
                unsigned int row_idx = mIsCategorical(row, 0) ? (row - dispCat[row] + mNumFeatsCont) : (row - dispCont[row]);
                unsigned int col_idx = mIsCategorical(col, 0) ? (col - dispCat[col] + mNumFeatsCont) : (col - dispCont[col]);
                if (mIsCategorical(row, col) && (mSigma(row, col) != 0))
                {
                    mCatVals.push_back(make_tuple(row_idx, col_idx, mSigma(row, col)));
                }
                mSigma(row - dispCont[row], col - dispCont[col]) = mSigma(row, col);
            }
        }
        for (const Triple &triple : mCatVals)
        {
            unsigned int row = get<0>(triple);
            unsigned int col = get<1>(triple);
            long double aggregate = get<2>(triple);
            mSigma(row, col) = aggregate;
        }

        for (unsigned int row = 0; row < mNumFeatsExp; row++)
        {
            for (unsigned int col = 0; col < mNumFeatsExp; col++)
            {
                cout << row << " " << col << " " << mSigma(row, col) << endl;
            }
        }
    }

    void QRDecomposition::expandSigma(vector<long double> &sigmaExpanded, bool isNaive)
    {
        unsigned int numRows = isNaive ?  mNumFeatsExp : mNumFeatsCont; 
        for (unsigned int row = 0; row < numRows; row ++)
        {
            for (unsigned int col = 0; col < numRows; col ++)
            {
                sigmaExpanded[expIdx(row, col, numRows)] = mSigma(row, col);
            }
        }
    }

    void QRDecomposition::getR(Eigen::MatrixXd &rEigen)
    {
        unsigned int N = mNumFeatsExp;

        rEigen.resize(N, N);
        rEigen = Eigen::MatrixXd::Zero(rEigen.rows(), rEigen.cols());

        for (unsigned int row = 0; row < N; row++)
        {
            for (unsigned int col = row; col < N; col++)
            {
                rEigen(row, col) = mR[col * N + row];
            }
        }
    }
}
