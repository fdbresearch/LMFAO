#include <iostream>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <limits>
#include <algorithm>
#include <cstdint>

#include "Utils/Logger.h"
#include "Utils/Utils.h"

#include "SVD/SVDecompAltMin.h"

namespace LMFAO::LinearAlgebra
{
    //typedef std::chrono::high_resolution_clock Clock;
    void SVDecompAltMin::decompose()
    {
        static constexpr long double tau = 1e-10;
        uint32_t rankK = mSigma.rows();
        LMFAO_LOG_DBG("RankK:", rankK);
        uint32_t iterations = UINT32_MAX;
        //Eigen::setNbThreads(8);
        //double initialisationTime = 0;
        //double totalAlgorithmTime = 0;

        //double minIterationTime = (1LL<<60);
        //double avgIterationTime = 0;
        //double maxIterationTime = -(1LL<<60);

        //double convergenceTime = 0;
        //double multTimeMin = (1LL<<60);
        //double multTimeAvg = 0;
        //double multTimeMax = -(1LL<<60);

        //double inverseTimeMin = (1LL<<60);
        //double inverseTimeAvg = 0;
        //double inverseTimeMax = -(1LL<<60);

        //auto totalAlgorithmTimeStart = Clock::now();
        //auto initialisationTimeStart = Clock::now();
        Yt = MatrixT::Zero(rankK, mSigma.rows());
        MatrixT Yold = MatrixT::Zero(rankK, mSigma.rows());
        Wt   = MatrixT::Zero(rankK, mSigma.rows());
        MatrixT Wold = MatrixT::Zero(rankK, mSigma.rows());
        MatrixT S = mSigma, Zt, Ztaux, Bt, Btaux;

        double traceS = S.trace();
        double avgTr = traceS / (1.0 * mSigma.rows());
        for (uint32_t i = 0; i < rankK; ++i)
        {
            Yt(i, i) = 1;
        }

        MatrixT Ik = MatrixT::Zero(rankK, rankK);
        for (uint32_t i = 0; i < rankK; ++i)
        {
            Ik(i, i) = 1;
        }

        MatrixT Ikgamma = MatrixT::Zero(rankK, rankK);
        MatrixT IkgammaPeTr = MatrixT::Zero(rankK, rankK);
        MatrixT SpeTr = S * (1.0 / avgTr);

        double gamma = 0;//1e-8;
        Ikgamma = Ik * gamma;
        IkgammaPeTr = Ik * (gamma / avgTr);

        uint32_t currentIteration = 0;
        bool done = false;
        /**
        auto initialisationTimeEnd = Clock::now();
        initialisationTime = (std::chrono::duration_cast<std::chrono::nanoseconds>(initialisationTimeEnd - initialisationTimeStart).count());

        cout << "Yt" << endl;
        cout << Yt << endl;
        cout << "Ikgamma" << endl;
        cout << Ikgamma << endl;
        cout << "IkgammaPeTr" << endl;
        cout << IkgammaPeTr << endl;
        cout << "S:" << endl;
        cout << S << endl;
        cout << "Avgtr: " << avgTr << endl;
        cout << "SpeTr" << endl;
        cout << SpeTr << endl;
        */

        Eigen::LLT<MatrixT> choleskySolver;
        for (currentIteration = 0; currentIteration < iterations; currentIteration++)
        {
            LMFAO_LOG_DBG("currentIteration", currentIteration);
            //auto iterationBegin = Clock::now();
            //double crtMult = 0;
            //double crtInv = 0;

            //auto m1 = Clock::now();
            //Ztaux = Yt * Yt.transpose() + Ikgamma;

            //cout << "Ztaux" << endl;
            //cout << Ztaux << endl;

            //auto m2 = Clock::now();
            //crtMult += (std::chrono::duration_cast<std::chrono::nanoseconds>(m2-m1).count());

            //auto i1 = Clock::now();
            choleskySolver.compute(Yt * Yt.transpose() + Ikgamma);//Ztaux);
            Zt = choleskySolver.solve(Ik);
            //Zt = Ztaux.inverse();
            //auto i2 = Clock::now();
            //crtInv += (std::chrono::duration_cast<std::chrono::nanoseconds>(i2-i1).count());

            //cout << "Zt" << endl;
            //cout << Zt << endl;

            //auto m3 = Clock::now();
            Wold = Wt;
            Wt = Zt * Yt;

            //cout << "Wt" << endl;
            //cout << Wt << endl;

            //Btaux = Wt*SpeTr*Wt.transpose() + IkgammaPeTr;
            //cout << "Btaux" << endl;
            //cout << Btaux << endl;

            //auto m4 = Clock::now();
            //crtMult += (std::chrono::duration_cast<std::chrono::nanoseconds>(m4-m3).count());

            //auto i3 = Clock::now();
            //Bt = Btaux.inverse();
            choleskySolver.compute(Wt * SpeTr * Wt.transpose() + IkgammaPeTr);//Btaux);
            Bt = choleskySolver.solve(Ik);
            //auto i4 = Clock::now();
            //crtInv += (std::chrono::duration_cast<std::chrono::nanoseconds>(i4-i3).count());

            //cout << "Bt" << endl;
            //cout << Bt << endl;

            //auto m5 = Clock::now();
            Yold = Yt;
            Yt = Bt * Wt * SpeTr;

            //cout << "Yt" << endl;
            //cout << Yt << endl;

            //auto m6 = Clock::now();
            //crtMult += (std::chrono::duration_cast<std::chrono::nanoseconds>(m6-m5).count());

            //inverseTimeMin = min(inverseTimeMin, crtInv);
            //inverseTimeAvg += crtInv;
            //inverseTimeMax = max(inverseTimeMax, crtInv);

            //multTimeMin = min(multTimeMin, crtMult);
            //multTimeAvg += crtMult;
            //multTimeMax = max(multTimeMax, crtMult);
            //printMatrixDense(std::cout, Yt);
            //printMatrixDense(std::cout, Wt);
            if ((Wold - Wt).squaredNorm() < tau &&
                (Yold - Yt).squaredNorm() < tau)
            {
                break;
            }
            //auto iterationEnd = Clock::now();
            //double itTime =  (std::chrono::duration_cast<std::chrono::nanoseconds>(iterationEnd - iterationBegin).count());
            //minIterationTime = min(minIterationTime, itTime);
            //avgIterationTime += itTime;
            //maxIterationTime = max(maxIterationTime, itTime);
        }
        //avgIterationTime /= (1.0*currentIteration);
        //multTimeAvg /= (1.0*currentIteration);
        //inverseTimeAvg /= (1.0*currentIteration);

        //auto totalAlgorithmTimeEnd = Clock::now();
        //totalAlgorithmTime = (std::chrono::duration_cast<std::chrono::nanoseconds>(totalAlgorithmTimeEnd - totalAlgorithmTimeStart).count());
        //bool benchmarking = true;
        /*
        if (benchmarking) {
            convergenceTime = avgIterationTime * currentIteration;
            printf("Benchmarking\n");
            printf("------------\n");
            printf("number of threads: %d\n", Eigen::nbThreads());
            printf("totalAlgorithmTime:%.6lf ms\n", totalAlgorithmTime / 1e6);
            printf("initialisationTime:%.6lf ms\n", initialisationTime / 1e6);
            printf("convergenceTime:%.6lf ms\n", convergenceTime / 1e6);
            printf("iterations:%d \n", currentIteration);
            printf("minIterationTime:%.6lf ms\n", minIterationTime / 1e6);
            printf("avgIterationTime:%.6lf ms\n", avgIterationTime / 1e6);
            printf("maxIterationTime:%.6lf ms\n", maxIterationTime / 1e6);
            printf("inverseTimeMin:%.6lf ms\n", inverseTimeMin / 1e6);
            printf("inverseTimeAvg:%.6lf ms\n", inverseTimeAvg / 1e6);
            printf("inverseTimeMax:%.6lf ms\n", inverseTimeMax / 1e6);
            printf("multTimeMin:%.6lf ms\n", multTimeMin / 1e6);
            printf("multTimeAvg:%.6lf ms\n", multTimeAvg / 1e6);
            printf("multTimeMax:%.6lf ms\n", multTimeMax / 1e6);
        }
        */
        //W = Wt;
        //Y = Yt;
    }

    void SVDecompAltMin::getSingularValues(Eigen::VectorXd& vSingularValues) const
    {
        MatrixT sigma = Yt.transpose() * Wt.transpose()
                        * mSigma * Wt * Yt;
        Eigen::SelfAdjointEigenSolver<MatrixT> eigenDecomposer(sigma);
        Eigen::VectorXd eigenVal = eigenDecomposer.eigenvalues();
        vSingularValues.resize(sigma.rows());

        for (uint32_t idx = 0; idx < eigenVal.rows(); idx++)
        {
            double maxVal = eigenVal[idx];
            uint32_t maxValIdx = idx;
            for (uint32_t idxMax = idx + 1; idxMax < eigenVal.rows(); idxMax++)
            {
                if (eigenVal[idxMax] >  eigenVal[idx])
                {
                    maxValIdx = idxMax;
                    maxVal = eigenVal[idxMax];
                }
            }
            std::swap(eigenVal[idx], eigenVal[maxValIdx]);
        }

        for (uint32_t idx = 0; idx < eigenVal.rows(); idx ++)
        {
            vSingularValues(idx) = std::sqrt(eigenVal(idx));
        }
    }
}
