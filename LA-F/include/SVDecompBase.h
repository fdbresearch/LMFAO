#ifndef _LMFAO_LA_SV_DECOMP_BASE_H_
#define _LMFAO_LA_SV_DECOMP_BASE_H_

#include "Utils.h"

namespace LMFAO::LinearAlgebra
{
    class SVDecompBase
    {
    protected:
        uint32_t mNumFeatsExp;
        uint32_t mNumFeatsCont;
        uint32_t mNumFeats;
        uint32_t mNumFeatsCat;

        const std::string mPath;
        Eigen::MatrixXd mSigma;
        std::vector<bool> mvIsCat;

        const LMFAO::LinearAlgebra::MapMatrixAggregate* mpmapMatAgg = nullptr;
        SVDecompBase(const std::string &path): mPath(path)
        {}
        // !!! ASSUMPTION, mapMatAgg lives in the same block as SVDDEcomp.
        SVDecompBase(const MapMatrixAggregate &mapMatAgg,
                 uint32_t numFeatsExp, uint32_t numFeats,
                 uint32_t numFeatsCont,
                 const std::vector<bool> &vIsCat) :
                 mNumFeatsExp(numFeatsExp), mNumFeatsCont(numFeatsCont),
                 mNumFeats(numFeats)
         {
            mpmapMatAgg = &mapMatAgg;
            mvIsCat = vIsCat;
         }
    public:
        virtual ~SVDecompBase() {}
        virtual void decompose(void) = 0;
        virtual void getSingularValues(Eigen::VectorXd& vSingularValues) const = 0;
        friend std::ostream& operator<<(std::ostream& os, const SVDecompBase& svdDecomp);
    };
}
#endif
