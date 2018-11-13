#include <iostream>
#include <fstream>

#include <matrix.h>
#include <matrixutil.h>
#include <clusteringutility.h>
#include <distributions.h>
#include <polynomial.h>

#include <time.h>
#include <random>
#include <algorithm>

using namespace std;

void unitTest1(int verbose = 0)
{
    Matrix<long double> init({
                              { 1,2,2},
                              {-1,1,2},
                              {-1,0,1},
                              { 1,1,2}
                            });
    Matrix<long double> Q, R, A;
    MatrixUtil<long double>::QRDecomposition(init, Q, R);
    if (verbose) {
        cout << Q << endl;
        cout << R << endl;
    }
    A = Q * R;
    if (verbose) {
        cout << MatrixUtil<long double>::compareEquals(A, init);
        cout << endl << "------------------------------------" << endl;
    }
    Matrix<long double> B({
                            {1,2, 1},
                            {0,1, 3},
                            {1,2, 4}
                          });
    Matrix<long double> BI;
    MatrixUtil<long double>::GaussJordan(B, BI);
    Matrix<long double> BB = B * BI;
    if (verbose) {
        cout << BB << endl << B << endl << BI;
        cout << "------------------------------" << endl;
    }
    /**
        x = 1 => y = 4
        x = 2 => y = 8
        x = 3 => y = 15 - trebuia 12
        x = 4 => y = 11 - trebuia 16
        // m*x + 1*n = y
           2.8*x + 2.5 = f(x)


        | 1*x + 1*0 = 4
        | 2*x + 1*0 = 8
        | 3*x + 1*0 = 15
        | 4*x + 1*0 = 11

    */
    Matrix<long double> M({
                           {1, 1},
                           {2, 1},
                           {3, 1},
                           {4, 1},
                           {5.28, 1},
                           {6,79, 1},
                          });
    Matrix<long double> b({
                          {4},
                          {8},
                          {15},
                          {11},
                          {13.47},
                          {16.35},
                          });
    Matrix<long double> x(2,1);
    MatrixUtil<long double>::LeastSquaresQR(M, b, x);

    if (verbose) {
        cout << x;
    }
}

void unitTest2(int verbose = 0)
{
    /// ax^2 + bx + c*1 = y
    Matrix<long double> M({
                           { 1, 1, 1},
                           { 4, 2, 1},
                           { 9, 3, 1},
                           {16, 4, 1},
                         });
    Matrix<long double> b({
                          {2},
                          {1},
                          {2},
                          {2}
                          });
    Matrix<long double> x(3,1);
    MatrixUtil<long double>::LeastSquaresQR(M, b, x);

    if (verbose) {
        cout << x;
    }
}

void unitTest3(int verbose = 0)
{
    /// w0*1 + w1x1 + w2x2 = y
    /// linear fit in 3D

    Matrix<long double> M({
                           { 5,  7, 1},
                           { 3, 12, 1},
                           { 4,  5, 1},
                         });
    vector<vector<long double> > vb = {
                                        {14},
                                        {19},
                                        {10},
                                      };
    Matrix<long double> b(vb);
    Matrix<long double> x(3,1);
    MatrixUtil<long double>::LeastSquaresQR(M, b, x);

    if (verbose) {
        cout << x;
    }
}

void unitTest4(int verbose = 0)
{
    /// w0*1 + w1*x1 + w2*x2 + w3*x1^2 + w4*x2^2 + w5*x1*x2 = y
    Matrix<long double> M({///1  x   y  x^2   y^2   x*y
                            { 1, 5,  7, 5*5,  7* 7, 5* 7},
                            { 1, 3, 12, 3*3, 12*12, 3*12},
                            { 1, 4,  8, 4*4,  8* 8, 4* 8 + 1},
                            { 1, 4,  8, 4*4,  8* 8+ 1, 4* 8},
                            { 1, 4,  8, 4*4+ 1,  8* 8, 4* 8},
                            { 1, 4,  8+ 1, 4*4,  8* 8, 4* 8},
                            { 1, 4+ 1,  8, 4*4,  8* 8, 4* 8},
                            { 1, 4,  5+2, 4*4,  5* 5 - 1, 4* 5},
                            { 1, 3, 17, 3*3, 17*17, 3*17},
                            { 1, 2, 25, 2*2, 25*25, 2*25},
                         });
    vector<vector<long double> > vb = {
                                        {14},
                                        {19},
                                        {12},
                                        {11},
                                        {20},
                                        {26},
                                        {17},
                                        {13},
                                        {22},
                                        {27},
                                      };
    Matrix<long double> b(vb);
    Matrix<long double> x(6,1);
    MatrixUtil<long double>::LeastSquaresQR(M, b, x);
    Matrix<long double> y(6,1);
    MatrixUtil<long double>::LeastSquaresQR(M, b, y, true);
    if (verbose) {
        cout << x << endl;
        cout << y;
        cout << MatrixUtil<long double>::compareEquals(x, y, 1e-10);
    }
}

