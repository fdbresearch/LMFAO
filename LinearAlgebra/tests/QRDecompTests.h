#include <gtest/gtest.h>
#include "QRDecomp.h"
#include "Eigen/Dense"


using namespace LMFAO::LinearAlgebra;
using namespace std;

constexpr long double QR_TEST_PRECISION_ERROR = 1e-10;

TEST(QRNaive, 2SizeCntMatrix) {
    static const string FILE_INPUT = "tests/data/test1/test.in";
    QRDecompositionNaive qrDecomp(FILE_INPUT);
    qrDecomp.decompose();
    Eigen::MatrixXd R;
    qrDecomp.getR(R);
    EXPECT_NEAR(R(0, 0), 3.1622776601683795, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(0, 1), 4.427188724235731, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 0), 0, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 1), 0.6324555320336757, QR_TEST_PRECISION_ERROR);
}

TEST(QRSingleThreaded, 2SizeCntMatrix)
{
    static const string FILE_INPUT = "tests/data/test1/test.in";
    QRDecompositionSingleThreaded qrDecomp(FILE_INPUT);
    qrDecomp.decompose();
    Eigen::MatrixXd R;
    qrDecomp.getR(R);
    EXPECT_NEAR(R(0, 0), 3.1622776601683795, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(0, 1), 4.427188724235731, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 0), 0, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 1), 0.6324555320336757, QR_TEST_PRECISION_ERROR);
}

TEST(QRMultiThreaded, 2SizeCntMatrix)
{
    static const string FILE_INPUT = "tests/data/test1/test.in";
    QRDecompositionMultiThreaded qrDecomp(FILE_INPUT);
    qrDecomp.decompose();
    Eigen::MatrixXd R;
    qrDecomp.getR(R);
    EXPECT_NEAR(R(0, 0), 3.1622776601683795, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(0, 1), 4.427188724235731, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 0), 0, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 1), 0.6324555320336757, QR_TEST_PRECISION_ERROR);
}


TEST(QRNaive, 3SizeCntMatrix)
{
    static const string FILE_INPUT = "tests/data/test2/test.in";
    QRDecompositionNaive qrDecomp(FILE_INPUT);
    qrDecomp.decompose();
    Eigen::MatrixXd R;
    qrDecomp.getR(R);
    EXPECT_NEAR(R(0, 0), 8.12403840463596, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(0, 1), 9.601136296387953, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(0, 2), 11.939874624995275, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 0), 0, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 1), 0.9045340337332909, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 2), 1.5075567228888185, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(2, 2), 0.40824829046386274, QR_TEST_PRECISION_ERROR);
}

TEST(QRSingleThreaded, 3SizeCntMatrix)
{
    static const string FILE_INPUT = "tests/data/test2/test.in";
    QRDecompositionSingleThreaded qrDecomp(FILE_INPUT);
    qrDecomp.decompose();
    Eigen::MatrixXd R;
    qrDecomp.getR(R);
    EXPECT_NEAR(R(0, 0), 8.12403840463596, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(0, 1), 9.601136296387953, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(0, 2), 11.939874624995275, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 0), 0, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 1), 0.9045340337332909, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 2), 1.5075567228888185, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(2, 2), 0.40824829046386274, QR_TEST_PRECISION_ERROR);
}

TEST(QRMultiThreaded, 3SizeCntMatrix)
{
    static const string FILE_INPUT = "tests/data/test2/test.in";
    QRDecompositionMultiThreaded qrDecomp(FILE_INPUT);
    qrDecomp.decompose();
    Eigen::MatrixXd R;
    qrDecomp.getR(R);
    EXPECT_NEAR(R(0, 0), 8.12403840463596, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(0, 1), 9.601136296387953, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(0, 2), 11.939874624995275, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 0), 0, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 1), 0.9045340337332909, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 2), 1.5075567228888185, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(2, 2), 0.40824829046386274, QR_TEST_PRECISION_ERROR);
}

TEST(QRNaive, 3Size2Cnt1Cat2atrix)
{
    static const string FILE_INPUT = "tests/data/test3/test.in";
    QRDecompositionNaive qrDecomp(FILE_INPUT);
    qrDecomp.decompose();
    Eigen::MatrixXd R;
    qrDecomp.getR(R);
    EXPECT_NEAR(R(0, 0), 1.41421356237, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(0, 1), 4.24264068712, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(0, 2), 6.36396103068, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(0, 3), 0.70710678118, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 0), 0, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 1), 1.41421356237, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 2), 2.12132034356, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 3), 0.707106781186, QR_TEST_PRECISION_ERROR);
}

