#include <QRDecompositionApplication.h>
#include <GlobalParams.hpp>

using multifaq::params::NUM_OF_VARIABLES;

std::string QRDecompApp::getCodeOfIncludes()
{
    std::string includeStr = "";
    includeStr += ""+ offset(0) + "#include \"../../LinearAlgebra/include/SVDecomp.h\"\n"+ offset(0) + "#include <map>\n"+ offset(0) + "#include <set>\n"+ offset(0) + "#include <unordered_map>\n"+ offset(0) + "\n"+ offset(0) + "typedef std::map< std::tuple<unsigned int, long double, long double>, long double> DomainAggregate;\n"+ offset(0) + "typedef std::vector<DomainAggregate> VectorOffset;\n"+ offset(0) + "typedef std::map<unsigned int, VectorOffset> MapView;\n"+ offset(0) + "\n"+ offset(0) + "typedef std::set<unsigned int> MapCategory;\n"+ offset(0) + "typedef std::vector<MapCategory> VectorDomain;\n"+ offset(0) + "\n"+ offset(0) + "typedef std::vector<std::unordered_map<unsigned int, unsigned int> > VectorShift;\n"+ offset(0) + "\n"+ offset(0) + "using namespace LMFAO::LinearAlgebra;\n";
     return includeStr;
}

