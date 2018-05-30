//--------------------------------------------------------------------
//
// CovarianceMatrix.cpp
//
// Created on: Feb 1, 2018
// Author: Max
//
//--------------------------------------------------------------------

#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <fstream>

#include <Launcher.h>
#include <CovarianceMatrix.h>

#define DEGREE_TWO

static const std::string FEATURE_CONF = "/features.conf";

static const char COMMENT_CHAR = '#';
static const char NUMBER_SEPARATOR_CHAR = ',';
static const char ATTRIBUTE_NAME_CHAR = ':';

using namespace std;
using namespace multifaq::params;
namespace phoenix = boost::phoenix;
using namespace boost::spirit;

CovarianceMatrix::CovarianceMatrix(
    const string& pathToFiles, shared_ptr<Launcher> launcher) :
    _pathToFiles(pathToFiles)
{
    _compiler = launcher->getCompiler();
    _td = launcher->getTreeDecomposition();
}

CovarianceMatrix::~CovarianceMatrix()
{
    delete[] _queryRootIndex;
}

void CovarianceMatrix::run()
{
    loadFeatures();
    modelToQueries();
    _compiler->compile();
}

void CovarianceMatrix::modelToQueries()
{
    size_t numberOfFunctions = 0;
    size_t featureToFunction[NUM_OF_VARIABLES],
        quadFeatureToFunction[NUM_OF_VARIABLES],
        cubicFeatureToFunction[NUM_OF_VARIABLES],
        quarticFeatureToFunction[NUM_OF_VARIABLES];
    
    for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
    {
        if (_features.test(var) && !_categoricalFeatures.test(var))
        {
            _compiler->addFunction(
                new Function({var}, Operation::sum));
            featureToFunction[var] = numberOfFunctions;
            numberOfFunctions++;
        }
    }

    for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
    {
        if (_features.test(var) && !_categoricalFeatures.test(var))
        {
            _compiler->addFunction(
                new Function({var}, Operation::quadratic_sum));
            quadFeatureToFunction[var] = numberOfFunctions;
            numberOfFunctions++;
        }
    }

    for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
    {
        if (_features.test(var) && !_categoricalFeatures.test(var))
        {
            _compiler->addFunction(
                new Function({var}, Operation::cubic_sum));
            cubicFeatureToFunction[var] = numberOfFunctions;
            numberOfFunctions++;
        }
    }

    for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
    {
        if (_features.test(var) && !_categoricalFeatures.test(var))
        {
            _compiler->addFunction(
                new Function({var}, Operation::quartic_sum));
            quarticFeatureToFunction[var] = numberOfFunctions;
            numberOfFunctions++;
        }
    }

#ifndef DEGREE_TWO
    for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
    {
        if (!_features[var])
            continue;
        
        prod_bitset prod_linear_v1;
        prod_bitset prod_quad_v1;

        size_t v1_root = _td->_root->_id;

        if (!_categoricalFeatures[var])
        {
            // linear function ...
            prod_linear_v1.set(featureToFunction[var]);
            
            // Aggregate* agg_linear = new Aggregate(1);
            // agg_linear->_agg.push_back(prod_linear_v1);
            // agg_linear->_m[0] = 1;

            Aggregate* agg_linear = new Aggregate();
            agg_linear->_agg.push_back(prod_linear_v1);
            
            Query* linear = new Query();
            linear->_aggregates.push_back(agg_linear);
            linear->_rootID = _td->_root->_id;
            // linear->_rootID = _queryRootIndex[var];
            _compiler->addQuery(linear);
            
            // quadratic function ...
            prod_quad_v1.set(quadFeatureToFunction[var]);

            // Aggregate* agg_quad = new Aggregate(1);
            // agg_quad->_agg.push_back(prod_quad_v1);
            // agg_quad->_m[0] = 1;
            Aggregate* agg_quad = new Aggregate();
            agg_quad->_agg.push_back(prod_quad_v1);
            
            Query* quad = new Query();
            quad->_aggregates.push_back(agg_quad);
            quad->_rootID = _td->_root->_id;
            // quad->_rootID = _queryRootIndex[var];
            _compiler->addQuery(quad);
        }
        else
        {
            v1_root = _queryRootIndex[var];

            // Aggregate* agg_linear = new Aggregate(1);
            // agg_linear->_agg.push_back(prod_linear_v1);
            // agg_linear->_m[0] = 1;
            Aggregate* agg_linear = new Aggregate();
            agg_linear->_agg.push_back(prod_linear_v1);
            
            Query* linear = new Query();
            linear->_aggregates.push_back(agg_linear);
            linear->_fVars.set(var);
            linear->_rootID = _queryRootIndex[var];
            _compiler->addQuery(linear);
        }
        

        for (size_t var2 = var+1; var2 < NUM_OF_VARIABLES; ++var2)
        {
            if (_features[var2])
            {
                //*****************************************//
                
                prod_bitset prod_quad_v2 = prod_linear_v1;
                
                // Aggregate* agg_quad_v2 = new Aggregate(1);
                // agg_quad_v2->_m[0] = 1;
                Aggregate* agg_quad_v2 = new Aggregate();
                
                Query* quad_v2 = new Query();
                quad_v2->_rootID = v1_root;
                
                if (_categoricalFeatures[var])
                {
                    quad_v2->_fVars.set(var);
                }
                
                if (_categoricalFeatures[var2])
                {
                    quad_v2->_fVars.set(var2);
                    // If both varaibles are categoricalVars - we choose the
                    // var2 as the root 
                    quad_v2->_rootID = _queryRootIndex[var2];
                }
                else
                {
                    // create prod_quad_v2 of both var and var2
                    prod_quad_v2.set(featureToFunction[var2]);                    
                }
                agg_quad_v2->_agg.push_back(prod_quad_v2);
                
                quad_v2->_aggregates.push_back(agg_quad_v2);
                _compiler->addQuery(quad_v2);
            }
        }
    }

#else

    // TODO: TODO: add count function for intercept
    
    for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
    {
        if (!_features[var])
            continue;

        Query* linear = new Query();
        Aggregate* agg_linear = new Aggregate();

        prod_bitset prod_linear;

        // add linear
        if (!_categoricalFeatures[var])
            prod_linear.set(featureToFunction[var]);
        else
            linear->_fVars.set(var);
        
        agg_linear->_agg.push_back(prod_linear);
        // agg_linear->_m[0] = 1;

        linear->_rootID = _queryRootIndex[var];
        linear->_aggregates.push_back(agg_linear);
        _compiler->addQuery(linear);

        /********** PRINT OUT ********/
        // std::cout << _td->getAttribute(var)->_name << std::endl;
        // std::cout << linear->_rootID << std::endl;
        /********** PRINT OUT ********/
    }
    
    for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
    {
        if (!_features[var])
            continue;

        if (!_categoricalFeatures[var])
        {            
            // add quad
            Query* quad = new Query();
            Aggregate* agg_quad = new Aggregate();
            prod_bitset prod_quad;

            // add quad
            prod_quad.set(quadFeatureToFunction[var]);

            agg_quad->_agg.push_back(prod_quad);
            // agg_quad->_m[0] = 1;

            quad->_rootID = _queryRootIndex[var];
            quad->_aggregates.push_back(agg_quad);
            _compiler->addQuery(quad);

            /********** PRINT OUT ********/
            // std::cout << _td->getAttribute(var)->_name + "*" +
            //     _td->getAttribute(var)->_name  << std::endl;
            // std::cout << quad->_rootID << std::endl;
            /********** PRINT OUT ********/
        }

        for (size_t var2 = var+1; var2 < NUM_OF_VARIABLES; ++var2)
        {
            if (!_features[var2])
                continue;

            // add combo linear, linear
            Query* combo = new Query();
            Aggregate* agg_combo = new Aggregate();
            prod_bitset prod_combo_v1;

            // add combo
            if (!_categoricalFeatures[var])
                prod_combo_v1.set(featureToFunction[var]);
            else
                combo->_fVars.set(var);

            combo->_rootID = _queryRootIndex[var];

            if (!_categoricalFeatures[var2])
                prod_combo_v1.set(featureToFunction[var2]);
            else
            {
                combo->_fVars.set(var2);
                combo->_rootID = _queryRootIndex[var2];
            }
            
            agg_combo->_agg.push_back(prod_combo_v1);
            // agg_combo->_m[0] = 1;

            combo->_aggregates.push_back(agg_combo);
            _compiler->addQuery(combo);

            /********** PRINT OUT ********/
            // std::cout << _td->getAttribute(var)->_name + "*" +
            //     _td->getAttribute(var2)->_name  << std::endl;
            // std::cout << combo->_rootID << std::endl;
            /********** PRINT OUT ********/
        }
    }
    
    for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
    {
        if (!_features[var])
            continue;

        if (!_categoricalFeatures[var])
        {   
            // add cubic (if degree 2)
            Query* cubic = new Query();
            Aggregate* agg_cubic = new Aggregate();
            prod_bitset prod_cubic_v1;

            // add cubic
            prod_cubic_v1.set(cubicFeatureToFunction[var]);

            agg_cubic->_agg.push_back(prod_cubic_v1);
            // agg_cubic->_m[0] = 1;

            cubic->_rootID = _queryRootIndex[var];
            cubic->_aggregates.push_back(agg_cubic);
            _compiler->addQuery(cubic);

            /********** PRINT OUT ********/
            // std::cout << "cubic: "<< _td->getAttribute(var)->_name + "*"
            //     +_td->getAttribute(var)->_name + "*" +
            //     _td->getAttribute(var)->_name << std::endl;
            // std::cout << cubic->_rootID << std::endl;
            /********** PRINT OUT ********/

            for (size_t var2 = var+1; var2 < NUM_OF_VARIABLES; ++var2)
            {
                if (!_features[var2] || _categoricalFeatures[var2])
                    continue;

                // add combo quad, linear (if degree 2) 
                Query* combo = new Query();
                Aggregate* agg_combo = new Aggregate();
                prod_bitset prod_combo_v1;

                // add quad feature 
                prod_combo_v1.set(quadFeatureToFunction[var]);
                combo->_rootID = _queryRootIndex[var];

                if (!_categoricalFeatures[var2])
                    prod_combo_v1.set(featureToFunction[var2]);
                else
                {
                    combo->_fVars.set(var2);
                    combo->_rootID = _queryRootIndex[var2];
                }
            
                agg_combo->_agg.push_back(prod_combo_v1);
                // agg_combo->_m[0] = 1;

                combo->_aggregates.push_back(agg_combo);
                _compiler->addQuery(combo);

                /********** PRINT OUT ********/
                // std::cout << _td->getAttribute(var)->_name + "*" +
                //     _td->getAttribute(var)->_name + "*" +
                //     _td->getAttribute(var2)->_name << std::endl;
                // std::cout << combo->_rootID << std::endl;
                /********** PRINT OUT ********/
            }
        }

        for (size_t var2 = var+1; var2 < NUM_OF_VARIABLES; ++var2)
        {
            if (!_features[var2])
                continue;

            if (!_categoricalFeatures[var2])
            {
                // add combo of linear, quad
                Query* combo = new Query();
                Aggregate* agg_combo = new Aggregate();
                prod_bitset prod_combo_v1;

                // add linear
                if (!_categoricalFeatures[var])
                    prod_combo_v1.set(featureToFunction[var]);
                else
                    combo->_fVars.set(var);

                combo->_rootID = _queryRootIndex[var];

                // add combo
                prod_combo_v1.set(quadFeatureToFunction[var2]);
            
                agg_combo->_agg.push_back(prod_combo_v1);
                // agg_combo->_m[0] = 1;

                combo->_aggregates.push_back(agg_combo);
                _compiler->addQuery(combo);

                /********** PRINT OUT ********/
                // std::cout << _td->getAttribute(var)->_name + "*" +
                //     _td->getAttribute(var2)->_name + "*" +
                //     _td->getAttribute(var2)->_name << std::endl;
                // std::cout << combo->_rootID << std::endl;
                /********** PRINT OUT ********/
            }

            // add combinations of linear, linear, linear
            for (size_t var3 = var2+1; var3 < NUM_OF_VARIABLES; ++var3)
            {
                if (!_features[var3])
                    continue;

                // add combo of linear, linear, linear
                Query* combo = new Query();
                Aggregate* agg_combo = new Aggregate();
                prod_bitset prod_combo_v1;

                // add linear
                if (!_categoricalFeatures[var])
                    prod_combo_v1.set(featureToFunction[var]);
                else
                    combo->_fVars.set(var);

                combo->_rootID = _queryRootIndex[var];

                // add linear
                if (!_categoricalFeatures[var2])
                    prod_combo_v1.set(featureToFunction[var2]);
                else
                {
                    combo->_fVars.set(var2);
                    combo->_rootID = _queryRootIndex[var2];
                }

                // add linear
                if (!_categoricalFeatures[var3])
                    prod_combo_v1.set(featureToFunction[var3]);
                else
                {    
                    combo->_fVars.set(var3);
                    combo->_rootID = _queryRootIndex[var3];
                }
                 
                agg_combo->_agg.push_back(prod_combo_v1);
                // agg_combo->_m[0] = 1;

                combo->_aggregates.push_back(agg_combo);
                _compiler->addQuery(combo);

                /********** PRINT OUT ********/
                // std::cout << _td->getAttribute(var)->_name + "*" +
                //     _td->getAttribute(var2)->_name + "*" +
                //     _td->getAttribute(var3)->_name << std::endl;
                // std::cout << combo->_rootID << std::endl;
                /********** PRINT OUT ********/
            }
        }
    }
    
    // do the same again for each combo of variables ... 
    for (size_t var = 0; var < NUM_OF_VARIABLES; var++)
    {
        if (!_features[var])
            continue;

        if(!_categoricalFeatures[var])
        {
            
            // add quartic (if degree 2)
            Query* quartic = new Query();
            Aggregate* agg_quartic = new Aggregate();
            prod_bitset prod_quartic_v1;

            // add quartic
            prod_quartic_v1.set(quarticFeatureToFunction[var]);

            agg_quartic->_agg.push_back(prod_quartic_v1);
            // agg_quartic->_m[0] = 1;

            quartic->_rootID = _queryRootIndex[var];
            quartic->_aggregates.push_back(agg_quartic);
            _compiler->addQuery(quartic);

            /********** PRINT OUT ********/
            std::cout << _td->getAttribute(var)->_name + "*" +
                _td->getAttribute(var)->_name + "*" +_td->getAttribute(var)->_name +
                "*" +_td->getAttribute(var)->_name << std::endl;
            // std::cout << quartic->_rootID << std::endl;
            /********** PRINT OUT ********/
            
            // add combo cubic and linear (if degree 2) 
            for (size_t var2 = var+1; var2 < NUM_OF_VARIABLES; ++var2)
            {
                if (!_features[var2] || _categoricalFeatures[var2])
                    continue;

                // add combo cubic, linear (if degree 2) 
                Query* combo = new Query();
                Aggregate* agg_combo = new Aggregate();
                prod_bitset prod_combo_v1;

                // add cubic feature 
                prod_combo_v1.set(cubicFeatureToFunction[var]);
                combo->_rootID = _queryRootIndex[var];

                if (!_categoricalFeatures[var2])
                    prod_combo_v1.set(featureToFunction[var2]);
                else
                {
                    combo->_fVars.set(var2);
                    combo->_rootID = _queryRootIndex[var2];
                }
            
                agg_combo->_agg.push_back(prod_combo_v1);
                // agg_combo->_m[0] = 1;

                combo->_aggregates.push_back(agg_combo);
                _compiler->addQuery(combo);

                /********** PRINT OUT ********/
                // std::cout << _td->getAttribute(var)->_name + "*" +
                //     _td->getAttribute(var)->_name + "*" +_td->getAttribute(var)->_name+
                //     "*" + _td->getAttribute(var2)->_name << std::endl;
                // std::cout << combo->_rootID << std::endl;        
                /********** PRINT OUT ********/

            }
            
            for (size_t var2 = var+1; var2 < NUM_OF_VARIABLES; ++var2)
            {
                if (!_features[var2])
                    continue;

                if (!_categoricalFeatures[var2])
                {
                    // add combo of quad, quad
                    Query* combo = new Query();
                    Aggregate* agg_combo = new Aggregate();
                    prod_bitset prod_combo_v1;

                    // add quad
                    prod_combo_v1.set(quadFeatureToFunction[var]);
                    combo->_rootID = _queryRootIndex[var];

                    // add quad
                    prod_combo_v1.set(quadFeatureToFunction[var2]);

                    if ( _queryRootIndex[var] != _queryRootIndex[var2])
                    {
                        std::cout << "there is a mistake here \n";
                        exit(0);
                    }
                    
                    agg_combo->_agg.push_back(prod_combo_v1);
                    // agg_combo->_m[0] = 1;

                    combo->_aggregates.push_back(agg_combo);
                    _compiler->addQuery(combo);

                    /********** PRINT OUT ********/
                    // std::cout << _td->getAttribute(var)->_name + "*" +
                    //     _td->getAttribute(var)->_name + "*" +
                    //     _td->getAttribute(var2)->_name + "*" <<
                    //     _td->getAttribute(var2)->_name << std::endl;
                    // std::cout << combo->_rootID << std::endl;        
                    /********** PRINT OUT ********/

                }

                // add combinations of quad, linear, linear
                for (size_t var3 = var2+1; var3 < NUM_OF_VARIABLES; ++var3)
                {
                    if (!_features[var3])
                        continue;

                    // add combo of quad, linear, linear
                    Query* combo = new Query();
                    Aggregate* agg_combo = new Aggregate();
                    prod_bitset prod_combo_v1;
                    
                    // add quad
                    prod_combo_v1.set(quadFeatureToFunction[var]);
                    combo->_rootID = _queryRootIndex[var];

                    // add linear
                    if (!_categoricalFeatures[var2])
                        prod_combo_v1.set(featureToFunction[var2]);
                    else
                    {
                        combo->_fVars.set(var2);
                        combo->_rootID = _queryRootIndex[var2];
                    }

                    // add linear
                    if (!_categoricalFeatures[var3])
                        prod_combo_v1.set(featureToFunction[var3]);
                    else
                    {    
                        combo->_fVars.set(var3);
                        combo->_rootID = _queryRootIndex[var3];
                    }
                 
                    agg_combo->_agg.push_back(prod_combo_v1);
                    // agg_combo->_m[0] = 1;

                    combo->_aggregates.push_back(agg_combo);
                    _compiler->addQuery(combo);
                    
                    /********** PRINT OUT ********/
                    // std::cout << _td->getAttribute(var)->_name + "*" +
                    //     _td->getAttribute(var)->_name + "*" +
                    //     _td->getAttribute(var2)->_name + "*" <<
                    //     _td->getAttribute(var3)->_name << std::endl;
                    // std::cout << combo->_rootID << std::endl;
                    /********** PRINT OUT ********/
                }
            }   
        }
        
        // add combo linear and cubic (if degree 2) 
        for (size_t var2 = var+1; var2 < NUM_OF_VARIABLES; ++var2)
        {
            if (!_features[var2] || _categoricalFeatures[var2])
                continue;

            // add combo linear, cubic (if degree 2) 
            Query* combo = new Query();
            Aggregate* agg_combo = new Aggregate();
            prod_bitset prod_combo_v1;

            if (!_categoricalFeatures[var])
                prod_combo_v1.set(featureToFunction[var]);
            else
            {
                combo->_fVars.set(var);
            }
            combo->_rootID = _queryRootIndex[var];

            // add cubic feature 
            prod_combo_v1.set(cubicFeatureToFunction[var2]);

            agg_combo->_agg.push_back(prod_combo_v1);
            // agg_combo->_m[0] = 1;

            combo->_aggregates.push_back(agg_combo);
            _compiler->addQuery(combo);

            /********** PRINT OUT ********/
            // std::cout << _td->getAttribute(var)->_name + "*" +
            //     _td->getAttribute(var2)->_name + "*" +_td->getAttribute(var2)->_name +
            //     "*" << _td->getAttribute(var2)->_name << std::endl;
            // std::cout << combo->_rootID << std::endl;
            /********** PRINT OUT ********/
            
            // for (size_t var2 = var+1; var2 < NUM_OF_VARIABLES; ++var2)
            // {
            //     if (!_features[var2])
            //         continue;

            //     if (!_categoricalFeatures[var2])
            //     {
            //         // add combo of quad, quad
            //         Query* combo = new Query();
            //         Aggregate* agg_combo = new Aggregate(1);
            //         // add quad
            //         prod_combo_v1.set(quadFeatureToFunction[var]);
            //         combo->_rootID = _queryRootIndex[var];
            //         // add quad
            //         prod_combo_v1.set(quadFeatureToFunction[var2]);
            //         agg_combo->_agg.push_back(prod_combo_v1);
            //         agg_combo->_m[0] = 1;
            //         combo->_aggregates.push_back(agg_combo);
            //         _compiler->addQuery(combo);
            //     }

            // add combinations of  linear, quad, linear
            for (size_t var3 = var2+1; var3 < NUM_OF_VARIABLES; ++var3)
            {
                if (!_features[var3])
                    continue;

                // add combo of linear, quad, linear
                Query* combo = new Query();
                Aggregate* agg_combo = new Aggregate();
                prod_bitset prod_combo_v1;

                // add linear
                if (!_categoricalFeatures[var])
                    prod_combo_v1.set(featureToFunction[var]);
                else
                    combo->_fVars.set(var);

                combo->_rootID = _queryRootIndex[var];

                // add quad
                prod_combo_v1.set(quadFeatureToFunction[var2]);
                    
                // add linear
                if (!_categoricalFeatures[var3])
                    prod_combo_v1.set(featureToFunction[var3]);
                else
                {    
                    combo->_fVars.set(var3);
                    combo->_rootID = _queryRootIndex[var3];
                }
                 
                agg_combo->_agg.push_back(prod_combo_v1);
                // agg_combo->_m[0] = 1;

                combo->_aggregates.push_back(agg_combo);
                _compiler->addQuery(combo);
                
                /********** PRINT OUT ********/
                // std::cout << _td->getAttribute(var)->_name + "*" +
                //     _td->getAttribute(var2)->_name + "*" +
                //     _td->getAttribute(var2)->_name + "*" <<
                //     _td->getAttribute(var3)->_name << std::endl;
                // std::cout << combo->_rootID << std::endl;
                /********** PRINT OUT ********/
            }

            for (size_t var3 = var2+1; var3 < NUM_OF_VARIABLES; ++var3)
            {
                if (!_features[var3])
                    continue;

                // add combinations of linear, linear, quad
                if (!_categoricalFeatures[var3])
                {
                    // add combo of linear, linear, quad
                    Query* combo = new Query();
                    Aggregate* agg_combo = new Aggregate();
                    prod_bitset prod_combo_v1;


                    // add linear
                    if (!_categoricalFeatures[var])
                        prod_combo_v1.set(featureToFunction[var]);
                    else
                        combo->_fVars.set(var);

                    combo->_rootID = _queryRootIndex[var];

                    // add linear
                    if (!_categoricalFeatures[var2])
                        prod_combo_v1.set(featureToFunction[var2]);
                    else
                    {
                        combo->_fVars.set(var2);
                        combo->_rootID = _queryRootIndex[var2];
                    }

                    // add quad
                    prod_combo_v1.set(quadFeatureToFunction[var3]);
                 
                    agg_combo->_agg.push_back(prod_combo_v1);
                    // agg_combo->_m[0] = 1;

                    combo->_aggregates.push_back(agg_combo);
                    _compiler->addQuery(combo);

                    /********** PRINT OUT ********/
                    // std::cout << _td->getAttribute(var)->_name + "*" +
                    //     _td->getAttribute(var2)->_name + "*" +
                    //     _td->getAttribute(var3)->_name + "*" << 
                    //     _td->getAttribute(var3)->_name << std::endl;
                    // std::cout << combo->_rootID << std::endl;
                    /********** PRINT OUT ********/       
                }

                // add combo linear, linear, linear, linear
                for (size_t var4 = var3+1; var4 < NUM_OF_VARIABLES; ++var4)
                {
                    if (!_features[var4])
                        continue;

                    // add combo of linear, linear, linear, linear 
                    Query* combo = new Query();
                    Aggregate* agg_combo = new Aggregate();
                    prod_bitset prod_combo_v1;
                    
                    // add linear
                    if (!_categoricalFeatures[var])
                        prod_combo_v1.set(featureToFunction[var]);
                    else
                        combo->_fVars.set(var);

                    combo->_rootID = _queryRootIndex[var];

                    // add linear
                    if (!_categoricalFeatures[var2])
                        prod_combo_v1.set(featureToFunction[var2]);
                    else
                    {
                        combo->_fVars.set(var2);
                        combo->_rootID = _queryRootIndex[var2];
                    }

                    // add linear
                    if (!_categoricalFeatures[var3])
                        prod_combo_v1.set(featureToFunction[var3]);
                    else
                    {
                        combo->_fVars.set(var3);
                        combo->_rootID = _queryRootIndex[var3];
                    }

                    // add linear
                    if (!_categoricalFeatures[var4])
                        prod_combo_v1.set(featureToFunction[var4]);
                    else
                    {
                        combo->_fVars.set(var4);
                        combo->_rootID = _queryRootIndex[var4];
                    }

                    agg_combo->_agg.push_back(prod_combo_v1);
                    // agg_combo->_m[0] = 1;

                    combo->_aggregates.push_back(agg_combo);
                    _compiler->addQuery(combo);

                    /********** PRINT OUT ********/
                    // std::cout << _td->getAttribute(var)->_name + "*" +
                    //     _td->getAttribute(var2)->_name + "*" +
                    //     _td->getAttribute(var3)->_name + "*" <<
                    //     _td->getAttribute(var4)->_name << std::endl;
                    // std::cout << combo->_rootID << std::endl;
                    /********** PRINT OUT ********/
 
                }   
            }
        }
    }
    
#endif

}

