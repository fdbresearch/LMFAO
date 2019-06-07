#ifndef _LMFAO_LA_UTILS_H_
#define _LMFAO_LA_UTILS_H_

#include <Eigen/SVD>
// There is a bug because Eigen include C complex.h which has I defined as macro
// while boost uses I as classname, and thus there is a clash in naming.
// I is not used anywhere in eigen as a variable except in src/SparseLU/SparseLU_gemm_kernel.h:
// which doesn't include any files, thus I is not included.
//
#undef I

#include <vector>
#include <map>

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
        // Number of features (categorical + continuous) in matrix.
        //
        uint32_t num = 0;
        // Number of real valued values in the expanded matrix
        // without linearly dependent columns.
        //
        uint32_t numExp = 0;
        // Number of continuous features in matrix.
        //
        uint32_t numCont = 0;
        // Number of categorical features in matrix.
        //
        uint32_t numCat = 0;
    };

    void readMatrix(const std::string& sPath, FeatDim& rFtDim,
                    Eigen::MatrixXd& rmACont, std::vector <Triple>& rvCatVals,
                    std::vector <Triple>& rvNaiveCatVals);

    void formMatrix(const MapMatrixAggregate &matrixAggregate,
                    const std::vector<bool>& vIsCat,
                    const FeatDim& ftDim, FeatDim& rFtDim,
                    Eigen::MatrixXd& rmACont, std::vector <Triple>& rvCatVals,
                    std::vector <Triple>& rvNaiveCatVals);

    void rearrangeMatrix(const std::vector<bool>& vIsCat, const FeatDim& ftDim,
                         Eigen::MatrixXd& rmACont, std::vector <Triple>& rvCatVals,
                         std::vector <Triple>& rvNaiveCatVals);
}

#endif
