#ifndef INCLUDE_REGRESSIONTREENODE_HPP_
#define INCLUDE_REGRESSIONTREENODE_HPP_

#include <stdio.h>
struct Condition
{
    size_t variable;
    double threshold;
    std::string op;

    Condition() {}
};

struct RegTreeNode
{
    Condition condition;

    size_t count = 0;
    double prediction = 0.0;
    double cost = 0.0;
    bool isLeaf = true;

    RegTreeNode* lchild = nullptr;
	RegTreeNode* rchild = nullptr;

    RegTreeNode() {}
};  


#endif /* INCLUDE_REGRESSIONTREENODE_H_ */
