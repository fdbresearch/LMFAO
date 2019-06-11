#include "UtilsTests.h"
#include "SVDecomp.h"

using namespace LMFAO::LinearAlgebra;
using namespace std;

constexpr double SVD_TEST_PRECISION_ERROR = 1e-10;
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

TEST(SVDNaive, DISABLED_3SizeCntMatrix) {
    static const string FILE_INPUT = getTestPath(2) + TEST_FILE_IN;
    static const string FILE_INPUT_EXP = getTestPath(2) + SVD_COMP_FILE_NAME;

    Eigen::VectorXd singularValues, expSinVals;
    SVDecompQR svdDecomp(FILE_INPUT, SVDecompQR::DecompType::NAIVE);
    svdDecomp.decompose();
    svdDecomp.getSingularValues(singularValues);
    readVector(FILE_INPUT_EXP, expSinVals);
    compareSinVals(singularValues, expSinVals);
}

TEST(SVDSingleThreaded, DISABLED_3SizeCntMatrix) {
    static const string FILE_INPUT = getTestPath(2) + TEST_FILE_IN;
    static const string FILE_INPUT_EXP = getTestPath(2) + SVD_COMP_FILE_NAME;

    Eigen::VectorXd singularValues, expSinVals;
    SVDecompQR svdDecomp(FILE_INPUT, SVDecompQR::DecompType::SINGLE_THREAD);
    svdDecomp.decompose();
    svdDecomp.getSingularValues(singularValues);
    readVector(FILE_INPUT_EXP, expSinVals);
    compareSinVals(singularValues, expSinVals);
}

TEST(SVDMultiThreaded, DISABLED_3SizeCntMatrix) {
        static const string FILE_INPUT = getTestPath(2) + TEST_FILE_IN;
    static const string FILE_INPUT_EXP = getTestPath(2) + SVD_COMP_FILE_NAME;

    Eigen::VectorXd singularValues, expSinVals;
    SVDecompQR svdDecomp(FILE_INPUT, SVDecompQR::DecompType::MULTI_THREAD);
    svdDecomp.decompose();
    svdDecomp.getSingularValues(singularValues);
    readVector(FILE_INPUT_EXP, expSinVals);
    compareSinVals(singularValues, expSinVals);
}


TEST(SVDEigenDecomp, DISABLED_3SizeCntMatrix) {
    static const string FILE_INPUT = getTestPath(2) + TEST_FILE_IN;
    static const string FILE_INPUT_EXP = getTestPath(2) + SVD_COMP_FILE_NAME;

    Eigen::VectorXd singularValues, expSinVals;
    SVDecompEigDec svdDecomp(FILE_INPUT);
    svdDecomp.decompose();
    svdDecomp.getSingularValues(singularValues);
    readVector(FILE_INPUT_EXP, expSinVals);
    compareSinVals(singularValues, expSinVals);
}
