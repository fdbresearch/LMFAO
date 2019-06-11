#include <fstream>
#include <iostream>
#include <iomanip>
#include <limits>
#include "QRDecompBase.h"

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

        for (uint32_t row = 0; row < N; row++)
        {
            for (uint32_t col = row; col < N; col++)
            {
                rEigen(row, col) = mR[col * N + row];
            }
        }
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