void CovarianceMatrix::loadFeatures()
{
    _queryRootIndex = new size_t[NUM_OF_VARIABLES]();
    
    /* Load the two-pass variables config file into an input stream. */
    ifstream input(_pathToFiles + FEATURE_CONF);

    if (!input)
    {
        ERROR(_pathToFiles + FEATURE_CONF+" does not exist. \n");
        exit(1);
    }

    /* String and associated stream to receive lines from the file. */
    string line;
    stringstream ssLine;

    int numOfFeatures = 0;
    int degreeOfInteractions = 0;
    
    /* Ignore comment and empty lines at the top */
    while (getline(input, line))
    {
        if (line[0] == COMMENT_CHAR || line == "")
            continue;

        break;
    }
    
    /* 
     * Extract number of labels, features and interactions from the config. 
     * Parse the line with the three numbers; ignore spaces. 
     */
    bool parsingSuccess =
        qi::phrase_parse(line.begin(), line.end(),

                         /* Begin Boost Spirit grammar. */
                         ((qi::int_[phoenix::ref(numOfFeatures) = qi::_1])
                          >> NUMBER_SEPARATOR_CHAR
                          >> (qi::int_[phoenix::ref(degreeOfInteractions) =
                                       qi::_1])),
                         /*  End grammar. */
                         ascii::space);

    assert(parsingSuccess && "The parsing of the features file has failed.");
    
    // std::vector<int> linearContinuousFeatures;
    // std::vector<int> linearCategoricalFeatures;

    /* Read in the features. */
    for (int featureNo = 0; featureNo < numOfFeatures; ++featureNo)
    {
        getline(input, line);
        if (line[0] == COMMENT_CHAR || line == "")
        {
            --featureNo;
            continue;
        }

        ssLine << line;
 
        string attrName;
        /* Extract the name of the attribute in the current line. */
        getline(ssLine, attrName, ATTRIBUTE_NAME_CHAR);

        string typeOfFeature;
        /* Extract the dimension of the current attribute. */
        getline(ssLine, typeOfFeature, ATTRIBUTE_NAME_CHAR);

        string rootName;
        /* Extract the dimension of the current attribute. */
        getline(ssLine, rootName, ATTRIBUTE_NAME_CHAR);

        int attributeID = _td->getAttributeIndex(attrName);
        int categorical = stoi(typeOfFeature); 
        int rootID = _td->getRelationIndex(rootName);

        if (attributeID == -1)
        {
            ERROR("Attribute |"+attrName+"| does not exist.");
            exit(1);
        }

        if (rootID == -1)
        {
            ERROR("Relation |"+rootName+"| does not exist.");
            exit(1);
        }

        if (featureNo == 0 && categorical == 1)
        {
            ERROR("The label needs to be continuous! ");
            exit(1);
        }

        _features.set(attributeID);

        if (categorical)
        {
            _categoricalFeatures.set(attributeID);
            _queryRootIndex[attributeID] = rootID;
        }
        else
            _queryRootIndex[attributeID] = 2; //_td->_root->_id;            
        
        /* Clear string stream. */
        ssLine.clear();
    }
}
