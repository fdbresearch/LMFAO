#include <Eigen/Dense>
#undef I
#include <boost/program_options.hpp>
#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <set>
#include <chrono>
#include <limits>
#include <iomanip>

using namespace boost::program_options;

#if defined(EIGEN_USE_BLAS)
#pragma message "Using EIGEN_USE_BLAS"
#pragma message "Konj"
#endif

#if defined(EIGEN_USE_LAPACKE)
#pragma message "Using EIGEN_USE_LAPACKE"
#endif

void getIsCatVector(std::vector<std::string> vFeats, std::vector<std::string> vCatFeats,
                    std::vector<bool>& vIsCat)
{
    vIsCat.resize(vFeats.size(), false);
    for (const auto& catFeat: vCatFeats)
    {
        auto it = std::find(vFeats.begin(), vFeats.end(), catFeat);
        if (it != vFeats.end())
        {
            unsigned int idx = std::distance(vFeats.begin(), it);
            vIsCat[idx] = true;
        }
    }
}

void getFeatures(const std::string& strFeats, const std::string& strCatFeats,
                 std::vector<std::string>& vFeats, std::vector<std::string>& vCatFeats)
{
    std::stringstream lineStream;
    std::string strFeat;

    lineStream << strFeats;
    /* In order to get number of features. */
    while (std::getline(lineStream, strFeat, ','))
    {
        vFeats.push_back(strFeat);
    }
    lineStream.clear();

    lineStream << strCatFeats;
    /* In order to get number of features. */
    while (std::getline(lineStream, strFeat, ','))
    {
        vCatFeats.push_back(strFeat);
    }
    lineStream.clear();
}

void getIdxAndCategories(const std::vector<std::string>& vFeats, const std::vector<std::string>& vCatFeats,
            std::set <unsigned int>& sIdxCatFeats,  std::map <unsigned int, std::set<unsigned int> >& mCntCatFeats)
{
    for (const auto& catFeat: vCatFeats)
    {
        auto it = std::find(vFeats.begin(), vFeats.end(), catFeat);
        if (it != vFeats.end())
        {
            unsigned int idx = std::distance(vFeats.begin(), it);
            sIdxCatFeats.insert(idx);
            mCntCatFeats[idx] = std::set<unsigned int>();
        }
    }
}

void getCntLinesAndFeatsAndCategoryCnt(const std::string& path, const std::vector<bool>& vIsCat,
                                        unsigned int& cntLines, unsigned int& cntFeat,
                                        std::map <unsigned int, std::set<unsigned int> >& mCntCatFeats)
{
    std::ifstream input(path);
    std::stringstream lineStream;
    std::string line;
    std::string cell;
    cntLines = 0;

    /* Scan through the tuples in the current table. */
    while (getline(input, line))
    {
        lineStream << line;
        cntFeat = 0;
        // Iterates column by column.
        //
        while (std::getline(lineStream, cell, '|'))
        {
            double val = std::stod(cell);
            if (vIsCat[cntFeat])
            {
                unsigned int valU = std::stoul(cell);
                mCntCatFeats[cntFeat].insert(valU);
            }
            cntFeat ++;
        }

        lineStream.clear();
        cntLines ++;
    }
    input.close();
}

unsigned int getMatrixColsNum(unsigned int featNum, const std::map <unsigned int, std::set<unsigned int> >& mCntCatFeats)
{
    unsigned int colNum = featNum;
    for (const auto & it: mCntCatFeats)
    {
        // Parenthesis are precaution measure. -2 for dummy var
        // and for already included categorical variable.
        //
        colNum  += (static_cast<int>(it.second.size()) - 2);
        std::cout << it.first << " "  << it.second.size() << std::endl;
    }
    std::cout << colNum << std::endl;
    return colNum;

}

