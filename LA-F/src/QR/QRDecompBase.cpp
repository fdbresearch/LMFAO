#include <fstream>
#include <iostream>
#include <iomanip>
#include <limits>
#include "QR/QRDecompBase.h"

namespace LMFAO::LinearAlgebra
{
    void QRDecompBase::expandSigma(std::vector<double> &sigmaExpanded, bool isNaive)
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

    void QRDecompBase::getR(Eigen::MatrixXd &rEigen) const
    {
        uint32_t N = mNumFeatsExp;

        rEigen.resize(N, N);
        rEigen = Eigen::MatrixXd::Zero(rEigen.rows(), rEigen.cols());

        // Default storage order order in Eigen is column-major.
        //
        for (uint32_t col = 0; col < N; col++)
        {
            for (uint32_t row = 0; row <= col; row++)
            {
                rEigen(row, col) = mR[col * N + row];
            }
        }
    }

    void QRDecompBase::normalizeRC(void)
    {
        uint32_t N = mNumFeatsExp;
        for (uint32_t col = 0; col < N; col++)
        {
            double norm = 1;;
            if (!mIsLinDepAllowed || (fabs(mR[col * N + col]) >= mcPrecisionError))
            {
                norm = sqrt(mR[col * N + col]);
            }
            // In this loop we normalize columns by dividing all of them with the norm.
            for (uint32_t row = 0; row <= col; row++)
            {
                mC[expIdx(row, col, N)] /= norm;
                LMFAO_LOG_DBG("NORM", row, col,norm);
                LMFAO_LOG_DBG("c:", row, col, N, mC[expIdx(row, col, N)]);
            }
        }

        //auto begin_timer = std::chrono::high_resolution_clock::now();
        // Normalise R' to obtain R
        for (uint32_t row = 0; row < N; row++)
        {
            double norm = 1;
            if (!mIsLinDepAllowed || (fabs(mR[row * N + row]) >= mcPrecisionError))
            {
                norm = sqrt(mR[row * N + row]);
            }
            // In this loop we normalize rows by dividing all of them with norm.
            for (uint32_t col = row; col < N; col++)
            {
                mR[col * N + row] /= norm;
            }
        }

    }

    void QRDecompBase::getC(Eigen::MatrixXd &cEigen) const
    {
        uint32_t N = mNumFeatsExp;

        cEigen.resize(N, N);
        cEigen = Eigen::MatrixXd::Zero(cEigen.rows(), cEigen.cols());

        for (uint32_t row = 0; row < N; row++)
        {
            for (uint32_t col = row; col < N; col++)
            {
                cEigen(row, col) = mC[expIdx(row, col, N)];
            }
        }
    }
    double QRDecompBase::getQTQFrobeniusError(void)
    {
        Eigen::MatrixXd R, C, res, I;
        FeatDim ftDim = {mNumFeats, mNumFeatsExp, mNumFeatsCont, mNumFeatsCat};
        getR(R);
        getC(C);
        if (m_pvCatVals != nullptr)
        {
            expandMatrixWithCatVals(ftDim, mSigma, *m_pvCatVals);
        }
        I = Eigen::MatrixXd::Identity(mNumFeatsExp, mNumFeatsExp);
        res = C.transpose() * mSigma * C - I;
        LMFAO_LOG_DBG("NORMA", res.norm());
        return res.norm() / I.norm();
    }

    std::ostream& operator<<(std::ostream& out, const QRDecompBase& qrDecomp)
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
