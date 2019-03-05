#include <iostream>
#include "SVDecomp.h"
#include "QRDecomp.h"
#include <omp.h>

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
        //Eigen::setNbThreads(8);
        //omp_set_num_threads(8);
        //cusolverDnHandle_t solver_handle;
        //cusolverDnCreate(&solver_handle);
        svdCuda(mR);
        /*
        auto begin_timer = std::chrono::high_resolution_clock::now();
        Eigen::BDCSVD<Eigen::MatrixXd> svdR(mR, Eigen::ComputeFullU | Eigen::ComputeFullV);
        auto end_timer = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed_time = end_timer - begin_timer;
        double time_spent = elapsed_time.count();
        std::cout << "Elapsed time time is:" << time_spent << std::endl;
        */
        /*
        std::cout << "Its singular values are:" << std::endl
             << svdR.singularValues() << std::endl;
        std::cout << "Its left singular vectors are the columns of the thin U matrix:" << std::endl
             << svdR.matrixU() << std::endl;
        std::cout << "Its right singular vectors are the columns of the thin V matrix:" << std::endl
             << svdR.matrixV() << std::endl;
        */
    }
}
