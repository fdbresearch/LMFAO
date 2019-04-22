#ifndef _LMFAO_SVD_DECOMP_H_
#define _LMFAO_SVD_DECOMP_H_

#include <Eigen/Dense>
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
        unsigned int mNumFeatsExp;
        unsigned int mNumFeatsCont;
        unsigned int mNumFeats;
        std::vector<bool> mvIsCat;
        public : SVDecomp(const std::string &path, DecompType decompType) : mDecompType(decompType), mPath(path)
        {

        }
        // !!! ASSUMPTION, mapMatAgg lives in the same block as SVDDEcomp.
        SVDecomp(DecompType decompType, const MapMatrixAggregate &mapMatAgg,
                 unsigned int numFeatsExp, unsigned int numFeats,
                 unsigned int numFeatsCont,
                 const std::vector<bool> &vIsCat) : mDecompType(decompType),
                 mNumFeatsExp(numFeatsExp), mNumFeatsCont(numFeatsCont),
                 mNumFeats(numFeats)
                 {
                    mpmapMatAgg = &mapMatAgg;
                    mvIsCat = vIsCat;
                 }
        void decompose();

        void getSingularValues(Eigen::VectorXd& vSingularValues);

    };


}
#endif
