#include <gtest/gtest.h>
#include "SVDecomp.h"

using namespace LMFAO::LinearAlgebra;
using namespace std;

constexpr double SVD_TEST_PRECISION_ERROR = 1e-10;

TEST(SVDNaive, 2SizeCntMatrix) {
    static const string FILE_INPUT = "tests/data/test1/test.in";
    SVDecompQR svdDecomp(FILE_INPUT, SVDecompQR::DecompType::NAIVE);
    svdDecomp.decompose();
    Eigen::VectorXd singularValues;
    svdDecomp.decompose();
    svdDecomp.getSingularValues(singularValues);
    EXPECT_NEAR(singularValues(0), 5.464985704219040, SVD_TEST_PRECISION_ERROR);
    EXPECT_NEAR(singularValues(1), 0.365966190626258, SVD_TEST_PRECISION_ERROR);
}

TEST(SVDSingleThreaded, 2SizeCntMatrix) {
    static const string FILE_INPUT = "tests/data/test1/test.in";
    SVDecompQR svdDecomp(FILE_INPUT, SVDecompQR::DecompType::SINGLE_THREAD);
    svdDecomp.decompose();
    Eigen::VectorXd singularValues;
    svdDecomp.decompose();
    svdDecomp.getSingularValues(singularValues);
    EXPECT_NEAR(singularValues(0), 5.464985704219040, SVD_TEST_PRECISION_ERROR);
    EXPECT_NEAR(singularValues(1), 0.365966190626258, SVD_TEST_PRECISION_ERROR);
}

TEST(SVDMultiThreaded, 2SizeCntMatrix) {
    static const string FILE_INPUT = "tests/data/test1/test.in";
    SVDecompQR svdDecomp(FILE_INPUT, SVDecompQR::DecompType::MULTI_THREAD);
    svdDecomp.decompose();
    Eigen::VectorXd singularValues;
    svdDecomp.decompose();
    svdDecomp.getSingularValues(singularValues);
    EXPECT_NEAR(singularValues(0), 5.464985704219040, SVD_TEST_PRECISION_ERROR);
    EXPECT_NEAR(singularValues(1), 0.365966190626258, SVD_TEST_PRECISION_ERROR);
}
