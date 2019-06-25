#include "Utils/Utils.h"

#include <iostream>
#include <fstream>

namespace LMFAO::LinearAlgebra
{
    void readMatrix(const std::string& sPath, FeatDim& rFtDim,
                    Eigen::MatrixXd& rmACont, std::vector <Triple>* pvCatVals,
                    bool isNaive)
    {
        std::ifstream f(sPath);
        uint32_t row, col;
        bool isCategorical;
        double val;
        f >> rFtDim.numExp;
        f >> rFtDim.num;
        f >> rFtDim.numCont;
        LMFAO_LOG_INFO("Number expanded features:", rFtDim.numExp, "Number features: ", rFtDim.num,
                          "Number continuous:", rFtDim.numCont);
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
        LMFAO_LOG_DBG("Finished reading content of file");
        rearrangeMatrix(vIsCat, rFtDim, rmACont, pvCatVals, isNaive);
    }

    void formMatrix(const MapMatrixAggregate &matrixAggregate,
                    const std::vector<bool>& vIsCat,
                    const FeatDim& ftDim, FeatDim& rFtDim,
                    Eigen::MatrixXd& rmACont, std::vector <Triple>* pvCatVals,
                    bool isNaive)
    {
        uint32_t row, col;
        // We exclued intercept column passed from LMFAO.
        //
        rFtDim.numExp = ftDim.numExp - 1;
        rFtDim.num = ftDim.num - 1;
        rFtDim.numCont = ftDim.numCont - 1;
        rFtDim.numCat = ftDim.num - ftDim.numCont;
        LMFAO_LOG_INFO("Number expanded features:", rFtDim.numExp, "Number features: ", rFtDim.num,
                      "Number continuous:", rFtDim.numCont, "Number categorical:", rFtDim.numCat);

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
        rearrangeMatrix(vIsCat, rFtDim, rmACont, pvCatVals, isNaive);
    }

    void rearrangeMatrix(const std::vector<bool>& vIsCat, const FeatDim& ftDim,
                         Eigen::MatrixXd& rmACont, std::vector <Triple>* pvCatVals,
                         bool isNaive)
    {
        std::vector <Triple> vNaiveCatVals;

        // Number of categorical columns up to that column, excluding it.
        // In rmA range [0, idx)
        //
        std::vector<uint32_t> cntCat(ftDim.numExp, 0);
        std::vector<uint32_t> cntCont(ftDim.numExp, 0);
        for (uint32_t idx = 1; idx < ftDim.numExp; idx++)
        {
            //std::cout << vIsCat[idx-1] << std::endl;
            cntCat[idx] = cntCat[idx - 1] + vIsCat[idx - 1];
            cntCont[idx] = cntCont[idx - 1] + !vIsCat[idx - 1];
        }

        LMFAO_LOG_DBG( "Finished expanding");
        for (uint32_t row = 0; row < ftDim.numExp; row++)
        {
            for (uint32_t col = 0; col < ftDim.numExp; col++)
            {
                uint32_t rowIdx = vIsCat[row] ? (row - cntCont[row] + ftDim.numCont) : (row - cntCat[row]);
                uint32_t colIdx = vIsCat[col] ? (col - cntCont[col] + ftDim.numCont) : (col - cntCat[col]);
                // Categorical values only.
                //
                if (vIsCat[row] || (vIsCat[col]))
                {
                    // Fully expand everything in dense format if naive flag is set.
                    //
                    if (isNaive)
                    {
                        vNaiveCatVals.push_back(std::make_tuple(rowIdx, colIdx, rmACont(row, col)));
                    }
                    // Otherwise store nonzero values to vector.
                    //
                    else if ((rmACont(row, col) != 0))
                    {
                        pvCatVals->push_back(std::make_tuple(rowIdx, colIdx, rmACont(row, col)));
                    }
                }
                else
                {
                    rmACont(row - cntCat[row], col - cntCat[col]) = rmACont(row, col);
                }
            }
        }
        LMFAO_LOG_DBG("Finished shuffling");
        // In the case of naive approach just add categorical values to
        // the continuous case.
        //
        if (isNaive)
        {
            for (const Triple &triple : vNaiveCatVals)
            {
                uint32_t row = std::get<0>(triple);
                uint32_t col = std::get<1>(triple);
                double aggregate = std::get<2>(triple);
                rmACont(row, col) = aggregate;
            }
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

    void readVector(const std::string& sPath, Eigen::VectorXd& rvV, char sep)
    {

        std::ifstream input(sPath);
        std::stringstream lineStream;
        std::string line;
        std::string cell;
        uint32_t rowNum = 0;
        uint32_t row = 0;

        input >> rowNum;
        getline(input, line);
        LMFAO_LOG_DBG("RowNum:", rowNum);
        rvV.resize(rowNum);

        getline(input, line);
        lineStream << line;
        row = 0;
        // Iterates column by column in search for values
        // separated by sep.
        //
        while (std::getline(lineStream, cell, sep))
        {
            double val = std::stod(cell);
            LMFAO_LOG_DBG("RowNum:", row, "Val:", val);
            rvV(row) = val;
            row ++;
        }
        lineStream.clear();
        row ++;
        input.close();
    }

    void readMatrixDense(const std::string& sPath, Eigen::MatrixXd& rmA, char sep)
    {
        std::ifstream input(sPath);
        std::stringstream lineStream;
        std::string line;
        std::string cell;
        uint32_t rowNum = 0;
        uint32_t colNum = 0;
        uint32_t row = 0;
        uint32_t col = 0;

        input >> rowNum >> colNum;
        getline(input, line);
        LMFAO_LOG_DBG("RowNum:", rowNum, "ColNum:", colNum);
        rmA.resize(rowNum, colNum);
        while (getline(input, line))
        {
            lineStream << line;
            col = 0;
            // Iterates column by column in search for values
            // separated by sep.
            //
            while (std::getline(lineStream, cell, sep))
            {
                double val = std::stod(cell);
                LMFAO_LOG_DBG("RowNum:", row, "ColNum:", col, "Val:", val);
                rmA(row, col) = val;
                col ++;
            }
            lineStream.clear();
            row ++;
        }
        input.close();
    }

    std::ostream& printVector(std::ostream& out, const Eigen::VectorXd& crvV, char sep)
    {
        uint32_t rowNum = 0;
        uint32_t row = 0;

        out << crvV.rows() << std::endl;
        // Iterates column by column in search for values
        // separated by sep.
        //
        for (uint32_t row = 0; row < crvV.rows(); row ++)
        {
            out << crvV(row);
            if (row != (crvV.rows() - 1))
            {
                out << sep;
            }
        }
        return out;
    }

    std::ostream& printMatrixDense(std::ostream& out, const Eigen::MatrixXd& crmA, char sep)
    {
        uint32_t rowNum = 0;
        uint32_t colNum = 0;
        uint32_t row = 0;
        uint32_t col = 0;

        out <<  crmA.rows() << " " << crmA.cols() << std::endl;
        for (uint32_t row = 0; row < crmA.rows(); row ++)
        {
            for (uint32_t col = 0; col < crmA.cols(); col++)
            {

                out << crmA(row, col);
                if (col != (crmA.cols() - 1))
                {
                    out << sep;
                }
            }
            out << std::endl;
        }
        return out;
    }

}
