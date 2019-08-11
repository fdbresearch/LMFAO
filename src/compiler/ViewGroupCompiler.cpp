//--------------------------------------------------------------------
//
// ViewGroupCompiler.cpp
//
// Created on: 07/10/2017
// Author: Max
//
//--------------------------------------------------------------------

#include <boost/dynamic_bitset.hpp>
#include <algorithm>
#include <fstream>
#include <string>
#include <thread>
#include <queue>
#include <unordered_set>

#include <Logging.hpp>
#include <QueryCompiler.h>
#include <ViewGroupCompiler.h>

typedef boost::dynamic_bitset<> dyn_bitset;

ViewGroupCompiler::ViewGroupCompiler(
    std::shared_ptr<Database> db,
    std::shared_ptr<TreeDecomposition> td,
    std::shared_ptr<QueryCompiler> qc) :
    _db(db), _td(td), _qc(qc)
{    
}

ViewGroupCompiler::~ViewGroupCompiler()
{
}


void ViewGroupCompiler::compile()
{
    viewToGroupMapping = new size_t[_qc->numberOfViews()];

    std::vector<std::vector<size_t>> viewsPerNode(
        _db->numberOfRelations(), std::vector<size_t>());
    
    for (size_t viewID = 0; viewID < _qc->numberOfViews(); ++viewID)
    {
        // Keep track of the views that origin from this node 
        viewsPerNode[_qc->getView(viewID)->_origin].push_back(viewID);
    }
    
    /* Next compute the topological order of the views */
    dyn_bitset processedViews(_qc->numberOfViews());
    dyn_bitset queuedViews(_qc->numberOfViews());
    
    dyn_bitset incomingViews(_qc->numberOfViews());
    
    // create orderedViewList -> which is empty
    std::vector<size_t> orderedViewList;
    std::queue<size_t> nodesToProcess;
    
    // Find all views that originate from leaf nodes
    for (const size_t& nodeID : _td->_leafNodes)
    {
        for (const size_t& viewID : viewsPerNode[nodeID])
        {
            std::vector<size_t>& incViews = _qc->getView(viewID)->_incomingViews;
            
            if (incViews.size() == 0)
            {
                queuedViews.set(viewID);
                nodesToProcess.push(viewID);
            }
        }
    }
    
    while (!nodesToProcess.empty()) // there is a node in the set
    {
        size_t viewID = nodesToProcess.front();
        nodesToProcess.pop();

        orderedViewList.push_back(viewID);
        processedViews.set(viewID);

        const size_t& destination = _qc->getView(viewID)->_destination;

        // For each view that has the same destination 
        for (const size_t& destView : viewsPerNode[destination])
        {
            // check if this has not been processed (case for root nodes)
            if (!processedViews[destView])
            {
                bool candidate = true;
                for (const size_t& incView : _qc->getView(destView)->_incomingViews)
                {    
                    if (!processedViews[incView] && !queuedViews[incView])
                    {
                        candidate = false;
                        break;
                    }
                }
                
                if (candidate && !queuedViews[destView])
                {
                    queuedViews.set(destView);
                    nodesToProcess.push(destView);
                }
            }
        }
    }

    /* Now generate the groups of views according to topological order*/

    View* prevView = _qc->getView(orderedViewList[0]);
    size_t prevOrigin = prevView->_origin;

    for (size_t incViewID : prevView->_incomingViews)
        incomingViews.set(incViewID);
    
    viewGroups.push_back(
        {orderedViewList[0],_td->getTDNode(prevOrigin)}
        );

    viewToGroupMapping[orderedViewList[0]] = 0;    
    size_t currentGroup = 0;
    for (size_t viewID = 1; viewID < orderedViewList.size(); ++viewID)
    {
        View* view = _qc->getView(orderedViewList[viewID]);
        
        size_t origin = _qc->getView(orderedViewList[viewID])->_origin;
        if (multifaq::cppgen::MULTI_OUTPUT && prevOrigin == origin)
        {
            // Combine the two into one set of
            viewGroups[currentGroup]._views.push_back(orderedViewList[viewID]);
            viewToGroupMapping[orderedViewList[viewID]] = currentGroup;
        }
        else
        {
            for (size_t incViewID = 0; incViewID < _qc->numberOfViews(); ++incViewID)
                if (incomingViews[incViewID])
                    viewGroups[currentGroup]._incomingViews.push_back(incViewID);
            
            incomingViews.reset();
            
            // Create a new group
            viewGroups.push_back({orderedViewList[viewID],
                    _td->getTDNode(_qc->getView(orderedViewList[viewID])->_origin)});
            
            ++currentGroup;
            viewToGroupMapping[orderedViewList[viewID]] = currentGroup;
        }

        for (size_t incViewID : view->_incomingViews)
            incomingViews.set(incViewID);
  
        prevOrigin = origin;
    }

    for (size_t incViewID = 0; incViewID < _qc->numberOfViews(); ++incViewID)
        if (incomingViews[incViewID])
            viewGroups[currentGroup]._incomingViews.push_back(incViewID);            

    /**************** PRINT OUT **********************/
    DINFO("Ordered View List: ");
    for (size_t& i : orderedViewList)
        DINFO(i << ", ");
    DINFO(std::endl);
    DINFO("View Groups: ");
    for (auto& group : viewGroups) {
        for (auto& i : group._views)
            DINFO(i << "  ");
        DINFO("|");
    }
    DINFO(std::endl);
    /**************** PRINT OUT **********************/
}
