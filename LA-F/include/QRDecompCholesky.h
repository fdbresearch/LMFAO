#ifndef _LMFAO_LA_QR_DECOMP_CHOLESKY_H_
#define _LMFAO_LA_QR_DECOMP_CHOLESKY_H_

#include "QRDecompBase.h"

namespace LMFAO::LinearAlgebra
{
    class QRDecompCholesky: public QRDecompBase
    {
    public:
        QRDecompCholesky(const std::string &path, const bool isLinDepAllowed=false) :
            QRDecompBase(path, true, isLinDepAllowed) {}
        QRDecompCholesky(const MapMatrixAggregate &mMatrix, uint32_t numFeatsExp,
                            uint32_t numFeats, uint32_t numFeatsCont,
                            const std::vector<bool>& vIsCat, const bool isLinDepAllowed=false) :
            QRDecompBase(mMatrix, numFeatsExp, numFeats, numFeatsCont,
                         vIsCat, true, isLinDepAllowed) {}
        ~QRDecompCholesky() {}
        virtual void decompose(void) override;
    };
}

#endif