// This function should calculate col_idx for columns when they are ohe
void getShifts(const std::vector<bool>& vIsCat,
               const std::map <unsigned int, std::set<unsigned int> >& mCntCatFeats,
               std::vector<unsigned int>& vShifts)
{
    vShifts.resize(vIsCat.size(), false);
    vShifts[0] = 0;
    for (unsigned int idx = 1; idx < vIsCat.size(); idx ++)
    {
        if (vIsCat[idx-1])
        {
            vShifts[idx] = vShifts[idx - 1] + mCntCatFeats.at(idx-1).size() - 1;
        }
        else
        {
            vShifts[idx] = vShifts[idx - 1] + 1;
        }
        //std::cout << "V " << vIsCat[idx] << " " << vShifts[idx] << std::endl;
    }
}


void getMatrix(const std::string& path, const std::vector<bool>& vIsCat,
               const std::map <unsigned int, std::set<unsigned int> >& mCntCatFeats,
               const std::vector<unsigned int>& vShifts,
               Eigen::MatrixXd& A)
{
    std::ifstream input(path);
    std::stringstream lineStream;
    std::string line;
    std::string cell;
    unsigned int cntLines = 0;
    unsigned int cntFeat = 0;

    while (getline(input, line))
    {
        lineStream << line;
        cntFeat = 0;
        // Iterates column by column.
        //
        while (std::getline(lineStream, cell, '|'))
        {
            double val = std::stod(cell);
            // Categorical variable.
            //
            //std::cout << "Val is: " << val << std::endl;
            if (vIsCat[cntFeat])
            {
                unsigned int valU = std::stoul(cell);
                unsigned int inShift = std::distance(mCntCatFeats.at(cntFeat).begin(),
                                                      mCntCatFeats.at(cntFeat).find(valU));
                //std::cout << "InShfit: " << inShift << std::endl;
                if (inShift != 0)
                {
                    // We subtract 1 from the expression because of dummy variable.
                    unsigned int featIdx = vShifts[cntFeat] + inShift - 1;
                    //std::cout << "featIdx: " << featIdx << std::endl;
                    A(cntLines, featIdx) = 1;
                }
            }
            else
            {
                A(cntLines, cntFeat) = val;
            }
            cntFeat ++;
        }
        //for (unsigned int idx = 0; idx < cntCatN; idx++)
        //{
        //    std:: cout << "Stored" << A(0, idx) << std::endl;
        //}

        lineStream.clear();
        cntLines ++;
    }
    input.close();
}

void dumpQR(std::ostream& out, const Eigen::MatrixXd& R)
{
    Eigen::MatrixXd RPositive(R.cols(), R.cols());
    Eigen::VectorXd rDiagonal = R.diagonal();

    for (unsigned int idx = 0; idx < rDiagonal.rows(); idx++)
    {
        rDiagonal[idx] = (rDiagonal[idx] > 0) ? 1 : -1;
    }
    Eigen::DiagonalMatrix<double, Eigen::Dynamic> diagonalM(rDiagonal);
    std::cout << diagonalM.rows() << " " << diagonalM.cols() << std::endl;

    for (unsigned int row = 0; row < R.cols(); row++)
    {
        for (unsigned int col = 0; col < R.cols(); col++)
        {
            RPositive(row, col) = R(row, col);
        }
    }
    RPositive = diagonalM * RPositive;

    out << RPositive.rows() << " " << RPositive.cols() << std::endl;
    out << std::fixed << std::setprecision(std::numeric_limits<double>::digits10 + 1);
    for (unsigned int row = 0; row < RPositive.cols(); row++)
    {
        for (unsigned int col = 0; col < RPositive.cols(); col++)
        {
            out << RPositive(row, col) << " ";
        }
        out << std::endl;
    }
}

void dumpSVD(std::ostream& out, const Eigen::BDCSVD<Eigen::MatrixXd>& svdR)
{
    out << std::fixed << std::setprecision(std::numeric_limits<double>::digits10 + 1);
    out << svdR.singularValues().rows() << std::endl;
    for (unsigned int row = 0; row < svdR.singularValues().rows(); row++)
    {
        out << svdR.singularValues()(row) << std::endl;
    }
}

