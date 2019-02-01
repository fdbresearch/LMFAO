#include <fstream>
#include <iostream>
#include "QRDecomposition.h"

using namespace std;

namespace LMFAO::LinearAlgebra
{

    void QRDecomposition::readMatrix(const std::string& path)
    {
        std::ifstream f(path);
        unsigned int dim, row, col;
        bool isCategorical; float val;
        f >> dim;
        mSigma = Eigen::MatrixXd::Zero(dim, dim);
        MatrixBool mIsCategorical = MatrixBool::Constant(dim, dim, false);

        mNumFeatsExp = dim;
        mNumFeats = 36;
        mNumFeatsCont = 32;
        mNumFeatsCat = mNumFeats - mNumFeatsCont;

        while (f >> row >> col >> val >> isCategorical) 
        {
            std::cout << row << " " <<  col << " " << val << " " 
                      << isCategorical << std::endl;
            mSigma(row, col) = val;
            mIsCategorical(row, col) = isCategorical;
        }
        
        vector <unsigned int> dispCont(mNumFeatsExp, 0);
        vector<unsigned int> dispCat(mNumFeatsExp, 0);
        for (unsigned int idx = 1; idx < mNumFeatsExp; idx ++)
        {
            dispCont[idx] = dispCont[idx - 1] +  mIsCategorical(idx - 1, 0);
            dispCat[idx] = dispCat[idx - 1] + !mIsCategorical(idx - 1, 0);
        }

        
        for (unsigned int row = 0; row < mNumFeatsExp; row ++)
        {
            for (unsigned int col = 0; col < mNumFeatsExp; col++)
            {
                unsigned int row_idx = mIsCategorical(row, 0) ? 
                (row - dispCat[row] + mNumFeatsCont) : (row - dispCont[row]);
                unsigned int col_idx = mIsCategorical(col, 0) ? 
                (col - dispCat[col] + mNumFeatsCont) : (col - dispCont[col]);
                if (mIsCategorical(row, col)  && (mSigma(row, col) != 0))
                {
                    mCatVals.push_back(make_tuple(row_idx, col_idx, mSigma(row, col)));
                }
                mSigma(row - dispCont[row], col - dispCont[col]) = mSigma(row, col);
            }
        }
        for (const Triple& triple: mCatVals)
        {
            unsigned int row = get<0>(triple);
            unsigned int col = get<1>(triple);
            long double  aggregate = get<2>(triple);
            mSigma(row, col) = aggregate;
        }

        for (unsigned int row = 0; row < mNumFeatsExp; row ++)
        {
            for (unsigned int col = 0; col < mNumFeatsExp; col++)
            {
                cout << row << " " << col << " " << mSigma(row, col) << endl; 
            }
        }
    } 

    void QRDecomposition::expandSigma(vector <long double> &sigmaExpanded, bool isNaive) 
    {
        unsigned int num_rows = isNaive ?  mNumFeatsExp : mNumFeatsCont; 
        for (unsigned int row = 0; row < num_rows; row ++)
        {
            for (unsigned int col = 0; col < num_rows; col ++)
            {
                sigmaExpanded[expIdx(row, col, num_rows)] = mSigma(row, col);
            }
        }
    }
}
