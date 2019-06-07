#include <SVDecompositionApplication.h>

std::string SVDecompApp::getCodeOfDecomposition()
{
    std::string strDecompCode = offset(2) + "SVDecompQR svdDecomp(SVDecompQR::DecompType::MULTI_THREAD, mSigma,\n"+ offset(8) + "LA_NUM_FEATURES +  vDomainSize.back(), LA_NUM_FEATURES,\n"+ offset(8) + "LA_NUM_FEATURES - vCatFeatCount.back(), vIsCat);\n"+ ""+ offset(2) + "//auto finish = std::chrono::high_resolution_clock::now();\n" + offset(2) + "//std::chrono::duration<double> elapsed = finish - begin;\n" + offset(2) + "//double time_spent = elapsed.count();\n"+ offset(2) + "//std::cout << \"Time in preparation: \" << time_spent << std::endl;\n" + offset(2) + "svdDecomp.decompose();\n"+ offset(0);
    if (mOutputDecomp)
    {
        strDecompCode += offset(2) + "std::ofstream outFile(\"" + mDumpFile + "\");\n" + offset(2) + "outFile << svdDecomp;\n" + offset(2) + "outFile.close();\n";
    }
    return strDecompCode;
}
