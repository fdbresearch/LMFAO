#include <fstream>
#include <iostream>
#include <iomanip>
#include <limits>
#include "QRDecomp.h"

namespace LMFAO::LinearAlgebra
{
    void QRDecomposition::readMatrix(const std::string& path)
    {
        std::ifstream f(path);
        uint32_t row, col;
        bool isCategorical;
        double val;
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

        for (uint32_t idx = 0; idx < mNumFeatsExp; idx ++)
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
                                     uint32_t numFeatsExp, uint32_t numFeats,
                                     uint32_t numFeatsCont,
                                     const std::vector<bool>& vIsCat)
    {
        uint32_t row, col;
        // We exclued intercept column passed from LMFAO.
        //
        mNumFeatsExp = numFeatsExp - 1;
        mNumFeats = numFeats - 1;
        mNumFeatsCont = numFeatsCont - 1;
        //std::cout << mNumFeatsExp << " " << mNumFeatsExp << " " <<
        //    mNumFeatsCont << std::endl;
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
        //std::cout << vIsCat.size() << std::endl;
        rearrangeMatrix(vIsCat);
    }

    void QRDecomposition::rearrangeMatrix(const std::vector<bool>& vIsCat)
    {
        // Number of categorical columns up to that column, excluding it.
        // In a range [0, idx)
        //
        std::vector<uint32_t> cntCat(mNumFeatsExp, 0);
        std::vector<uint32_t> cntCont(mNumFeatsExp, 0);
        for (uint32_t idx = 1; idx < mNumFeatsExp; idx++)
        {
            //std::cout << vIsCat[idx-1] << std::endl;
            cntCat[idx] = cntCat[idx - 1] + vIsCat[idx - 1];
            cntCont[idx] = cntCont[idx - 1] + !vIsCat[idx - 1];
        }

        for (uint32_t row = 0; row < mNumFeatsExp; row++)
        {
            for (uint32_t col = 0; col < mNumFeatsExp; col++)
            {
                uint32_t row_idx = vIsCat[row] ? (row - cntCont[row] + mNumFeatsCont) : (row - cntCat[row]);
                uint32_t col_idx = vIsCat[col] ? (col - cntCont[col] + mNumFeatsCont) : (col - cntCat[col]);
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
            uint32_t row = std::get<0>(triple);
            uint32_t col = std::get<1>(triple);
            double aggregate = std::get<2>(triple);
            mSigma(row, col) = aggregate;
        }
        //std::cout << "Matrix" << std::endl;
        /*
        for (uint32_t row = 0; row < mNumFeatsExp; row++)
        {
            for (uint32_t col = 0; col < mNumFeatsExp; col++)
            {
                cout << row << " " << col << " " << mSigma(row, col) << endl;
            }
        }
        std::cout << "******************" << std::endl;
        */
    }

    void QRDecomposition::expandSigma(std::vector<double> &sigmaExpanded, bool isNaive)
    {
        uint32_t numRows = isNaive ?  mNumFeatsExp : mNumFeatsCont;
        for (uint32_t row = 0; row < numRows; row ++)
        {
            for (uint32_t col = 0; col < numRows; col ++)
            {
                sigmaExpanded[expIdx(row, col, numRows)] = mSigma(row, col);
            }
        }
    }

    void QRDecomposition::getR(Eigen::MatrixXd &rEigen)
    {
        uint32_t N = mNumFeatsExp;

        rEigen.resize(N, N);
        rEigen = Eigen::MatrixXd::Zero(rEigen.rows(), rEigen.cols());

        for (uint32_t row = 0; row < N; row++)
        {
            for (uint32_t col = row; col < N; col++)
            {
                rEigen(row, col) = mR[col * N + row];
            }
        }
    }

    std::ostream& operator<<(std::ostream& out, const QRDecomposition& qrDecomp)
    {
        uint32_t N = qrDecomp.mNumFeatsExp;
        out << N << " " << N << std::fixed << std::setprecision(2) << std::endl;
        out << std::fixed << std::setprecision(std::numeric_limits<double>::digits10 + 1);

        for (uint32_t row = 0; row < N; row++)
        {
            for (uint32_t col = 0; col < row; col++)
            {
                out << "0 ";
            }
            for (uint32_t col = row; col < N; col++)
            {
                out << qrDecomp.mR[col * N + row] << " ";
            }
            out << std::endl;
        }
        return out;
    }
}