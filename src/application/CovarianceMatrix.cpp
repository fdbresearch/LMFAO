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
// #include <CodegenUtils.hpp>

// #define DEGREE_TWO
static const std::string FEATURE_CONF = "/features.conf";

static const char COMMENT_CHAR = '#';
static const char NUMBER_SEPARATOR_CHAR = ',';
static const char ATTRIBUTE_NAME_CHAR = ':';

using namespace std;
using namespace multifaq::params;
namespace phoenix = boost::phoenix;
using namespace boost::spirit;



struct PartialGradient
{
    Query* query;
    std::vector<std::pair<size_t,size_t>> inputVariables;
    // vector<size_t> ouputVaribales;
};

std::vector<PartialGradient> partialGradients;

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
    generateCode();
}

void CovarianceMatrix::modelToQueries()
{
    size_t numberOfFunctions = 0;
    size_t featureToFunction[NUM_OF_VARIABLES],
        quadFeatureToFunction[NUM_OF_VARIABLES];
    
    parameterQueries = new Query*[NUM_OF_VARIABLES];

    // std::vector<PartialGradient> partialGradients;
    
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

    // --- TODO: TODO: add count query for intercept!! TODO: TODO: --- //
    Aggregate* agg_count = new Aggregate();
    agg_count->_agg.push_back(prod_bitset());
            
    Query* count = new Query();
    count->_aggregates.push_back(agg_count);
    count->_rootID = _td->_root->_id;
    _compiler->addQuery(count);

    PartialGradient countGradient;
    countGradient.query = count;
    countGradient.inputVariables =
        {{NUM_OF_VARIABLES,NUM_OF_VARIABLES}};
    partialGradients.push_back(countGradient);

#ifndef DEGREE_TWO

    size_t numberOfQueries = 0;
    
    size_t cont_var_root = _td->_root->_id;

    for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
    {
        if (!_features[var])
            continue;
        
        prod_bitset prod_linear_v1;
        prod_bitset prod_quad_v1;

        if (!_categoricalFeatures[var])
        {
            // Linear function per Feature 
            prod_linear_v1.set(featureToFunction[var]);
 
            Aggregate* agg_linear = new Aggregate();
            agg_linear->_agg.push_back(prod_linear_v1);
            
            Query* linear = new Query();
            linear->_aggregates.push_back(agg_linear);
            linear->_rootID = cont_var_root;

            _compiler->addQuery(linear);

            
            parameterQueries[var] = linear;
            
            //TODO: THIS ONE WOULD NEED TO BE MULTIPLIED BY THE PARAM FOR INTERCEPT
            PartialGradient linGradient;
            linGradient.query = linear;
            linGradient.inputVariables =
                {{var,NUM_OF_VARIABLES},{NUM_OF_VARIABLES,var}};
            partialGradients.push_back(linGradient);
            
            ++numberOfQueries;
            
            // TODO: What is the queryID ?
            // TODO: what is the variable of the intercept? NUM_OF_VARS + 1?
            // TODO: We should also add this to the NUM_OF_VARS + 1 gradient for
            // the intercept multiplied by param for this var!
            
            // Quadratic function for each feature
            prod_quad_v1.set(quadFeatureToFunction[var]);
            
            Aggregate* agg_quad = new Aggregate();
            agg_quad->_agg.push_back(prod_quad_v1);
            
            Query* quad = new Query();
            quad->_aggregates.push_back(agg_quad);
            quad->_rootID = _td->_root->_id; // cont_var_root;

            _compiler->addQuery(quad);

            // TODO: THIS ONE WOULD NEED TO BE MULTIPLIED BY THE PARAM FOR THIS var!!
            PartialGradient quadGradient;
            quadGradient.query = quad;
            quadGradient.inputVariables = {{var,var}};
            partialGradients.push_back(quadGradient);

            ++numberOfQueries;
        }
        else
        {
            // v1_root = _queryRootIndex[var];

            Aggregate* agg_linear = new Aggregate();
            agg_linear->_agg.push_back(prod_linear_v1);
            
            Query* linear = new Query();
            linear->_aggregates.push_back(agg_linear);
            linear->_fVars.set(var);
            linear->_rootID = _queryRootIndex[var];
            _compiler->addQuery(linear);

            parameterQueries[var] = linear;

            // THIS ONE WOULD NEED TO BE MULTIPLIED BY THE PARAM FOR INTERCEPT

            //TODO: THIS ONE WOULD NEED TO BE MULTIPLIED BY THE PARAM FOR INTERCEPT
            PartialGradient linGradient;
            linGradient.query = linear;
            linGradient.inputVariables =
                {{var,NUM_OF_VARIABLES},{NUM_OF_VARIABLES,var}};
            partialGradients.push_back(linGradient);

            ++numberOfQueries;
        }
        

        for (size_t var2 = var+1; var2 < NUM_OF_VARIABLES; ++var2)
        {
            if (_features[var2])
            {
                //*****************************************//
                prod_bitset prod_quad_v2 = prod_linear_v1;
                
                Aggregate* agg_quad_v2 = new Aggregate();
                
                Query* quad_v2 = new Query();
                quad_v2->_rootID = _td->_root->_id;
                
                if (_categoricalFeatures[var])
                {
                    quad_v2->_rootID = _queryRootIndex[var];
                    quad_v2->_fVars.set(var);
                }
                
                if (_categoricalFeatures[var2])
                {
                    quad_v2->_fVars.set(var2);
                    // If both varaibles are categoricalVars - we choose the
                    // var2 as the root
                    // TODO: We should use the root that is highest to the
                    // original root
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
                
                // TODO: Here we add both sides! multiply var times param of
                // var2 -- add that to gradient of var. And then multiply var2
                // times param of var and add to gradient of var2! -- this can
                // be done togther in one pass over the vector!
                PartialGradient quadGradient;
                quadGradient.query = quad_v2;
                quadGradient.inputVariables = {{var,var2},{var2,var}};
                partialGradients.push_back(quadGradient);
                
                ++numberOfQueries;
                
                // TODO: once we have identified the queries and how they are
                // multiplied, we then need to identify the actual views that
                // are computed over and use the information group computation
                // over this view!
            }
        }
    }

#else

    size_t cubicFeatureToFunction[NUM_OF_VARIABLES];
    size_t quarticFeatureToFunction[NUM_OF_VARIABLES];
    
    for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
    {
        if (_features.test(var) && !_categoricalFeatures.test(var))
        {
            _compiler->addFunction(new Function({var}, Operation::cubic_sum));
            cubicFeatureToFunction[var] = numberOfFunctions;
            numberOfFunctions++;
        }
    }

    for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
    {
        if (_features.test(var) && !_categoricalFeatures.test(var))
        {
            _compiler->addFunction(new Function({var}, Operation::quartic_sum));
            quarticFeatureToFunction[var] = numberOfFunctions;
            numberOfFunctions++;
        }
    }
    
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
                //   _td->getAttribute(var)->_name + "*" +_td->getAttribute(var)->_name+
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


    // for (size_t q = 0; q < _compiler->numberOfQueries(); q++)
    // {
    //     Query* query = _compiler->getQuery(q);
        
    //     if (query->_fVars.none())
    //     {
    //         std::cout << q << " : " << std::to_string(query->_rootID) << std::endl;
    //     }
    // }

    // exit(1);

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

        _isFeature.set(attributeID);
        _features.set(attributeID);

        Feature f;

        if (categorical)
        {
            _categoricalFeatures.set(attributeID);
            _queryRootIndex[attributeID] = rootID;

            f.head.set(attributeID);
            _isCategoricalFeature.set(attributeID);
        }
        else
        {
            _queryRootIndex[attributeID] = _td->_root->_id;
            ++f.body[attributeID];
        }

        _listOfFeatures.push_back(f);

        if (featureNo == 0)
            labelID = attributeID;
        
        /* Clear string stream. */
        ssLine.clear();
    }
}


