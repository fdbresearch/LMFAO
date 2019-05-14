//--------------------------------------------------------------------
//
// CovarianceMatrix.h
//
// Created on: Feb 1, 2018
// Author: Max
//
//--------------------------------------------------------------------


#ifndef INCLUDE_APP_QRDECOMP_H_
#define INCLUDE_APP_QRDECOMP_H_

#include <CovarianceMatrix.h>

/**
 * Class that launches regression model on the data, using d-trees.
 */
class QRDecompApp: public CovarianceMatrix
{
public:
    QRDecompApp(const std::string& pathToFiles,
        std::shared_ptr<Launcher> launcher,
        bool useLinearDependencyCheck,
        bool outputDecomp) :
            CovarianceMatrix(pathToFiles, launcher),
            mUseLinearDependencyCheck(useLinearDependencyCheck),
            mOutputDecomp(outputDecomp)            {}
    virtual ~QRDecompApp() override {}
protected:
    virtual std::string getCodeOfIncludes() override;
    virtual std::string getCodeOfFunDefinitions() override;
    virtual std::string getCodeOfRunAppFun() override;
    virtual std::string getCodeOfDecomposition();
    bool mUseLinearDependencyCheck;
    bool mOutputDecomp;
private:
	std::string getVectorOfFeatures();
};

#endif /* INCLUDE_APP_QRDECOMP_H_ */
