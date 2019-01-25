#include "ComputeGroup3.h"
namespace lmfao
{
   void computeGroup3()
   {
      int64_t startProcess = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();

      int max_locn;
      int max_zip;
      bool found[2] = {}, atEnd[2] = {}, addTuple[2] = {};
      size_t leap, low, mid, high;
      size_t lowerptr_Location[2] = {}, upperptr_Location[2] = {}; 
      upperptr_Location[0] = Location.size()-1;
      size_t lowerptr_V0[2] = {}, upperptr_V0[2] = {}; 
      upperptr_V0[0] = V0.size()-1;
      size_t ptr_Location = 0;
      double *aggregates_V1 = nullptr, *aggregates_V0 = nullptr;
      double aggregateRegister[1] = {};
      double postRegister[1] = {};
      atEnd[0] = false;
      while(!atEnd[0])
      {
         max_locn = Location[lowerptr_Location[0]].locn;
         upperptr_Location[0] = lowerptr_Location[0];
         while(upperptr_Location[0]<Location.size()-1 && Location[upperptr_Location[0]+1].locn == max_locn)
         {
            ++upperptr_Location[0];
         }
         addTuple[0] = false;
         Location_tuple &LocationTuple = Location[lowerptr_Location[0]];
         V0_tuple &V0Tuple = V0[lowerptr_V0[0]];
         double *aggregates_V0 = V0Tuple.aggregates;
         upperptr_Location[1] = upperptr_Location[0];
         lowerptr_Location[1] = lowerptr_Location[0];
         upperptr_V0[1] = upperptr_V0[0];
         lowerptr_V0[1] = lowerptr_V0[0];
         atEnd[1] = false;
         memset(&postRegister[0], 0, sizeof(double) * 1);
         while(!atEnd[1])
         {
            max_zip = V0[lowerptr_V0[1]].zip;
            // Seek Value
            do
            {
               found[1] = true;
               if(max_zip > V0[upperptr_V0[0]].zip)
               {
                  atEnd[1] = true;
                  break;
               }
               else if (max_zip > V0[lowerptr_V0[1]].zip)
               {
                  leap = 1;
                  high = lowerptr_V0[1];
                  while (high <= upperptr_V0[0] && V0[high].zip < max_zip)
                  {
                     high += leap;
                     leap *= 2;
                  }
                  /* Backtrack with binary search */
                  leap /= 2;
                  high = std::min(high,upperptr_V0[0]);
                  low = high - leap; mid = 0;
                  while (low <= high)
                  {
                     mid = (high + low) / 2;
                     if (max_zip < V0[mid].zip)
                        high = mid - 1;
                     else if(max_zip > V0[mid].zip)
                        low = mid + 1;
                     else if  (low != mid)
                        high = mid;
                     else
                        break;
                  }
                  lowerptr_V0[1] = mid;
                  if (max_zip > V0[lowerptr_V0[1]].zip)
                     std::cout << "ERROR: max_zip > V0[lowerptr_V0[1]].zip\n";
               }
                  max_zip = V0[lowerptr_V0[1]].zip;
            upperptr_V0[1] = lowerptr_V0[1];
            while(upperptr_V0[1]<upperptr_V0[0] && V0[upperptr_V0[1]+1].zip == max_zip)
            {
               ++upperptr_V0[1];
            }
               if(max_zip > Location[upperptr_Location[0]].zip)
               {
                  atEnd[1] = true;
                  break;
               }
               else if (max_zip > Location[lowerptr_Location[1]].zip)
               {
                  leap = 1;
                  high = lowerptr_Location[1];
                  while (high <= upperptr_Location[0] && Location[high].zip < max_zip)
                  {
                     high += leap;
                     leap *= 2;
                  }
                  /* Backtrack with binary search */
                  leap /= 2;
                  high = std::min(high,upperptr_Location[0]);
                  low = high - leap; mid = 0;
                  while (low <= high)
                  {
                     mid = (high + low) / 2;
                     if (max_zip < Location[mid].zip)
                        high = mid - 1;
                     else if(max_zip > Location[mid].zip)
                        low = mid + 1;
                     else if  (low != mid)
                        high = mid;
                     else
                        break;
                  }
                  lowerptr_Location[1] = mid;
                  if (max_zip > Location[lowerptr_Location[1]].zip)
                     std::cout << "ERROR: max_zip > Location[lowerptr_Location[1]].zip\n";
               }
                  if (max_zip < Location[lowerptr_Location[1]].zip)
                  {
                     max_zip = Location[lowerptr_Location[1]].zip;
                     found[1] = false;
                     continue;
                  }
            upperptr_Location[1] = lowerptr_Location[1];
            while(upperptr_Location[1]<upperptr_Location[0] && Location[upperptr_Location[1]+1].zip == max_zip)
            {
               ++upperptr_Location[1];
            }
            } while (!found[1] && !atEnd[1]);
            // End Seek Value
            // If atEnd break loop
            if(atEnd[1])
               break;
            addTuple[1] = false;
            memset(&aggregateRegister[0], 0, sizeof(double) * 1);
            Location_tuple &LocationTuple = Location[lowerptr_Location[1]];
            V0_tuple &V0Tuple = V0[lowerptr_V0[1]];
            double *aggregates_V0 = V0Tuple.aggregates;
            double count = upperptr_Location[1] - lowerptr_Location[1] + 1;
            memset(&aggregateRegister[0], 0, sizeof(double) * 1);
            aggregateRegister[0] += aggregates_V0[0]*count;
            memset(addTuple, true, sizeof(bool) * 2);
            postRegister[0] += aggregateRegister[0];
            lowerptr_Location[1] = upperptr_Location[1] + 1;
            if(lowerptr_Location[1] > upperptr_Location[0])
               atEnd[1] = true;
            lowerptr_V0[1] = upperptr_V0[1] + 1;
            if(lowerptr_V0[1] > upperptr_V0[0])
               atEnd[1] = true;
         }
         if (addTuple[0]) 
         {
            V1.emplace_back(LocationTuple.locn);
            aggregates_V1 = V1.back().aggregates;
            aggregates_V1[0] += postRegister[0];
         }
         lowerptr_Location[0] = upperptr_Location[0] + 1;
         if(lowerptr_Location[0] > Location.size()-1)
            atEnd[0] = true;
      }

      int64_t endProcess = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()-startProcess;
      std::cout <<"Compute Group 3 process: "+std::to_string(endProcess)+"ms.\n";
      std::ofstream ofs("times.txt",std::ofstream::out| std::ofstream::app);
      ofs << "\t" << endProcess;
      ofs.close();

   }

}
