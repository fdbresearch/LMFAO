#ifndef MATRIXUTIL_H
#define MATRIXUTIL_H

#include <matrix.h>
#include <fstream>

template <typename T>
class MatrixUtil
{
  private:
    static constexpr long double EPS = 1e-16;
  public:
    static bool compareEquals(Matrix<T> A, Matrix<T> B, T epsilon = EPS);
    /// Compare equality between two matrices. Returns true if the component
    /// wise difference never exceeds EPS in absolute value.

    static void QRDecomposition(Matrix<T> A, Matrix<T> &Q, Matrix<T> &R);
    /// Decomposes the matrix A into a orthonormal matrix Q, whose columns
    /// are orthonormal vectors spanning R^(Q.numColumns). R = Q^T * A.

    static void QRLosslessDecomposition(Matrix<T> A, Matrix<T> &Q, Matrix<T> &R);
    /// Decomposes the matrix A into a orthonormal matrix Q, whose columns
    /// are orthonormal vectors spanning R^(Q.numColumns). R = Q^T * A.

    static void GaussJordan(Matrix<T> A, Matrix<T> &AI);
    /// Perform the Gauss Jordan elimination on 'A' which produces the inverse
    /// of 'A' in 'AI'. The algorithm produces the identity matrix from A and
    /// equivalently apply the same operations to AI which initially is the
    /// identity matrix.

    static void LeastSquaresQR(Matrix<T> A, Matrix<T> b, Matrix<T> &x, bool lossless = false);
    /// Compute the least squares solution x to the system A*x = b by using
    /// QR decomposition. (A^T*A)x = A^T * b <=> (QR)^T(QR) * x = (QR)^T * b <=>
    /// R^T (Q^TQ) R * x = R^T * Q^Tb <=> R * x = Q^T * b <=> x = R^-1 Q^T * b
    /// the boolean lossless decides whether to compute the solution x to the
    /// system A*x = b using a stable QR decomposition.

    static void HouseholderQR(Matrix<T> A, Matrix<T> &Q, Matrix<T> &R);

    static void HouseholderQbR(Matrix<T> Ab, Matrix<T> &Qb, Matrix<T> &R);

    static void Reader(Matrix<T> &M, string fileName);
    /// Read a matrix 'M' given a 'fileName' the format of the file should be:
    /// on the first line M.numRows() M.numCols() separated by a space and
    /// on each of the next M.numRows() lines, M.numCols() type T values.
};

template <typename T>
bool MatrixUtil<T>::compareEquals(Matrix<T> A, Matrix<T> B, T eps)
{
    if (A.numCols() != B.numCols() || A.numRows() != B.numRows())
        return false;
    for (int i = 0; i < A.numRows(); ++i)
        for (int j = 0; j < A.numCols(); ++j)
            if (abs(A.get(i, j) - B.get(i, j)) > eps) /// major difference spotted!
            {
                cout << setprecision(16) << fixed;
                cout << A.get(i, j) << " vs \n" << B.get(i, j) << "\n";
                return false;
            }
    return true;
}

template <typename T>
void MatrixUtil<T>::QRDecomposition(Matrix<T> A, Matrix<T> &Q, Matrix<T> &R)
{   /// Decompose matrix A in the output parameters Q and R such that A = QR.
    int m = A.numRows(), n = A.numCols();

    Q = Matrix<T>(m, n);
    R = Matrix<T>(n, n);
    Matrix<T> V = A;

    for (int j = 0; j < n; ++j) {
        for (int i = 0; i < j; ++i) {
            R.set(i, j, (Q(i).transpose() * A(j)).get(0, 0));
            V.setColumn(j, V(j) - Q(i) * R.get(i, j));
        }
        R.set(j, j, V(j).frobeniusNorm());
        Q.setColumn(j, V(j) / R.get(j, j));
    }
}

template <typename T>
void MatrixUtil<T>::QRLosslessDecomposition(Matrix<T> A, Matrix<T> &Q, Matrix<T> &R)
{
    int m = A.numRows(), n = A.numCols();

    Q = Matrix<T>(m, n);
    R = Matrix<T>(n, n);
    Matrix<T> V = A;

    for (int j = 0; j < n; ++j) {
        for (int i = 0; i < j; ++i) {
            R.set(i, j, (Q(i).transpose() * V(j)).get(0, 0));
            V.setColumn(j, V(j) - Q(i) * R.get(i, j));
        }
        R.set(j, j, V(j).frobeniusNorm());
        Q.setColumn(j, V(j) / R.get(j, j));
    }
}

