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
    SVDecompApp(const std::string& pathToFiles,
        std::shared_ptr<Launcher> launcher,
        bool useLinearDependencyCheck,
        bool outputDecomp, const std::string& dumpFile) :
    QRDecompApp(pathToFiles, launcher, useLinearDependencyCheck, outputDecomp, dumpFile){}
    virtual ~SVDecompApp() override {}
protected:
    virtual std::string getCodeOfDecomposition() override;
};
#endif /* INCLUDE_APP_SVDECOMP_H_ */