TEST(QRSingleThreaded, 3Size2Cnt1Cat2Matrix)
{
    static const string FILE_INPUT = "tests/data/test3/test.in";
    QRDecompositionSingleThreaded qrDecomp(FILE_INPUT);
    qrDecomp.decompose();
    Eigen::MatrixXd R;
    qrDecomp.getR(R);
    EXPECT_NEAR(R(0, 0), 1.41421356237, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(0, 1), 4.24264068712, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(0, 2), 6.36396103068, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(0, 3), 0.70710678118, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 0), 0, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 1), 1.41421356237, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 2), 2.12132034356, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 3), 0.707106781186, QR_TEST_PRECISION_ERROR);
}

TEST(QRMultiThreaded, 3Size2Cnt1Cat2Matrix)
{
    static const string FILE_INPUT = "tests/data/test3/test.in";
    QRDecompositionMultiThreaded qrDecomp(FILE_INPUT);
    qrDecomp.decompose();
    Eigen::MatrixXd R;
    qrDecomp.getR(R);
    EXPECT_NEAR(R(0, 0), 1.41421356237, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(0, 1), 4.24264068712, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(0, 2), 6.36396103068, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(0, 3), 0.70710678118, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 0), 0, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 1), 1.41421356237, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 2), 2.12132034356, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 3), 0.707106781186, QR_TEST_PRECISION_ERROR);
}

TEST(QRNaive, 3Size2Cnt1Cat3Matrix)
{
    static const string FILE_INPUT = "tests/data/test4/test.in";
    QRDecompositionNaive qrDecomp(FILE_INPUT);
    qrDecomp.decompose();
    Eigen::MatrixXd R;
    qrDecomp.getR(R);
    EXPECT_NEAR(R(0, 0), 2, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(0, 1), 11, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(0, 2), 0.5, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(0, 3), 1, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 0), 0, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 1), 5.38516480713, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 2), -0.27854300726, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 3), 0.92847669088, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(2, 2), 0.82000841038, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(2, 3), -0.29436199347, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(3, 3), 0.22645540682, QR_TEST_PRECISION_ERROR);
}

TEST(QRSingleThreaded, 3Size2Cnt1Cat3Matrix)
{
    static const string FILE_INPUT = "tests/data/test4/test.in";
    QRDecompositionSingleThreaded qrDecomp(FILE_INPUT);
    qrDecomp.decompose();
    Eigen::MatrixXd R;
    qrDecomp.getR(R);
    EXPECT_NEAR(R(0, 0), 2, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(0, 1), 11, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(0, 2), 0.5, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(0, 3), 1, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 0), 0, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 1), 5.38516480713, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 2), -0.27854300726, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 3), 0.92847669088, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(2, 2), 0.82000841038, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(2, 3), -0.29436199347, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(3, 3), 0.22645540682, QR_TEST_PRECISION_ERROR);
}

TEST(QRMultiThreaded, 3Size2Cnt1Cat3Matrix)
{
    static const string FILE_INPUT = "tests/data/test4/test.in";
    QRDecompositionMultiThreaded qrDecomp(FILE_INPUT);
    qrDecomp.decompose();
    Eigen::MatrixXd R;
    qrDecomp.getR(R);
    EXPECT_NEAR(R(0, 0), 2, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(0, 1), 11, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(0, 2), 0.5, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(0, 3), 1, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 0), 0, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 1), 5.38516480713, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 2), -0.27854300726, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 3), 0.92847669088, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(2, 2), 0.82000841038, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(2, 3), -0.29436199347, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(3, 3), 0.22645540682, QR_TEST_PRECISION_ERROR);
}

TEST(QRNaive, 3Size2Cnt2Cat33Matrix)
{
    static const string FILE_INPUT = "tests/data/test5/test.in";
    QRDecompositionNaive qrDecomp(FILE_INPUT);
    qrDecomp.decompose();
    Eigen::MatrixXd R;
    qrDecomp.getR(R);
    EXPECT_NEAR(R(0, 0), 2, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(0, 1), 11, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(0, 2), 0.5, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(0, 3), 1, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(0, 4), 1, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(0, 5), 0.5, QR_TEST_PRECISION_ERROR);

    EXPECT_NEAR(R(1, 0), 0, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 1), 5.38516480713, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 2), -0.27854300726, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 3), 0.92847669088, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 4), 0, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 5), -0.64993368362, QR_TEST_PRECISION_ERROR);

    EXPECT_NEAR(R(2, 2), 0.82000841038, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(2, 3), -0.29436199347, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(2, 4), 0.60974984362, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(2, 5), -0.52564641691, QR_TEST_PRECISION_ERROR);

    EXPECT_NEAR(R(3, 3), 0.22645540682, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(3, 4), 0.7925939239, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(3, 5), -0.22645540682, QR_TEST_PRECISION_ERROR);
}

