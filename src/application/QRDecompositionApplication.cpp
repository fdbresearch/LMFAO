#include <QRDecompositionApplication.h>
#include <GlobalParams.hpp>

using multifaq::params::NUM_OF_VARIABLES;

std::string QRDecompApp::getCodeOfIncludes()
{
    std::string includeStr = "";
    includeStr += 
           std::string("") 
         + "#include \"../../LinearAlgebra/include/SVDecomp.h\"\n" 
         + "#include <map>\n"
         + "typedef std::map< std::tuple<unsigned int, long double, long double>, long double> DomainAggregate;\n"
         + "typedef std::vector<DomainAggregate> VectorOffset;\n"
         + "typedef std::map<unsigned int, VectorOffset> MapView;\n"
         + "using namespace LMFAO::LinearAlgebra;\n\n";
     return includeStr;
}

std::string QRDecompApp::getCodeOfFunDefinitions()
{
    unsigned int numOfFeatures = _listOfFeatures.size() + 1;
    std::string numOfFeaturesStr = std::to_string(numOfFeatures);
    std::string functionsString = ""+ offset(0) + "LMFAO::LinearAlgebra::MapMatrixAggregate mSigma;\n"+ offset(1) + "constexpr unsigned int LA_NUM_FEATURES = 33;\n"+ offset(0) + "\n"+ offset(1) + "unsigned int getTriangularIdx(unsigned int row, unsigned int col,\n"+ offset(11) + "const std::vector<bool> &vIsCatFeature,\n"+ offset(11) + "const std::vector<unsigned int> &vCatFeatCount)\n"+ offset(1) + "{\n"+ offset(2) + "unsigned int idxCur, idx, offset, offsetCat;\n"+ offset(2) + "if ((row == 0) && (col == 0))\n"+ offset(2) + "{\n"+ offset(3) + "return 0;\n"+ offset(2) + "}\n"+ offset(2) + "//offsetCat = vCatFeatCount[row + 1];\n"+ offset(2) + "if (row == 0)\n"+ offset(2) + "{\n"+ offset(3) + "row = col - 1;\n"+ offset(3) + "col = row;\n"+ offset(3) + "offsetCat = vCatFeatCount[row + 1] - vIsCatFeature[row + 1];\n"+ offset(2) + "}\n"+ offset(2) + "else\n"+ offset(2) + "{\n"+ offset(3) + "row = row - 1;\n"+ offset(3) + "offsetCat = vCatFeatCount[row + 1];\n"+ offset(2) + "}\n"+ offset(2) + "unsigned int n = LA_NUM_FEATURES;\n"+ offset(2) + "idxCur = n - row;\n"+ offset(2) + "idx = n * (n + 1) / 2 - idxCur * (idxCur + 1) / 2;\n"+ offset(2) + "//std :: cout << \"idx \" << idx << \" row \" << row << \" col \" << col << std::endl;\n"+ offset(2) + "offset = col - row - offsetCat;\n"+ offset(2) + "return idx + offset + 1;\n"+ offset(1) + "}\n"+ offset(0) + "\n"+ offset(1) + "const DomainAggregate &getCellValueUpper(unsigned int row, unsigned int col,\n"+ offset(14) + "MapView &mViews,\n"+ offset(14) + "const std::vector<bool> &vIsCatFeature,\n"+ offset(14) + "const std::vector<unsigned int> &vCatFeatCount)\n"+ offset(1) + "{\n"+ offset(2) + "unsigned int idx = getTriangularIdx(row, col, vIsCatFeature, vCatFeatCount);\n"+ offset(2) + "//std::cout << \"idx\" << idx << std::endl;\n"+ offset(2) + "const auto &viewOffset = vectorCofactorViews[idx];\n"+ offset(2) + "//std::cout << \"v \" << viewOffset.first << \" o \" << viewOffset.second << std::endl;\n"+ offset(2) + "return (mViews[viewOffset.first][viewOffset.second]);\n"+ offset(1) + "}\n"+ offset(1) + "const DomainAggregate &getCellValue(unsigned int row, unsigned int col,\n"+ offset(13) + "MapView &mViews,\n"+ offset(13) + "const std::vector<bool> &vIsCatFeature,\n"+ offset(13) + "const std::vector<unsigned int> &vCatFeatCount)\n"+ offset(1) + "{\n"+ offset(2) + "if (row > col)\n"+ offset(2) + "{\n"+ offset(3) + "std::swap(col, row);\n"+ offset(2) + "}\n"+ offset(2) + "return getCellValueUpper(row, col, mViews, vIsCatFeature, vCatFeatCount);\n"+ offset(1) + "}\n"+ offset(0) + "\n"+ offset(0) + "\n"+ offset(0) + "\n"+ offset(1) + "void printCell(unsigned int row, unsigned int col,\n"+ offset(6) + "const DomainAggregate& aggregate, char cellType)\n"+ offset(1) + "{\n"+ offset(2) + "switch (cellType)\n"+ offset(2) + "{\n"+ offset(3) + "case 'c':\n"+ offset(4) + "std::cout << row << \" \" <<  col << \" \" <<\n"+ offset(4) + "aggregate.begin()->second << std::endl;\n"+ offset(4) + "mSigma[std::make_pair(row, col)] =\n"+ offset(5) + "std::make_pair(aggregate.begin()->second, 0);\n"+ offset(5) + "break;\n"+ offset(2) + "}\n"+ offset(1) + "}\n"+ offset(0) + "\n"+ offset(1) + "void calculateCatFeatCount(const std::vector<bool> &vIsCatFeature,\n"+ offset(10) + "std::vector<unsigned int> &vCatFeatCount)\n"+ offset(1) + "{\n"+ offset(2) + "vCatFeatCount.resize(vIsCatFeature.size());\n"+ offset(2) + "vCatFeatCount[0] = 0;\n"+ offset(2) + "for (unsigned int idx = 1; idx < vIsCatFeature.size(); idx ++)\n"+ offset(2) + "{\n"+ offset(4) + "vCatFeatCount[idx] = vCatFeatCount[idx - 1] + vIsCatFeature[idx];\n"+ offset(2) + "}\n"+ offset(1) + "}\n";
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
    returnString += ""+ offset(2) + "std::vector<bool> vIsCatFeature;\n"
                  + offset(2) + "std::vector<unsigned int> vCatFeatCount;\n";
    // Intercept feature. 
    returnString +=  
                offset(2) + "vIsCatFeature.push_back(false);\n";
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
    returnString += offset(2) + "calculateCatFeatCount(vIsCatFeature, vCatFeatCount);\n";
    return returnString;
}

