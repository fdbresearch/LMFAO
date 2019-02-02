#include <Eigen/Dense>


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
        const std::string& mPath;
        
    public:
        SVDecomp(const std::string& path, DecompType decompType) : 
        mDecompType(decompType), mPath(path) {}
        void decompose();
    };
}
