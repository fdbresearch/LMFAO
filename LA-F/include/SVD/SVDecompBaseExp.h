#ifndef _LMFAO_LA_SV_DECOMP_BASE_EXP_H_
#define _LMFAO_LA_SV_DECOMP_BASE_EXP_H_

#include "SVDecompBase.h"

namespace LMFAO::LinearAlgebra
{
    class SVDecompBaseExp : public SVDecompBase
    {
    protected:
        Eigen::VectorXd mvSingularValues;
        Eigen::MatrixXd mSigma;
    public:
        SVDecompBaseExp(const std::string &path): SVDecompBase(path)
        {
            FeatDim oFeatDim;
            LMFAO::LinearAlgebra::readMatrix(
                path, oFeatDim, mSigma,
                nullptr, true /* init naive */);
            mNumFeats = oFeatDim.num;
            mNumFeatsExp = oFeatDim.numExp;
            mNumFeatsCont = oFeatDim.numCont;
            mNumFeatsCat = oFeatDim.numCat;
        }

        SVDecompBaseExp(const MapMatrixAggregate& mMatrix, uint32_t numFeatsExp,
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
                                             nullptr, true /* init naive*/);
            mNumFeats = oFeatDim.num;
            mNumFeatsExp = oFeatDim.numExp;
            mNumFeatsCont = oFeatDim.numCont;
            mNumFeatsCat = oFeatDim.numCat;
        }

        virtual ~SVDecompBaseExp() {}

        virtual void getSingularValues(Eigen::VectorXd& vSingularValues) const override;
    };
}
#endif
