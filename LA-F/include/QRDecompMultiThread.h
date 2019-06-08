#ifndef _LMFAO_LA_QR_DECOMP_MULTI_THREAD_H_
#define _LMFAO_LA_QR_DECOMP_MULTI_THREAD_H_

#include "QRDecompBase.h"

#include <mutex>
#include <boost/thread/barrier.hpp>

namespace LMFAO::LinearAlgebra
{
    class QRDecompMultiThread: public QRDecompBase
    {
        // Sigma; Categorical cofactors as an ordered coordinate list
        std::vector<Triple> mCofactorList;

        // Phi; Categorical cofactors as list of lists
        std::vector<std::vector<Pair>> mCofactorPerFeature;

        const uint32_t mNumThreads = 8;
        boost::barrier mBarrier{mNumThreads};
        std::mutex mMutex;
    public:
        QRDecompMultiThread(const std::string &path, const bool isLinDepAllowed=false) :
        QRDecompBase(path, false, isLinDepAllowed) {}
        QRDecompMultiThread(const MapMatrixAggregate &mMatrix, uint32_t numFeatsExp,
                            uint32_t numFeats, uint32_t numFeatsCont,
                            const std::vector<bool>& vIsCat, const bool isLinDepAllowed=false) :
        QRDecompBase(mMatrix, numFeatsExp, numFeats, numFeatsCont, vIsCat, false, isLinDepAllowed) {}

        ~QRDecompMultiThread() {}
        virtual void decompose(void) override;
        void processCofactors(void);
        void calculateCR(uint32_t threadId);
    };
}
#endif
