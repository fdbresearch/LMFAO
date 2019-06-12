#include <QRDecompositionApplication.h>
#include <GlobalParams.hpp>

using multifaq::params::NUM_OF_VARIABLES;

std::string QRDecompApp::getCodeOfIncludes()
{
    std::string includeStr = "";
    includeStr += ""+ offset(0) + "#include \"../../../LA-F/include/QRDecomp.h\"\n"+ ""+ offset(0) + "#include \"../../../LA-F/include/SVDecomp.h\"\n" + offset(0) + "#include <map>\n"+ offset(0) + "#include <set>\n"+ offset(0) + "#include <unordered_map>\n"+ offset(0) + "\n"+ "#include <iostream>\n" + "#include <fstream>\n"+  offset(0) + "typedef std::map< std::tuple<uint32_t, double, double>, double> DomainAggregate;\n"+ offset(0) + "typedef std::vector<DomainAggregate> VectorOffset;\n"+ offset(0) + "typedef std::map<uint32_t, VectorOffset> MapView;\n"+ offset(0) + "\n"+ offset(0) + "typedef std::set<uint32_t> MapCategory;\n"+ offset(0) + "typedef std::vector<MapCategory> VectorDomain;\n"+ offset(0) + "\n"+ offset(0) + "typedef std::vector<std::unordered_map<uint32_t, uint32_t> > VectorShift;\n"+ offset(0) + "\n"+ offset(0) + "using namespace LMFAO::LinearAlgebra;\n";
    return includeStr;
}

