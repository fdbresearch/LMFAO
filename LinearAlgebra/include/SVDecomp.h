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
        unsigned int mNumFeatures;
        public : SVDecomp(const std::string &path, DecompType decompType) : mDecompType(decompType), mPath(path)
        {
            
        }
        // !!! ASSUMPTION, mapMatAgg lives in the same block.
        SVDecomp(DecompType decompType, const MapMatrixAggregate &mapMatAgg,
                 unsigned int numFeatures) : mDecompType(decompType), 
                 mNumFeatures(numFeatures)  {
            mpmapMatAgg = &mapMatAgg;
                 }
        void decompose();
    };
}
#endif 