// TODO: Can we ensure that the continuous parameters are in the order in which
// they are needed for the computation for continuous aggregates?


std::string CovarianceMatrix::generateParameters()
{
    _parameterIndex.resize(NUM_OF_VARIABLES+1);
    
    std::string constructParam = offset(1)+"size_t numberOfParameters;\n"+
        offset(1)+"double *params = nullptr, *grad = nullptr, error, *lambda, "+
        "*prevParams = nullptr, *prevGrad = nullptr;\n";
    
    std::string constructGrad = "";

    std::string numOfParameters = "numberOfParameters = ";
    size_t numOfContParameters = 1;
    
    // Computing the number of parameters we have
    for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
    {
        if (!_features.test(var))
            continue;
        if (!_categoricalFeatures.test(var))
        {
            _parameterIndex[var] = numOfContParameters;
            ++numOfContParameters;
        }
        else
        {
            size_t& viewID = parameterQueries[var]->_aggregates[0]->_incoming[0].first;
            numOfParameters += "V"+std::to_string(viewID)+".size() + ";
        }
    }
    _parameterIndex[NUM_OF_VARIABLES] = 0;
    numOfParameters += std::to_string(numOfContParameters+1)+";\n";
    
    std::string initParam = offset(2)+numOfParameters+
        offset(2)+"params = new double[numberOfParameters];\n"+
        offset(2)+"grad = new double[numberOfParameters]();\n"+
        offset(2)+"prevParams = new double[numberOfParameters];\n"+
        offset(2)+"prevGrad = new double[numberOfParameters];\n"+
        offset(2)+"lambda = new double[numberOfParameters];\n";

    // TODO: make sure you initialize the params and grad arrays
    if (_categoricalFeatures.any()) 
        initParam += offset(2)+"size_t categIndex = "+
            std::to_string(numOfContParameters+1)+";\n";
    
    for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
    {
        if (!_features.test(var))
            continue;

        if (_categoricalFeatures.test(var))
        {
            size_t& viewID = parameterQueries[var]->_aggregates[0]->_incoming[0].first;
            
            constructGrad += offset(1)+"std::unordered_map<int,size_t> index_"+
                getAttributeName(var)+";\n";

            initParam += offset(2)+"index_"+getAttributeName(var)+".reserve(V"+
                std::to_string(viewID)+".size());\n";
            // offset(2)+"firstindex_"+getAttributeName(var)+" = categIndex;\n";

            initParam += offset(2)+"for (V"+std::to_string(viewID)+
                "_tuple& tuple : V"+
                std::to_string(viewID)+")\n"+
                offset(3)+"index_"+getAttributeName(var)+
                "[tuple."+getAttributeName(var)+"] = categIndex++;\n";
        }
    }

    initParam += offset(2)+"for(size_t j = 0; j < numberOfParameters; ++j)\n"+
        offset(2)+"{\n"+
        offset(3)+"params[j] = ((double) (rand() % 500 + 1) - 100) / 100;\n"+
        offset(3)+"lambda[j] = 0.1;\n"+
        offset(2)+"}\n"+
        offset(2)+"params["+std::to_string(_parameterIndex[labelID])+"] = -1;\n"+
        offset(2)+"lambda[0] = 0.0;\n";
    
    std::string outString = constructParam+constructGrad+"\n"+
        offset(1)+"void initParametersGradients()\n"+
        offset(1)+"{\n"+initParam+offset(1)+"}\n";
    
    return outString;
}

