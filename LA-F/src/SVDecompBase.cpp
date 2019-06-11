#include <iostream>
#include <fstream>
#include <iomanip>
#include <limits>

#include "SVDecompBase.h"

namespace LMFAO::LinearAlgebra
{
    std::ostream& operator<<(std::ostream& out, const SVDecompBase& svdDecomp)
    {
        Eigen::VectorXd evSingularValues;
        svdDecomp.getSingularValues(evSingularValues);
        uint32_t N = evSingularValues.rows();
        out << N << std::endl;
        out << std::fixed << std::setprecision(std::numeric_limits<double>::digits10 + 1);

        for (uint32_t idx = 0; idx < N; idx ++)
        {
            out << evSingularValues(idx) << std::endl;
        }

        return out;
    }
}
