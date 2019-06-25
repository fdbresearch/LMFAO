#include "UtilsTests.h"
#include "SVD/SVDecomp.h"

using namespace LMFAO::LinearAlgebra;
using namespace std;

constexpr double SVD_TEST_PRECISION_ERROR = 1e-8;
const std::string SVD_COMP_FILE_NAME="testExpSinVals.in";

void compareSinVals(Eigen::VectorXd& sinVals, Eigen::VectorXd& expSinVals)
{
    for (uint32_t row = 0; row < expSinVals.rows(); row ++)
    {
        EXPECT_NEAR(sinVals(row), expSinVals(row), SVD_TEST_PRECISION_ERROR);
    }
}

TEST(SVDNaive, 2SizeCntMatrix) {
    static const string FILE_INPUT = getTestPath(1) + TEST_FILE_IN;
    static const string FILE_INPUT_EXP = getTestPath(1) + SVD_COMP_FILE_NAME;

    Eigen::VectorXd singularValues, expSinVals;
    SVDecompQR svdDecomp(FILE_INPUT, SVDecompQR::DecompType::NAIVE);
    svdDecomp.decompose();
    svdDecomp.getSingularValues(singularValues);
    readVector(FILE_INPUT_EXP, expSinVals);
    compareSinVals(singularValues, expSinVals);
}

TEST(SVDSingleThreaded, 2SizeCntMatrix) {
    static const string FILE_INPUT = getTestPath(1) + TEST_FILE_IN;
    static const string FILE_INPUT_EXP = getTestPath(1) + SVD_COMP_FILE_NAME;

    Eigen::VectorXd singularValues, expSinVals;
    SVDecompQR svdDecomp(FILE_INPUT, SVDecompQR::DecompType::SINGLE_THREAD);
    svdDecomp.decompose();
    svdDecomp.getSingularValues(singularValues);
    readVector(FILE_INPUT_EXP, expSinVals);
    compareSinVals(singularValues, expSinVals);
}

TEST(SVDMultiThreaded, 2SizeCntMatrix) {
        static const string FILE_INPUT = getTestPath(1) + TEST_FILE_IN;
    static const string FILE_INPUT_EXP = getTestPath(1) + SVD_COMP_FILE_NAME;

    Eigen::VectorXd singularValues, expSinVals;
    SVDecompQR svdDecomp(FILE_INPUT, SVDecompQR::DecompType::MULTI_THREAD);
    svdDecomp.decompose();
    svdDecomp.getSingularValues(singularValues);
    readVector(FILE_INPUT_EXP, expSinVals);
    compareSinVals(singularValues, expSinVals);
}

TEST(SVDCholesky, 2SizeCntMatrix) {
    static const string FILE_INPUT = getTestPath(1) + TEST_FILE_IN;
    static const string FILE_INPUT_EXP = getTestPath(1) + SVD_COMP_FILE_NAME;

    Eigen::VectorXd singularValues, expSinVals;
    SVDecompQR svdDecomp(FILE_INPUT, SVDecompQR::DecompType::CHOLESKY);
    svdDecomp.decompose();
    svdDecomp.getSingularValues(singularValues);
    readVector(FILE_INPUT_EXP, expSinVals);
    compareSinVals(singularValues, expSinVals);
}

TEST(SVDEigenDecomp, 2SizeCntMatrix) {
    static const string FILE_INPUT = getTestPath(1) + TEST_FILE_IN;
    static const string FILE_INPUT_EXP = getTestPath(1) + SVD_COMP_FILE_NAME;

    Eigen::VectorXd singularValues, expSinVals;
    SVDecompEigDec svdDecomp(FILE_INPUT);
    svdDecomp.decompose();
    svdDecomp.getSingularValues(singularValues);
    readVector(FILE_INPUT_EXP, expSinVals);
    compareSinVals(singularValues, expSinVals);
}

