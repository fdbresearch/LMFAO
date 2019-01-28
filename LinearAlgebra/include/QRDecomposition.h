#include <Eigen/Dense>
#include <vector>

namespace LMFAO::LinearAlgebra
{
    class QRDecomposition 
    {
        void readMatrix(const std::string& path);
    protected:
        Eigen::MatrixXd mSigma;
        std::vector <long double> mC;
        std::vector <long double> mR;
        
        unsigned int mNumFeats = 0;
        unsigned int mNumFeatsExp = 0;
        unsigned int mNumConFeats = 0;

        //virtual void calculateQR(void) = 0;
        virtual void decompose(void) = 0;
        void expandSigma(std::vector <long double> &sigmaExpanded);
    public:
        QRDecomposition(const std::string& path) 
        {
            readMatrix(path);
        }

        Eigen::MatrixXd& getMatrix() { return mSigma; }
    }; 

    class QRDecompositionNaive: public QRDecomposition
    {
    public:
        //virtual void calculateQR(void);
        virtual void decompose(void) override;
        void calculateQR(void);
        QRDecompositionNaive(const std::string& path): QRDecomposition(path) {}
    };
}