struct ActualGradient
{
    size_t aggNum;
    std::vector<std::pair<size_t,size_t>> incVars;
};

std::string CovarianceMatrix::getAttributeName(size_t attID)
{
    if (attID == NUM_OF_VARIABLES)
        return "int";
    return _td->getAttribute(attID)->_name;
}



std::string CovarianceMatrix::generateGradients()
{
    size_t numberOfViews = _compiler->numberOfViews();

    std::vector<std::vector<ActualGradient>> actualGradients(numberOfViews);
        
    for(PartialGradient& pGrad : partialGradients)
    {
        // get view and aggregate corresponding to the query
        std::pair<size_t,size_t>& viewAggPair =
            pGrad.query->_aggregates[0]->_incoming[0];
        
        // Assign to view, the aggregate and the corresponding param, grad pair
        ActualGradient ag;
        ag.aggNum = viewAggPair.second;
        ag.incVars = pGrad.inputVariables;

        actualGradients[viewAggPair.first].push_back(ag);
    }

    // TODO: *Observations:* the view with all continuous aggregates only has one tuple
    // Ideally, we would have the corresponding params in an array so this
    // computation can be vectorized ! Ideally also the gradients should be a vector !
    
    std::string gradString =  offset(1)+"void computeGradient()\n"+offset(1)+"{\n"+
        offset(2)+"memset(grad, 0, numberOfParameters * "+
        std::to_string(sizeof(double))+");\n";

    std::string errorString =  offset(1)+"void computeError()\n"+offset(1)+"{\n"+
        offset(2)+"error = 0.0;\n";
    
    for (size_t viewID = 0; viewID < numberOfViews; ++viewID)
    {
        // TODO: check if this view has at least one free variable
        // If it does not then avoid the for loop, get the tuple reference and
        // adjust the offset
        bool viewHasFreeVars = _compiler->getView(viewID)->_fVars.any();
        size_t off = 3;
        
        if (!actualGradients[viewID].empty())
        {
            if (viewHasFreeVars)
            {
                gradString += offset(2)+"for (V"+std::to_string(viewID)+
                    "_tuple& tuple : V"+std::to_string(viewID)+")\n"+offset(2)+"{\n";
                errorString += offset(2)+"for (V"+std::to_string(viewID)+
                    "_tuple& tuple : V"+std::to_string(viewID)+")\n"+offset(2)+"{\n";
            }
            else
            {
                gradString += offset(2)+"V"+std::to_string(viewID)+
                    "_tuple& tuple = V"+std::to_string(viewID)+"[0];\n";
                errorString += offset(2)+"V"+std::to_string(viewID)+
                    "_tuple& tuple = V"+std::to_string(viewID)+"[0];\n";
                off = 2;
            }
        }
        
        for (ActualGradient& ag : actualGradients[viewID])
        {
            size_t prevGradID = 1000;
            size_t prevParamID = 1000;

            // TODO: TODO: We go at great length to avoid adding errors twice
            // here, this could surely be simplified by adding each combination
            // to actualGradients only once!
            
            for (size_t i = 0; i < ag.incVars.size(); i++)
            {
                size_t& gradID = ag.incVars[i].first;
                size_t& paramID = ag.incVars[i].second;

                if (gradID == labelID)
                    continue;

                if (prevGradID != paramID || prevParamID != gradID) 
                    errorString += offset(off)+ "error += ";

                if (gradID == paramID &&
                    (prevGradID != paramID || prevParamID != gradID))
                    errorString += "0.5 * ";
                
                if (_categoricalFeatures[gradID])
                {   
                    gradString += offset(off)+"grad[index_"+getAttributeName(gradID)+
                        "[tuple."+getAttributeName(gradID)+"]] += ";

                    if (prevGradID != paramID || prevParamID != gradID)
                        errorString += "params[index_"+getAttributeName(gradID)+
                            "[tuple."+getAttributeName(gradID)+"]] * ";
                }
                else
                {
                    gradString += offset(off)+"grad["+
                        std::to_string(_parameterIndex[gradID])+"] += ";
                    
                    if (prevGradID != paramID || prevParamID != gradID)
                        errorString += "params["+
                            std::to_string(_parameterIndex[gradID])+"] * ";
                }
                
                gradString += "tuple.aggregates["+std::to_string(ag.aggNum)+"] * ";

                if (prevGradID != paramID || prevParamID != gradID)
                    errorString += "tuple.aggregates["+std::to_string(ag.aggNum)+"] * ";
            
                if (_categoricalFeatures[paramID])
                {
                    gradString += "params[index_"+getAttributeName(paramID)+
                        "[tuple."+getAttributeName(paramID)+"]];\n";
                    
                    if (prevGradID != paramID || prevParamID != gradID)
                        errorString += "params[index_"+getAttributeName(paramID)+
                            "[tuple."+getAttributeName(paramID)+"]];\n";
                }
                else
                {
                    // gradString += "param_"+getAttributeName(paramID)+";\n";
                    gradString += "params["+
                        std::to_string(_parameterIndex[paramID])+"];\n";

                    if (prevGradID != paramID || prevParamID != gradID)
                        errorString += "params["+
                            std::to_string(_parameterIndex[paramID])+"];\n";
                }

                prevGradID = gradID;
                prevParamID = paramID;
            }
        }
        if (!actualGradients[viewID].empty() && viewHasFreeVars)
        {    
            gradString += offset(2)+"}\n";
            errorString += offset(2)+"}\n";
        }
        
    }
    gradString += offset(2)+"for (size_t j = 0; j < numberOfParameters; ++j)\n"+
        offset(3)+"grad[j] /= tuple.aggregates[0];\n"+
        offset(1)+"}\n\n";

    std::string labelStr = std::to_string(_parameterIndex[labelID]);
    
    errorString +=  offset(2)+"error /= tuple.aggregates[0];\n\n"+
        offset(2)+"double paramNorm = 0.0;\n"+
        offset(2)+"/* Adding the regulariser to the error */\n"+
        offset(2)+"for (size_t j = 1; j < numberOfParameters; ++j)\n"+
        offset(3)+"paramNorm += params[j] * params[j];\n"+
        offset(2)+"paramNorm -= params["+labelStr+"] * params["+labelStr+"];\n\n"+
        offset(2)+"error += lambda[1] * paramNorm;\n"+
        offset(1)+"}\n";

    return gradString + errorString;
}


