#include <gtest/gtest.h>

/*
TEST(Matrix, Compare) {
    Matrix<long double> init({
                              { 1,2,2},
                              {-1,1,2},
                              {-1,0,1},
                              { 1,1,2}
                            });
    Matrix<long double> Q, Qb, R, Rb, A;

    MatrixUtil<long double>::QRDecomposition(init, Q, R);
    A = Q * R;
    ASSERT_EQ(MatrixUtil<long double>::compareEquals(A, init), true);

    MatrixUtil<long double>::HouseholderQR(init, Q, R);

    A = Q * R;
    ASSERT_EQ(MatrixUtil<long double>::compareEquals(A, init), true);
}
*/