int main(int argc, const char *argv[])
{
    // Arguments passed from the command line.
    //
    std::string path;
    std::string operation;
    std::string strFeats;
    std::string strCatFeats;
    bool dump = false;
    std::string dumpPath;
    std::ofstream out;

    unsigned int cntLines = 0;
    unsigned int cntFeat = 0;
    // Store feats names.
    //
    std::vector<std::string> vFeats;
    std::vector<std::string> vCatFeats;
    std::vector<bool> vIsCat;
    int numIt;

    // Stores idx of categorical columns regarding input data.
    //
    std::set <unsigned int> sIdxCatFeats;
    // Stores for each categorical column a set of distinct categories.
    //
    std::map <unsigned int, std::set<unsigned int> > mCntCatFeats;

    std::vector <unsigned int> vShifts;
    try
    {
        options_description desc{"Options"};
        desc.add_options()
          ("help,h", "Help screen")
          ("path,p", value<std::string>(&path), "Path")
          ("operation,o", value<std::string>(&operation), "Operation")
          ("features,f", value<std::string>(&strFeats), "Features")
          ("categorical_features,c", value<std::string>(&strCatFeats), "Categorical features")
          ("dump", value<bool>(&dump), "Dump")
          ("dump_path", value<std::string>(&dumpPath), "Dump path")
          ("num_it,n", value<int>(&numIt), "Number of iterations")
          ;

        variables_map vm;
        store(parse_command_line(argc, argv, desc), vm);
        notify(vm);

        if (vm.count("help"))
        {
            std::cout << desc << std::endl;
        }
        if (vm.count("dump"))
        {
            dump = true;
        }
        if (vm.count("path"))
        {
            std::cout << "Path: " << path << std::endl;
        }
        if (vm.count("operation"))
        {
          std::cout << "Decomposition: " << operation << std::endl;
        }
        std::cout << "Dump val" << dump << std::endl;
        std::cout << "Dump path" << dumpPath << std::endl;

    }
    catch (const error &ex)
    {
        std::cerr << ex.what() << '\n';
    }

    getFeatures(strFeats, strCatFeats, vFeats, vCatFeats);
    getIsCatVector(vFeats, vCatFeats, vIsCat);
    getIdxAndCategories(vFeats, vCatFeats, sIdxCatFeats, mCntCatFeats);

    getCntLinesAndFeatsAndCategoryCnt(path, vIsCat, cntLines, cntFeat, mCntCatFeats);
    unsigned int cntCatN = getMatrixColsNum(cntFeat, mCntCatFeats);
    Eigen::MatrixXd A = Eigen::MatrixXd::Zero(cntLines, cntCatN);
    getShifts(vIsCat, mCntCatFeats, vShifts);
    getMatrix(path, vIsCat, mCntCatFeats, vShifts, A);

    for (unsigned int it = 0; it < numIt; it ++)
    {
        if (dump)
        {
            out.open(dumpPath);
        }
        auto begin_timer = std::chrono::high_resolution_clock::now();

        if (operation.find("qr") != std::string::npos)
        {
            Eigen::HouseholderQR<Eigen::MatrixXd> qr(A);
            if (dump)
            {
                //mat.triangularView<Lower>()
                Eigen::MatrixXd R = qr.matrixQR().triangularView<Eigen::Upper>();
                dumpQR(out, R);
            }
        }
        else if (operation.find("svd") != std::string::npos)
        {
            Eigen::BDCSVD<Eigen::MatrixXd> svdR(A, Eigen::ComputeThinV);
            if (dump)
            {
                dumpSVD(out, svdR);
            }
        }
        if (dump)
        {
            out.close();
        }

        auto end_timer = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed_time = end_timer - begin_timer;
        double time_spent = elapsed_time.count();
        std::cout << "##LMFAO## LinearAlgebra ## " << time_spent << std::endl;
    }
}
