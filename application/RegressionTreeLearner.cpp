#ifndef INCLUDE_REGRESSIONTREELEARNER_HPP_
#define INCLUDE_REGRESSIONTREELEARNER_HPP_

#include <fstream>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sstream>
#include <vector>

#include "RegressionTreeNode.hpp"
#include "../runtime/cpp/DynamicFunctionsGenerator.hpp"

const size_t max_depth = 2;
const double min_size = 1000.0;

void loadSplit(RegTreeNode* node, double& leftCount, double& rightCount)
{
    std::ifstream input("bestsplit.out");
    if (!input)
    {
        std::cout << "bestsplit.out does is not exist. \n";
        exit(1);
    }

    std::string line, attr;
    getline(input, line);

    int varID;
    double threshold;
    bool categorical;

    std::stringstream ss(line);

    getline(ss, attr, '\t');
    varID = std::stoi(attr);

    getline(ss, attr, '\t');
    threshold = std::stod(attr);

    getline(ss, attr, '\t');
    categorical = std::stoi(attr);

    getline(ss, attr, '\t');
    leftCount = std::stod(attr);
    
    getline(ss, attr, '\t');
    rightCount = std::stod(attr);

    getline(ss, attr, '\t');
    leftCount = std::stod(attr);
    
    getline(ss, attr, '\t');
    rightCount = std::stod(attr);
    
    Condition* cond = new Condition();
    node->condition.variable = varID;
    node->condition.threshold = threshold;
    if (categorical)
    {
        node->condition.op = "==";
        rightCount -= leftCount;
    }
    else
        node->condition.op = "<=";    

    input.close();
}

void splitNode(RegTreeNode* node, std::vector<Condition> conditions, size_t depth)
{
    // Overwrite the Dynamic Functions following the conditions of this node 
    generateDynamicFunctions(conditions);

    system("make");
    system("./lmfao");

    // Load the new split proposed by LMFAO and the corresponding stats
    double leftCount, rightCount;
    loadSplit(node, leftCount, rightCount);

    // Check if you continue Splitting left
    if (depth == max_depth)
        return;
    
    // Push it to the queue -- if it should be split further 
    if (leftCount > min_size)
    {
        // pushCondition to the the vector
        std::vector<Condition> left = conditions; 
        left.push_back(node->condition);

        RegTreeNode* leftNode = new RegTreeNode();
        node->lchild = leftNode;

        splitNode(leftNode, left, depth+1);
    }


    // Push it to the queue -- if it should be split further
    if (rightCount > min_size)
    {
        // pushCondition to the the vector
        std::vector<Condition> right = conditions; 

        Condition rightCondition = node->condition; 
        if (rightCondition.op.compare("=") == 0)
            rightCondition.op = "!=";
        if (rightCondition.op.compare("<=") == 0)
            rightCondition.op = ">";

        right.push_back(rightCondition);

        RegTreeNode* rightNode = new RegTreeNode();
        node->rchild = rightNode;

        splitNode(rightNode, right, depth+1);
    }
}

int main(int argc, char *argv[])
{
    RegTreeNode* root = new RegTreeNode();
    std::vector<Condition> conditions;
    
    splitNode(root, conditions, 1);

    return 0;
};

#endif /* INCLUDE_REGRESSIONTREELEARNER_H_ */
