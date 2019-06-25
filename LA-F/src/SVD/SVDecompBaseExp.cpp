#include <iostream>
#include <fstream>

#include "SVD/SVDecompBaseExp.h"

namespace LMFAO::LinearAlgebra
{
    void SVDecompBaseExp::getSingularValues(Eigen::VectorXd& vSingularValues) const
    {
        vSingularValues.resize(mvSingularValues.rows());
        for (uint32_t idx = 0; idx < mvSingularValues.rows(); idx ++)
        {
            vSingularValues(idx) = mvSingularValues(idx);
        }
    }
}
