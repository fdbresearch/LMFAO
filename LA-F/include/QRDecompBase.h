#ifndef _LMFAO_LA_QR_DECOMP_BASE_H_
#define _LMFAO_LA_QR_DECOMP_BASE_H_

#include "Utils.h"

#include <iostream>
#include <vector>

namespace LMFAO::LinearAlgebra
{
    class QRDecompBase
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

    protected:
        QRDecompBase(const std::string& path, bool initNaiveVect, const bool isLinDepAllowed=false) :
        mIsLinDepAllowed(isLinDepAllowed)
        {
            FeatDim oFeatDim;
            LMFAO::LinearAlgebra::readMatrix(path, oFeatDim, mSigma,
                                 &mCatVals, initNaiveVect);
            mNumFeats = oFeatDim.num;
            mNumFeatsExp = oFeatDim.numExp;
            mNumFeatsCont = oFeatDim.numCont;
            mNumFeatsCat = oFeatDim.numCat;
        }
        QRDecompBase(const MapMatrixAggregate& mMatrix, uint32_t numFeatsExp,
                     uint32_t numFeats, uint32_t numFeatsCont,
                     const std::vector<bool>& vIsCat, bool initNaiveVect,
                     const bool isLinDepAllowed=false):
        mIsLinDepAllowed(isLinDepAllowed)
        {
            FeatDim featDim;
            FeatDim oFeatDim;
            featDim.num = numFeats;
            featDim.numExp = numFeatsExp;
            featDim.numCont = numFeatsCont;
            LMFAO::LinearAlgebra::formMatrix(mMatrix, vIsCat, featDim, oFeatDim, mSigma,
                                             &mCatVals, initNaiveVect);
            mNumFeats = oFeatDim.num;
            mNumFeatsExp = oFeatDim.numExp;
            mNumFeatsCont = oFeatDim.numCont;
            mNumFeatsCat = oFeatDim.numCat;
        }
    public:
        virtual void decompose(void) = 0;
        virtual ~QRDecompBase() {}
        void getR(Eigen::MatrixXd &rEigen) const;
        friend std::ostream& operator<<(std::ostream& os, const QRDecompBase& qrDecomp);
    };
}
#endif
