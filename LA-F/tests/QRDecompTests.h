#include "UtilsTests.h"
#include "QR/QRDecomp.h"

using namespace LMFAO::LinearAlgebra;
using namespace std;

constexpr double QR_TEST_PRECISION_ERROR = 1e-10;
const std::string R_COMP_FILE_NAME = "testExpR.in";
const std::string C_COMP_FILE_NAME = "testExpC.in";

void compareMatrices(Eigen::MatrixXd& R, Eigen::MatrixXd& expR, bool cmpRowNum=true, bool cmpColNum=true)
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
    static const string FILE_INPUT_EXP_R = getTestPath(1) + R_COMP_FILE_NAME;
    static const string FILE_INPUT_EXP_C = getTestPath(1) + C_COMP_FILE_NAME;
    QRDecompNaive qrDecomp(FILE_INPUT);
    qrDecomp.decompose();
    Eigen::MatrixXd R, expR, C, expC, covar;
    qrDecomp.getR(R);
    qrDecomp.getC(C);
    readMatrixDense(FILE_INPUT_EXP_R, expR);
    readMatrixDense(FILE_INPUT_EXP_C, expC);
    compareMatrices(R, expR);
    compareMatrices(C, expC);
    EXPECT_NEAR(qrDecomp.getQTQFrobeniusError(), 0, QR_TEST_PRECISION_ERROR);
}

TEST(QRSingleThreaded, 2SizeCntMatrix)
{
    static const string FILE_INPUT = getTestPath(1) + TEST_FILE_IN;
    static const string FILE_INPUT_EXP_R = getTestPath(1) + R_COMP_FILE_NAME;
    static const string FILE_INPUT_EXP_C = getTestPath(1) + C_COMP_FILE_NAME;
    QRDecompSingleThread qrDecomp(FILE_INPUT);
    qrDecomp.decompose();
    Eigen::MatrixXd R, expR, C, expC;
    qrDecomp.getR(R);
    qrDecomp.getC(C);
    readMatrixDense(FILE_INPUT_EXP_R, expR);
    readMatrixDense(FILE_INPUT_EXP_C, expC);
    compareMatrices(R, expR);
    compareMatrices(C, expC);
    EXPECT_NEAR(qrDecomp.getQTQFrobeniusError(), 0, QR_TEST_PRECISION_ERROR);
}

TEST(QRMultiThreaded, 2SizeCntMatrix)
{
    static const string FILE_INPUT = getTestPath(1) + TEST_FILE_IN;
    static const string FILE_INPUT_EXP_R = getTestPath(1) + R_COMP_FILE_NAME;
    static const string FILE_INPUT_EXP_C = getTestPath(1) + C_COMP_FILE_NAME;
    QRDecompMultiThread qrDecomp(FILE_INPUT);
    qrDecomp.decompose();
    Eigen::MatrixXd R, expR, C, expC;
    qrDecomp.getR(R);
    qrDecomp.getC(C);
    readMatrixDense(FILE_INPUT_EXP_R, expR);
    readMatrixDense(FILE_INPUT_EXP_C, expC);
    compareMatrices(R, expR);
    compareMatrices(C, expC);
    EXPECT_NEAR(qrDecomp.getQTQFrobeniusError(), 0, QR_TEST_PRECISION_ERROR);
}

TEST(QRCholesky, 2SizeCntMatrix)
{
    static const string FILE_INPUT = getTestPath(1) + TEST_FILE_IN;
    static const string FILE_INPUT_EXP_R = getTestPath(1) + R_COMP_FILE_NAME;
    static const string FILE_INPUT_EXP_C = getTestPath(1) + C_COMP_FILE_NAME;
    QRDecompCholesky qrDecomp(FILE_INPUT);
    qrDecomp.decompose();
    Eigen::MatrixXd R, expR, C, expC;
    qrDecomp.getR(R);
    qrDecomp.getC(C);
    readMatrixDense(FILE_INPUT_EXP_R, expR);
    readMatrixDense(FILE_INPUT_EXP_C, expC);
    compareMatrices(R, expR);
    compareMatrices(C, expC);
    EXPECT_NEAR(qrDecomp.getQTQFrobeniusError(), 0, QR_TEST_PRECISION_ERROR);
}

TEST(QRNaive, 3SizeCntMatrix)
{
    static const string FILE_INPUT = getTestPath(2) + TEST_FILE_IN;
    static const string FILE_INPUT_EXP_R = getTestPath(2) + R_COMP_FILE_NAME;
    QRDecompNaive qrDecomp(FILE_INPUT);
    qrDecomp.decompose();
    Eigen::MatrixXd R, expR;
    qrDecomp.getR(R);
    readMatrixDense(FILE_INPUT_EXP_R, expR);
    compareMatrices(R, expR);
    EXPECT_NEAR(qrDecomp.getQTQFrobeniusError(), 0, QR_TEST_PRECISION_ERROR);
}