std::string QRDecompApp::getCodeOfFunDefinitions()
{
    uint32_t numOfFeatures = _listOfFeatures.size() + 1;
    std::string numOfFeaturesStr = std::to_string(numOfFeatures);
    std::string functionsString = ""+ offset(1) + "LMFAO::LinearAlgebra::MapMatrixAggregate mSigma;\n"+ offset(1) + "constexpr uint32_t LA_NUM_FEATURES = " + numOfFeaturesStr + ";\n"+ offset(0) + "\n"+ offset(1) + "uint32_t getTriangularIdx(uint32_t row, uint32_t col,\n"+ offset(11) + "const std::vector<bool> &vIsCatFeature,\n"+ offset(11) + "const std::vector<uint32_t> &vCatFeatCount)\n"+ offset(1) + "{\n"+ offset(2) + "uint32_t idxCur, idx, offset, offsetCat;\n"+ offset(2) + "if ((row == 0) && (col == 0))\n"+ offset(2) + "{\n"+ offset(3) + "return 0;\n"+ offset(2) + "}\n"+ offset(2) + "//offsetCat = vCatFeatCount[row + 1];\n"+ offset(2) + "if (row == 0)\n"+ offset(2) + "{\n"+ offset(3) + "row = col - 1;\n"+ offset(3) + "col = row;\n"+ offset(3) + "offsetCat = vCatFeatCount[row + 1] - vIsCatFeature[row + 1];\n"+ offset(2) + "}\n"+ offset(2) + "else\n"+ offset(2) + "{\n"+ offset(3) + "row = row - 1;\n"+ offset(3) + "offsetCat = vCatFeatCount[row + 1];\n"+ offset(2) + "}\n"+ offset(2) + "uint32_t n = LA_NUM_FEATURES;\n"+ offset(2) + "idxCur = n - row;\n"+ offset(2) + "idx = n * (n + 1) / 2 - idxCur * (idxCur + 1) / 2;\n"+ offset(2) + "//std :: cout << \"idx \" << idx << \" row \" << row << \" col \" << col << std::endl;\n"+ offset(2) + "offset = col - row - offsetCat;\n"+ offset(2) + "return idx + offset + 1;\n"+ offset(1) + "}\n"+ offset(0) + "\n"+ offset(1) + "const DomainAggregate &getCellValueUpper(uint32_t row, uint32_t col,\n"+ offset(14) + "const MapView &mViews,\n"+ offset(14) + "const std::vector<bool> &vIsCatFeature,\n"+ offset(14) + "const std::vector<uint32_t> &vCatFeatCount)\n"+ offset(1) + "{\n"+ offset(2) + "uint32_t idx = getTriangularIdx(row, col, vIsCatFeature, vCatFeatCount);\n"+ offset(2) + "//std::cout << \"idx\" << idx << std::endl;\n"+ offset(2) + "const auto &viewOffset = vectorCofactorViews[idx];\n"+ offset(2) + "//std::cout << \"v \" << viewOffset.first << \" o \" << viewOffset.second << std::endl;\n"+ offset(2) + "return mViews.at(viewOffset.first)[viewOffset.second];\n"+ offset(1) + "}\n"+ offset(0) + "\n"+ offset(1) + "const DomainAggregate &getCellValue(uint32_t row, uint32_t col,\n"+ offset(13) + "const MapView &mViews,\n"+ offset(13) + "const std::vector<bool> &vIsCatFeature,\n"+ offset(13) + "const std::vector<uint32_t> &vCatFeatCount)\n"+ offset(1) + "{\n"+ offset(2) + "if (row > col)\n"+ offset(2) + "{\n"+ offset(3) + "std::swap(col, row);\n"+ offset(2) + "}\n"+ offset(2) + "return getCellValueUpper(row, col, mViews, vIsCatFeature, vCatFeatCount);\n"+ offset(1) + "}\n"+ offset(0) + "\n"+ offset(1) + "uint32_t getCatIdx(uint32_t featureIdx, uint32_t category,\n"+ offset(7) + "const std::vector<uint32_t>& vDomainSize,\n"+ offset(7) + "const VectorShift& vCatShifts)\n"+ offset(1) + "{\n"+ offset(2) + "return vDomainSize[featureIdx] + vCatShifts[featureIdx].at(category) - 1;\n"+ offset(1) + "}\n"+ offset(0) + "\n"+ offset(1) + "unsigned isFirstCategory(uint32_t featureIdx, uint32_t category,\n"+ offset(9) + "const VectorShift& vCatShifts)\n"+ offset(1) + "{\n"+ offset(2) + "return vCatShifts[featureIdx].at(category) == 0;\n"+ offset(1) + "}\n"+ offset(0) + "\n"+ offset(1) + "void inline addEntryToSigma(uint32_t row, uint32_t col,\n"+ offset(8) + "double aggregate)\n"+ offset(1) + "{\n"+ offset(2) + "/*std::cout << row << \" \" <<  col << \" \" << std::fixed\n"+ offset(5) + "<< aggregate  <<  \" \" <<  std::endl;*/\n"+ offset(2) + "mSigma[std::make_pair(row, col)] = aggregate;\n"+ offset(1) + "}\n"+ offset(0) + "\n"+ offset(1) + "void printCell(uint32_t row, uint32_t col,\n"+ offset(6) + "const DomainAggregate& aggregate, char cellType,\n"+ offset(6) + "const std::vector<uint32_t>& vDomainSize,\n"+ offset(6) + "const VectorShift& vCatShifts)\n"+ offset(1) + "{\n"+ offset(2) + "uint32_t category, catIdx, outRow, outCol,\n"+ offset(6) + "catIdxRow, catIdxCol, categoryRow, categoryCol;\n"+ offset(2) + "double outVal;\n"+ offset(2) + "switch (cellType)\n"+ offset(2) + "{\n"+ offset(3) + "case 'c':\n"+ offset(4) + "outRow = row + vDomainSize[row];\n"+ offset(4) + "outCol = col + vDomainSize[col];\n"+ offset(4) + "outVal = aggregate.begin()->second;\n"+ offset(4) + "addEntryToSigma(outRow, outCol, outVal);\n"+ offset(4) + "break;\n"+ offset(3) + "case 'r':\n"+ offset(4) + "for (const auto& it: aggregate)\n"+ offset(4) + "{\n"+ offset(6) + "category = static_cast<uint32_t>(std::get<1>(it.first));\n"+ offset(6) + "if (isFirstCategory(col, category, vCatShifts))\n"+ offset(6) + "{\n"+ offset(7) + "continue;\n"+ offset(6) + "}\n"+ offset(6) + "catIdx = getCatIdx(col, category, vDomainSize, vCatShifts);\n"+ offset(6) + "outRow = row + vDomainSize[row];\n"+ offset(6) + "outCol = col + catIdx;\n"+ offset(6) + "outVal = it.second;\n"+ offset(6) + "addEntryToSigma(outRow, outCol, outVal);\n"+ offset(4) + "}\n"+ offset(4) + "break;\n"+ offset(3) + "case 'k':\n"+ offset(4) + "for (const auto& it: aggregate)\n"+ offset(4) + "{\n"+ offset(6) + "category = static_cast<uint32_t>(std::get<1>(it.first));\n"+ offset(6) + "if (isFirstCategory(row, category, vCatShifts))\n"+ offset(6) + "{\n"+ offset(7) + "continue;\n"+ offset(6) + "}\n"+ offset(6) + "catIdx = getCatIdx(row, category, vDomainSize, vCatShifts);\n"+ offset(6) + "outRow = row + catIdx;\n"+ offset(6) + "outCol = col + vDomainSize[col];\n"+ offset(6) + "outVal = it.second;\n"+ offset(6) + "addEntryToSigma(outRow, outCol, outVal);\n"+ offset(4) + "}\n"+ offset(4) + "break;\n"+ offset(3) + "case 'd':\n"+ offset(4) + "for (const auto& it: aggregate)\n"+ offset(4) + "{\n"+ offset(6) + "category = static_cast<uint32_t>(std::get<1>(it.first));\n"+ offset(6) + "if (isFirstCategory(row, category, vCatShifts))\n"+ offset(6) + "{\n"+ offset(7) + "continue;\n"+ offset(6) + "}\n"+ offset(6) + "catIdx = getCatIdx(row, category, vDomainSize, vCatShifts);\n"+ offset(6) + "outRow = row + catIdx;\n"+ offset(6) + "outCol = col + catIdx;\n"+ offset(6) + "outVal = it.second;\n"+ offset(6) + "addEntryToSigma(outRow, outCol, outVal);\n"+ offset(4) + "}\n"+ offset(4) + "break;\n"+ offset(3) + "case 'm':\n"+ offset(4) + "for (const auto& it: aggregate)\n"+ offset(4) + "{\n"+ offset(6) + "categoryRow = static_cast<uint32_t>(std::get<1>(it.first));\n"+ offset(6) + "categoryCol = static_cast<uint32_t>(std::get<2>(it.first));\n"+ offset(6) + "if (row > col)\n"+ offset(6) + "{\n"+ offset(8) + "std::swap(categoryRow, categoryCol);\n"+ offset(6) + "}\n"+ offset(6) + "if (isFirstCategory(row, categoryRow, vCatShifts) ||\n"+ offset(8) + "isFirstCategory(col, categoryCol, vCatShifts))\n"+ offset(6) + "{\n"+ offset(7) + "continue;\n"+ offset(6) + "}\n"+ offset(0) + "\n"+ offset(6) + "catIdxRow = getCatIdx(row, categoryRow, vDomainSize, vCatShifts);\n"+ offset(6) + "catIdxCol = getCatIdx(col, categoryCol, vDomainSize, vCatShifts);\n"+ offset(6) + "outRow = row + catIdxRow;\n"+ offset(6) + "outCol = col + catIdxCol;\n"+ offset(6) + "outVal = it.second;\n"+ offset(6) + "addEntryToSigma(outRow, outCol, outVal);\n"+ offset(4) + "}\n"+ offset(4) + "break;\n"+ offset(2) + "}\n"+ offset(1) + "}\n"+ offset(0) + "\n"+ offset(1) + "void calculateCatFeatCount(const std::vector<bool> &vIsCatFeature,\n"+ offset(10) + "std::vector<uint32_t> &vCatFeatCount)\n"+ offset(1) + "{\n"+ offset(2) + "vCatFeatCount.resize(vIsCatFeature.size());\n"+ offset(2) + "vCatFeatCount[0] = 0;\n"+ offset(2) + "for (uint32_t idx = 1; idx < vIsCatFeature.size(); idx ++)\n"+ offset(2) + "{\n"+ offset(4) + "vCatFeatCount[idx] = vCatFeatCount[idx - 1] + vIsCatFeature[idx];\n"+ offset(2) + "}\n"+ offset(1) + "}\n"+ offset(0) + "\n"+ offset(1) + "void calculateDomains(VectorDomain &vDomains,\n"+ offset(8) + "const MapView& mViews,\n"+ offset(8) + "const std::vector<bool> &vIsCatFeature,\n"+ offset(8) + "const std::vector<uint32_t> &vCatFeatCount)\n"+ offset(1) + "{\n"+ offset(2) + "const uint32_t n = vIsCatFeature.size();\n"+ offset(2) + "for (uint32_t idx = 0; idx < n; idx ++)\n"+ offset(2) + "{\n"+ offset(4) + "vDomains.push_back(std::set<uint32_t>());\n"+ offset(2) + "}\n"+ offset(2) + "for (uint32_t row = 0; row < n; row ++)\n"+ offset(2) + "{\n"+ offset(3) + "if (vIsCatFeature[row])\n"+ offset(3) + "{\n"+ offset(4) + "const DomainAggregate &m = getCellValue(row, row, mViews, vIsCatFeature, vCatFeatCount);\n"+ offset(4) + "for (const auto& it: m)\n"+ offset(4) + "{\n"+ offset(5) + "vDomains[row].insert(std::get<1>(it.first));\n"+ offset(4) + "}\n"+ offset(3) + "}\n"+ offset(2) + "}\n"+ offset(1) + "}\n"+ offset(0) + "\n"+ offset(0) + "\n"+ offset(1) + "void getDomainsSize(std::vector<uint32_t>& vDomainSize,\n"+ offset(8) + "const std::vector<bool> &vIsCatFeature,\n"+ offset(8) + "const VectorDomain &vDomains)\n"+ offset(1) + "{\n"+ offset(2) + "const uint32_t n = vIsCatFeature.size() + 1;\n"+ offset(2) + "vDomainSize.resize(n);\n"+ offset(2) + "vDomainSize[0] = 0;\n"+ offset(0) + "\n"+ offset(2) + "for (uint32_t idx = 1; idx < n; idx ++)\n"+ offset(2) + "{\n"+ offset(4) + "vDomainSize[idx] = vDomainSize[idx - 1] + vDomains[idx-1].size()\n"+ offset(10) + "- 2 * vIsCatFeature[idx-1];\n"+ offset(2) + "}\n"+ offset(1) + "}\n"+ offset(0) + "\n"+ offset(1) + "void getCatShifts(VectorShift& vCatShifts,\n"+ offset(7) + "const std::vector<bool> &vIsCatFeature,\n"+ offset(7) + "const VectorDomain &vDomains)\n"+ offset(1) + "{\n"+ offset(2) + "const uint32_t n = vIsCatFeature.size();\n"+ offset(2) + "vCatShifts.resize(n);\n"+ offset(2) + "for (uint32_t idx = 1; idx < n; idx ++)\n"+ offset(2) + "{\n"+ offset(4) + "const MapCategory& mCategories = vDomains[idx];\n"+ offset(4) + "uint32_t shiftIdx = 0;\n"+ offset(4) + "for (const auto& it: mCategories)\n"+ offset(4) + "{\n"+ offset(5) + "vCatShifts[idx][it] = shiftIdx;\n"+ offset(5) + "shiftIdx++;\n"+ offset(4) + "}\n"+ offset(2) + "}\n"+ offset(1) +"}\n";
    return offset(1) + "std::vector< std::pair<uint32_t, uint32_t> > vectorCofactorViews;\n"
            + functionsString;
}

