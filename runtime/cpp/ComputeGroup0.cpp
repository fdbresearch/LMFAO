#include "ComputeGroup0.h"
namespace lmfao
{
   void computeGroup0()
   {
      int64_t startProcess = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();

      int max_zip;
      bool found[1] = {}, atEnd[1] = {}, addTuple[1] = {};
      size_t leap, low, mid, high;
      size_t lowerptr_Census[1] = {}, upperptr_Census[1] = {}; 
      upperptr_Census[0] = Census.size()-1;
      size_t ptr_Census = 0;
      double *aggregates_V0 = nullptr;
      double aggregateRegister[1] = {};
      atEnd[0] = false;
      while(!atEnd[0])
      {
         max_zip = Census[lowerptr_Census[0]].zip;
         upperptr_Census[0] = lowerptr_Census[0];
         while(upperptr_Census[0]<Census.size()-1 && Census[upperptr_Census[0]+1].zip == max_zip)
         {
            ++upperptr_Census[0];
         }
         addTuple[0] = false;
         memset(&aggregateRegister[0], 0, sizeof(double) * 1);
         Census_tuple &CensusTuple = Census[lowerptr_Census[0]];
         double count = upperptr_Census[0] - lowerptr_Census[0] + 1;
         memset(&aggregateRegister[0], 0, sizeof(double) * 1);
         aggregateRegister[0] += count;
         memset(addTuple, true, sizeof(bool) * 1);
         V0.emplace_back(CensusTuple.zip);
         aggregates_V0 = V0.back().aggregates;
         aggregates_V0[0] += aggregateRegister[0];
         lowerptr_Census[0] = upperptr_Census[0] + 1;
         if(lowerptr_Census[0] > Census.size()-1)
            atEnd[0] = true;
      }

      int64_t endProcess = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()-startProcess;
      std::cout <<"Compute Group 0 process: "+std::to_string(endProcess)+"ms.\n";
      std::ofstream ofs("times.txt",std::ofstream::out| std::ofstream::app);
      ofs << "\t" << endProcess;
      ofs.close();

   }

}