TEST(SVDAltMin, 2SizeCntMatrix) {
    static const string FILE_INPUT = getTestPath(1) + TEST_FILE_IN;
    static const string FILE_INPUT_EXP = getTestPath(1) + SVD_COMP_FILE_NAME;

    Eigen::VectorXd singularValues, expSinVals;
    SVDecompAltMin svdDecomp(FILE_INPUT);
    svdDecomp.decompose();
    svdDecomp.getSingularValues(singularValues);
    readVector(FILE_INPUT_EXP, expSinVals);
    compareSinVals(singularValues, expSinVals);
}

TEST(SVDNaive, 3SizeCntMatrix) {
    static const string FILE_INPUT = getTestPath(2) + TEST_FILE_IN;
    static const string FILE_INPUT_EXP = getTestPath(2) + SVD_COMP_FILE_NAME;

    Eigen::VectorXd singularValues, expSinVals;
    SVDecompQR svdDecomp(FILE_INPUT, SVDecompQR::DecompType::NAIVE);
    svdDecomp.decompose();
    svdDecomp.getSingularValues(singularValues);
    readVector(FILE_INPUT_EXP, expSinVals);
    compareSinVals(singularValues, expSinVals);
}

TEST(SVDSingleThreaded, 3SizeCntMatrix) {
    static const string FILE_INPUT = getTestPath(2) + TEST_FILE_IN;
    static const string FILE_INPUT_EXP = getTestPath(2) + SVD_COMP_FILE_NAME;

    Eigen::VectorXd singularValues, expSinVals;
    SVDecompQR svdDecomp(FILE_INPUT, SVDecompQR::DecompType::SINGLE_THREAD);
    svdDecomp.decompose();
    svdDecomp.getSingularValues(singularValues);
    readVector(FILE_INPUT_EXP, expSinVals);
    compareSinVals(singularValues, expSinVals);
}

TEST(SVDMultiThreaded, 3SizeCntMatrix) {
        static const string FILE_INPUT = getTestPath(2) + TEST_FILE_IN;
    static const string FILE_INPUT_EXP = getTestPath(2) + SVD_COMP_FILE_NAME;

    Eigen::VectorXd singularValues, expSinVals;
    SVDecompQR svdDecomp(FILE_INPUT, SVDecompQR::DecompType::MULTI_THREAD);
    svdDecomp.decompose();
    svdDecomp.getSingularValues(singularValues);
    readVector(FILE_INPUT_EXP, expSinVals);
    compareSinVals(singularValues, expSinVals);
}

TEST(SVDCholesky, 3SizeCntMatrix) {
    static const string FILE_INPUT = getTestPath(2) + TEST_FILE_IN;
    static const string FILE_INPUT_EXP = getTestPath(2) + SVD_COMP_FILE_NAME;

    Eigen::VectorXd singularValues, expSinVals;
    SVDecompQR svdDecomp(FILE_INPUT, SVDecompQR::DecompType::CHOLESKY);
    svdDecomp.decompose();
    svdDecomp.getSingularValues(singularValues);
    readVector(FILE_INPUT_EXP, expSinVals);
    compareSinVals(singularValues, expSinVals);
}


TEST(SVDEigenDecomp, 3SizeCntMatrix) {
    static const string FILE_INPUT = getTestPath(2) + TEST_FILE_IN;
    static const string FILE_INPUT_EXP = getTestPath(2) + SVD_COMP_FILE_NAME;

    Eigen::VectorXd singularValues, expSinVals;
    SVDecompEigDec svdDecomp(FILE_INPUT);
    svdDecomp.decompose();
    svdDecomp.getSingularValues(singularValues);
    readVector(FILE_INPUT_EXP, expSinVals);
    compareSinVals(singularValues, expSinVals);
}

TEST(SVDAltMin, 3SizeCntMatrix) {
    static const string FILE_INPUT = getTestPath(2) + TEST_FILE_IN;
    static const string FILE_INPUT_EXP = getTestPath(2) + SVD_COMP_FILE_NAME;

    Eigen::VectorXd singularValues, expSinVals;
    SVDecompAltMin svdDecomp(FILE_INPUT);
    svdDecomp.decompose();
    svdDecomp.getSingularValues(singularValues);
    readVector(FILE_INPUT_EXP, expSinVals);
    compareSinVals(singularValues, expSinVals);
}

