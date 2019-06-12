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
    enum class QrType
    {
        QR_NAIVE, QR_SINGLE_THREAD, QR_MULTI_THREAD, QR_CHOL,
        QR_DUMMY
    };
private:
    QrType mQrType;
public:
    QRDecompApp(const std::string& pathToFiles,
        std::shared_ptr<Launcher> launcher,
        bool useLinearDependencyCheck,
        bool outputDecomp, const std::string& dumpFile,
        QrType qrType) :
            CovarianceMatrix(pathToFiles, launcher),
            mUseLinearDependencyCheck(useLinearDependencyCheck),
            mOutputDecomp(outputDecomp), mDumpFile(dumpFile),
            mQrType(qrType)            {}
    virtual ~QRDecompApp() override {}
protected:
    virtual std::string getCodeOfIncludes() override;
    virtual std::string getCodeOfFunDefinitions() override;
    virtual std::string getCodeOfRunAppFun() override;
    virtual std::string getCodeOfDecomposition();
    bool mUseLinearDependencyCheck;
    bool mOutputDecomp;
    std::string mDumpFile;
private:
	std::string getVectorOfFeatures();
};

#endif /* INCLUDE_APP_QRDECOMP_H_ */
