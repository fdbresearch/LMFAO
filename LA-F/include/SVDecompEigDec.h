#ifndef _LMFAO_LA_SV_DECOMP_EIG_DEC_H_
#define _LMFAO_LA_SV_DECOMP_EIG_DEC_H_

#include "Utils.h"
#include "SVDecompBase.h"

namespace LMFAO::LinearAlgebra
{
    class SVDecompEigDec : public SVDecompBase
    {
        typedef Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic> MatrixT;
        typedef Eigen::Matrix<double, Eigen::Dynamic, 1> VectorT;
        std::vector<double> mvSingularValues;

    public:
        SVDecompEigDec(const std::string &path): SVDecompBase(path)
        {
            FeatDim oFeatDim;
            LMFAO::LinearAlgebra::readMatrix(
                path, oFeatDim, mSigma,
                nullptr, false /* init naive */);
            mNumFeats = oFeatDim.num;
            mNumFeatsExp = oFeatDim.numExp;
            mNumFeatsCont = oFeatDim.numCont;
            mNumFeatsCat = oFeatDim.numCat;
        }

        SVDecompEigDec(const MapMatrixAggregate& mMatrix, uint32_t numFeatsExp,
                     uint32_t numFeats, uint32_t numFeatsCont,
                     const std::vector<bool>& vIsCat) :
        SVDecompBase(mMatrix, numFeatsExp, numFeats, numFeatsCont, vIsCat)
        {
            FeatDim featDim;
            FeatDim oFeatDim;
            featDim.num = numFeats;
            featDim.numExp = numFeatsExp;
            featDim.numCont = numFeatsCont;
            LMFAO::LinearAlgebra::formMatrix(mMatrix, vIsCat, featDim, oFeatDim, mSigma,
                                             nullptr, false /* init naive*/);
            mNumFeats = oFeatDim.num;
            mNumFeatsExp = oFeatDim.numExp;
            mNumFeatsCont = oFeatDim.numCont;
            mNumFeatsCat = oFeatDim.numCat;
        }

        virtual ~SVDecompEigDec() {}

        virtual void decompose() override;
        virtual void getSingularValues(Eigen::VectorXd& vSingularValues) const override;
    };
}
#endif