inline std::string getViewName(size_t viewID)
{
    return "V" + std::to_string(viewID);
}

std::string QRDecompApp::getVectorOfFeatures()
{
    std::string returnString = "";
    returnString += ""+ offset(2) + "//auto begin = std::chrono::high_resolution_clock::now();\n" + offset(2) + "MapView mViews;\n"+ offset(2) + "VectorDomain vDomains;\n"+ offset(2) + "std::vector<uint32_t> vDomainSize;\n"+ offset(2) + "VectorShift vCatShifts;\n"+ offset(2) + "std::vector<bool> vIsCatFeature;\n"+ offset(2) + "std::vector<uint32_t> vCatFeatCount;\n\n";

    // Intercept feature.
    returnString +=  offset(2) + "vIsCatFeature.push_back(false);\n";
    for (size_t var = 0; var < NUM_OF_VARIABLES; var ++)
    {
        if (_features[var])
        {
            Attribute *pFeature = _td->getAttribute(var);
            std::string isCatStr = _categoricalFeatures.test(var) ? "true" : "false";
            returnString +=
                offset(2) + "vIsCatFeature.push_back(" + isCatStr
                + ");\n";

        }
    }
    returnString += offset(2) + "calculateCatFeatCount(vIsCatFeature, vCatFeatCount);\n\n";
    return returnString;
}

