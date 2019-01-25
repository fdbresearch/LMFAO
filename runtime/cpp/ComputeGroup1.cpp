#include "ComputeGroup1.h"
namespace lmfao
{
   void computeGroup1()
   {
      int64_t startProcess = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();

      int max_ksn;
      bool found[1] = {}, atEnd[1] = {}, addTuple[1] = {};
      size_t leap, low, mid, high;
      size_t lowerptr_Item[1] = {}, upperptr_Item[1] = {}; 
      upperptr_Item[0] = Item.size()-1;
      size_t ptr_Item = 0;
      double *aggregates_V2 = nullptr;
      double aggregateRegister[1] = {};
      atEnd[0] = false;
      while(!atEnd[0])
      {
         max_ksn = Item[lowerptr_Item[0]].ksn;
         upperptr_Item[0] = lowerptr_Item[0];
         while(upperptr_Item[0]<Item.size()-1 && Item[upperptr_Item[0]+1].ksn == max_ksn)
         {
            ++upperptr_Item[0];
         }
         addTuple[0] = false;
         memset(&aggregateRegister[0], 0, sizeof(double) * 1);
         Item_tuple &ItemTuple = Item[lowerptr_Item[0]];
         double count = upperptr_Item[0] - lowerptr_Item[0] + 1;
         memset(&aggregateRegister[0], 0, sizeof(double) * 1);
         aggregateRegister[0] += count;
         memset(addTuple, true, sizeof(bool) * 1);
         V2.emplace_back(ItemTuple.ksn);
         aggregates_V2 = V2.back().aggregates;
         aggregates_V2[0] += aggregateRegister[0];
         lowerptr_Item[0] = upperptr_Item[0] + 1;
         if(lowerptr_Item[0] > Item.size()-1)
            atEnd[0] = true;
      }

      int64_t endProcess = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()-startProcess;
      std::cout <<"Compute Group 1 process: "+std::to_string(endProcess)+"ms.\n";
      std::ofstream ofs("times.txt",std::ofstream::out| std::ofstream::app);
      ofs << "\t" << endProcess;
      ofs.close();

   }

}
