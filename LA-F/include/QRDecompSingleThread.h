#ifndef _LMFAO_LA_QR_DECOMP_SINGLE_THREAD_H_
#define _LMFAO_LA_QR_DECOMP_SINGLE_THREAD_H_

#include "QRDecompBase.h"

namespace LMFAO::LinearAlgebra
{
    class QRDecompSingleThread: public QRDecompBase
    {
    // Sigma; Categorical cofactors as an ordered coordinate list
    std::vector<Triple> mCofactorList;

    // Phi; Categorical cofactors as list of lists
    std::vector<std::vector<Pair>> mCofactorPerFeature;

    public:
        QRDecompSingleThread(const std::string &path, const bool isLinDepAllowed=false)
        : QRDecompBase(path, isLinDepAllowed) {}
        QRDecompSingleThread(const MapMatrixAggregate &mMatrix, uint32_t numFeatsExp,
                            uint32_t numFeats, uint32_t numFeatsCont,
                            const std::vector<bool>& vIsCat, const bool isLinDepAllowed=false) :
         QRDecompBase(mMatrix, numFeatsExp, numFeats, numFeatsCont, vIsCat, isLinDepAllowed) {}

        ~QRDecompSingleThread() {}
        virtual void decompose(void) override;
        void processCofactors(void);
        void calculateCR(void);
    };
}
#endif