void unitTest5(int verbose = 0)
{
    Matrix<long double> init({
                              { 1,2,2},
                              {-1,1,2},
                              {-1,0,1},
                              { 1,1,2}
                            });
    Matrix<long double> Q, Qb, R, Rb, A;

    MatrixUtil<long double>::QRDecomposition(init, Q, R);
    if (verbose) {
        cout << Q << endl;
        cout << R << endl;
    }
    A = Q * R;
    assert(MatrixUtil<long double>::compareEquals(A, init));

    MatrixUtil<long double>::HouseholderQR(init, Q, R);

    A = Q * R;
    assert(MatrixUtil<long double>::compareEquals(A, init));

}

void unitTest6(int verbose)
{
    Matrix<long double> M({
                                {1,1,1,1},
                                {1,1,1,1},
                                {1,1,1,1},
                                {1,1,1,1},
                         });

    Matrix<long double> N({
                             {3,3},
                             {3,3},
                         });
    M.setSubmatrix(1, 2, 1, 2, N);
    if(verbose) {
        cout << M;
    }
}

void clusteringTest()
{
    srand(time(0));
    Matrix<long double> X( {
                                {-3.82, 3.24} ,
                                {-4.24, 3.9} ,
                                {-3.36, 4.16} ,
                                {-3.5, 3.54} ,
                                {-2.62, 3.5} ,
                                {-2.76, 4.26} ,
                                {-3.02, 3.14} ,
                                {2.44, 5.16} ,
                                {2.22, 6.08} ,
                                {2.44, 5.74} ,
                                {2.04, 5.5} ,
                                {1.62, 5.5} ,
                                {1.7, 5.98} ,
                                {1.76, 5.04} ,
                                {2.16, 4.82} ,
                                {3.06, 5.16} ,
                                {4.5, 1.5} ,
                                {4.18, 2.22} ,
                                {4.1, 1.84} ,
                                {5, 2} ,
                                {4.78, 2.24} ,
                                {4.44, 1.9} ,
                                {4.78, 1.7} ,
                                {5.1, 1.68} ,
                                {5, 1} ,
                                {4.54, 1.2} ,
                                {-1.88, -1.74} ,
                                {-2.24, -1.5} ,
                                {-2.64, -1.84} ,
                                {-2.4, -2.1} ,
                                {-1.94, -2.26} ,
                                {-1.46, -2.06} ,
                                {-1.4, -1.3} ,
                                {-1.82, -1.12}
                            });

    vector<int> indices;
    Matrix<long double> centroids;

    vector<long double> lossW;
    ClusteringUtility<long double>::KMeansKSelector(X, lossW);
    for (auto it : lossW)
        cout << it << " ";
    cout << "\n\n\n";

    cout << ClusteringUtility<long double>::KMeans(X, 4, indices, centroids, false) << "\n";
    cout << centroids;
    for (int i = 0; i < (int)indices.size(); ++i)
        cout << indices[i] << " ";
    cout << endl;
}

