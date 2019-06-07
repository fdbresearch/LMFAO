#include "Utils.h"

#include <iostream>
#include <fstream>

namespace LMFAO::LinearAlgebra
{
    void readMatrix(const std::string& sPath, FeatDim& rFtDim,
                    Eigen::MatrixXd& rmACont, std::vector <Triple>& rvCatVals,
                    std::vector <Triple>& rvNaiveCatVals)
    {
        std::ifstream f(sPath);
        uint32_t row, col;
        bool isCategorical;
        double val;
        f >> rFtDim.numExp;
        f >> rFtDim.num;
        f >> rFtDim.numCont;

        std::vector<bool> vIsCat(rFtDim.numExp, false);
        rmACont = Eigen::MatrixXd::Zero(rFtDim.numExp, rFtDim.numExp);

        rFtDim.numCat = rFtDim.num - rFtDim.numCont;
        for (uint32_t idx = 0; idx < rFtDim.numExp; idx ++)
        {
            f >> isCategorical;
            vIsCat[idx] = isCategorical;
        }
        while (f >> row >> col >> val)
        {
            rmACont(row, col) = val;
        }
        rearrangeMatrix(vIsCat, rFtDim, rmACont, rvCatVals, rvNaiveCatVals);
    }

    void formMatrix(const MapMatrixAggregate &matrixAggregate,
                    const std::vector<bool>& vIsCat,
                    const FeatDim& ftDim, FeatDim& rFtDim,
                    Eigen::MatrixXd& rmACont, std::vector <Triple>& rvCatVals,
                    std::vector <Triple>& rvNaiveCatVals)
    {
        uint32_t row, col;
        // We exclued intercept column passed from LMFAO.
        //
        rFtDim.numExp = ftDim.numExp - 1;
        rFtDim.num = ftDim.num - 1;
        rFtDim.numCont = ftDim.numCont - 1;
        //std::cout << rFtDim.numExp << " " << rFtDim.numExp << " " <<
        //    mNumFeatsCont << std::endl;
        rFtDim.numCat = ftDim.num - ftDim.numCont;
        rmACont = Eigen::MatrixXd::Zero(rFtDim.numExp, rFtDim.numExp);

        for (const auto& keyValue: matrixAggregate)
        {
            const auto& key = keyValue.first;
            const auto& value = keyValue.second;
            if ((key.first > 0) && (key.second > 0))
            {
                row = key.first - 1;
                col = key.second - 1;
                rmACont(row, col) = value;
            }
        }
        //std::cout << vIsCat.size() << std::endl;
        const_cast<std::vector<bool>&>(vIsCat).erase(vIsCat.begin());
        //std::cout << vIsCat.size() << std::endl;
        rearrangeMatrix(vIsCat, rFtDim, rmACont, rvCatVals, rvNaiveCatVals);
    }

    void rearrangeMatrix(const std::vector<bool>& vIsCat, const FeatDim& ftDim,
                         Eigen::MatrixXd& rmACont, std::vector <Triple>& rvCatVals,
                         std::vector <Triple>& rvNaiveCatVals)
    {
        // Number of categorical columns up to that column, excluding it.
        // In a range [0, idx)
        //
        std::vector<uint32_t> cntCat(ftDim.numExp, 0);
        std::vector<uint32_t> cntCont(ftDim.numExp, 0);
        for (uint32_t idx = 1; idx < ftDim.numExp; idx++)
        {
            //std::cout << vIsCat[idx-1] << std::endl;
            cntCat[idx] = cntCat[idx - 1] + vIsCat[idx - 1];
            cntCont[idx] = cntCont[idx - 1] + !vIsCat[idx - 1];
        }

        for (uint32_t row = 0; row < ftDim.numExp; row++)
        {
            for (uint32_t col = 0; col < ftDim.numExp; col++)
            {
                uint32_t rowIdx = vIsCat[row] ? (row - cntCont[row] + ftDim.numCont) : (row - cntCat[row]);
                uint32_t colIdx = vIsCat[col] ? (col - cntCont[col] + ftDim.numCont) : (col - cntCat[col]);
                if (vIsCat[row] || (vIsCat[col]))
                {
                    rvNaiveCatVals.push_back(std::make_tuple(rowIdx, colIdx, rmACont(row, col)));
                    if ((rmACont(row, col) != 0))
                    {
                        rvCatVals.push_back(std::make_tuple(rowIdx, colIdx, rmACont(row, col)));
                    }
                }
                else
                {
                    rmACont(row - cntCat[row], col - cntCat[col]) = rmACont(row, col);
                }
            }
        }
        // TODO: Remove this initialization for non naive part.
        // 1) Remove naive completely.
        // 2) For all non continuous values, set everything to zero.
        // 3) Just iterate over categorical values to update values.
        for (const Triple &triple : rvNaiveCatVals)
        {
            uint32_t row = std::get<0>(triple);
            uint32_t col = std::get<1>(triple);
            double aggregate = std::get<2>(triple);
            rmACont(row, col) = aggregate;
        }
        //std::cout << "Matrix" << std::endl;
        /*
        for (uint32_t row = 0; row < rFtDim.numExp; row++)
        {
            for (uint32_t col = 0; col < rFtDim.numExp; col++)
            {
                cout << row << " " << col << " " << rmACont(row, col) << endl;
            }
        }
        std::cout << "******************" << std::endl;
        */
    }
}
