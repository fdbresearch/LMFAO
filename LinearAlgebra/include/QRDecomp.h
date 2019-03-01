#ifndef _LMFAO_QR_DECOMP_H_
#define _LMFAO_QR_DECOMP_H_

#include <Eigen/Dense>
#include <vector>
#include <map>

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

    typedef std::map<std::pair<unsigned int, unsigned int>,
                     long double> MapMatrixAggregate;
    class QRDecomposition
    {
        void formMatrix(const MapMatrixAggregate &, unsigned int numFeatsExp,
                        unsigned int mNumFeats, unsigned int mNumFeatsCont,
                        const std::vector<bool>& vIsCat);
        void readMatrix(const std::string &path);
        void rearrangeMatrix(const std::vector<bool> &vIsCat);
    protected :
        Eigen::MatrixXd mSigma;
        std::vector <long double> mC;
        std::vector <long double> mR;
        std::vector <Triple> mCatVals;
        std::vector <Triple> mNaiveCatVals;

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

        void expandSigma(std::vector <long double> &sigmaExpanded, bool isNaive);

    public:
        virtual void decompose(void) = 0;
        QRDecomposition(const std::string& path)
        {
            readMatrix(path);
        }
        QRDecomposition(const MapMatrixAggregate& mMatrix, unsigned int numFeatsExp,
                        unsigned int numFeats, unsigned int numFeatsCont,
                        const std::vector<bool>& vIsCat)
        {
            formMatrix(mMatrix, numFeatsExp, numFeats, numFeatsCont, vIsCat);
        }
        virtual ~QRDecomposition() {}
        void getR(Eigen::MatrixXd &rEigen);
    };

    class QRDecompositionNaive: public QRDecomposition
    {
    public:
        QRDecompositionNaive(const std::string &path) : QRDecomposition(path) {}
        QRDecompositionNaive(const MapMatrixAggregate &mMatrix, unsigned int numFeatsExp,
                            unsigned int numFeats, unsigned int numFeatsCont,
                            const std::vector<bool>& vIsCat) :
            QRDecomposition(mMatrix, numFeatsExp, numFeats, numFeatsCont, vIsCat) {}
        ~QRDecompositionNaive() {}
        virtual void decompose(void) override;
        void calculateCR(void);
    };

    class QRDecompositionSingleThreaded: public QRDecomposition
    {
    // Sigma; Categorical cofactors as an ordered coordinate list
    std::vector<Triple> mCofactorList;

    // Phi; Categorical cofactors as list of lists
    std::vector<std::vector<Pair>> mCofactorPerFeature;

    public:
        QRDecompositionSingleThreaded(const std::string &path) : QRDecomposition(path) {}
        QRDecompositionSingleThreaded(const MapMatrixAggregate &mMatrix, unsigned int numFeatsExp,
                            unsigned int numFeats, unsigned int numFeatsCont,
                            const std::vector<bool>& vIsCat) :
         QRDecomposition(mMatrix, numFeatsExp, numFeats, numFeatsCont, vIsCat) {}

        ~QRDecompositionSingleThreaded() {}
        virtual void decompose(void) override;
        void processCofactors(void);
        void calculateCR(void);
    };

    class QRDecompositionParallel : public QRDecomposition
    {
        // Sigma; Categorical cofactors as an ordered coordinate list
        std::vector<Triple> mCofactorList;

        // Phi; Categorical cofactors as list of lists
        std::vector<std::vector<Pair>> mCofactorPerFeature;

      public:
        ~QRDecompositionParallel() {}
        virtual void decompose(void) override;
        void processCofactors(void);
        void calculateCR(void);
        QRDecompositionParallel(const std::string &path) : QRDecomposition(path) {}
    };

}
#endif