void measureError()
{
    Matrix<long double> A, Q, R, QL, RL, MatlabR, MatlabRL;
    MatrixUtil<long double>::Reader(A, string("A.in"));
    MatrixUtil<long double>::Reader(MatlabR, string("R.in"));
    MatrixUtil<long double>::Reader(MatlabRL, string("RL.in"));
    ///A.print(20);

    MatrixUtil<long double>::QRDecomposition(A, Q, R);
    assert(MatrixUtil<long double>::compareEquals(A, Q*R, 1e-8));

    MatrixUtil<long double>::QRLosslessDecomposition(A, QL, RL);
    assert(MatrixUtil<long double>::compareEquals(A, QL*RL, 1e-16));


    ofstream fcout("A.out");


    fcout << setprecision(20) << fixed;

    for (int i = 0; i < 80; ++i)
        fcout << R.get(i, i)<<  endl;/// << MatlabR.get(i, i) << endl << endl;

    fcout << "\n-------------\n";

    for (int i = 0; i < 80; ++i)
        fcout << RL.get(i, i) << endl;/// << MatlabRL.get(i, i) << endl << endl;
    fcout << "\n";

    /// Inverse testing.
    Matrix<long double> RI;
    MatrixUtil<long double>::GaussJordan(R, RI);
    RI *= R * R;
    assert(MatrixUtil<long double>::compareEquals(RI, R));
}



void measurePrecisionATAI()
{
    Matrix<long double> A, b, Q, R, QL, RL, ATA, ATAI, xmatlab;

    MatrixUtil<long double>::Reader(A, string("precisionA.in"));
    MatrixUtil<long double>::Reader(b, string("precisionB.in"));

    ATA = A.transpose() * A;
    MatrixUtil<long double>::GaussJordan(ATA, ATAI);
    ///MatrixUtil<long double>::Reader(ATAI, string("precisionATAI.in"));

    Matrix<long double> x = ATAI * (A.transpose() * b);
    cout << setprecision(30) << fixed;
    cout << "My      answer: " << x.get(14, 0) << endl;
    MatrixUtil<long double>::Reader(xmatlab, string("precisionTest1.in"));
    cout << "Matlab  answer: " << xmatlab.get(14,0) << endl;
    cout << "Correct answer: 1" << endl;
}

void measurePrecisionGS()
{
    Matrix<long double> A, b, Q, R, RI, xmatlab;

    MatrixUtil<long double>::Reader(A, string("precisionA.in"));
    MatrixUtil<long double>::Reader(b, string("precisionB.in"));

    MatrixUtil<long double>::QRDecomposition(A, Q, R);
    MatrixUtil<long double>::GaussJordan(R, RI);

    Matrix<long double> x = RI * Q.transpose() * b;
    MatrixUtil<long double>::Reader(xmatlab, string("precisionTest2.in"));

    cout << setprecision(30) << fixed;
    cout << "My      answer: " << x.get(14, 0) << endl;
    cout << "Matlab  answer: " << xmatlab.get(14,0) << endl;
    cout << "Correct answer: 1" << endl;

}

void measurePrecisionMGS()
{
    Matrix<long double> A, b, Q, R, RI, xmatlab;

    MatrixUtil<long double>::Reader(A, string("precisionA.in"));
    MatrixUtil<long double>::Reader(b, string("precisionB.in"));

    MatrixUtil<long double>::QRLosslessDecomposition(A, Q, R);
    MatrixUtil<long double>::GaussJordan(R, RI);

    Matrix<long double> x = RI * Q.transpose() * b;
    MatrixUtil<long double>::Reader(xmatlab, string("precisionTest2.in"));

    cout << setprecision(30) << fixed;
    cout << "My      answer: " << x.get(14, 0) << endl;
    cout << "Matlab  answer: " << xmatlab.get(14,0) << endl;
    cout << "Correct answer: 1" << endl;

}

void measurePrecisionInverse()
{
    Matrix<long double> A, b, AI, xmatlab;

    MatrixUtil<long double>::Reader(A, string("precisionA.in"));
    MatrixUtil<long double>::Reader(b, string("precisionB.in"));


    MatrixUtil<long double>::Reader(xmatlab, string("precisionTest3.in"));

    cout << setprecision(30) << fixed;
    cout << "My      answer: Impossible to invert - not square"<< endl;
    cout << "Matlab  answer: " << xmatlab.get(14,0) << endl;
    cout << "Correct answer: 1" << endl;
}

