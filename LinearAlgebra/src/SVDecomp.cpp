#include <iostream>
#include "SVDecomp.h"
#include "QRDecomp.h"

namespace LMFAO::LinearAlgebra
{
    void SVDecomp::decompose()
    {
        QRDecomposition *pQRDecomp = nullptr;
        switch (mDecompType)
        {
            case DecompType::NAIVE:
                if (nullptr == mpmapMatAgg)
                {
                    pQRDecomp = new QRDecompositionNaive(mPath);
                }
                else
                {
                    pQRDecomp = new QRDecompositionNaive(*mpmapMatAgg, mNumFeatsExp,
                                                         mNumFeats, mNumFeatsCont,
                                                         mvIsCat);
                }

            break;
            case DecompType::SINGLE_THREAD:
                if (nullptr == mpmapMatAgg)
                {
                    pQRDecomp = new QRDecompositionSingleThreaded(mPath);
                }
                else
                {
                    pQRDecomp = new QRDecompositionSingleThreaded(*mpmapMatAgg, mNumFeatsExp,
                                                                  mNumFeats, mNumFeatsCont,
                                                                  mvIsCat);
                }
                break;
            case DecompType::MULTI_THREAD:
                if (nullptr == mpmapMatAgg)
                {
                    pQRDecomp = new QRDecompositionMultiThreaded(mPath);
                }
                else
                {
                    pQRDecomp = new QRDecompositionMultiThreaded(*mpmapMatAgg, mNumFeatsExp,
                                                                  mNumFeats, mNumFeatsCont,
                                                                  mvIsCat);
                }
            default:
                break;
        }
        pQRDecomp->decompose();
        pQRDecomp->getR(mR);
        delete pQRDecomp;

        Eigen::BDCSVD<Eigen::MatrixXd> svdR(mR, Eigen::ComputeFullU | Eigen::ComputeFullV);

        std::cout << "Its singular values are:" << std::endl
             << svdR.singularValues() << std::endl;
        std::cout << "Its left singular vectors are the columns of the thin U matrix:" << std::endl
             << svdR.matrixU() << std::endl;
        std::cout << "Its right singular vectors are the columns of the thin V matrix:" << std::endl
             << svdR.matrixV() << std::endl;
    }
}
