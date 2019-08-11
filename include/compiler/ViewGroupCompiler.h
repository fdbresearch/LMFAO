//--------------------------------------------------------------------
//
// ViewGroupCompiler.h
//
// Created on: 07/10/2017
// Author: Max
//
// The view group compiler reads in a list of views and groups them 
// into view groups. 
//--------------------------------------------------------------------

#ifndef INCLUDE_VIEWGROUPCOMPILER_H_
#define INCLUDE_VIEWGROUPCOMPILER_H_

#include <QueryCompiler.h>

class ViewGroupCompiler 
{
public:

    ViewGroupCompiler(std::shared_ptr<Database> db,
                      std::shared_ptr<TreeDecomposition> td,
                      std::shared_ptr<QueryCompiler> qc);

    ~ViewGroupCompiler();

    void compile();
    
    size_t numberOfGroups()
    {
        return viewGroups.size();
    }

    ViewGroup& getGroup(size_t group_id) 
    {
        return viewGroups[group_id];
    }
    
private:
    std::shared_ptr<Database> _db;
    
    std::shared_ptr<TreeDecomposition> _td;
    
    std::shared_ptr<QueryCompiler> _qc;

    // For groups of views that can be computed together
    std::vector<ViewGroup> viewGroups;

    // Mapping from View ID to Group ID
    size_t* viewToGroupMapping = nullptr;
    
    // void incomingViews();
};




#endif /* INCLUDE_VIEWGROUPCOMPILER_H_ */