std::string QRDecompApp::getCodeOfFunDefinitions()
{
    unsigned int numOfFeatures = _listOfFeatures.size() + 1;
    std::string numOfFeaturesStr = std::to_string(numOfFeatures);
    std::string functionsString = ""+ offset(0) + "LMFAO::LinearAlgebra::MapMatrixAggregate mSigma;\n"+ offset(1) + "constexpr unsigned int LA_NUM_FEATURES = " + numOfFeaturesStr + ";\n"+ offset(0) + "\n"+ offset(1) + "unsigned int getTriangularIdx(unsigned int row, unsigned int col,\n"+ offset(11) + "const std::vector<bool> &vIsCatFeature,\n"+ offset(11) + "const std::vector<unsigned int> &vCatFeatCount)\n"+ offset(1) + "{\n"+ offset(2) + "unsigned int idxCur, idx, offset, offsetCat;\n"+ offset(2) + "if ((row == 0) && (col == 0))\n"+ offset(2) + "{\n"+ offset(3) + "return 0;\n"+ offset(2) + "}\n"+ offset(2) + "//offsetCat = vCatFeatCount[row + 1];\n"+ offset(2) + "if (row == 0)\n"+ offset(2) + "{\n"+ offset(3) + "row = col - 1;\n"+ offset(3) + "col = row;\n"+ offset(3) + "offsetCat = vCatFeatCount[row + 1] - vIsCatFeature[row + 1];\n"+ offset(2) + "}\n"+ offset(2) + "else\n"+ offset(2) + "{\n"+ offset(3) + "row = row - 1;\n"+ offset(3) + "offsetCat = vCatFeatCount[row + 1];\n"+ offset(2) + "}\n"+ offset(2) + "unsigned int n = LA_NUM_FEATURES;\n"+ offset(2) + "idxCur = n - row;\n"+ offset(2) + "idx = n * (n + 1) / 2 - idxCur * (idxCur + 1) / 2;\n"+ offset(2) + "//std :: cout << \"idx \" << idx << \" row \" << row << \" col \" << col << std::endl;\n"+ offset(2) + "offset = col - row - offsetCat;\n"+ offset(2) + "return idx + offset + 1;\n"+ offset(1) + "}\n"+ offset(0) + "\n"+ offset(1) + "const DomainAggregate &getCellValueUpper(unsigned int row, unsigned int col,\n"+ offset(14) + "const MapView &mViews,\n"+ offset(14) + "const std::vector<bool> &vIsCatFeature,\n"+ offset(14) + "const std::vector<unsigned int> &vCatFeatCount)\n"+ offset(1) + "{\n"+ offset(2) + "unsigned int idx = getTriangularIdx(row, col, vIsCatFeature, vCatFeatCount);\n"+ offset(2) + "//std::cout << \"idx\" << idx << std::endl;\n"+ offset(2) + "const auto &viewOffset = vectorCofactorViews[idx];\n"+ offset(2) + "//std::cout << \"v \" << viewOffset.first << \" o \" << viewOffset.second << std::endl;\n"+ offset(2) + "return mViews.at(viewOffset.first)[viewOffset.second];\n"+ offset(1) + "}\n"+ offset(0) + "\n"+ offset(1) + "const DomainAggregate &getCellValue(unsigned int row, unsigned int col,\n"+ offset(13) + "const MapView &mViews,\n"+ offset(13) + "const std::vector<bool> &vIsCatFeature,\n"+ offset(13) + "const std::vector<unsigned int> &vCatFeatCount)\n"+ offset(1) + "{\n"+ offset(2) + "if (row > col)\n"+ offset(2) + "{\n"+ offset(3) + "std::swap(col, row);\n"+ offset(2) + "}\n"+ offset(2) + "return getCellValueUpper(row, col, mViews, vIsCatFeature, vCatFeatCount);\n"+ offset(1) + "}\n"+ offset(0) + "\n"+ offset(1) + "unsigned int getCatIdx(unsigned int featureIdx, unsigned int category,\n"+ offset(7) + "const std::vector<unsigned int>& vDomainSize,\n"+ offset(7) + "const VectorShift& vCatShifts)\n"+ offset(1) + "{\n"+ offset(2) + "return vDomainSize[featureIdx] + vCatShifts[featureIdx].at(category) - 1;\n"+ offset(1) + "}\n"+ offset(0) + "\n"+ offset(1) + "unsigned isFirstCategory(unsigned int featureIdx, unsigned int category,\n"+ offset(9) + "const VectorShift& vCatShifts)\n"+ offset(1) + "{\n"+ offset(2) + "return vCatShifts[featureIdx].at(category) == 0;\n"+ offset(1) + "}\n"+ offset(0) + "\n"+ offset(1) + "void inline addEntryToSigma(unsigned int row, unsigned int col,\n"+ offset(8) + "long double aggregate, bool isCat)\n"+ offset(1) + "{\n"+ offset(2) + "std::cout << row << \" \" <<  col << \" \" << std::fixed\n"+ offset(5) + "<< aggregate  <<  \" \" << isCat << std::endl;\n"+ offset(2) + "mSigma[std::make_pair(row, col)] = std::make_pair(aggregate, isCat);\n"+ offset(1) + "}\n"+ offset(0) + "\n"+ offset(1) + "void printCell(unsigned int row, unsigned int col,\n"+ offset(6) + "const DomainAggregate& aggregate, char cellType,\n"+ offset(6) + "const std::vector<unsigned int>& vDomainSize,\n"+ offset(6) + "const VectorShift& vCatShifts)\n"+ offset(1) + "{\n"+ offset(2) + "unsigned int category, catIdx, outRow, outCol,\n"+ offset(6) + "catIdxRow, catIdxCol, categoryRow, categoryCol;\n"+ offset(2) + "long double outVal;\n"+ offset(2) + "switch (cellType)\n"+ offset(2) + "{\n"+ offset(3) + "case 'c':\n"+ offset(4) + "outRow = row + vDomainSize[row];\n"+ offset(4) + "outCol = col + vDomainSize[col];\n"+ offset(4) + "outVal = aggregate.begin()->second;\n"+ offset(4) + "addEntryToSigma(outRow, outCol, outVal, false);\n"+ offset(4) + "break;\n"+ offset(3) + "case 'r':\n"+ offset(4) + "for (const auto& it: aggregate)\n"+ offset(4) + "{\n"+ offset(6) + "category = static_cast<unsigned int>(std::get<1>(it.first));\n"+ offset(6) + "if (isFirstCategory(col, category, vCatShifts))\n"+ offset(6) + "{\n"+ offset(7) + "continue;\n"+ offset(6) + "}\n"+ offset(6) + "catIdx = getCatIdx(col, category, vDomainSize, vCatShifts);\n"+ offset(6) + "outRow = row + vDomainSize[row];\n"+ offset(6) + "outCol = col + catIdx;\n"+ offset(6) + "outVal = it.second;\n"+ offset(6) + "addEntryToSigma(outRow, outCol, outVal, true);\n"+ offset(4) + "}\n"+ offset(4) + "break;\n"+ offset(3) + "case 'k':\n"+ offset(4) + "for (const auto& it: aggregate)\n"+ offset(4) + "{\n"+ offset(6) + "category = static_cast<unsigned int>(std::get<1>(it.first));\n"+ offset(6) + "if (isFirstCategory(row, category, vCatShifts))\n"+ offset(6) + "{\n"+ offset(7) + "continue;\n"+ offset(6) + "}\n"+ offset(6) + "catIdx = getCatIdx(row, category, vDomainSize, vCatShifts);\n"+ offset(6) + "outRow = row + catIdx;\n"+ offset(6) + "outCol = col + vDomainSize[col];\n"+ offset(6) + "outVal = it.second;\n"+ offset(6) + "addEntryToSigma(outRow, outCol, outVal, true);\n"+ offset(4) + "}\n"+ offset(4) + "break;\n"+ offset(3) + "case 'd':\n"+ offset(4) + "for (const auto& it: aggregate)\n"+ offset(4) + "{\n"+ offset(6) + "category = static_cast<unsigned int>(std::get<1>(it.first));\n"+ offset(6) + "if (isFirstCategory(row, category, vCatShifts))\n"+ offset(6) + "{\n"+ offset(7) + "continue;\n"+ offset(6) + "}\n"+ offset(6) + "catIdx = getCatIdx(row, category, vDomainSize, vCatShifts);\n"+ offset(6) + "outRow = row + catIdx;\n"+ offset(6) + "outCol = col + catIdx;\n"+ offset(6) + "outVal = it.second;\n"+ offset(6) + "addEntryToSigma(outRow, outCol, outVal, true);\n"+ offset(4) + "}\n"+ offset(4) + "break;\n"+ offset(3) + "case 'm':\n"+ offset(4) + "for (const auto& it: aggregate)\n"+ offset(4) + "{\n"+ offset(6) + "categoryRow = static_cast<unsigned int>(std::get<1>(it.first));\n"+ offset(6) + "categoryCol = static_cast<unsigned int>(std::get<2>(it.first));\n"+ offset(6) + "if (row > col)\n"+ offset(6) + "{\n"+ offset(8) + "std::swap(categoryRow, categoryCol);\n"+ offset(6) + "}\n"+ offset(6) + "if (isFirstCategory(row, categoryRow, vCatShifts) ||\n"+ offset(8) + "isFirstCategory(col, categoryCol, vCatShifts))\n"+ offset(6) + "{\n"+ offset(7) + "continue;\n"+ offset(6) + "}\n"+ offset(0) + "\n"+ offset(6) + "catIdxRow = getCatIdx(row, categoryRow, vDomainSize, vCatShifts);\n"+ offset(6) + "catIdxCol = getCatIdx(col, categoryCol, vDomainSize, vCatShifts);\n"+ offset(6) + "outRow = row + catIdxRow;\n"+ offset(6) + "outCol = col + catIdxCol;\n"+ offset(6) + "outVal = it.second;\n"+ offset(6) + "addEntryToSigma(outRow, outCol, outVal, true);\n"+ offset(4) + "}\n"+ offset(4) + "break;\n"+ offset(2) + "}\n"+ offset(1) + "}\n"+ offset(0) + "\n"+ offset(1) + "void calculateCatFeatCount(const std::vector<bool> &vIsCatFeature,\n"+ offset(10) + "std::vector<unsigned int> &vCatFeatCount)\n"+ offset(1) + "{\n"+ offset(2) + "vCatFeatCount.resize(vIsCatFeature.size());\n"+ offset(2) + "vCatFeatCount[0] = 0;\n"+ offset(2) + "for (unsigned int idx = 1; idx < vIsCatFeature.size(); idx ++)\n"+ offset(2) + "{\n"+ offset(4) + "vCatFeatCount[idx] = vCatFeatCount[idx - 1] + vIsCatFeature[idx];\n"+ offset(2) + "}\n"+ offset(1) + "}\n"+ offset(0) + "\n"+ offset(1) + "void calculateDomains(VectorDomain &vDomains,\n"+ offset(8) + "const MapView& mViews,\n"+ offset(8) + "const std::vector<bool> &vIsCatFeature,\n"+ offset(8) + "const std::vector<unsigned int> &vCatFeatCount)\n"+ offset(1) + "{\n"+ offset(2) + "const unsigned int n = vIsCatFeature.size();\n"+ offset(2) + "for (unsigned int idx = 0; idx < n; idx ++)\n"+ offset(2) + "{\n"+ offset(4) + "vDomains.push_back(std::set<unsigned int>());\n"+ offset(2) + "}\n"+ offset(2) + "for (unsigned int row = 0; row < n; row ++)\n"+ offset(2) + "{\n"+ offset(3) + "if (vIsCatFeature[row])\n"+ offset(3) + "{\n"+ offset(4) + "const DomainAggregate &m = getCellValue(row, row, mViews, vIsCatFeature, vCatFeatCount);\n"+ offset(4) + "for (const auto& it: m)\n"+ offset(4) + "{\n"+ offset(5) + "vDomains[row].insert(std::get<1>(it.first));\n"+ offset(4) + "}\n"+ offset(3) + "}\n"+ offset(2) + "}\n"+ offset(1) + "}\n"+ offset(0) + "\n"+ offset(0) + "\n"+ offset(1) + "void getDomainsSize(std::vector<unsigned int>& vDomainSize,\n"+ offset(8) + "const std::vector<bool> &vIsCatFeature,\n"+ offset(8) + "const VectorDomain &vDomains)\n"+ offset(1) + "{\n"+ offset(2) + "const unsigned int n = vIsCatFeature.size() + 1;\n"+ offset(2) + "vDomainSize.resize(n);\n"+ offset(2) + "vDomainSize[0] = 0;\n"+ offset(0) + "\n"+ offset(2) + "for (unsigned int idx = 1; idx < n; idx ++)\n"+ offset(2) + "{\n"+ offset(4) + "vDomainSize[idx] = vDomainSize[idx - 1] + vDomains[idx-1].size()\n"+ offset(10) + "- 2 * vIsCatFeature[idx-1];\n"+ offset(2) + "}\n"+ offset(1) + "}\n"+ offset(0) + "\n"+ offset(1) + "void getCatShifts(VectorShift& vCatShifts,\n"+ offset(7) + "const std::vector<bool> &vIsCatFeature,\n"+ offset(7) + "const VectorDomain &vDomains)\n"+ offset(1) + "{\n"+ offset(2) + "const unsigned int n = vIsCatFeature.size();\n"+ offset(2) + "vCatShifts.resize(n);\n"+ offset(2) + "for (unsigned int idx = 1; idx < n; idx ++)\n"+ offset(2) + "{\n"+ offset(4) + "const MapCategory& mCategories = vDomains[idx];\n"+ offset(4) + "unsigned int shiftIdx = 0;\n"+ offset(4) + "for (const auto& it: mCategories)\n"+ offset(4) + "{\n"+ offset(5) + "vCatShifts[idx][it] = shiftIdx;\n"+ offset(5) + "shiftIdx++;\n"+ offset(4) + "}\n"+ offset(2) + "}\n"+ offset(1) +"}\n";
    return offset(1) + "std::vector< std::pair<unsigned int, unsigned int> > vectorCofactorViews;\n" 
            + functionsString;
}

