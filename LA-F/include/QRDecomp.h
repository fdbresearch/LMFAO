#ifndef _LMFAO_QR_DECOMP_H_
#define _LMFAO_QR_DECOMP_H_

#define EIGEN_USE_BLAS
#define EIGEN_USE_LAPACKE

#include <boost/thread/barrier.hpp>
#include <Eigen/Dense>
#include <vector>
#include <map>
#include <mutex>
#include <iostream>

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

    void svdCuda(const Eigen::MatrixXd &a);

    typedef std::tuple<unsigned int, unsigned int,
                           double> Triple;
    typedef std::tuple<unsigned int, double> Pair;

    typedef std::map<std::pair<unsigned int, unsigned int>,
                     double> MapMatrixAggregate;
    class QRDecomposition
    {
        void formMatrix(const MapMatrixAggregate &, unsigned int numFeatsExp,
                        unsigned int mNumFeats, unsigned int mNumFeatsCont,
                        const std::vector<bool>& vIsCat);
        void readMatrix(const std::string &path);
        void rearrangeMatrix(const std::vector<bool> &vIsCat);
    protected :
        Eigen::MatrixXd mSigma;
        // R is  stored column-wise to exploit cache locality.
        // C is stored row-wise look at the line 112 -> cache locality in each thread important.
        // They have to be double instead of long double because
        // code runs 40% faster.
        //
        std::vector <double> mC;
        std::vector <double> mR;
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
        // Controls whether linearly independent columns are allowed.
        // When they are allowed this can lead to numerical instability.
        // Use it with caution!!!
        //
        const bool mIsLinDepAllowed = false;
        static constexpr double mcPrecisionError = 1e-12;

        void expandSigma(std::vector <double> &sigmaExpanded, bool isNaive);

    public:
        virtual void decompose(void) = 0;
        QRDecomposition(const std::string& path, const bool isLinDepAllowed=false) :
        mIsLinDepAllowed(isLinDepAllowed)
        {
            readMatrix(path);
        }
        QRDecomposition(const MapMatrixAggregate& mMatrix, unsigned int numFeatsExp,
                        unsigned int numFeats, unsigned int numFeatsCont,
                        const std::vector<bool>& vIsCat, const bool isLinDepAllowed=false):
        mIsLinDepAllowed(isLinDepAllowed)
        {
            formMatrix(mMatrix, numFeatsExp, numFeats, numFeatsCont, vIsCat);
        }
        virtual ~QRDecomposition() {}
        void getR(Eigen::MatrixXd &rEigen);
        friend std::ostream& operator<<(std::ostream& os, const QRDecomposition& qrDecomp);
    };

    class QRDecompositionNaive: public QRDecomposition
    {
    public:
        QRDecompositionNaive(const std::string &path, const bool isLinDepAllowed=false) :
            QRDecomposition(path, isLinDepAllowed) {}
        QRDecompositionNaive(const MapMatrixAggregate &mMatrix, unsigned int numFeatsExp,
                            unsigned int numFeats, unsigned int numFeatsCont,
                            const std::vector<bool>& vIsCat, const bool isLinDepAllowed=false) :
            QRDecomposition(mMatrix, numFeatsExp, numFeats, numFeatsCont, vIsCat, isLinDepAllowed) {}
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
        QRDecompositionSingleThreaded(const std::string &path, const bool isLinDepAllowed=false)
        : QRDecomposition(path, isLinDepAllowed) {}
        QRDecompositionSingleThreaded(const MapMatrixAggregate &mMatrix, unsigned int numFeatsExp,
                            unsigned int numFeats, unsigned int numFeatsCont,
                            const std::vector<bool>& vIsCat, const bool isLinDepAllowed=false) :
         QRDecomposition(mMatrix, numFeatsExp, numFeats, numFeatsCont, vIsCat, isLinDepAllowed) {}

        ~QRDecompositionSingleThreaded() {}
        virtual void decompose(void) override;
        void processCofactors(void);
        void calculateCR(void);
    };

    class QRDecompositionMultiThreaded: public QRDecomposition
    {
    // Sigma; Categorical cofactors as an ordered coordinate list
    std::vector<Triple> mCofactorList;

    // Phi; Categorical cofactors as list of lists
    std::vector<std::vector<Pair>> mCofactorPerFeature;

    const unsigned int mNumThreads = 8;
    boost::barrier mBarrier{mNumThreads};
    std::mutex mMutex;
    public:
        QRDecompositionMultiThreaded(const std::string &path, const bool isLinDepAllowed=false) :
        QRDecomposition(path, isLinDepAllowed) {}
        QRDecompositionMultiThreaded(const MapMatrixAggregate &mMatrix, unsigned int numFeatsExp,
                            unsigned int numFeats, unsigned int numFeatsCont,
                            const std::vector<bool>& vIsCat, const bool isLinDepAllowed=false) :
        QRDecomposition(mMatrix, numFeatsExp, numFeats, numFeatsCont, vIsCat, isLinDepAllowed) {}

        ~QRDecompositionMultiThreaded() {}
        virtual void decompose(void) override;
        void processCofactors(void);
        void calculateCR(unsigned int threadId);
    };
}

#endif
