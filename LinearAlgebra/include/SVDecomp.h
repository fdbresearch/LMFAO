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
        const std::string mPath;
        const LMFAO::LinearAlgebra::MapMatrixAggregate* mpmapMatAgg = nullptr;
        unsigned int mNumFeatsExp;
        unsigned int mNumFeatsCont;
        unsigned int mNumFeats;
        public : SVDecomp(const std::string &path, DecompType decompType) : mDecompType(decompType), mPath(path)
        {
            
        }
        // !!! ASSUMPTION, mapMatAgg lives in the same block.
        SVDecomp(DecompType decompType, const MapMatrixAggregate &mapMatAgg,
                 unsigned int numFeatsExp, unsigned int numFeats, 
                 unsigned int numFeatsCont) : mDecompType(decompType), 
                 mNumFeatsExp(numFeatsExp), mNumFeats(numFeats), 
                 mNumFeatsCont(numFeatsCont)  
                 {
                    mpmapMatAgg = &mapMatAgg;
                 }
        void decompose();
    };
}
#endif 