inline std::string CovarianceMatrix::offset(size_t off)
{
    return std::string(off*3, ' ');
}

void CovarianceMatrix::generateCode()
{
    // This is necessary here, because it constructs the firstEntry array used below
    std::string parameterGeneration = generateParameters();
    
    std::string stepSizeFunction = offset(1)+"inline double computeStepSize()\n"+
        offset(1)+"{\n"+
        offset(2)+"double DSS = 0.0, GSS = 0.0, DGS = 0.0, paramDiff, gradDiff;\n"+
        offset(2)+"for(size_t j = 0; j < numberOfParameters; ++j)\n"+offset(2)+"{\n"+
        offset(3)+"if (j == "+std::to_string(_parameterIndex[labelID])+") continue;\n\n"+
        offset(3)+"paramDiff = params[j] - prevParams[j];\n"+
        offset(3)+"gradDiff = grad[j] - prevGrad[j];\n\n"+
        offset(3)+"DSS += paramDiff * paramDiff;\n"+
        offset(3)+"GSS += gradDiff * gradDiff;\n"+
        offset(3)+"DGS += paramDiff * gradDiff;\n"+
        offset(2)+"}\n"+
        offset(2)+"if (DGS == 0.0 || GSS == 0.0) return 0.1;\n"+
        offset(2)+"double Ts = DSS / DGS;\n"+
        offset(2)+"double Tm = DGS / GSS;\n"+
        offset(2)+"if (Tm < 0.0 || Ts < 0.0) return 0.1;\n"+
        offset(2)+"return (Tm / Ts > 0.5)? Tm : Ts - 0.5 * Tm;\n"+
        offset(1)+"}\n";
    
    std::string runFunction = offset(1)+"void runApplication()\n"+offset(1)+"{\n"+
        offset(2)+"initParametersGradients();\n"+
        generateConvergenceLoop()+
        offset(2)+"printOutput();\n"+
        offset(2)+"std::cout << \"numberOfIterations: \" << iteration << \"\\n\";\n"+
        offset(2)+"delete[] update;\n"+
        offset(1)+"}\n";
    
    std::ofstream ofs("runtime/cpp/ApplicationHandler.h", std::ofstream::out);
    ofs << "#ifndef INCLUDE_APPLICATIONHANDLER_HPP_\n"<<
        "#define INCLUDE_APPLICATIONHANDLER_HPP_\n\n"<<
        "#include \"DataHandler.h\"\n\n"<<
        "namespace lmfao\n{\n"<<
        offset(1)+"void runApplication();\n"<<
        "}\n\n#endif /* INCLUDE_APPLICATIONHANDLER_HPP_*/\n";    
    ofs.close();

    ofs.open("runtime/cpp/ApplicationHandler.cpp", std::ofstream::out);
    ofs << "#include \"ApplicationHandler.h\"\n#include <cmath>\nnamespace lmfao\n{\n";
    ofs << parameterGeneration << std::endl;
    ofs << generateGradients() << std::endl;
    ofs << generatePrintFunction() << std::endl;
    ofs << stepSizeFunction << std::endl;
    ofs << runFunction << std::endl;
    ofs << "}\n";
    ofs.close();
}

