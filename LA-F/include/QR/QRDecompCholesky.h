#ifndef _LMFAO_LA_QR_DECOMP_CHOLESKY_H_
#define _LMFAO_LA_QR_DECOMP_CHOLESKY_H_

#include "QRDecompBase.h"

namespace LMFAO::LinearAlgebra
{
    class QRDecompCholesky: public QRDecompBase
    {
        bool m_onlyR;
    public:
        QRDecompCholesky(const std::string &path, const bool isLinDepAllowed=false, const bool onlyR=false) :
            QRDecompBase(path, true, isLinDepAllowed), m_onlyR(onlyR) {}
        QRDecompCholesky(const MapMatrixAggregate &mMatrix, uint32_t numFeatsExp,
                            uint32_t numFeats, uint32_t numFeatsCont,
                            const std::vector<bool>& vIsCat, const bool isLinDepAllowed=false,
                            const bool onlyR=false) :
            QRDecompBase(mMatrix, numFeatsExp, numFeats, numFeatsCont,
                         vIsCat, true, isLinDepAllowed), m_onlyR(onlyR) {}
        ~QRDecompCholesky() {}
        virtual void decompose(void) override;
    };
}

#endif
