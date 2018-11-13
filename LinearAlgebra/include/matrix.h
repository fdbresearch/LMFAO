#ifndef MATRIX_H
#define MATRIX_H

#include <vector>
#include <valarray>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <assert.h>
#include <cmath>

#define sliceRowI slice(i * numCols(), numCols(), 1)
#define sliceRowJ slice(j * numCols(), numCols(), 1)
#define sliceColI slice(i, numRows(), numCols())
#define sliceColJ slice(j, numRows(), numCols())

using namespace std;

template <typename T>
class Matrix
{
  private:
    typedef std::valarray<T> valArr;
    valArr d_data;
    int d_numRows;
    int d_numCols;

  public:
/// Creators
    Matrix();
    ///Default constructor

    Matrix(int rows, int columns);
    /// Constructs an empty matrix with 'rows' rows and 'columns' columns.

    Matrix(const vector<vector<T> >& other);
    /// Constructs a matrix from a C++ vector of vectors.

    Matrix(const Matrix& other) = default;
    /// Copy constructs a matrix with the same number of rows and columns
    /// as the given one deep copies the entries.

    Matrix& operator=(const Matrix& other) = default;
    /// Construct the current object with the specified 'other' matrix.

/// Manipulators

    void rowScale(int i, const T& scalar);
    /// scale row i by T

    void colScale(int j, T scalar);
    /// scale column j by T

    void rowSwap(int i, int j);
    /// swap two rows of a matrix

    void colSwap(int i, int j);
    /// swap two columns of a matrix

    void addToRow(int i, const Matrix& other);
    /// add to a row of a matrix a given row in matrix form

    void addToCol(int j, const Matrix& other);
    /// add to the column of a matrix a given column in matrix form

    void normalize();
    /// Divide by the Frobenius Norm.

    void setSubmatrix(int upRow, int downRow, int leftCol, int rightCol, const Matrix& other);
    /// Set the submatrix given by the intersection of [upRow, downRow] rows
    /// and [leftCol, rightCol] columns to the specified matrix.

    void setColumn(int index, const Matrix& other);
    /// Set the column 'index' to equal 'other'.

    void setRow(int index, const Matrix& other);
    /// Set the row 'index' to equal other.

    void set(int row, int col, T value);
    /// Set the value at specified 'row' and specified 'col' to 'value'.

    Matrix transpose();
    /// Return the transpose of 'this' Matrix. If it's cached then just returns it.

    T frobeniusNorm();
    /// Return the entry square norm

    T trace();
    /// Returns the trace of a matrix

/// Accessors

    const void print(int indent) const;
    /// Print the matrix with the given amount of right indentation.

    const T get(int row, int col) const;
    /// Get the value at specified 'row' and specified 'col'.

    const int numRows() const;
    /// Get the number of rows.

    const int numCols() const;
    /// Get the number of columns.

    Matrix operator[](int index) const;
    /// Get the 'index'th row of the Matrix in a Matrix row form.

    Matrix operator()(int upRow, int downRow, int leftCol, int rightCol);
    /// Get the submatrix defined by rows [upRow, downRow] and columns [leftCol, rightCol].

    Matrix operator()(int index) const;
    /// Get the 'index'th column of the Matrix in a Matrix column form.

/// Free operators
    bool operator == (const Matrix& other);
    /// Returns if the two matrices are equal

    bool operator != (const Matrix& other);
    /// Returns if the two matrices are different

    Matrix& operator *= (const Matrix& rhs);
    /// Multiply 'this' by 'rhs' matrix.

    Matrix& operator *= (const T& rhs);
    /// Multiply 'this' by 'rhs' scalar.

    Matrix& operator += (const Matrix& rhs);
    /// Add to 'this' matrix the 'rhs' matrix

    Matrix& operator -= (const Matrix& rhs);
    /// Subtract from 'this' matrix the 'rhs' matrix

    Matrix& operator /= (const T& rhs);
    /// Divide 'this' matrix by the 'rhs' scalar

/// Friend Operators

    friend ostream& operator<<(ostream& out, Matrix& other)
    {
        other.print(10);
        return out;
    }
    /// Nicely print with indentation 10
};

/*******************************************************/
///                  Creators                         ///
/*******************************************************/

template <typename T>
Matrix<T>::Matrix()
: d_data()
{
    d_numRows = 0;
    d_numCols = 0;
}

template <typename T>
Matrix<T>::Matrix(int rows, int columns)
: d_data(rows*columns)
, d_numRows(rows)
, d_numCols(columns)
{
}

template <typename T>
Matrix<T>::Matrix(const vector<vector<T> >& other)
{
    d_numRows = other.size();
    if(other.size())
        d_numCols = other[0].size();
    int pos = 0;
    d_data.resize(numRows() * numCols());
    for (int i = 0; i < numRows(); ++i)
        for (int j = 0; j < numCols(); ++j)
            d_data[pos++] = other[i][j];
}