std::string CovarianceMatrix::generateConvergenceLoop()
{
    std::string convergenceCheck =
        offset(3)+"/* Normalized residual stopping condition */\n"+
	offset(3)+"if (sqrt(gradientNorm) / (firstGradientNorm + 0.01) < 0.000001)\n"+
        offset(3)+"{\n"+
        // offset(4)+"converged = true;\n"+
        offset(4)+"std::cout << \"We have converged!! \\n\";\n"+
        offset(4)+"break;\n"+
        offset(3)+"}\n";

    std::string labelStr = std::to_string(_parameterIndex[labelID]);
    
    std::string updateParams = offset(3)+"gradientNorm = 0.0;\n"+
        offset(3)+"for (int j = 0; j < numberOfParameters; ++j)\n"+
        offset(3)+"{\n"+
        offset(4)+"update[j] = grad[j] + lambda[j] * params[j];\n"+
        offset(4)+"gradientNorm += update[j] * update[j];\n"+
        offset(4)+"prevParams[j] = params[j];\n"+
        offset(4)+"prevGrad[j] = grad[j];\n"+
        offset(4)+"params[j] = params[j] - stepSize * update[j];\n"+
        offset(3)+"}\n"+
        offset(3)+"params["+labelStr+"] = -1;\n"+
        offset(3)+"prevParams["+labelStr+"] = -1;\n"+
        offset(3)+"gradientNorm -= update["+labelStr+"] * update["+labelStr+"];\n\n"+
        offset(3)+"computeError();\n\n";
    
    std::string backtracking =
        offset(3)+"/* Backtracking Line Search: Decrease stepSize until condition"+
        " is satisfied */\n"+offset(3)+"backtrackingSteps = 0;\n"+
        offset(3)+"while(error > previousError - (stepSize/2) * gradientNorm &&"+
        " backtrackingSteps < 500)\n"+
        offset(3)+"{\n"+
        offset(4)+"stepSize /= 2;"+
        offset(4)+"// Update parameters based on the new stepSize.\n"+
        offset(4)+"for (int j = 0; j < numberOfParameters; ++j)\n"+
        offset(5)+"params[j] = prevParams[j] - stepSize * update[j];\n"+
        offset(4)+"params["+std::to_string(_parameterIndex[labelID])+"] = -1;\n"+
        offset(4)+"computeError();\n"+
        offset(4)+"++backtrackingSteps;\n"+
        offset(3)+"}\n";
    
    // TODO: TODO: TODO: SHOULD THE GRADIENT NORM INCLUDE THE REGULARIZER!!?!?
    return offset(2)+"double stepSize = 0.1, gradientNorm = 0.0, "+
        "*update = new double[numberOfParameters];\n"+
        offset(2)+"size_t iteration = 0, backtrackingSteps = 0;\n\n"+
        offset(2)+"computeGradient();\n"+
        offset(2)+"for (int j = 0; j < numberOfParameters; ++j)\n"+offset(2)+"{\n"+
        offset(3)+"update[j] = grad[j] + lambda[j] * params[j];\n"+
        offset(3)+"gradientNorm += update[j] * update[j];\n"+offset(2)+"}\n"+
        offset(2)+"gradientNorm -= update["+labelStr+"] * update["+labelStr+"];\n"+
        offset(2)+"double firstGradientNorm = sqrt(gradientNorm);\n\n"+
        offset(2)+"computeError();\n"+
        offset(2)+"double previousError = error;\n\n"+
        offset(2)+"do\n"+offset(2)+"{\n"+
        updateParams+backtracking+convergenceCheck+
        offset(3)+"computeGradient();\n"+
        offset(3)+"stepSize = computeStepSize();\n"+
        offset(3)+"previousError = error;\n"+
        offset(3)+"++iteration;\n"+
        offset(2)+"} while (iteration < 1000);\n\n";
}


