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
        bool outputDecomp) :
    QRDecompApp(pathToFiles, launcher, useLinearDependencyCheck, outputDecomp){}
    virtual ~SVDecompApp() override {}
protected:
    virtual std::string getCodeOfDecomposition() override;
};
#endif /* INCLUDE_APP_SVDECOMP_H_ */
