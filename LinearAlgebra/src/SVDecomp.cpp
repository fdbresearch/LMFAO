#include <iostream>
#include "SVDecomp.h"
#include "QRDecomp.h"

using namespace std;

namespace LMFAO::LinearAlgebra
{
    void SVDecomp::decompose() 
    {
        QRDecomposition *pQRDecomp = nullptr;
        switch (mDecompType)
        {
            case DecompType::NAIVE:
                pQRDecomp = new QRDecompositionNaive(mPath);
                break;
            case DecompType::SINGLE_THREAD:
                pQRDecomp = new QRDecompositionSingleThreaded(mPath);
                break;

            default:
                break;
        }
        pQRDecomp->decompose();
        pQRDecomp->getR(mR);
        delete pQRDecomp;

        Eigen::BDCSVD<Eigen::MatrixXd> svdR(mR, Eigen::ComputeFullU | Eigen::ComputeFullV);

        cout << "Its singular values are:" << endl
             << svdR.singularValues() << endl;
        cout << "Its left singular vectors are the columns of the thin U matrix:" << endl
             << svdR.matrixU() << endl;
        cout << "Its right singular vectors are the columns of the thin V matrix:" << endl
             << svdR.matrixV() << endl;
    }
}