std::string QRDecompApp::getCodeOfRunAppFun()
{
    //std::string dumpListOfQueries = "";
    std::string vectorCofactorViews = "";

    for (Query* query : listOfQueries)
    {
        std::pair<size_t,size_t>& viewAggPair = query->_aggregates[0]->_incoming[0];
        /*       
        dumpListOfQueries += std::to_string(viewAggPair.first)+","+
            std::to_string(viewAggPair.second)+"\\n";
        */
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

    std::string dumpCovarianceString = ""+ offset(2) + "for (unsigned int row = 0; row < LA_NUM_FEATURES; row++)\n"+ offset(2) + "{\n"+ offset(3) + "for (unsigned int col = 0; col < LA_NUM_FEATURES; col++)\n"+ offset(3) + "{\n"+ offset(4) + "const auto &m = getCellValue(row, col, mViews, vIsCatFeature, vCatFeatCount);\n"+ offset(4) + "char cellType;\n"+ offset(4) + "if (!vIsCatFeature[row] && !vIsCatFeature[col])\n"+ offset(4) + "{\n"+ offset(6) + "cellType = 'c';\n"+ offset(4) + "}\n"+ offset(4) + "else if (vIsCatFeature[row] && !vIsCatFeature[col])\n"+ offset(4) + "{\n"+ offset(6) + "cellType = 'k';\n"+ offset(4) + "}\n"+ offset(4) + "else if (!vIsCatFeature[row] && vIsCatFeature[col])\n"+ offset(4) + "{\n"+ offset(6) + "cellType = 'r';\n"+ offset(4) + "}\n"+ offset(4) + "else if (vIsCatFeature[row] &&  vIsCatFeature[col]\n"+ offset(6) + "&& (row == col))\n"+ offset(4) + "{\n"+ offset(6) + "cellType == 'd';\n"+ offset(4) + "}\n"+ offset(4) + "else\n"+ offset(4) + "{\n"+ offset(6) + "cellType = 'm';\n"+ offset(4) + "}\n"+ offset(4) + "printCell(row, col, m, cellType);\n"+ offset(3) + "}\n"+ offset(2) + "}\n"+ offset(2) + "//SVDecomp svdDecomp(SVDecomp::DecompType::NAIVE, mSigma, LA_NUM_FEATURES);\n"+ offset(2) + "//svdDecomp.decompose()\n";
    returnString += dumpCovarianceString;
    std::string runFunction = offset(1)+"void runApplication()\n"+offset(1)+"{\n"
        //offset(2)+"std::ofstream ofs(\"output/covarianceMatrix.out\");\n"+
         + getVectorOfFeatures() 
         + vectorCofactorViews + "\n"+
         offset(2) + "MapView mViews;\n" + 
        //offset(2)+"ofs << \""+dumpListOfQueries+"\";\n"+
        //offset(2)+"ofs.close();\n\n"+
         returnString + 

        offset(1)+"}\n";
    return runFunction;
}