TEST(SVDEigenDecomp, 3Size2Cnt1Cat2atrix) {
    static const string FILE_INPUT = getTestPath(3) + TEST_FILE_IN;
    static const string FILE_INPUT_EXP = getTestPath(3) + SVD_COMP_FILE_NAME;

    Eigen::VectorXd singularValues, expSinVals;
    SVDecompEigDec svdDecomp(FILE_INPUT);
    svdDecomp.decompose();
    svdDecomp.getSingularValues(singularValues);
    readVector(FILE_INPUT_EXP, expSinVals);
    compareSinVals(singularValues, expSinVals);
}

TEST(SVDAltMin, DISABLED_3Size2Cnt1Cat2atrix) {
    static const string FILE_INPUT = getTestPath(3) + TEST_FILE_IN;
    static const string FILE_INPUT_EXP = getTestPath(3) + SVD_COMP_FILE_NAME;

    Eigen::VectorXd singularValues, expSinVals;
    SVDecompAltMin svdDecomp(FILE_INPUT);
    svdDecomp.decompose();
    svdDecomp.getSingularValues(singularValues);
    readVector(FILE_INPUT_EXP, expSinVals);
    compareSinVals(singularValues, expSinVals);
}

TEST(SVDNaive, 3Size2Cnt1Cat3Matrix) {
    static const string FILE_INPUT = getTestPath(4) + TEST_FILE_IN;
    static const string FILE_INPUT_EXP = getTestPath(4) + SVD_COMP_FILE_NAME;

    Eigen::VectorXd singularValues, expSinVals;
    SVDecompQR svdDecomp(FILE_INPUT, SVDecompQR::DecompType::NAIVE);
    svdDecomp.decompose();
    svdDecomp.getSingularValues(singularValues);
    readVector(FILE_INPUT_EXP, expSinVals);
    compareSinVals(singularValues, expSinVals);
}

TEST(SVDSingleThreaded, 3Size2Cnt1Cat3Matrix) {
    static const string FILE_INPUT = getTestPath(4) + TEST_FILE_IN;
    static const string FILE_INPUT_EXP = getTestPath(4) + SVD_COMP_FILE_NAME;

    Eigen::VectorXd singularValues, expSinVals;
    SVDecompQR svdDecomp(FILE_INPUT, SVDecompQR::DecompType::SINGLE_THREAD);
    svdDecomp.decompose();
    svdDecomp.getSingularValues(singularValues);
    readVector(FILE_INPUT_EXP, expSinVals);
    compareSinVals(singularValues, expSinVals);
}

TEST(SVDMultiThreaded, 3Size2Cnt1Cat3Matrix) {
        static const string FILE_INPUT = getTestPath(4) + TEST_FILE_IN;
    static const string FILE_INPUT_EXP = getTestPath(4) + SVD_COMP_FILE_NAME;

    Eigen::VectorXd singularValues, expSinVals;
    SVDecompQR svdDecomp(FILE_INPUT, SVDecompQR::DecompType::MULTI_THREAD);
    svdDecomp.decompose();
    svdDecomp.getSingularValues(singularValues);
    readVector(FILE_INPUT_EXP, expSinVals);
    compareSinVals(singularValues, expSinVals);
}

TEST(SVDCholesky, 3Size2Cnt1Cat3Matrix) {
    static const string FILE_INPUT = getTestPath(4) + TEST_FILE_IN;
    static const string FILE_INPUT_EXP = getTestPath(4) + SVD_COMP_FILE_NAME;

    Eigen::VectorXd singularValues, expSinVals;
    SVDecompQR svdDecomp(FILE_INPUT, SVDecompQR::DecompType::CHOLESKY);
    svdDecomp.decompose();
    svdDecomp.getSingularValues(singularValues);
    readVector(FILE_INPUT_EXP, expSinVals);
    compareSinVals(singularValues, expSinVals);
}