std::string QRDecompApp::getCodeOfDecomposition()
{
    std::string strUseLinearDependencyCheck = mUseLinearDependencyCheck ? "true" : "false";
    std::cout << "linDep "  << strUseLinearDependencyCheck << std::endl;
    std::cout << "outputDecomp "  << mOutputDecomp << std::endl;
    std::cout << "dumpFile "  << mDumpFile << std::endl;
    std::string strDecompCode;
    switch (mQrType)
    {
        case QrType::QR_MULTI_THREAD:
            strDecompCode = ""+ offset(0) + "\n"+ offset(2) + "QRDecompMultiThread qrDecomp(mSigma, LA_NUM_FEATURES +  vDomainSize.back(), LA_NUM_FEATURES,\n"+ offset(15) + "LA_NUM_FEATURES - vCatFeatCount.back(), vIsCat, "+ strUseLinearDependencyCheck + ");\n" + offset(2) + "//auto finish = std::chrono::high_resolution_clock::now();\n"+ offset(2) + "//std::chrono::duration<double> elapsed = finish - begin;\n"+ offset(2) + "//double time_spent = elapsed.count();\n"+ offset(2) + "//std::cout << \"Time in preparation: \" << time_spent << std::endl;\n"+ offset(2) + "qrDecomp.decompose();\n";
            break;
        case QrType::QR_CHOL:
            strDecompCode = ""+ offset(0) + "\n"+ offset(2) + "QRDecompCholesky qrDecomp(mSigma, LA_NUM_FEATURES +  vDomainSize.back(), LA_NUM_FEATURES,\n"+ offset(15) + "LA_NUM_FEATURES - vCatFeatCount.back(), vIsCat, "+ strUseLinearDependencyCheck + ");\n" + offset(2) + "//auto finish = std::chrono::high_resolution_clock::now();\n"+ offset(2) + "//std::chrono::duration<double> elapsed = finish - begin;\n"+ offset(2) + "//double time_spent = elapsed.count();\n"+ offset(2) + "//std::cout << \"Time in preparation: \" << time_spent << std::endl;\n"+ offset(2) + "qrDecomp.decompose();\n";
            break;
    }
    if (mOutputDecomp)
    {
        strDecompCode += offset(2) + "std::ofstream outFile(\"" + mDumpFile + "\");\n" + offset(2) + "outFile << qrDecomp;\n" + offset(2) + "outFile.close();\n";
    }
    return strDecompCode;
    //return ""+ offset(0) + "\n"+ offset(2) + "QRDecompNaive qrDecomp(mSigma, LA_NUM_FEATURES +  vDomainSize.back(), LA_NUM_FEATURES,\n"+ offset(15) + "LA_NUM_FEATURES - vCatFeatCount.back(), vIsCat);\n" + offset(2) + "auto finish = std::chrono::high_resolution_clock::now();\n"+ offset(2) + "std::chrono::duration<double> elapsed = finish - begin;\n"+ offset(2) + "double time_spent = elapsed.count();\n"+ offset(2) + "std::cout << \"Time in preparation: \" << time_spent << std::endl;\n"+ offset(2) + "qrDecomp.decompose();\n";
}

