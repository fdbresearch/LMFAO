#include "UtilsTests.h"
#include "QRDecomp.h"

using namespace LMFAO::LinearAlgebra;
using namespace std;

constexpr double QR_TEST_PRECISION_ERROR = 1e-10;
const std::string R_COMP_FILE_NAME = "testExpR.in";

void compareR(Eigen::MatrixXd& R, Eigen::MatrixXd& expR, bool cmpRowNum=true, bool cmpColNum=true)
{
    if (cmpRowNum)
    {
        EXPECT_EQ(R.rows(), expR.rows());
    }
    if (cmpColNum)
    {
        EXPECT_EQ(R.cols(), expR.cols());
    }
    for (uint32_t row = 0; row < expR.rows(); row ++)
    {
        for (uint32_t col = 0; col < expR.cols(); col++)
        {
            EXPECT_NEAR(R(row, col), expR(row, col), QR_TEST_PRECISION_ERROR);
        }
    }
}

TEST(QRNaive, 2SizeCntMatrix)
{
    static const string FILE_INPUT = getTestPath(1) + TEST_FILE_IN;
    static const string FILE_INPUT_EXP = getTestPath(1) + R_COMP_FILE_NAME;
    QRDecompNaive qrDecomp(FILE_INPUT);
    qrDecomp.decompose();
    Eigen::MatrixXd R, expR;
    qrDecomp.getR(R);
    readMatrixDense(FILE_INPUT_EXP, expR);
    compareR(R, expR);
}

TEST(QRSingleThreaded, 2SizeCntMatrix)
{
    static const string FILE_INPUT = TEST_PATH + "test1/test.in";
    static const string FILE_INPUT_EXP = TEST_PATH + "test1/testExpR.in";
    QRDecompSingleThread qrDecomp(FILE_INPUT);
    qrDecomp.decompose();
    Eigen::MatrixXd R, expR;
    qrDecomp.getR(R);
    readMatrixDense(FILE_INPUT_EXP, expR);
    compareR(R, expR);
}

TEST(QRMultiThreaded, 2SizeCntMatrix)
{
    static const string FILE_INPUT = TEST_PATH + "test1/test.in";
    static const string FILE_INPUT_EXP = TEST_PATH + "test1/testExpR.in";
    QRDecompMultiThread qrDecomp(FILE_INPUT);
    qrDecomp.decompose();
    Eigen::MatrixXd R, expR;
    qrDecomp.getR(R);
    readMatrixDense(FILE_INPUT_EXP, expR);
    compareR(R, expR);
}


TEST(QRNaive, 3SizeCntMatrix)
{
    static const string FILE_INPUT = TEST_PATH + "test2/test.in";
    static const string FILE_INPUT_EXP = TEST_PATH + "test2/testExpR.in";
    QRDecompNaive qrDecomp(FILE_INPUT);
    qrDecomp.decompose();
    Eigen::MatrixXd R, expR;
    qrDecomp.getR(R);
    readMatrixDense(FILE_INPUT_EXP, expR);
    compareR(R, expR);
}

TEST(QRSingleThreaded, 3SizeCntMatrix)
{
    static const string FILE_INPUT = TEST_PATH + "test2/test.in";
    static const string FILE_INPUT_EXP = TEST_PATH + "test2/testExpR.in";
    QRDecompSingleThread qrDecomp(FILE_INPUT);
    qrDecomp.decompose();
    Eigen::MatrixXd R, expR;
    qrDecomp.getR(R);
    readMatrixDense(FILE_INPUT_EXP, expR);
    compareR(R, expR);
}

TEST(QRMultiThreaded, 3SizeCntMatrix)
{
    static const string FILE_INPUT = TEST_PATH + "test2/test.in";
    static const string FILE_INPUT_EXP = TEST_PATH + "test2/testExpR.in";
    QRDecompMultiThread qrDecomp(FILE_INPUT);
    qrDecomp.decompose();
    Eigen::MatrixXd R, expR;
    qrDecomp.getR(R);
    readMatrixDense(FILE_INPUT_EXP, expR);
    compareR(R, expR);
}

TEST(QRNaive, 3Size2Cnt1Cat2atrix)
{
    static const string FILE_INPUT = TEST_PATH + "test3/test.in";
    static const string FILE_INPUT_EXP = TEST_PATH + "test3/testExpR.in";
    QRDecompNaive qrDecomp(FILE_INPUT);
    qrDecomp.decompose();
    Eigen::MatrixXd R, expR;
    qrDecomp.getR(R);
    readMatrixDense(FILE_INPUT_EXP, expR);
    compareR(R, expR, false, true);
}

