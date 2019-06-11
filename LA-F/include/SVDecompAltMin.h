#ifndef _LMFAO_LA_SV_DECOMP_ALT_MIN_H_
#define _LMFAO_LA_SV_DECOMP_ALT_MIN_H_

#include "SVDecompBase.h"
#include "Utils.h"

namespace LMFAO::LinearAlgebra
{
    class SVDecompAltMin : public SVDecompBase
    {
        virtual ~SVDecompAltMin() {}
        //virtual void getSingularValues(Eigen::VectorXd& vSingularValues) override;
    };
}
#endif
