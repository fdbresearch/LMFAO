#ifndef _LMFAO_LA_UTILS_H_
#define _LMFAO_LA_UTILS_H_

#include <Eigen/Dense>
// There is a bug because Eigen include C complex.h which has I defined as macro
// while boost uses I as classname, and thus there is a clash in naming.
// I is not used anywhere in eigen as a variable except in src/SparseLU/SparseLU_gemm_kernel.h:
// which doesn't include any files, thus I is not included.
//
#undef I

#include <vector>
#include <map>
#include <iostream>
#include "Logger.h"

namespace LMFAO::LinearAlgebra
{
    template <typename T>
    T inline expIdx(T row, T col, T numCols)
    {
        return row * numCols + col;
    }

    /**
     * Used to help the compiler make branching predictions.
     */
    #ifdef __GNUC__
    #define likely(x) __builtin_expect((x),1)
    #define unlikely(x) __builtin_expect((x),0)
    #else
    #define likely(x) x
    #define unlikely(x) x
    #endif

    //void svdCuda(const Eigen::MatrixXd &a);

    typedef std::tuple<uint32_t, uint32_t,
                       double> Triple;
    typedef std::tuple<uint32_t, double> Pair;

    typedef std::map<std::pair<uint32_t, uint32_t>,
                     double> MapMatrixAggregate;

    struct FeatDim
    {
        // Number of features (categorical + continuous) in the matrix.
        //
        uint32_t num = 0;
        // Number of real values in the expanded matrix
        // without linearly dependent columns.
        //
        uint32_t numExp = 0;
        // Number of continuous features in the matrix.
        //
        uint32_t numCont = 0;
        // Number of categorical features in the matrix.
        //
        uint32_t numCat = 0;
    };


    void readMatrix(const std::string& sPath, FeatDim& rFtDim,
                    Eigen::MatrixXd& rmACont, std::vector <Triple>* pvCatVals,
                    bool isNaive);

    void expandMatrixWithCatVals(const FeatDim& rFtDim, Eigen::MatrixXd& rmACont,
                                const std::vector <Triple>& vCatVals);

    void formMatrix(const MapMatrixAggregate &matrixAggregate,
                    const std::vector<bool>& vIsCat,
                    const FeatDim& ftDim, FeatDim& rFtDim,
                    Eigen::MatrixXd& rmACont, std::vector <Triple>* pvCatVals,
                    bool isNaive);

    void rearrangeMatrix(const std::vector<bool>& vIsCat, const FeatDim& ftDim,
                         Eigen::MatrixXd& rmACont, std::vector <Triple>* pvCatVals,
                         bool isNaive);

    // Reads vector from wile @p sPath where in the first line are vector dimenstions.
    // In the following line, are written the components of vector.
    //
    void readVector(const std::string& sPath, Eigen::VectorXd& rvV, char sep=' ');

    // Reads matrix from wile @p sPath where in the first line are matrix dimenstions.
    // In the following lines, matrix rows are written separated by space.
    //
    void readMatrixDense(const std::string& sPath, Eigen::MatrixXd& rmA, char sep=' ');

    std::ostream& printMatrixDense(std::ostream& out, const Eigen::MatrixXd& crmA, char sep=' ');

    std::ostream& printVector(std::ostream& out, const Eigen::VectorXd& crvV, char sep=' ');
}

#endif