void measurePrecisionMGSE()
{
    Matrix<long double> A, b, QE, REI, RE, xmatlab;

    MatrixUtil<long double>::Reader(A, string("precisionA.in"));
    MatrixUtil<long double>::Reader(b, string("precisionB.in"));

    Matrix<long double> AE(A.numRows(), A.numCols() + 1);

    for (int i = 0; i < A.numCols(); ++i)
        AE.setColumn(i, A(i));
    AE.setColumn(A.numCols(), b);

    MatrixUtil<long double>::QRLosslessDecomposition(AE, QE, RE);
    QE = RE(0, A.numCols() - 1, A.numCols(), A.numCols());
    RE = RE(0, A.numCols() - 1, 0, A.numCols() - 1);

    MatrixUtil<long double>::GaussJordan(RE, REI);

    Matrix<long double> x = REI * QE;

    MatrixUtil<long double>::Reader(xmatlab, string("precisionTest4.in"));

    cout << setprecision(30) << fixed;
    cout << "My      answer: " << x.get(14, 0) << endl;
    cout << "Matlab  answer: " << xmatlab.get(14,0) << endl;
    cout << "Correct answer: 1" << endl;
}

void measurePrecisionHouseholderInverse()
{
    Matrix<long double> A, b, Q, R, RI;

    MatrixUtil<long double>::Reader(A, string("precisionA.in"));
    MatrixUtil<long double>::Reader(b, string("precisionB.in"));

    MatrixUtil<long double>::HouseholderQR(A, Q, R);
    MatrixUtil<long double>::GaussJordan(R, RI);

    Matrix<long double> x = RI * Q.transpose() * b;


    cout << setprecision(30) << fixed;
    cout << "My      answer: " << x.get(14, 0) << endl;
    cout << "Correct answer: 1" << endl;
}

void measurePrecisionHouseholderExtended()
{
    Matrix<long double> A, b, Qb, R, RI, Ab;

    MatrixUtil<long double>::Reader(A, string("precisionA.in"));
    MatrixUtil<long double>::Reader(b, string("precisionB.in"));

    Matrix<long double> AE(A.numRows(), A.numCols() + 1);

    for (int i = 0; i < A.numCols(); ++i)
        AE.setColumn(i, A(i));
    AE.setColumn(A.numCols(), b);

    MatrixUtil<long double>::HouseholderQbR(AE, Qb, R);

    MatrixUtil<long double>::GaussJordan(R, RI);

    Matrix<long double> x = RI * Qb;

    cout << setprecision(30) << fixed;
    cout << "My      answer: " << x.get(14, 0) << endl;
    cout << "Correct answer: 1" << endl;
}

void testGaussian()
{
    long double mean = 0, standardDeviation = 1;

    Gaussian<long double> gaussian(mean, standardDeviation);
    for (int i = 1; i <= 100; ++i)
        cout << gaussian.sample() << endl;
}

void testBasisExpansion()
{
    Matrix<long double> row(BasisExpansion<long double>::expand(5.0L, 4));
    cout << row;
}

void testPolynomial()
{
    Gaussian<long double> gauss(0, 100);
    vector<long double> coeff;
    coeff.push_back(-2);
    coeff.push_back(3);
    coeff.push_back(-5);
    coeff.push_back(7);
    /// 2 + 3x + 5x^2 + 7x^3

    Polynomial<long double> p1(coeff), p2(coeff, &gauss);
    for (int i = 0; i <= 20; i += 4)
        cerr << "(" << i << ", " << p1.evaluate(i) << ") ";
    cerr << endl;

    for (int i = 0; i <= 20; i += 4)
        cerr << "(" << i << ", " << p2.evaluate(i) << ") ";
    cerr << endl;
}

void testFitExactPolynomial()
{
    vector<long double> coeff;
    coeff.push_back(-2);
    coeff.push_back(3);
    coeff.push_back(-5);
    coeff.push_back(7);
    coeff.push_back(-11);

    /// -2 + 3x - 5x^2 + 7x^3 - 11x^4

    vector<pair<long double, long double> > points;
    Polynomial<long double> p1(coeff);
    for (int i = -20; i <= 20; i += 3) {
        points.push_back({i, p1.evaluate(i)});
    }

    for (int degree = 1; degree <= 6; ++degree) {
        /// we choose the degree of line we try to fit
        /// for degree we get degree + 1 features
        Matrix<long double> A(points.size(), degree + 1), x(degree + 1, 1), b(points.size(), 1);
        for (int i = 0; i < points.size(); ++i) {
            A.setRow(i, Matrix<long double>(BasisExpansion<long double>::expand(points[i].first, degree)));
            b.set(i, 0, points[i].second);
        }
        ///A.print(15);
        ///cerr << "\n---------------------------\n";
        MatrixUtil<long double>::LeastSquaresQR(A, b, x, true);
        Matrix<long double> y = A*x - b;
        cout << x << "\n Error: ";
        cout << setprecision(10) << fixed;
        cout << (y.transpose() * y).get(0, 0) << endl << endl;
    }
}

