#include <iostream>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <limits>
#include <omp.h>

#include "QRDecomp.h"
#include "SVDecompQR.h"
//#include "RedSVD.h"

namespace LMFAO::LinearAlgebra
{
    void SVDecompQR::decompose()
    {
        auto begin_t_copying = std::chrono::high_resolution_clock::now();
        QRDecompBase *pQRDecomp = nullptr;
        Eigen::MatrixXd mSigma;

        Eigen::setNbThreads(8);
        omp_set_num_threads(8);

        switch (mDecompType)
        {
            case DecompType::NAIVE:
                if (nullptr == mpmapMatAgg)
                {
                    pQRDecomp = new QRDecompNaive(mPath);
                }
                else
                {
                    pQRDecomp = new QRDecompNaive(*mpmapMatAgg, mNumFeatsExp,
                                                  mNumFeats, mNumFeatsCont,
                                                  mvIsCat);
                }

            break;
            case DecompType::SINGLE_THREAD:
                if (nullptr == mpmapMatAgg)
                {
                    pQRDecomp = new QRDecompSingleThread(mPath);
                }
                else
                {
                    pQRDecomp = new QRDecompSingleThread(*mpmapMatAgg, mNumFeatsExp,
                                                          mNumFeats, mNumFeatsCont,
                                                          mvIsCat);
                }
                break;
            case DecompType::MULTI_THREAD:
                if (nullptr == mpmapMatAgg)
                {
                    pQRDecomp = new QRDecompMultiThread(mPath);
                }
                else
                {
                    pQRDecomp = new QRDecompMultiThread(*mpmapMatAgg, mNumFeatsExp,
                                                         mNumFeats, mNumFeatsCont,
                                                         mvIsCat);
                }
            case DecompType::CHOLESKY:
                if (nullptr == mpmapMatAgg)
                {
                    pQRDecomp = new QRDecompCholesky(mPath);
                }
                else
                {
                    pQRDecomp = new QRDecompCholesky(*mpmapMatAgg, mNumFeatsExp,
                                                     mNumFeats, mNumFeatsCont,
                                                     mvIsCat);
                }
            default:
                break;
        }
        if (pQRDecomp){
            //auto finish_t_copying = std::chrono::high_resolution_clock::now();
            //std::chrono::duration<double> elapsed_copying = finish_t_copying - begin_t_copying;
            //double time_spent = elapsed_copying.count();
            //std::cout << "Time copying is: " << time_spent << std::endl;

            //auto begin_t_qr = std::chrono::high_resolution_clock::now();

            pQRDecomp->decompose();
            //auto finish_t_qr = std::chrono::high_resolution_clock::now();
            //std::chrono::duration<double> elapsed_qr = finish_t_qr - begin_t_qr;
            //time_spent = elapsed_qr.count();
            //std::cout << "Time qr is: " << time_spent << std::endl;

            //auto begin_t_copying_back = std::chrono::high_resolution_clock::now();
            pQRDecomp->getR(mR);
            delete pQRDecomp;
            //auto finish_t_copying_back = std::chrono::high_resolution_clock::now();
            //std::chrono::duration<double> elapsed_copying_b = finish_t_copying_back - begin_t_copying_back;
            //time_spent = elapsed_copying_b.count();
            //std::cout << "Time copying back is: " << time_spent << std::endl;
        }

        //cusolverDnHandle_t solver_handle;
        //cusolverDnCreate(&solver_handle);
        //svdCuda(mR);

        //auto begin_timer = std::chrono::high_resolution_clock::now();
        //Eigen::BDCSVD<Eigen::MatrixXd> svdR(mR, Eigen::ComputeFullU | Eigen::ComputeFullV);
        svdR.compute(mR, Eigen::ComputeFullU | Eigen::ComputeFullV);
        //RedSVD::RedSVD<Eigen::MatrixXd> testSVD(mR);
        /*
        auto t = testSVD.singularValues();
        for (uint32_t idx = 0; idx < t.rows(); idx ++)
        {
            std::cout << t(idx) << std::endl;
        }
        */

        //auto end_timer = std::chrono::high_resolution_clock::now();
        //std::chrono::duration<double> elapsed_time = end_timer - begin_timer;
        //time_spent = elapsed_time.count();
        //std::cout << "Elapsed SVD time is:" << time_spent << std::endl;

        //std::cout << "Its singular values are:" << std::endl
        //     << svdR.singularValues() << std::endl;
        /*
        std::cout << "Its left singular vectors are the columns of the thin U matrix:" << std::endl
             << svdR.matrixU() << std::endl;
        std::cout << "Its right singular vectors are the columns of the thin V matrix:" << std::endl
             << svdR.matrixV() << std::endl;
        */
    }

    void SVDecompQR::getSingularValues(Eigen::VectorXd& vSingularValues) const
    {
        vSingularValues.resize(svdR.singularValues().rows());
        for (uint32_t idx = 0; idx < svdR.singularValues().rows(); idx ++)
        {
            vSingularValues(idx) = svdR.singularValues()(idx);
        }
    }
}
