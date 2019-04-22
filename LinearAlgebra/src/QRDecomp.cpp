#include <fstream>
#include <iostream>
#include "QRDecomp.h"

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
        /*
        std::cout << mNumFeatsExp << " " << mNumFeats << 
                " " << mNumFeatsCont << std::endl;
        */
        std::vector<bool> vIsCat(mNumFeatsExp, false);
        mSigma = Eigen::MatrixXd::Zero(mNumFeatsExp, mNumFeatsExp);

        mNumFeatsExp = mNumFeatsExp;
        mNumFeatsCat = mNumFeats - mNumFeatsCont;

        for (unsigned int idx = 0; idx < mNumFeatsExp; idx ++)
        {
            f >> isCategorical;
            vIsCat[idx] = isCategorical;
            //std::cout << idx << " " << vIsCat[idx] << " ";
        }
        //std::cout << std::endl;

        while (f >> row >> col >> val)
        {
            //std::cout << row << " " <<  col << " " << val << " " << std::endl;
            mSigma(row, col) = val;
        }
        rearrangeMatrix(vIsCat);
    }

    void QRDecomposition::formMatrix(const MapMatrixAggregate &matrixAggregate,
                                     unsigned int numFeatsExp, unsigned int numFeats,
                                     unsigned int numFeatsCont,
                                     const std::vector<bool>& vIsCat)
    {
        unsigned int row, col;
        mNumFeatsExp = numFeatsExp - 1;
        mNumFeats = numFeats - 1;
        mNumFeatsCont = numFeatsCont - 1;
        std::cout << mNumFeatsExp << " " << mNumFeatsExp << " " <<
            mNumFeatsCont << std::endl;
        mNumFeatsCat = mNumFeats - mNumFeatsCont;
        mSigma = Eigen::MatrixXd::Zero(mNumFeatsExp, mNumFeatsExp);

        for (const auto& keyValue: matrixAggregate)
        {
            const auto& key = keyValue.first;
            const auto& value = keyValue.second;
            if ((key.first > 0) && (key.second > 0))
            {
                row = key.first - 1;
                col = key.second - 1;
                mSigma(row, col) = value;
            }
        }
        //std::cout << vIsCat.size() << std::endl;
        const_cast<std::vector<bool>&>(vIsCat).erase(vIsCat.begin());
        std::cout << vIsCat.size() << std::endl;
        rearrangeMatrix(vIsCat);
    }

    void QRDecomposition::rearrangeMatrix(const std::vector<bool>& vIsCat)
    {
        std::vector<unsigned int> cntCat(mNumFeatsExp, 0);
        std::vector<unsigned int> cntCont(mNumFeatsExp, 0);
        for (unsigned int idx = 1; idx < mNumFeatsExp; idx++)
        {
            //std::cout << vIsCat[idx-1] << std::endl;
            cntCat[idx] = cntCat[idx - 1] + vIsCat[idx - 1];
            cntCont[idx] = cntCont[idx - 1] + !vIsCat[idx - 1];
        }

        for (unsigned int row = 0; row < mNumFeatsExp; row++)
        {
            for (unsigned int col = 0; col < mNumFeatsExp; col++)
            {
                unsigned int row_idx = vIsCat[row] ? (row - cntCont[row] + mNumFeatsCont) : (row - cntCat[row]);
                unsigned int col_idx = vIsCat[col] ? (col - cntCont[col] + mNumFeatsCont) : (col - cntCat[col]);
                if (vIsCat[row] || (vIsCat[col]))
                {
                    mNaiveCatVals.push_back(std::make_tuple(row_idx, col_idx, mSigma(row, col)));
                    if ((mSigma(row, col) != 0))
                    {
                        mCatVals.push_back(std::make_tuple(row_idx, col_idx, mSigma(row, col)));
                    }
                }
                else
                {
                    mSigma(row - cntCat[row], col - cntCat[col]) = mSigma(row, col);
                }
            }
        }
        for (const Triple &triple : mNaiveCatVals)
        {
            unsigned int row = std::get<0>(triple);
            unsigned int col = std::get<1>(triple);
            long double aggregate = std::get<2>(triple);
            mSigma(row, col) = aggregate;
        }
        //std::cout << "Matrix" << std::endl;
        /*
        for (unsigned int row = 0; row < mNumFeatsExp; row++)
        {
            for (unsigned int col = 0; col < mNumFeatsExp; col++)
            {
                cout << row << " " << col << " " << mSigma(row, col) << endl;
            }
        }
        std::cout << "******************" << std::endl;
        */
    }

    void QRDecomposition::expandSigma(std::vector<long double> &sigmaExpanded, bool isNaive)
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
