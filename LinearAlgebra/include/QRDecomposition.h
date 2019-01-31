#include <Eigen/Dense>
#include <vector>

namespace LMFAO::LinearAlgebra
{
    template <typename T> 
    T inline expIdx(T row, T col, T numCols)
    {
        return row * numCols + col;
    }

    /**
     * Used to help the compiler make branching predictions.
     */
    #ifdef __GNUC__
    #define likely(x) __builtin_expect((x),1)
    #define unlikely(x) __builtin_expect((x),0)
    #else
    #define likely(x) x
    #define unlikely(x) x
    #endif

    typedef std::tuple<unsigned int, unsigned  int,  
                       long double> Triple;
    typedef std::tuple<unsigned int, long double> Pair;

    typedef Eigen::Matrix<bool, Eigen::Dynamic, Eigen::Dynamic> MatrixBool;

    class QRDecomposition 
    {
        void readMatrix(const std::string& path);

    protected:
        Eigen::MatrixXd mSigma;
        std::vector <long double> mC;
        std::vector <long double> mR;
        std::vector <Triple> mCatVals;
        
        // Number of features (categorical + continuous) in sigma matrix.
        //
        unsigned int mNumFeats = 0;
        // Number of real valued values in the expanded sigma matrix 
        // without linearly dependent columns.
        //
        unsigned int mNumFeatsExp = 0;
        // Number of continuous features in sigma matrix.
        //
        unsigned int mNumFeatsCont = 0;
        // Number of categorical features in sigma matrix.
        //
        unsigned int mNumFeatsCat;

        //virtual void calculateQR(void) = 0;
        virtual void decompose(void) = 0;
        void expandSigma(std::vector <long double> &sigmaExpanded, bool isNaive);

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
        void calculateCR(void);
        QRDecompositionNaive(const std::string& path): QRDecomposition(path) {}
    };

    class QRDecompositionSingleThreaded: public QRDecomposition
    {
    //! Sigma; Categorical cofactors as an ordered coordinate list
    std::vector<Triple> _cofactorList;

    //! Phi; Categorical cofactors as list of lists
    std::vector<std::vector<Pair>> _cofactorPerFeature;

    public:
        //virtual void calculateQR(void);
        virtual void decompose(void) override;
        void processCofactors(void);
        void calculateCR(void);
        QRDecompositionSingleThreaded(const std::string& path): QRDecomposition(path) {}
    };
}
