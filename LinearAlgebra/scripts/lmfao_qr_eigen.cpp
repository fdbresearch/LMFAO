#include <boost/program_options.hpp>

#include <string>
#include <iostream>
#include <fstream>
#include <chrono>
#include <Eigen/Dense>


using namespace std;
using namespace boost::program_options;

int main(int argc, const char *argv[]) 
{
    string path;
    string decomposition;
    try
    {
        options_description desc{"Options"};
        desc.add_options()
          ("help,h", "Help screen")
          ("path,p", value<std::string>(&path), "Path")
          ("decomposition,d", value<std::string>(&decomposition), "Decomposition")
          ;

        variables_map vm;
        store(parse_command_line(argc, argv, desc), vm);
        notify(vm);

        if (vm.count("help"))
        {
            std::cout << desc << '\n';
        }
        if (vm.count("path"))
        {
            std::cout << "Path: " << path;
        }
        if (vm.count("decomposition"))
        {
          std::cout << "Decomposition: " << decomposition;
        }
    }
    catch (const error &ex)
    {
        std::cerr << ex.what() << '\n';
    }

    std::ifstream input(path);
    
    if (!input) {
        exit(1);
    }

    /* String to receive lines (ie. tuples) from the file. */
    string line;

    std::stringstream lineStream;
    std::string cell;
    unsigned int cntLines = 0;
    unsigned int cntFeat = 0;
    /* Scan through the tuples in the current table. */
    getline(input, line);
    lineStream << line;
    while (std::getline(lineStream, cell, '|'))
    {
        cntFeat++;
    }
    lineStream.clear();
    input.clear();
    input.seekg(0, ios::beg);
    
    while (getline(input, line))
    {
        cntLines ++;
    }
    input.clear();
    input.seekg(0, ios::beg);

    std::cout  << "Vals" << cntLines << " " << cntFeat << std::endl;
    
    Eigen::MatrixXd A = Eigen::MatrixXd::Zero(cntLines, cntFeat);
    cntLines = 0;
    while (getline(input, line))
    {
        lineStream << line;
        cntFeat = 0;
        while (std::getline(lineStream, cell, '|'))
        {
            double val = std::stod(cell);
            A(cntLines, cntFeat) = val;
            cntFeat ++;
        }

        lineStream.clear();   
        cntLines ++;
    }
    input.close();

    auto begin_timer = std::chrono::high_resolution_clock::now();
    if (decomposition.find("qr") != string::npos)
    {
        Eigen::HouseholderQR<Eigen::MatrixXd> qr(A.rows(), A.cols());
        qr.compute(A);
        std::cout << "QR";
    }
    else if (decomposition.find("svd") != string::npos)
    {
        Eigen::BDCSVD<Eigen::MatrixXd> svdR(A, Eigen::ComputeThinU | Eigen::ComputeThinV);
        std::cout << "SVD";
    }
    
    auto end_timer = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed_time = end_timer - begin_timer;
    double time_spent = elapsed_time.count();
    std::cout << "Time spent " << time_spent << std::endl;

}