std::string CovarianceMatrix::generatePrintFunction()
{
    std::string printParam = offset(2)+"std::cout << \"param_int = \" << "+
        "params["+std::to_string(_parameterIndex[NUM_OF_VARIABLES])+"] << \";\\n\";\n";
    
    std::string printGrad = offset(2)+"std::cout << \"grad_int = \" << "+
        "grad["+std::to_string(_parameterIndex[NUM_OF_VARIABLES])+"] << \";\\n\";\n";
    
    for (size_t var = 0; var < NUM_OF_VARIABLES; ++var)
    {
        if (!_features.test(var))
            continue;

        if (!_categoricalFeatures.test(var))
        {
            printParam += offset(2)+"std::cout << \"param_"+getAttributeName(var)+" = "
                "\" << params["+std::to_string(_parameterIndex[var])+"] << \";\\n\";\n";
            printGrad += offset(2)+"std::cout << \"grad_"+getAttributeName(var)+" = "
                "\" << grad["+std::to_string(_parameterIndex[var])+"] <<\";\\n\";\n";
        }
        else
        {            
            printParam += offset(2)+"for (std::pair<int,size_t> tuple : index_"+
                getAttributeName(var)+")\n"+
                offset(3)+"std::cout << \"param_"+getAttributeName(var)+
                "[\"<< tuple.first <<\"] = \"<< params[tuple.second] << \";\\n\";\n";
            printGrad += offset(2)+"for (std::pair<int,size_t> tuple : index_"+
                getAttributeName(var)+")\n"+
                offset(3)+"std::cout << \"grad_"+getAttributeName(var)+
                "[\"<< tuple.first <<\"] = \"<< grad[tuple.second] << \";\\n\";\n";
        }
    }

    return offset(1)+"void printOutput()\n"+offset(1)+"{\n"+printParam+"\n"+
        printGrad+offset(1)+"}\n";
}