/*******************************************************/
///                  End of Creators                  ///
/*******************************************************/

/*******************************************************/
///                  Manipulators                     ///
/*******************************************************/

template <typename T>
void Matrix<T>::rowScale(int i, const T& scalar)
{
    d_data[sliceRowI] *= valArr(scalar, numCols());
}

template <typename T>
void Matrix<T>::colScale(int j, T scalar)
{
    d_data[sliceColJ] *= valArr(scalar, numRows());
}

template <typename T>
void Matrix<T>::rowSwap(int i, int j)
{
    valArr aux(d_data[sliceRowI]);
    d_data[sliceRowI] = valArr(d_data[sliceRowJ]);
    d_data[sliceRowJ] = aux;
}

template <typename T>
void Matrix<T>::colSwap(int i, int j)
{
    valArr aux(d_data[sliceColI]);
    d_data[sliceColI] = valArr(d_data[sliceColJ]);
    d_data[sliceColJ] = aux;
}

template <typename T>
void Matrix<T>::addToRow(int i, const Matrix& other)
{
    for (int col = 0; col < numCols(); ++col)
        set(i, col, get(i,col) + other.get(0, col));
}

template <typename T>
void Matrix<T>::addToCol(int j, const Matrix& other)
{
    for (int row = 0; row < numRows(); ++row)
        set(row, j, get(row, j) + other.get(row, 0));
}

template <typename T>
void Matrix<T>::normalize()
{
    T norm = frobeniusNorm();
    assert(norm > 0);
    d_data /= norm;
}

template <typename T>
void Matrix<T>::setSubmatrix(int upRow, int downRow, int leftCol, int rightCol, const Matrix<T>& other)
{
    assert(other.numRows() == downRow - upRow + 1 &&
           other.numCols() == rightCol - leftCol + 1);
    d_data[gslice( upRow * numCols() + leftCol,
                  {downRow - upRow + 1, rightCol - leftCol + 1},
                  {numCols(), 1}
                 )] = other.d_data;

}

template <typename T>
void Matrix<T>::setRow(int i, const Matrix<T>& other)
{
    assert(numCols() == other.numCols() && other.numRows() == 1);
    d_data[sliceRowI] = other.d_data;
}

template <typename T>
void Matrix<T>::setColumn(int j, const Matrix<T>& other)
{
    assert(numRows() == other.numRows() && other.numCols() == 1);
    d_data[sliceColJ] = other.d_data;
}

template <typename T>
void Matrix<T>::set(int row, int col, T value)
{
    d_data[row * numCols() + col] = value;
}

template <typename T>
Matrix<T> Matrix<T>::transpose()
{
    Matrix<T> transp(numCols(), numRows());
    for (int i = 0; i < numRows(); ++i)
        for (int j = 0; j < numCols(); ++j)
            transp.set(j, i, get(i, j));
    return transp;
}

template <typename T>
T Matrix<T>::frobeniusNorm()
{
    T sum = 0;
    for (int i = 0; i < numRows(); ++i)
        for (int j = 0; j < numCols(); ++j)
            sum += get(i, j) * get(i, j);
    return sqrt(sum);
}

template <typename T>
T Matrix<T>::trace()
{
    assert(numRows() == numCols() && "not square matrix!");
    T sum = 0;
    for (int i = 0; i < numRows(); ++i)
        sum += get(i, i);
    return sum;
}

/*******************************************************/
///                  End of Manipulators              ///
/*******************************************************/

/*******************************************************/
///                  Accessors                        ///
/*******************************************************/

template <typename T>
const void Matrix<T>::print(int indent) const
{
    if(numRows() == 0 || numCols() == 0) {
        cout << "empty\n";
        return;
    }
    for (int i = 0; i < numRows(); ++i) {
        for (int j = 0; j < numCols(); ++j) {
            T jt = get(i, j);
            stringstream s;
            s << setprecision(indent-4) << fixed;
            s << jt;

            assert(indent - s.str().size() >= 0);
            for (int i = 0; i < (int)(indent - s.str().size()); ++i)
                cout << " ";
            cout << s.str();
        }
        cout << "\n";
    }
}

template <typename T>
const T Matrix<T>::get(int row, int col) const
{
    return d_data[row * numCols() + col];
}

template <typename T>
const int Matrix<T>::numRows() const
{
    return d_numRows;
}

template <typename T>
const int Matrix<T>::numCols() const
{
    return d_numCols;
}

template <typename T>
Matrix<T> Matrix<T>::operator[](int i) const
{
    Matrix<T> result(1, numCols());
    result.d_data = d_data[sliceRowI];
    return result;
}

