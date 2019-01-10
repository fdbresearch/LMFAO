//--------------------------------------------------------------------
//
// KMeans.h
//
// Created on: Oct 18, 2019
// Author: Max
//
//--------------------------------------------------------------------


#ifndef INCLUDE_APP_KMEANS_H_
#define INCLUDE_APP_KMEANS_H_

#include <bitset>
#include <string>
#include <vector>

#include <Application.hpp>

/** Forward declarations to allow pointer to launcher and Linear Regressionas
 * without cyclic includes. */
class Launcher;

/**
 * Class that launches regression model on the data, using d-trees.
 */
class KMeans: public Application
{
public:

    KMeans(const std::string& pathToFiles,
           std::shared_ptr<Launcher> launcher,
           const int k);

    ~KMeans();

    void run();
    
    void generateCode(const std::string& outputString);
        
private:
    
    //! Physical path to the schema and table files.
    std::string _pathToFiles;
    
    std::shared_ptr<Launcher> _launcher;
    
    std::shared_ptr<QueryCompiler> _compiler;
    
    std::shared_ptr<TreeDecomposition> _td;

    const size_t _k;

    size_t numberOfOriginalVariables;
    
    size_t* _queryRootIndex = nullptr;
    
    std::bitset<multifaq::params::NUM_OF_VARIABLES> clusterVariables;
    std::vector<std::pair<size_t,Query*>> categoricalQueries;
    std::vector<std::pair<size_t,Query*>> continuousQueries;

    Query* varToQuery[multifaq::params::NUM_OF_VARIABLES];

    void modelToQueries();

    void loadFeatures();

    std::string genComputeGridPointsFunction();

    std::string genClusterTuple();
    std::string genKMeansFunction();
    std::string genModelEvaluationFunction();

    // The two bitsets are defined in ApplicationHandler
    // var_bitset _isFeature;
    // var_bitset _isCategoricalFeature
};

#endif /* INCLUDE_APP_KMEANS_H_ */