TEST(SVDEigenDecomp, 3Size2Cnt1Cat3Matrix) {
    static const string FILE_INPUT = getTestPath(4) + TEST_FILE_IN;
    static const string FILE_INPUT_EXP = getTestPath(4) + SVD_COMP_FILE_NAME;

    Eigen::VectorXd singularValues, expSinVals;
    SVDecompEigDec svdDecomp(FILE_INPUT);
    svdDecomp.decompose();
    svdDecomp.getSingularValues(singularValues);
    readVector(FILE_INPUT_EXP, expSinVals);
    compareSinVals(singularValues, expSinVals);
}

TEST(SVDAltMin, 3Size2Cnt1Cat3Matrix) {
    static const string FILE_INPUT = getTestPath(4) + TEST_FILE_IN;
    static const string FILE_INPUT_EXP = getTestPath(4) + SVD_COMP_FILE_NAME;

    Eigen::VectorXd singularValues, expSinVals;
    SVDecompAltMin svdDecomp(FILE_INPUT);
    svdDecomp.decompose();
    svdDecomp.getSingularValues(singularValues);
    readVector(FILE_INPUT_EXP, expSinVals);
    compareSinVals(singularValues, expSinVals);
}

TEST(SVDCholesky, 3Size2Cnt1Cat33Matrix) {
    static const string FILE_INPUT = getTestPath(5) + TEST_FILE_IN;
    static const string FILE_INPUT_EXP = getTestPath(5) + SVD_COMP_FILE_NAME;

    Eigen::VectorXd singularValues, expSinVals;
    SVDecompQR svdDecomp(FILE_INPUT, SVDecompQR::DecompType::CHOLESKY);
    svdDecomp.decompose();
    svdDecomp.getSingularValues(singularValues);
    readVector(FILE_INPUT_EXP, expSinVals);
    compareSinVals(singularValues, expSinVals);
}

TEST(SVDEigenDecomp, 3Size2Cnt1Cat33Matrix) {
    static const string FILE_INPUT = getTestPath(5) + TEST_FILE_IN;
    static const string FILE_INPUT_EXP = getTestPath(5) + SVD_COMP_FILE_NAME;

    Eigen::VectorXd singularValues, expSinVals;
    SVDecompEigDec svdDecomp(FILE_INPUT);
    svdDecomp.decompose();
    svdDecomp.getSingularValues(singularValues);
    readVector(FILE_INPUT_EXP, expSinVals);
    compareSinVals(singularValues, expSinVals);
}

TEST(SVDEigenAltMin, DISABLED_3Size2Cnt1Cat33Matrix) {
    static const string FILE_INPUT = getTestPath(5) + TEST_FILE_IN;
    static const string FILE_INPUT_EXP = getTestPath(5) + SVD_COMP_FILE_NAME;

    Eigen::VectorXd singularValues, expSinVals;
    SVDecompAltMin svdDecomp(FILE_INPUT);
    svdDecomp.decompose();
    svdDecomp.getSingularValues(singularValues);
    readVector(FILE_INPUT_EXP, expSinVals);
    compareSinVals(singularValues, expSinVals);
}

TEST(SVDCholesky, DISABLED_3SizeCntZeroMatrix) {
    static const string FILE_INPUT = getTestPath(6) + TEST_FILE_IN;
    static const string FILE_INPUT_EXP = getTestPath(6) + SVD_COMP_FILE_NAME;

    Eigen::VectorXd singularValues, expSinVals;
    SVDecompQR svdDecomp(FILE_INPUT, SVDecompQR::DecompType::CHOLESKY);
    svdDecomp.decompose();
    svdDecomp.getSingularValues(singularValues);
    readVector(FILE_INPUT_EXP, expSinVals);
    compareSinVals(singularValues, expSinVals);
}