TEST(QRSingleThreaded, 3Size2Cnt1Cat2Matrix)
{
    static const string FILE_INPUT = TEST_PATH + "test3/test.in";
    static const string FILE_INPUT_EXP = TEST_PATH + "test3/testExpR.in";
    QRDecompSingleThread qrDecomp(FILE_INPUT);
    qrDecomp.decompose();
    Eigen::MatrixXd R, expR;
    qrDecomp.getR(R);
    readMatrixDense(FILE_INPUT_EXP, expR);
    compareR(R, expR, false, true);
}

TEST(QRMultiThreaded, 3Size2Cnt1Cat2Matrix)
{
    static const string FILE_INPUT = TEST_PATH + "test3/test.in";
    static const string FILE_INPUT_EXP = TEST_PATH + "test3/testExpR.in";
    QRDecompMultiThread qrDecomp(FILE_INPUT);
    qrDecomp.decompose();
    Eigen::MatrixXd R, expR;
    qrDecomp.getR(R);
    readMatrixDense(FILE_INPUT_EXP, expR);
    compareR(R, expR, false, true);
}

TEST(QRNaive, 3Size2Cnt1Cat3Matrix)
{
    static const string FILE_INPUT = TEST_PATH + "test4/test.in";
    static const string FILE_INPUT_EXP = TEST_PATH + "test4/testExpR.in";
    QRDecompNaive qrDecomp(FILE_INPUT);
    qrDecomp.decompose();
    Eigen::MatrixXd R, expR;
    qrDecomp.getR(R);
    readMatrixDense(FILE_INPUT_EXP, expR);
    compareR(R, expR);
}

TEST(QRSingleThreaded, 3Size2Cnt1Cat3Matrix)
{
    static const string FILE_INPUT = TEST_PATH + "test4/test.in";
    static const string FILE_INPUT_EXP = TEST_PATH + "test4/testExpR.in";
    QRDecompSingleThread qrDecomp(FILE_INPUT);
    qrDecomp.decompose();
    Eigen::MatrixXd R, expR;
    qrDecomp.getR(R);
    readMatrixDense(FILE_INPUT_EXP, expR);
    compareR(R, expR);
}

TEST(QRMultiThreaded, 3Size2Cnt1Cat3Matrix)
{
    static const string FILE_INPUT = TEST_PATH + "test4/test.in";
    static const string FILE_INPUT_EXP = TEST_PATH + "test4/testExpR.in";
    QRDecompMultiThread qrDecomp(FILE_INPUT);
    qrDecomp.decompose();
    Eigen::MatrixXd R, expR;
    qrDecomp.getR(R);
    readMatrixDense(FILE_INPUT_EXP, expR);
    compareR(R, expR);
}

TEST(QRNaive, 3Size2Cnt2Cat33Matrix)
{
    static const string FILE_INPUT = TEST_PATH + "test5/test.in";
    static const string FILE_INPUT_EXP = TEST_PATH + "test5/testExpR.in";
    QRDecompNaive qrDecomp(FILE_INPUT);
    qrDecomp.decompose();
    Eigen::MatrixXd R, expR;
    qrDecomp.getR(R);
    readMatrixDense(FILE_INPUT_EXP, expR);
    compareR(R, expR, false, true);
}

TEST(QRSingleThreaded, 3Size2Cnt1Cat33Matrix)
{
  static const string FILE_INPUT = TEST_PATH + "test5/test.in";
    static const string FILE_INPUT_EXP = TEST_PATH + "test5/testExpR.in";
    QRDecompSingleThread qrDecomp(FILE_INPUT);
    qrDecomp.decompose();
    Eigen::MatrixXd R, expR;
    qrDecomp.getR(R);
    readMatrixDense(FILE_INPUT_EXP, expR);
    compareR(R, expR, false, true);
}

TEST(QRMultiThreaded, 3Size2Cnt1Cat33Matrix)
{
    static const string FILE_INPUT = TEST_PATH + "test5/test.in";
    static const string FILE_INPUT_EXP = TEST_PATH + "test5/testExpR.in";
    QRDecompMultiThread qrDecomp(FILE_INPUT);
    qrDecomp.decompose();
    Eigen::MatrixXd R, expR;
    qrDecomp.getR(R);
    readMatrixDense(FILE_INPUT_EXP, expR);
    compareR(R, expR, false, true);
}

TEST(QRNaive, 3SizeCntZeroMatrix)
{
    static const string FILE_INPUT = "tests/data/test6/test.in";
    QRDecompNaive qrDecomp(FILE_INPUT, true /*isLinDepAllowed*/);
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
    QRDecompSingleThread qrDecomp(FILE_INPUT, true /*isLinDepAllowed*/);
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
    QRDecompMultiThread qrDecomp(FILE_INPUT, true /*isLinDepAllowed*/);
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
