#ifndef INCLUDE_REGRESSIONTREENODE_HPP_
#define INCLUDE_REGRESSIONTREENODE_HPP_

#include <stdio.h>
struct Condition
{
    size_t variable;
    double threshold;
    std::string op;

    Condition() 
    {
    };

};

struct RegTreeNode
{
    Condition condition;

    // Set of parent conditions, stored as a product of functions. 
    // std::vector<Condition> _parentConditions;

    size_t count;
    double prediction;
    double variance;

    RegTreeNode* lchild;
	RegTreeNode* rchild;

    RegTreeNode() {}
};  


#endif /* INCLUDE_REGRESSIONTREENODE_H_ */
