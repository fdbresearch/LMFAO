#include <gtest/gtest.h>
#include "QRDecomp.h"
#include "Eigen/Dense"


using namespace LMFAO::LinearAlgebra;
using namespace std;

constexpr long double PRECISION_ERROR = 1e-10; 

TEST(QRNaive, 2SIzeMatrix) {
    static const string FILE_INPUT = "tests/data/test2.in";
    QRDecompositionNaive qrDecomp(FILE_INPUT);
    qrDecomp.decompose();
    Eigen::MatrixXd R;
    qrDecomp.getR(R);
    EXPECT_NEAR(R(0, 0), 3.1622776601683795, PRECISION_ERROR);
    EXPECT_NEAR(R(0, 1), 4.427188724235731, PRECISION_ERROR);
    EXPECT_NEAR(R(1, 0), 0, PRECISION_ERROR);
    EXPECT_NEAR(R(1, 1), 0.6324555320336757, PRECISION_ERROR);
}

TEST(QRSingleThreaded, 2SIzeMatrix)
{
    static const string FILE_INPUT = "tests/data/test2.in";
    QRDecompositionSingleThreaded qrDecomp(FILE_INPUT);
    qrDecomp.decompose();
    Eigen::MatrixXd R;
    qrDecomp.getR(R);
    EXPECT_NEAR(R(0, 0), 3.1622776601683795, PRECISION_ERROR);
    EXPECT_NEAR(R(0, 1), 4.427188724235731, PRECISION_ERROR);
    EXPECT_NEAR(R(1, 0), 0, PRECISION_ERROR);
    EXPECT_NEAR(R(1, 1), 0.6324555320336757, PRECISION_ERROR);
}

TEST(QRNaive, 3SIzeMatrix)
{
    static const string FILE_INPUT = "tests/data/test3.in";
    QRDecompositionNaive qrDecomp(FILE_INPUT);
    qrDecomp.decompose();
    Eigen::MatrixXd R;
    qrDecomp.getR(R);
    EXPECT_NEAR(R(0, 0), 8.12403840463596, PRECISION_ERROR);
    EXPECT_NEAR(R(0, 1), 9.601136296387953, PRECISION_ERROR);
    EXPECT_NEAR(R(0, 2), 11.939874624995275, PRECISION_ERROR);
    EXPECT_NEAR(R(1, 0), 0, PRECISION_ERROR);
    EXPECT_NEAR(R(1, 1), 0.9045340337332909, PRECISION_ERROR);
    EXPECT_NEAR(R(1, 2), 1.5075567228888185, PRECISION_ERROR);
    EXPECT_NEAR(R(2, 2), 0.40824829046386274, PRECISION_ERROR);
}

TEST(QRSingleThreaded, 3SIzeMatrix)
{
    static const string FILE_INPUT = "tests/data/test3.in";
    QRDecompositionSingleThreaded qrDecomp(FILE_INPUT);
    qrDecomp.decompose();
    Eigen::MatrixXd R;
    qrDecomp.getR(R);
    EXPECT_NEAR(R(0, 0), 8.12403840463596, PRECISION_ERROR);
    EXPECT_NEAR(R(0, 1), 9.601136296387953, PRECISION_ERROR);
    EXPECT_NEAR(R(0, 2), 11.939874624995275, PRECISION_ERROR);
    EXPECT_NEAR(R(1, 0), 0, PRECISION_ERROR);
    EXPECT_NEAR(R(1, 1), 0.9045340337332909, PRECISION_ERROR);
    EXPECT_NEAR(R(1, 2), 1.5075567228888185, PRECISION_ERROR);
    EXPECT_NEAR(R(2, 2), 0.40824829046386274, PRECISION_ERROR);
}