TEST(QRSingleThreaded, 3SizeCntMatrix)
{
    static const string FILE_INPUT = getTestPath(2) + TEST_FILE_IN;
    static const string FILE_INPUT_EXP_R = getTestPath(2) + R_COMP_FILE_NAME;
    QRDecompSingleThread qrDecomp(FILE_INPUT);
    qrDecomp.decompose();
    Eigen::MatrixXd R, expR;
    qrDecomp.getR(R);
    readMatrixDense(FILE_INPUT_EXP_R, expR);
    compareMatrices(R, expR);
    EXPECT_NEAR(qrDecomp.getQTQFrobeniusError(), 0, QR_TEST_PRECISION_ERROR);
}

TEST(QRMultiThreaded, 3SizeCntMatrix)
{
    static const string FILE_INPUT = getTestPath(2) + TEST_FILE_IN;
    static const string FILE_INPUT_EXP_R = getTestPath(2) + R_COMP_FILE_NAME;
    QRDecompMultiThread qrDecomp(FILE_INPUT);
    qrDecomp.decompose();
    Eigen::MatrixXd R, expR;
    qrDecomp.getR(R);
    readMatrixDense(FILE_INPUT_EXP_R, expR);
    compareMatrices(R, expR);
    EXPECT_NEAR(qrDecomp.getQTQFrobeniusError(), 0, QR_TEST_PRECISION_ERROR);
}

TEST(QRCholesky, 3SizeCntMatrix)
{
    static const string FILE_INPUT = getTestPath(2) + TEST_FILE_IN;
    static const string FILE_INPUT_EXP_R = getTestPath(2) + R_COMP_FILE_NAME;
    QRDecompCholesky qrDecomp(FILE_INPUT);
    qrDecomp.decompose();
    Eigen::MatrixXd R, expR;
    qrDecomp.getR(R);
    readMatrixDense(FILE_INPUT_EXP_R, expR);
    compareMatrices(R, expR);
    EXPECT_NEAR(qrDecomp.getQTQFrobeniusError(), 0, QR_TEST_PRECISION_ERROR);
}

TEST(QRNaive, 3Size2Cnt1Cat2atrix)
{
    static const string FILE_INPUT = getTestPath(3) + TEST_FILE_IN;
    static const string FILE_INPUT_EXP_R = getTestPath(3) + R_COMP_FILE_NAME;
    QRDecompNaive qrDecomp(FILE_INPUT);
    qrDecomp.decompose();
    Eigen::MatrixXd R, expR;
    qrDecomp.getR(R);
    readMatrixDense(FILE_INPUT_EXP_R, expR);
    compareMatrices(R, expR, false, true);
}

TEST(QRSingleThreaded, 3Size2Cnt1Cat2Matrix)
{
    static const string FILE_INPUT = getTestPath(3) + TEST_FILE_IN;
    static const string FILE_INPUT_EXP_R = getTestPath(3) + R_COMP_FILE_NAME;
    QRDecompSingleThread qrDecomp(FILE_INPUT);
    qrDecomp.decompose();
    Eigen::MatrixXd R, expR;
    qrDecomp.getR(R);
    readMatrixDense(FILE_INPUT_EXP_R, expR);
    compareMatrices(R, expR, false, true);
}

TEST(QRMultiThreaded, 3Size2Cnt1Cat2Matrix)
{
    static const string FILE_INPUT = getTestPath(3) + TEST_FILE_IN;
    static const string FILE_INPUT_EXP_R = getTestPath(3) + R_COMP_FILE_NAME;
    QRDecompMultiThread qrDecomp(FILE_INPUT);
    qrDecomp.decompose();
    Eigen::MatrixXd R, expR;
    qrDecomp.getR(R);
    readMatrixDense(FILE_INPUT_EXP_R, expR);
    compareMatrices(R, expR, false, true);
}

TEST(QRCholesky, 3Size2Cnt1Cat2Matrix)
{
    static const string FILE_INPUT = getTestPath(3) + TEST_FILE_IN;
    static const string FILE_INPUT_EXP_R = getTestPath(3) + R_COMP_FILE_NAME;
    QRDecompCholesky qrDecomp(FILE_INPUT);
    qrDecomp.decompose();
    Eigen::MatrixXd R, expR;
    qrDecomp.getR(R);
    readMatrixDense(FILE_INPUT_EXP_R, expR);
    compareMatrices(R, expR, false, true);
}

TEST(QRNaive, 3Size2Cnt1Cat3Matrix)
{
    static const string FILE_INPUT = getTestPath(4) + TEST_FILE_IN;
    static const string FILE_INPUT_EXP_R = getTestPath(4) + R_COMP_FILE_NAME;
    QRDecompNaive qrDecomp(FILE_INPUT);
    qrDecomp.decompose();
    Eigen::MatrixXd R, expR;
    qrDecomp.getR(R);
    readMatrixDense(FILE_INPUT_EXP_R, expR);
    compareMatrices(R, expR);
    EXPECT_NEAR(qrDecomp.getQTQFrobeniusError(), 0, QR_TEST_PRECISION_ERROR);
}