TEST(QRSingleThreaded, 3Size2Cnt1Cat33Matrix)
{
    static const string FILE_INPUT = "tests/data/test5/test.in";
    QRDecompositionSingleThreaded qrDecomp(FILE_INPUT);
    qrDecomp.decompose();
    Eigen::MatrixXd R;
    qrDecomp.getR(R);
    EXPECT_NEAR(R(0, 0), 2, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(0, 1), 11, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(0, 2), 0.5, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(0, 3), 1, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(0, 4), 1, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(0, 5), 0.5, QR_TEST_PRECISION_ERROR);

    EXPECT_NEAR(R(1, 0), 0, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 1), 5.38516480713, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 2), -0.27854300726, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 3), 0.92847669088, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 4), 0, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 5), -0.64993368362, QR_TEST_PRECISION_ERROR);

    EXPECT_NEAR(R(2, 2), 0.82000841038, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(2, 3), -0.29436199347, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(2, 4), 0.60974984362, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(2, 5), -0.52564641691, QR_TEST_PRECISION_ERROR);

    EXPECT_NEAR(R(3, 3), 0.22645540682, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(3, 4), 0.7925939239, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(3, 5), -0.22645540682, QR_TEST_PRECISION_ERROR);
}

TEST(QRMultiThreaded, 3Size2Cnt1Cat33Matrix)
{
    static const string FILE_INPUT = "tests/data/test5/test.in";
    QRDecompositionMultiThreaded qrDecomp(FILE_INPUT);
    qrDecomp.decompose();
    Eigen::MatrixXd R;
    qrDecomp.getR(R);
    EXPECT_NEAR(R(0, 0), 2, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(0, 1), 11, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(0, 2), 0.5, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(0, 3), 1, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(0, 4), 1, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(0, 5), 0.5, QR_TEST_PRECISION_ERROR);

    EXPECT_NEAR(R(1, 0), 0, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 1), 5.38516480713, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 2), -0.27854300726, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 3), 0.92847669088, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 4), 0, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 5), -0.64993368362, QR_TEST_PRECISION_ERROR);

    EXPECT_NEAR(R(2, 2), 0.82000841038, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(2, 3), -0.29436199347, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(2, 4), 0.60974984362, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(2, 5), -0.52564641691, QR_TEST_PRECISION_ERROR);

    EXPECT_NEAR(R(3, 3), 0.22645540682, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(3, 4), 0.7925939239, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(3, 5), -0.22645540682, QR_TEST_PRECISION_ERROR);
}


TEST(QRNaive, 3SizeCntZeroMatrix)
{
    static const string FILE_INPUT = "tests/data/test6/test.in";
    QRDecompositionNaive qrDecomp(FILE_INPUT, true /*isLinDepAllowed*/);
    qrDecomp.decompose();
    Eigen::MatrixXd R;
    qrDecomp.getR(R);
    EXPECT_NEAR(R(0, 0), 3.74165738677, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(0, 1), 7.48331477355, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(0, 2), 11.2249721603, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 0), 0, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 1), 0, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 2), 0, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(2, 2), 0, QR_TEST_PRECISION_ERROR);
}

TEST(QRSingleThreaded, 3SizeCntZeroMatrix)
{
    static const string FILE_INPUT = "tests/data/test6/test.in";
    QRDecompositionSingleThreaded qrDecomp(FILE_INPUT, true /*isLinDepAllowed*/);
    qrDecomp.decompose();
    Eigen::MatrixXd R;
    qrDecomp.getR(R);
    EXPECT_NEAR(R(0, 0), 3.74165738677, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(0, 1), 7.48331477355, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(0, 2), 11.2249721603, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 0), 0, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 1), 0, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 2), 0, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(2, 2), 0, QR_TEST_PRECISION_ERROR);
}


TEST(QRMultiThreaded, 3SizeCntZeroMatrix)
{
    static const string FILE_INPUT = "tests/data/test6/test.in";
    QRDecompositionMultiThreaded qrDecomp(FILE_INPUT, true /*isLinDepAllowed*/);
    qrDecomp.decompose();
    Eigen::MatrixXd R;
    qrDecomp.getR(R);
    EXPECT_NEAR(R(0, 0), 3.74165738677, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(0, 1), 7.48331477355, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(0, 2), 11.2249721603, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 0), 0, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 1), 0, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(1, 2), 0, QR_TEST_PRECISION_ERROR);
    EXPECT_NEAR(R(2, 2), 0, QR_TEST_PRECISION_ERROR);
}
