#ifndef CLUSTERINGUTILITY_H
#define CLUSTERINGUTILITY_H

#include <matrix.h>
#include <random>
#include <algorithm>
#include <cfloat>

template <typename T>
class ClusteringUtility
{
  private:
    static const constexpr long double EPS = 1e-16;

  public:
    static T KMeans(Matrix<T> X, int K, vector<int> &clusters, Matrix<T> &centroids, bool resets = false);
    /// X is a matrix of X.numRows() input points in X.numCols() dimensions. 'clusters[i]' tells which
    /// cluster does i'th point belong to in range [0..K).  'centroids[i]' tells what's the centroid of the
    /// i'th data point.  Optionally, we can use 'resets' as a probabilitic optimisation.

    static T KMeansFixedK(Matrix<T> X, int rounds, int K, vector<int> &clusters, Matrix<T> &centroids);
    /// Compute the KMeans for a given K and a given number of rounds, outputs the results in 'clusters'
    /// and 'centroids'.

    static void KMeansKSelector(Matrix<T> X, vector<T> &lossWithK);
    /// Compute the KMeans for various K's and return a vector of losses. The shape of an "elbow" will help
    /// us decid what's the best number of clusters.
};


template <typename T>
void ClusteringUtility<T>::KMeansKSelector(Matrix<T> X, vector<T> &lossWithK)
{
    vector<int> clusters;
    Matrix<T> centroids;
    for (int k = 2; k <= min(10, X.numRows() / 2); ++k)
        lossWithK.emplace_back(KMeans(X, k, clusters, centroids, true));
}

template <typename T>
T ClusteringUtility<T>::KMeans(Matrix<T> X, int K, vector<int> &clusters, Matrix<T> &centroids, bool resets)
{
    return KMeansFixedK(X, 50 * resets + 1, K, clusters, centroids);
}

template <typename T>
T ClusteringUtility<T>::KMeansFixedK(Matrix<T> X, int rounds, int K, vector<int> &clusterIndices, Matrix<T> &centroids)
{
    int steps = 0;
    int converge = 0;
    T lastRound = DBL_MAX;
    vector<int> clusters;
    for (int round = 1; round <= rounds; ++ round) {
        vector<int> selector(X.numRows());
        for(int i = 0; i < X.numRows(); ++i)
            selector[i] = i;
        random_shuffle(selector.begin(), selector.end());
        Matrix<T> means(K, X.numCols());
        for (int i = 0; i < K; ++i)
            means.setRow(i, X[selector[i]]);

        ///cout << means << "\n";
        clusters.clear();
        clusters.resize(X.numRows());

        T last = DBL_MAX; /// needs to be updated depending on type T
        int conv = 0;
        while (true) {
            ++conv;
            T currentRound = 0;
            for (int i = 0; i < X.numRows(); ++i) {
                T currentPoint = DBL_MAX;
                int meanIndex = - 1;
                Matrix<T> point = X[i];
                for (int k = 0; k < K; ++k) {
                    Matrix<T> mean = means[k];
                    ///cout << "Distance between:\n" << point << mean << "is \n";
                    T dist = ((point - means[k]) * (point - means[k]).transpose()).get(0, 0);
                    ///cout << dist << "\n";
                    ++steps;
                    if (dist < currentPoint) {
                        currentPoint = dist;
                        meanIndex = k;
                    }
                }
                clusters[i] = meanIndex;
                currentRound += currentPoint;

            }

            if (fabs(currentRound - last) < 1e-16)
                break;
            last = currentRound;

            for (int k = 0; k < K; ++k) {
                Matrix<T> crtMean(1, X.numCols());
                int numberElements = 0;
                ///cout << "Points in cluster " << k << ":\n";
                for (int i = 0; i < X.numRows(); ++i)
                    if (clusters[i] == k) {
                        auto crt = X[i];
                        ///cout << crt;
                        crtMean = crtMean + X[i];
                        numberElements += 1;
                    }
                crtMean = crtMean / (1.0L*numberElements);
                Matrix<T> meanK = means[k];
                ///cout << "Old mean: " << meanK << "\n";
                ///cout << "New mean: " << crtMean << "\n";
                ///cout << "\n-----------------------------------------\n";
                means.setRow(k ,crtMean);
            }
        }
        converge = max(converge, conv);
        if (last < lastRound) {
            lastRound = last;
            centroids = means;
            clusterIndices = clusters;
        }
    }
    ///cout << "Steps: " << steps << endl;
    ///cout << "Convergence steps: " << converge << endl;
    return lastRound;
}

#endif // CLUSTERINGUTILITY_H