void testFitNoisyPolynomial()
{
    vector<long double> coeff;
    Gaussian<long double> gauss(0, 30);
    coeff.push_back(-2);
    coeff.push_back(3);
    coeff.push_back(-5);
    coeff.push_back(7);
    coeff.push_back(-11);

    /// -2 + 3x - 5x^2 + 7x^3 - 11x^4

    vector<pair<long double, long double> > points;
    Polynomial<long double> p1(coeff, &gauss);
    for (int i = -500000; i <= 500000; ++i) {
        points.push_back({i, p1.evaluate(i)});
    }

    for (int degree = 1; degree <= 6; ++degree) {
        /// we choose the degree of line we try to fit
        /// for degree we get degree + 1 features
        Matrix<long double> A(points.size(), degree + 1), x(degree + 1, 0), b(points.size(), 1);
        for (int i = 0; i < points.size(); ++i) {
            A.setRow(i, Matrix<long double>(BasisExpansion<long double>::expand(points[i].first, degree)));
            b.set(i, 0, points[i].second);
        }
        ///A.print(15);
        ///cerr << "\n---------------------------\n";
        MatrixUtil<long double>::LeastSquaresQR(A, b, x, true);
        Matrix<long double> y = A*x - b;
        cout << x << "\n Error: ";
        cout << setprecision(10) << fixed;
        cout << (y.transpose() * y).get(0, 0) << endl << endl;
    }
}

void testFitNoisyPolynomial2()
{
    vector<long double> coeff;
    Gaussian<long double> gauss(0, 1);
    coeff.push_back(-2);
    coeff.push_back(2);
    coeff.push_back(-2);
    coeff.push_back(2);
    coeff.push_back(-2);

    /// -2 + 2x - 2x^2 + 2x^3 - 2x^4

    vector<pair<long double, long double> > points;
    Polynomial<long double> p1(coeff, &gauss);
    for (int i = -10000; i <= 10000; i += 1) {
        points.push_back({i, p1.evaluate(i)});
    }

    for (int degree = 1; degree <= 10; ++degree) {
        /// we choose the degree of line we try to fit
        /// for degree we get degree + 1 features
        Matrix<long double> A(points.size(), degree + 1), x(degree + 1, 0), b(points.size(), 1);
        for (int i = 0; i < points.size(); ++i) {
            A.setRow(i, Matrix<long double>(BasisExpansion<long double>::expand(points[i].first, degree)));
            b.set(i, 0, points[i].second);
        }
        ///A.print(15);
        ///cerr << "\n---------------------------\n";
        MatrixUtil<long double>::LeastSquaresQR(A, b, x, true);
        Matrix<long double> y = A*x - b;
        cout << x << "\n Error: ";
        cout << setprecision(10) << fixed;
        cout << (y.transpose() * y).get(0, 0) << endl << endl;
    }
}

int main()
{
    unitTest1(true);
    ///unitTest2(true);
    ///unitTest3(true);
    ///unitTest4(true);
    ///unitTest5(true);
    ///unitTest6(true);


    ///clusteringTest();
    ///measureError();
    ///measurePrecisionATAI();
    ///measurePrecisionGS();
    ///measurePrecisionMGS();
    ///measurePrecisionMGSE();
    ///measurePrecisionInverse(); /// matlab turns to best fit as inverse doesn't exist.
    ///measurePrecisionHouseholderInverse();
    ///measurePrecisionHouseholderExtended();

    ///testGaussian();
    ///testBasisExpansion();
    ///testPolynomial();
    ///testFitExactPolynomial();
    ///testFitNoisyPolynomial();
    ///testFitNoisyPolynomial2();

    return 0;
}
