#ifndef _LMFAO_SVD_DECOMP_H_
#define _LMFAO_SVD_DECOMP_H_

#include "QRDecomp.h"

namespace LMFAO::LinearAlgebra
{
    class SVDecomp
    {
    public:
        enum class DecompType
        {
            NAIVE, SINGLE_THREAD, MULTI_THREAD, TRIANGLE
        };
    private:
        DecompType mDecompType;
        Eigen::MatrixXd mR;
        Eigen::BDCSVD<Eigen::MatrixXd> svdR;
        const std::string mPath;
        const LMFAO::LinearAlgebra::MapMatrixAggregate* mpmapMatAgg = nullptr;
        uint32_t mNumFeatsExp;
        uint32_t mNumFeatsCont;
        uint32_t mNumFeats;
        std::vector<bool> mvIsCat;
        public : SVDecomp(const std::string &path, DecompType decompType) : mDecompType(decompType), mPath(path)
        {

        }
        // !!! ASSUMPTION, mapMatAgg lives in the same block as SVDDEcomp.
        SVDecomp(DecompType decompType, const MapMatrixAggregate &mapMatAgg,
                 uint32_t numFeatsExp, uint32_t numFeats,
                 uint32_t numFeatsCont,
                 const std::vector<bool> &vIsCat) : mDecompType(decompType),
                 mNumFeatsExp(numFeatsExp), mNumFeatsCont(numFeatsCont),
                 mNumFeats(numFeats)
                 {
                    mpmapMatAgg = &mapMatAgg;
                    mvIsCat = vIsCat;
                 }
        void decompose();

        void getSingularValues(Eigen::VectorXd& vSingularValues);
        friend std::ostream& operator<<(std::ostream& os, const SVDecomp& svdDecomp);
    };


}
#endif