inline std::string getViewName(size_t viewID)
{
    return "V" + std::to_string(viewID);
}

std::string QRDecompApp::getVectorOfFeatures()
{
    std::string returnString = "";
    returnString += ""+ offset(2) + "MapView mViews;\n"+ offset(2) + "VectorDomain vDomains;\n"+ offset(2) + "std::vector<unsigned int> vDomainSize;\n"+ offset(2) + "VectorShift vCatShifts;\n"+ offset(2) + "std::vector<bool> vIsCatFeature;\n"+ offset(2) + "std::vector<unsigned int> vCatFeatCount;\n\n";
    
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
    std::string calculateDomains = ""+ offset(2) + "calculateDomains(vDomains, mViews, vIsCatFeature, vCatFeatCount);\n"+ offset(2) + "getDomainsSize(vDomainSize, vIsCatFeature, vDomains);\n"+ offset(2) + "getCatShifts(vCatShifts, vIsCatFeature, vDomains);\n\n";
    std::string generateQR = ""+ offset(2) + "for (unsigned int row = 0; row < LA_NUM_FEATURES; row++)\n"+ offset(2) + "{\n"+ offset(3) + "for (unsigned int col = 0; col < LA_NUM_FEATURES; col++)\n"+ offset(3) + "{\n"+ offset(4) + "const auto &m = getCellValue(row, col, mViews, vIsCatFeature, vCatFeatCount);\n"+ offset(4) + "char cellType;\n"+ offset(4) + "if (!vIsCatFeature[row] && !vIsCatFeature[col])\n"+ offset(4) + "{\n"+ offset(6) + "cellType = 'c';\n"+ offset(4) + "}\n"+ offset(4) + "else if (vIsCatFeature[row] && !vIsCatFeature[col])\n"+ offset(4) + "{\n"+ offset(6) + "cellType = 'k';\n"+ offset(4) + "}\n"+ offset(4) + "else if (!vIsCatFeature[row] && vIsCatFeature[col])\n"+ offset(4) + "{\n"+ offset(6) + "cellType = 'r';\n"+ offset(4) + "}\n"+ offset(4) + "else if (vIsCatFeature[row] &&  vIsCatFeature[col]\n"+ offset(6) + "&& (row == col))\n"+ offset(4) + "{\n"+ offset(6) + "cellType = 'd';\n"+ offset(4) + "}\n"+ offset(4) + "else\n"+ offset(4) + "{\n"+ offset(6) + "cellType = 'm';\n"+ offset(4) + "}\n"+ offset(4) + "printCell(row, col, m, cellType, vDomainSize, vCatShifts);\n"+ offset(3) + "}\n"+ offset(2) + "}\n"+ offset(2) + "//SVDecomp svdDecomp(SVDecomp::DecompType::NAIVE, mSigma, LA_NUM_FEATURES);\n"+ offset(2) + "//svdDecomp.decompose()\n";
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