template <typename T>
void MatrixUtil<T>::GaussJordan(Matrix<T> A, Matrix<T> &AI)
{   /// Invert a square matrix A into the output parameter AI
    assert(A.numRows() == A.numCols() && "inverse does not exist");
    AI = Matrix<T>(A.numRows(), A.numCols());
    for (int i = 0; i < AI.numRows(); ++i)
        AI.set(i, i, 1);
    for (int i = 0; i < A.numRows(); ++i) {
        if (A.get(i, i) == 0) {
            /// we need a new row here
            int j = i;
            while (++j < A.numRows())
                if(A.get(j, i) != 0)
                    break;
            assert( j != A.numRows() && "inverse does not exist" );
            A.rowSwap(i, j);
            AI.rowSwap(i, j);
        }
        AI.rowScale(i, 1.0/A.get(i, i));
        A.rowScale(i, 1.0/A.get(i, i));
        for (int j = i + 1; j < A.numRows(); ++j) {
            AI.addToRow(j, AI[i] * (-A.get(j, i)));
            A.addToRow(j, A[i] * (-A.get(j, i)) );
        }
    }
    /// We are now in an upper unitriangular shape
    for (int i = (int)A.numRows() - 1; i >= 0; --i)
        for (int j = i - 1; j >= 0; --j) {
            AI.addToRow(j, AI[i] * (-A.get(j, i)));
            A.addToRow(j, A[i] * (-A.get(j,i)));
        }
    /// We now have A = Identity matrix, AI = inverse matrix
}

template <typename T>
void MatrixUtil<T>::LeastSquaresQR(Matrix<T> A, Matrix<T> b, Matrix<T> &x, bool lossless)
{   /// A * x = b <=> Q*R * x = b <=> R * x = Q^T*b <=> x = R^-1 * Q^T *b
    Matrix<T> Q, R, RI;
    if (lossless == false)
        QRDecomposition(A, Q, R);
    else
        QRLosslessDecomposition(A, Q, R);
    GaussJordan(R, RI);
    x = RI * Q.transpose() * b;
}

template <typename T>
void MatrixUtil<T>::HouseholderQR(Matrix<T> A, Matrix<T> &Q, Matrix<T> &R)
{
    int n = A.numCols() - 1;
    int m = A.numRows() - 1;

    Matrix<T> savedA(A);

    auto sign = [](T x) -> T{
        if (x >= 0) return 1;
        return -1;
    };


    for (int k = 0; k <= n; ++k) {
        Matrix<T> x = A(k, m, k, k);

        Matrix<T> e1(m - k + 1, 1);
        e1.set(0, 0, 1);

        Matrix<T> vk = e1 * sign(x.get(0, 0))*x.frobeniusNorm() + x;
        vk.normalize();
        A.setSubmatrix(k, m, k, n, A(k, m, k, n) - vk*(vk.transpose()*A(k, m, k, n)) * 2.0L);
    }

    R = A(0, n, 0, n);
    Matrix<T> RI;
    GaussJordan(R, RI);
    Q = savedA * RI;
}

template <typename T>
void MatrixUtil<T>::HouseholderQbR(Matrix<T> Ab, Matrix<T> &Qb, Matrix<T> &R)
{
    int n = Ab.numCols() - 1;
    int m = Ab.numRows() - 1;


    auto sign = [](T x) -> T{
        if (x >= 0) return 1;
        return -1;
    };

    for (int k = 0; k <= n; ++k) {
        Matrix<T> x = Ab(k, m, k, k);

        Matrix<T> e1(m - k + 1, 1);
        e1.set(0, 0, 1);

        Matrix<T> vk = e1 * sign(x.get(0, 0))*x.frobeniusNorm() + x;
        vk.normalize();
        Ab.setSubmatrix(k, m, k, n, Ab(k, m, k, n) - vk*(vk.transpose()*Ab(k, m, k, n)) * 2.0L);
    }
    R = Ab(0, n - 1, 0, n - 1);
    Qb = Ab(0, n - 1, n, n);
}

template <typename T>
void MatrixUtil<T>::Reader(Matrix<T> &M, string fileName)
{
    ifstream inputStream(fileName.c_str());
    int R, C;
    inputStream >> R >> C;
    M = Matrix<T>(R, C);
    for (int i = 0; i < R; ++i)
        for (int j = 0; j < C; ++j) {
            T scalar;
            inputStream >> scalar;
            M.set(i, j, scalar);
        }
}

#endif // MATRIXUTIL_H
