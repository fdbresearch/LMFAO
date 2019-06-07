#ifndef _LMFAO_LA_QR_DECOMP_H_
#define _LMFAO_LA_QR_DECOMP_H_

#include "Utils.h"

#include <iostream>
#include <vector>
#include <map>
#include <mutex>
#include <boost/thread/barrier.hpp>

namespace LMFAO::LinearAlgebra
{
    class QRDecomposition
    {
     protected:
        Eigen::MatrixXd mSigma;
        // R is  stored column-wise to exploit cache locality.
        // C is stored row-wise look at the line 112 -> cache locality in each thread important.
        // They have to be double instead of long double because
        // code runs 40% faster.
        //
        std::vector <double> mC;
        std::vector <double> mR;
        std::vector <Triple> mCatVals;
        std::vector <Triple> mNaiveCatVals;

        // Number of features (categorical + continuous) in sigma matrix.
        //
        uint32_t mNumFeats = 0;
        // Number of real valued values in the expanded sigma matrix
        // without linearly dependent columns.
        //
        uint32_t mNumFeatsExp = 0;
        // Number of continuous features in sigma matrix.
        //
        uint32_t mNumFeatsCont = 0;
        // Number of categorical features in sigma matrix.
        //
        uint32_t mNumFeatsCat;
        // Controls whether linearly independent columns are allowed.
        // When they are allowed this can lead to numerical instability.
        // Use it with caution!!!
        //
        const bool mIsLinDepAllowed = false;
        static constexpr double mcPrecisionError = 1e-12;

        void expandSigma(std::vector <double> &sigmaExpanded, bool isNaive);

    public:
        virtual void decompose(void) = 0;
        QRDecomposition(const std::string& path, const bool isLinDepAllowed=false) :
        mIsLinDepAllowed(isLinDepAllowed)
        {
            FeatDim oFeatDim;
            LMFAO::LinearAlgebra::readMatrix(path, oFeatDim, mSigma,
                                 mCatVals, mNaiveCatVals);
            mNumFeats = oFeatDim.num;
            mNumFeatsExp = oFeatDim.numExp;
            mNumFeatsCont = oFeatDim.numCont;
            mNumFeatsCat = oFeatDim.numCat;
        }
        QRDecomposition(const MapMatrixAggregate& mMatrix, uint32_t numFeatsExp,
                        uint32_t numFeats, uint32_t numFeatsCont,
                        const std::vector<bool>& vIsCat, const bool isLinDepAllowed=false):
        mIsLinDepAllowed(isLinDepAllowed)
        {
            FeatDim featDim;
            FeatDim oFeatDim;
            featDim.num = numFeats;
            featDim.numExp = numFeatsExp;
            featDim.numCont = numFeatsCont;
            LMFAO::LinearAlgebra::formMatrix(mMatrix, vIsCat, featDim, oFeatDim, mSigma,
                                             mCatVals, mNaiveCatVals);
            mNumFeats = oFeatDim.num;
            mNumFeatsExp = oFeatDim.numExp;
            mNumFeatsCont = oFeatDim.numCont;
            mNumFeatsCat = oFeatDim.numCat;
        }
        virtual ~QRDecomposition() {}
        void getR(Eigen::MatrixXd &rEigen);
        friend std::ostream& operator<<(std::ostream& os, const QRDecomposition& qrDecomp);
    };

    class QRDecompositionNaive: public QRDecomposition
    {
    public:
        QRDecompositionNaive(const std::string &path, const bool isLinDepAllowed=false) :
            QRDecomposition(path, isLinDepAllowed) {}
        QRDecompositionNaive(const MapMatrixAggregate &mMatrix, uint32_t numFeatsExp,
                            uint32_t numFeats, uint32_t numFeatsCont,
                            const std::vector<bool>& vIsCat, const bool isLinDepAllowed=false) :
            QRDecomposition(mMatrix, numFeatsExp, numFeats, numFeatsCont, vIsCat, isLinDepAllowed) {}
        ~QRDecompositionNaive() {}
        virtual void decompose(void) override;
        void calculateCR(void);
    };

    class QRDecompositionSingleThreaded: public QRDecomposition
    {
    // Sigma; Categorical cofactors as an ordered coordinate list
    std::vector<Triple> mCofactorList;

    // Phi; Categorical cofactors as list of lists
    std::vector<std::vector<Pair>> mCofactorPerFeature;

    public:
        QRDecompositionSingleThreaded(const std::string &path, const bool isLinDepAllowed=false)
        : QRDecomposition(path, isLinDepAllowed) {}
        QRDecompositionSingleThreaded(const MapMatrixAggregate &mMatrix, uint32_t numFeatsExp,
                            uint32_t numFeats, uint32_t numFeatsCont,
                            const std::vector<bool>& vIsCat, const bool isLinDepAllowed=false) :
         QRDecomposition(mMatrix, numFeatsExp, numFeats, numFeatsCont, vIsCat, isLinDepAllowed) {}

        ~QRDecompositionSingleThreaded() {}
        virtual void decompose(void) override;
        void processCofactors(void);
        void calculateCR(void);
    };

    class QRDecompositionMultiThreaded: public QRDecomposition
    {
    // Sigma; Categorical cofactors as an ordered coordinate list
    std::vector<Triple> mCofactorList;

    // Phi; Categorical cofactors as list of lists
    std::vector<std::vector<Pair>> mCofactorPerFeature;

    const uint32_t mNumThreads = 8;
    boost::barrier mBarrier{mNumThreads};
    std::mutex mMutex;
    public:
        QRDecompositionMultiThreaded(const std::string &path, const bool isLinDepAllowed=false) :
        QRDecomposition(path, isLinDepAllowed) {}
        QRDecompositionMultiThreaded(const MapMatrixAggregate &mMatrix, uint32_t numFeatsExp,
                            uint32_t numFeats, uint32_t numFeatsCont,
                            const std::vector<bool>& vIsCat, const bool isLinDepAllowed=false) :
        QRDecomposition(mMatrix, numFeatsExp, numFeats, numFeatsCont, vIsCat, isLinDepAllowed) {}

        ~QRDecompositionMultiThreaded() {}
        virtual void decompose(void) override;
        void processCofactors(void);
        void calculateCR(uint32_t threadId);
    };
}

#endif