TEST(QRSingleThreaded, 3Size2Cnt1Cat3Matrix)
{
    static const string FILE_INPUT = getTestPath(4) + TEST_FILE_IN;
    static const string FILE_INPUT_EXP_R = getTestPath(4) + R_COMP_FILE_NAME;
    QRDecompSingleThread qrDecomp(FILE_INPUT);
    qrDecomp.decompose();
    Eigen::MatrixXd R, expR;
    qrDecomp.getR(R);
    readMatrixDense(FILE_INPUT_EXP_R, expR);
    compareMatrices(R, expR);
    EXPECT_NEAR(qrDecomp.getQTQFrobeniusError(), 0, QR_TEST_PRECISION_ERROR);
}

TEST(QRMultiThreaded, 3Size2Cnt1Cat3Matrix)
{
    static const string FILE_INPUT = getTestPath(4) + TEST_FILE_IN;
    static const string FILE_INPUT_EXP_R = getTestPath(4) + R_COMP_FILE_NAME;
    QRDecompMultiThread qrDecomp(FILE_INPUT);
    qrDecomp.decompose();
    Eigen::MatrixXd R, expR;
    qrDecomp.getR(R);
    readMatrixDense(FILE_INPUT_EXP_R, expR);
    compareMatrices(R, expR);
    EXPECT_NEAR(qrDecomp.getQTQFrobeniusError(), 0, QR_TEST_PRECISION_ERROR);
}

TEST(QRCholesky, 3Size2Cnt1Cat3Matrix)
{
    static const string FILE_INPUT = getTestPath(4) + TEST_FILE_IN;
    static const string FILE_INPUT_EXP_R = getTestPath(4) + R_COMP_FILE_NAME;
    QRDecompCholesky qrDecomp(FILE_INPUT);
    qrDecomp.decompose();
    Eigen::MatrixXd R, expR;
    qrDecomp.getR(R);
    readMatrixDense(FILE_INPUT_EXP_R, expR);
    compareMatrices(R, expR);
    EXPECT_NEAR(qrDecomp.getQTQFrobeniusError(), 0, QR_TEST_PRECISION_ERROR);
}

TEST(QRNaive, 3Size2Cnt2Cat33Matrix)
{
    static const string FILE_INPUT = getTestPath(5) + TEST_FILE_IN;
    static const string FILE_INPUT_EXP_R = getTestPath(5) + R_COMP_FILE_NAME;
    QRDecompNaive qrDecomp(FILE_INPUT);
    qrDecomp.decompose();
    Eigen::MatrixXd R, expR;
    qrDecomp.getR(R);
    readMatrixDense(FILE_INPUT_EXP_R, expR);
    compareMatrices(R, expR, false, true);
}

TEST(QRSingleThreaded, 3Size2Cnt1Cat33Matrix)
{
    static const string FILE_INPUT = getTestPath(5) + TEST_FILE_IN;
    static const string FILE_INPUT_EXP_R = getTestPath(5) + R_COMP_FILE_NAME;
    QRDecompSingleThread qrDecomp(FILE_INPUT);
    qrDecomp.decompose();
    Eigen::MatrixXd R, expR;
    qrDecomp.getR(R);
    readMatrixDense(FILE_INPUT_EXP_R, expR);
    compareMatrices(R, expR, false, true);
}

TEST(QRMultiThreaded, 3Size2Cnt1Cat33Matrix)
{
    static const string FILE_INPUT = getTestPath(5) + TEST_FILE_IN;
    static const string FILE_INPUT_EXP_R = getTestPath(5) + R_COMP_FILE_NAME;
    QRDecompMultiThread qrDecomp(FILE_INPUT);
    qrDecomp.decompose();
    Eigen::MatrixXd R, expR;
    qrDecomp.getR(R);
    readMatrixDense(FILE_INPUT_EXP_R, expR);
    compareMatrices(R, expR, false, true);
}

TEST(QRCholesky, 3Size2Cnt1Cat33Matrix)
{
    static const string FILE_INPUT = getTestPath(5) + TEST_FILE_IN;
    static const string FILE_INPUT_EXP_R = getTestPath(5) + R_COMP_FILE_NAME;
    QRDecompCholesky qrDecomp(FILE_INPUT);
    qrDecomp.decompose();
    Eigen::MatrixXd R, expR;
    qrDecomp.getR(R);
    readMatrixDense(FILE_INPUT_EXP_R, expR);
    compareMatrices(R, expR, false, true);
}

TEST(QRNaive, 3SizeCntZeroMatrix)
{
    static const string FILE_INPUT = getTestPath(6) + TEST_FILE_IN;

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
    static const string FILE_INPUT = getTestPath(6) + TEST_FILE_IN;
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
    static const string FILE_INPUT = getTestPath(6) + TEST_FILE_IN;
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

// Cholesky is instable if we have linearly dependent columns
//
TEST(QRCholesky, DISABLED_3SizeCntZeroMatrix)
{
    static const string FILE_INPUT = getTestPath(6) + TEST_FILE_IN;
    QRDecompCholesky qrDecomp(FILE_INPUT, true /*isLinDepAllowed*/);
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
