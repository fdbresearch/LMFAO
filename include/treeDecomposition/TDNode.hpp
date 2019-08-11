//--------------------------------------------------------------------
//
// TDNode.hpp
//
// Created on: 12/09/2017
// Author: Max
//
//--------------------------------------------------------------------

#ifndef INCLUDE_TDNODE_HPP_
#define INCLUDE_TDNODE_HPP_

#include <bitset>
#include <string>
#include <vector>

#include <GlobalParams.hpp>

class TDNode
{
  
public:

    TDNode(std::string name, size_t ID) : _name(name), _id(ID) { }

    ~TDNode()
    {
        delete[] _neighborSchema;
    }

    //! Name of the node (relation name).
    std::string _name;

    //! ID of the node.
    size_t _id;
   
    //! Parent of the current node
    TDNode* _parent = nullptr;

    //! Number of children of current node.
    size_t _numOfNeighbors;

    //! Array of neighbors.
    std::vector<size_t> _neighbors;
    
    // Bag of this TD node.
    std::bitset<multifaq::params::NUM_OF_VARIABLES> _bag;
    
    //! Bitset that indicates schema of subtree root at neighbors 
    std::bitset<multifaq::params::NUM_OF_VARIABLES>* _neighborSchema = nullptr;
    
    //! Number of threads for this relations 
    size_t _threads = 1;

    /**
     * Adds node to child list by appending to the end of linked list.
     */
    // void addChild(TDNode* node)
    // {
    //     if (node == NULL)
    //         return;
    //     if (this->_firstChild == NULL)
    //     {
    //         /* It is first child. */
    //         this->_firstChild = node;
    //         this->_lastChild = node;
    //         node->_next = NULL;
    //         node->_prev = NULL;
    //     }
    //     else
    //     {
    //         /* We know there is already at least one child. */
    //         this->_lastChild->_next = node;
    //         node->_prev = this->_lastChild;
    //         this->_lastChild = node;
    //         node->_next = NULL;
    //     }
    //     ++this->_numOfChildren;
    //     node->_parent = this;
    // }
    
};

#endif /* INCLUDE_TDNODE_HPP_ */ 
