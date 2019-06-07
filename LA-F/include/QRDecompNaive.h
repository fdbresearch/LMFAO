#ifndef _LMFAO_LA_QR_DECOMP_NAIVE_H_
#define _LMFAO_LA_QR_DECOMP_NAIVE_H_

#include "QRDecompBase.h"

namespace LMFAO::LinearAlgebra
{
    class QRDecompNaive: public QRDecompBase
    {
    public:
        QRDecompNaive(const std::string &path, const bool isLinDepAllowed=false) :
            QRDecompBase(path, isLinDepAllowed) {}
        QRDecompNaive(const MapMatrixAggregate &mMatrix, uint32_t numFeatsExp,
                            uint32_t numFeats, uint32_t numFeatsCont,
                            const std::vector<bool>& vIsCat, const bool isLinDepAllowed=false) :
            QRDecompBase(mMatrix, numFeatsExp, numFeats, numFeatsCont, vIsCat, isLinDepAllowed) {}
        ~QRDecompNaive() {}
        virtual void decompose(void) override;
        void calculateCR(void);
    };
}

#endif
