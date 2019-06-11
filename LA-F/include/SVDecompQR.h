#ifndef _LMFAO_LA_SV_DECOMP_QR_H_
#define _LMFAO_LA_SV_DECOMP_QR_H_

#include "SVDecompBase.h"
#include "Utils.h"

namespace LMFAO::LinearAlgebra
{
    class SVDecompQR : public SVDecompBase
    {
    public:
        enum class DecompType
        {
            NAIVE, SINGLE_THREAD, MULTI_THREAD
        };
    private:
        DecompType mDecompType;
        Eigen::MatrixXd mR;
        Eigen::BDCSVD<Eigen::MatrixXd> svdR;
    public:
        SVDecompQR(const std::string &path, DecompType decompType) :
            SVDecompBase(path), mDecompType(decompType)
        {}

        // !!! ASSUMPTION, mapMatAgg lives in the same block as SVDDEcomp.
        SVDecompQR(DecompType decompType, const MapMatrixAggregate &mapMatAgg,
                 uint32_t numFeatsExp, uint32_t numFeats,
                 uint32_t numFeatsCont,
                 const std::vector<bool> &vIsCat) :
         SVDecompBase(mapMatAgg, numFeatsExp, numFeats, numFeatsCont, vIsCat),
         mDecompType(decompType)
         {}

        virtual void decompose(void) override;
        virtual ~SVDecompQR() override {}
        virtual void getSingularValues(Eigen::VectorXd& vSingularValues) const override;
    };
}
#endif