std::string QRDecompApp::getCodeOfRunAppFun()
{
    std::string vectorCofactorViews = "";

    for (Query* query : listOfQueries)
    {
        std::pair<size_t,size_t>& viewAggPair = query->_aggregates[0]->_incoming[0];
        vectorCofactorViews +=
        offset(2) + "vectorCofactorViews.push_back(std::make_pair("
        + std::to_string(viewAggPair.first) + "," +
        std::to_string(viewAggPair.second) + "));\n";
    }

    std::string returnString = "";

    for (size_t viewID = 0; viewID < _compiler->numberOfViews(); ++viewID)
    {
        View* view = _compiler->getView(viewID);

        if (view->_origin != view->_destination)
            continue;
        std::string dimCountSize = std::to_string(view->_fVars.count());
        std::string viewCountSize = std::to_string(view->_aggregates.size());
        std::string tupleType =  "const " +  getViewName(viewID) +"_tuple& tuple";

        std::string fieldsForMap = dimCountSize + ",";
        for (size_t var = 0; var < multifaq::params::NUM_OF_VARIABLES; ++var)
        {
            if (view->_fVars[var])
            {
                Attribute* attr = _td->getAttribute(var);
                fieldsForMap += "tuple." + attr->_name + ",";
            }
        }
        for (int i = view->_fVars.count(); i < 2; i ++)
        {
            fieldsForMap += "0,";
        }
        // Pops back the last ,
        fieldsForMap.pop_back();

        std::string calcViews = "" +
            offset(3) + "VectorOffset vOffsets;\n" +
            offset(3) + "for (size_t offset=0; offset <" +
            viewCountSize + "; ++offset)\n"
            + offset(3) + "{\n"
            + offset(4) + "DomainAggregate mDomAgg;\n"
            + offset(4) + "for (size_t i=0; i <" + getViewName(viewID) + ".size(); ++i)\n"
            + offset(4) + "{\n"
            + offset(5) + tupleType + "=" + getViewName(viewID) +"[i];\n"
            + offset(5) + "mDomAgg[std::make_tuple("+
                        fieldsForMap + ")]" + "= tuple.aggregates[offset];\n"
            + offset(4) + "}\n"
            + offset(4) + "vOffsets.push_back(mDomAgg);\n"
            + offset(3) + "}\n"
            + offset(3) + "mViews[" + getViewName(viewID).substr(1) + "] = vOffsets;\n";

        returnString += offset(2) + "{\n" + calcViews +
                        offset(2) + "}\n";
    }
    std::string decompose = getCodeOfDecomposition();
    std::string calculateDomains = ""+ offset(2) + "calculateDomains(vDomains, mViews, vIsCatFeature, vCatFeatCount);\n"+ offset(2) + "getDomainsSize(vDomainSize, vIsCatFeature, vDomains);\n"+ offset(2) + "getCatShifts(vCatShifts, vIsCatFeature, vDomains);\n\n";
    std::string generateQR = ""+ offset(0) + "for (uint32_t row = 0; row < LA_NUM_FEATURES; row++)\n"+ offset(2) + "{\n"+ offset(3) + "for (uint32_t col = 0; col < LA_NUM_FEATURES; col++)\n"+ offset(3) + "{\n"+ offset(4) + "const auto &m = getCellValue(row, col, mViews, vIsCatFeature, vCatFeatCount);\n"+ offset(4) + "char cellType;\n"+ offset(4) + "if (!vIsCatFeature[row] && !vIsCatFeature[col])\n"+ offset(4) + "{\n"+ offset(6) + "cellType = 'c';\n"+ offset(4) + "}\n"+ offset(4) + "else if (vIsCatFeature[row] && !vIsCatFeature[col])\n"+ offset(4) + "{\n"+ offset(6) + "cellType = 'k';\n"+ offset(4) + "}\n"+ offset(4) + "else if (!vIsCatFeature[row] && vIsCatFeature[col])\n"+ offset(4) + "{\n"+ offset(6) + "cellType = 'r';\n"+ offset(4) + "}\n"+ offset(4) + "else if (vIsCatFeature[row] &&  vIsCatFeature[col]\n"+ offset(6) + "&& (row == col))\n"+ offset(4) + "{\n"+ offset(6) + "cellType = 'd';\n"+ offset(4) + "}\n"+ offset(4) + "else\n"+ offset(4) + "{\n"+ offset(6) + "cellType = 'm';\n"+ offset(4) + "}\n"+ offset(4) + "printCell(row, col, m, cellType, vDomainSize, vCatShifts);\n"+ offset(3) + "}\n"+ offset(2) + "}\n"+ offset(2) + "std::vector<bool> vIsCat;\n"+ offset(2) + "for (uint32_t idxFeat = 0; idxFeat < LA_NUM_FEATURES; idxFeat++)\n"+ offset(2) + "{\n"+ offset(3) + "uint32_t domSize = vDomainSize[idxFeat+1] -\n"+ offset(11) + "vDomainSize[idxFeat] + 1;\n"+ offset(3) + "for (uint32_t idxDom = 0; idxDom < domSize; idxDom ++)\n"+ offset(3) + "{\n"+ offset(4) + "vIsCat.push_back(vIsCatFeature[idxFeat]);\n"+ offset(3) + "}\n"+ offset(2) + "}\n"+ offset(0) + "\n" + decompose;
    returnString += calculateDomains + generateQR;
    std::string runFunction =
         offset(1) + "void runApplication()\n"
         + offset(1) + "{\n"
         + getVectorOfFeatures()
         + vectorCofactorViews + "\n"
         + returnString
         + offset(1) + "}\n";
    return runFunction;
}
