#ifndef _LMFAO_LA_SV_DECOMP_EIG_DEC_H_
#define _LMFAO_LA_SV_DECOMP_EIG_DEC_H_

#include "SVDecompBaseExp.h"

namespace LMFAO::LinearAlgebra
{
    class SVDecompEigDec : public SVDecompBaseExp
    {
        typedef Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic> MatrixT;
        typedef Eigen::Matrix<double, Eigen::Dynamic, 1> VectorT;
    public:
        SVDecompEigDec(const std::string &path): SVDecompBaseExp(path)
        {}

        SVDecompEigDec(const MapMatrixAggregate& mMatrix, uint32_t numFeatsExp,
                     uint32_t numFeats, uint32_t numFeatsCont,
                     const std::vector<bool>& vIsCat) :
        SVDecompBaseExp(mMatrix, numFeatsExp, numFeats, numFeatsCont, vIsCat)
        {}

        virtual ~SVDecompEigDec() {}

        virtual void decompose() override;
    };
}
#endif
