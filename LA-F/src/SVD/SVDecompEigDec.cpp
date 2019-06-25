#include <iostream>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <limits>
#include <algorithm>

#include "SVD/SVDecompEigDec.h"

namespace LMFAO::LinearAlgebra
{
    //typedef std::chrono::high_resolution_clock Clock;
    void SVDecompEigDec::decompose()
    {
        Eigen::setNbThreads(8);
        //MatrixT Y;
        //MatrixT W;
        constexpr long double EPS = 1e-16;
        //constexpr long double tau = 1e-5;
        uint32_t rankK = mSigma.rows();
        double accuracy = 0;
        //double initialisationTime = 0;
        //double totalAlgorithmTime = 0;
        //double multTime = 0;
        //double eigenDecompTime = 0;
        //double VSTime = 0;

        //auto totalAlgorithmTimeStart = Clock::now();

        // A = USV.transpose = (U*S^1/2) * (S^1/2*V.transpose)
        // A * V * S^(-1/2) = (U * S^1/2)
        //mSigma = VS^2V.tranpose
        //auto initialisationTimeStart = Clock::now();
        MatrixT L    = MatrixT::Zero(mSigma.rows(), mSigma.rows());
        MatrixT Diag = MatrixT::Zero(mSigma.rows(), mSigma.rows());
        VectorT D    = VectorT::Zero(mSigma.rows());

        //auto initialisationTimeEnd = Clock::now();
        //initialisationTime = (std::chrono::duration_cast<std::chrono::nanoseconds>(initialisationTimeEnd - initialisationTimeStart).count());

        //auto eigenDecompStart = Clock::now();
        // TODO: Rethink about using different type of eigen solver.
        Eigen::SelfAdjointEigenSolver<MatrixT> eigenDecomposer(mSigma);

        L = eigenDecomposer.eigenvectors();
        D = eigenDecomposer.eigenvalues();
        //auto eigenDecompEnd = Clock::now();
        //eigenDecompTime = (std::chrono::duration_cast<std::chrono::nanoseconds>(eigenDecompEnd - eigenDecompStart).count());

        double gamma = 0;//0.0001;

        double sum = 0;
        double current = 0;

        //auto VSTimeStart = Clock::now();
        for (uint32_t i = 0; i < mSigma.rows(); ++i) {
            Diag(i, i) = std::max((double)0, sqrt(D(mSigma.rows() - i - 1)) - gamma);
            sum += Diag(i, i) * Diag(i, i);
        }

        for (uint32_t i = 0; i < mSigma.rows(); ++i) {
            current += Diag(i, i) * Diag(i, i);
            rankK = std::max(rankK, i + 1);
            //if (current + EPS >= accuracy * sum)
            //  break;
        }

        //cerr << "Chosen rank: " << rankK << std::endl;
        MatrixT V    = MatrixT::Zero(mSigma.rows(), rankK);
        //MatrixT Sh    = MatrixT::Zero(rankK, rankK);
        //MatrixT Shinv = MatrixT::Zero(rankK, rankK);


        for (uint32_t i = 0; i < rankK; ++i) {
            //Sh(i, i) = std::sqrt(Diag(i, i));
            //Shinv(i, i) = 1.0 / Sh(i, i);
            V.col(i) = L.col(mSigma.rows() - i - 1);
        }
        //auto VSTimeEnd = Clock::now();
        //VSTime = (std::chrono::duration_cast<std::chrono::nanoseconds>(VSTimeEnd - VSTimeStart).count());

        //cerr << setprecision(20) << fixed;
        // cerr << "Eigenvectors of mSigma:\n" <<  L << std::endl;
        // cerr << "Top " << rankK << " eigenvectors:\n" << V << std::endl;
        // cerr << "Eigenvalues of mSigma:\n" << Diag << std::endl;
        // cerr << "Top " << rankK << " eigenvalues:\n" << (S*S) << std::endl;
        // cerr << "mSigma:\n" << mSigma << std::endl;
        // cerr << "Approx:\n" << L*Diag*L.transpose() << std::endl;
        //cerr << "Expecting identity:\n" << V.transpose() * V  << std::endl;

        //auto multTimeStart = Clock::now();

        mvSingularValues.resize(Diag.rows());
        for (uint32_t idx = 0; idx < Diag.rows(); idx++)
        {
            mvSingularValues(idx) = Diag(idx, idx);
        }
        //Y = Sh * V.transpose();
        //W = Shinv * V.transpose();

        //auto multTimeEnd = Clock::now();
        //multTime = (std::chrono::duration_cast<std::chrono::nanoseconds>(multTimeEnd - multTimeStart).count());

        //auto totalAlgorithmTimeEnd = Clock::now();
        //totalAlgorithmTime = (std::chrono::duration_cast<std::chrono::nanoseconds>(totalAlgorithmTimeEnd - totalAlgorithmTimeStart).count());
        //bool benchmarking = true;
        /*
        if (benchmarking) {
            printf("Benchmarking\n");
            printf("------------\n");
            printf("totalAlgorithmTime:     %.6lf ms\n", totalAlgorithmTime / 1e6);
            printf("initialisationTime:     %.6lf ms\n", initialisationTime / 1e6);
            printf("eigendecompositionTime: %.6lf ms\n", eigenDecompTime / 1e6);
            printf("computing V and S Time: %.6lf ms\n", VSTime / 1e6);
            printf("multiplying time:       %.6lf ms\n", multTime / 1e6);
        }
        */
    }
}
