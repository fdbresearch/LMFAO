//--------------------------------------------------------------------
//
// CovarianceMatrix.h
//
// Created on: Tue 30, 2019
// Author: Djordje
//
//--------------------------------------------------------------------


#ifndef INCLUDE_APP_SVDECOMP_H_
#define INCLUDE_APP_SVDECOMP_H_

#include <QRDecompositionApplication.h>

/**
 * Class that launches regression model on the data, using d-trees.
 */
class SVDecompApp: public QRDecompApp
{
public:
    enum class SvdType
    {
        SVD_QR, SVD_QR_CHOL, SVD_EIG_DEC, SVD_ALT_MIN
    };
private:
    SvdType mSvdType;
public:
    SVDecompApp(const std::string& pathToFiles,
        std::shared_ptr<Launcher> launcher,
        bool useLinearDependencyCheck,
        bool outputDecomp, const std::string& dumpFile,
        SvdType svdType) :
    QRDecompApp(pathToFiles, launcher, useLinearDependencyCheck, outputDecomp, dumpFile,
                QRDecompApp::QrType::QR_DUMMY), mSvdType(svdType){}
    virtual ~SVDecompApp() override {}
protected:
    virtual std::string getCodeOfDecomposition() override;
};
#endif /* INCLUDE_APP_SVDECOMP_H_ */