template <typename T>
Matrix<T> Matrix<T>::operator()(int upRow,
                                int downRow,
                                int leftCol,
                                int rightCol)
{
    Matrix<T> result(downRow - upRow + 1, rightCol - leftCol + 1);
    result.d_data = d_data[gslice(
                                    upRow * numCols() + leftCol,
                                    {downRow - upRow + 1, rightCol - leftCol + 1},
                                    {numCols(), 1}
                                 )];
    return result;
}

template <typename T>
Matrix<T> Matrix<T>::operator()(int j) const
{
    Matrix<T> result(numRows(), 1);
    result.d_data = d_data[sliceColJ];
    return result;
}

/*******************************************************/
///               End Of Accessors                    ///
/*******************************************************/

/*******************************************************/
///                Free Operators                     ///
/*******************************************************/

template <typename T>
bool Matrix<T>::operator == (const Matrix& other)
{/// this does not take into account floating point precision!
    if(numRows() != other.numRows() || numCols() != other.numCols())
        return false;
    ///----------------------------///
    return d_data == other.d_data; /// Compiler optimised
    ///----------------------------///
}

template <typename T>
bool Matrix<T>::operator != (const Matrix& other)
{
    return !(this == other);
}

template <typename T>
Matrix<T>& Matrix<T>::operator *= (const Matrix<T>& rhs) {

    if (numRows() == 1 && numCols() == 1) { /// Treat the case when lhs is scalar
        Matrix<T> rez(rhs);
        rez *= get(0, 0);
        (*this) = rez;
        return *this;
    }

    if (rhs.numRows() == 1 && rhs.numCols() == 1) { /// Treat the case when rhs is scalar
        Matrix<T> rez(*this);
        rez *= rhs.get(0, 0);
        (*this) = rez;
        return *this;
    }

    assert(numCols() == rhs.numRows() && "matrix multiplication impossible!");


    Matrix<T> rez(numRows(), rhs.numCols());

    ///-------------------------------------------------------------------------------///
    for (int i = 0; i < numRows(); ++i)                                               ///
        for (int j = 0; j < rhs.numCols(); ++j)                                       /// Compiler optimised
            rez.set(i, j, (    d_data[slice(i * numCols(), numCols(), 1)] *           ///
                           rhs.d_data[slice(j, rhs.numRows(), rhs.numCols())]).sum());///
    ///-------------------------------------------------------------------------------///


    (*this) = rez;
    return *this;
}

template <typename T>
Matrix<T>& Matrix<T>::operator *= (const T& rhs) {

    ///-----------///
    d_data *= rhs;/// Compiler optimised.
    ///-----------///

    return *this;
}

template <typename T>
Matrix<T>& Matrix<T>::operator += (const Matrix<T>& rhs) {
    assert(numRows() == rhs.numRows() &&
           numCols() == rhs.numCols() &&
           "matrix addition impossible!");

    ///------------------///
    d_data += rhs.d_data;/// Compiler optimised.
    ///------------------///

    return *this;
}

template <typename T>
Matrix<T>& Matrix<T>::operator -= (const Matrix<T>& rhs) {
    assert(numRows() == rhs.numRows() &&
           numCols() == rhs.numCols() &&
           "matrix addition impossible!");

    ///------------------///
    d_data -= rhs.d_data;/// Compiler optimised.
    ///------------------///

    return *this;
}

template <typename T>
Matrix<T>& Matrix<T>::operator /= (const T& rhs){

    ///-----------///
    d_data /= rhs;/// Compiler optimised.
    ///-----------///

    return *this;
}

/*******************************************************/
///              End of Free Operators                ///
/*******************************************************/

/*******************************************************/
///                 Free Functions                    ///
/*******************************************************/

template <typename T>
Matrix<T> operator*(const Matrix<T>& lhs, const Matrix<T>& rhs) {
    Matrix<T> aux(lhs);
    aux *= rhs;
    return aux;
}

template <typename T>
Matrix<T> operator*(const Matrix<T>& lhs, const T& rhs) {
    Matrix<T> aux(lhs);
    aux *= rhs;
    return aux;
}

template <typename T>
Matrix<T> operator+(const Matrix<T>& lhs, const Matrix<T>& rhs) {
    Matrix<T> aux(lhs);
    aux += rhs;
    return aux;
}

template <typename T>
Matrix<T> operator-(const Matrix<T>& lhs, const Matrix<T>& rhs) {
    Matrix<T> aux(lhs);
    aux -= rhs;
    return aux;
}

template <typename T>
Matrix<T> operator/(const Matrix<T>& lhs, const T& rhs) {
    Matrix<T> aux(lhs);
    aux /= rhs;
    return aux;
}

/*******************************************************/
///              End of Free Functions                ///
/*******************************************************/

#endif /// MATRIX_H
