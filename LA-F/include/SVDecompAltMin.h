#ifndef _LMFAO_LA_SV_DECOMP_ALT_MIN_H_
#define _LMFAO_LA_SV_DECOMP_ALT_MIN_H_

#include "Utils.h"
#include "SVDecompBaseExp.h"

namespace LMFAO::LinearAlgebra
{
    class SVDecompAltMin : public SVDecompBaseExp
    {
        typedef Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic> MatrixT;
        typedef Eigen::Matrix<double, Eigen::Dynamic, 1> VectorT;
        MatrixT Yt;
        MatrixT Wt;
    public:
        SVDecompAltMin(const std::string &path): SVDecompBaseExp(path)
        {}

        SVDecompAltMin(const MapMatrixAggregate& mMatrix, uint32_t numFeatsExp,
                     uint32_t numFeats, uint32_t numFeatsCont,
                     const std::vector<bool>& vIsCat) :
        SVDecompBaseExp(mMatrix, numFeatsExp, numFeats, numFeatsCont, vIsCat)
        {}

        virtual ~SVDecompAltMin() {}

        virtual void decompose() override;
        virtual void getSingularValues(Eigen::VectorXd& vSingularValues) const override;
    };
}
#endif
