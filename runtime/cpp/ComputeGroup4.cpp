#include "ComputeGroup4.h"
namespace lmfao
{
   void computeGroup4()
   {
      int64_t startProcess = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();

      int max_locn;
      int max_dateid;
      int max_ksn;
      bool found[3] = {}, atEnd[3] = {}, addTuple[3] = {};
      size_t leap, low, mid, high;
      size_t lowerptr_Inventory[3] = {}, upperptr_Inventory[3] = {}; 
      upperptr_Inventory[0] = Inventory.size()-1;
      size_t lowerptr_V1[3] = {}, upperptr_V1[3] = {}; 
      upperptr_V1[0] = V1.size()-1;
      size_t lowerptr_V2[3] = {}, upperptr_V2[3] = {}; 
      upperptr_V2[0] = V2.size()-1;
      size_t lowerptr_V3[3] = {}, upperptr_V3[3] = {}; 
      upperptr_V3[0] = V3.size()-1;
      size_t ptr_Inventory = 0;
      double *aggregates_V4 = nullptr, *aggregates_V1 = nullptr, *aggregates_V2 = nullptr, *aggregates_V3 = nullptr;
      double aggregateRegister[3] = {};
      double postRegister[3] = {};
      atEnd[0] = false;
      memset(&postRegister[2], 0, sizeof(double) * 1);
      while(!atEnd[0])
      {
         max_locn = V1[lowerptr_V1[0]].locn;
         // Seek Value
         do
         {
            found[0] = true;
            if(max_locn > V1[V1.size()-1].locn)
            {
               atEnd[0] = true;
               break;
            }
            else if (max_locn > V1[lowerptr_V1[0]].locn)
            {
               leap = 1;
               high = lowerptr_V1[0];
               while (high <= V1.size()-1 && V1[high].locn < max_locn)
               {
                  high += leap;
                  leap *= 2;
               }
               /* Backtrack with binary search */
               leap /= 2;
               high = std::min(high,V1.size()-1);
               low = high - leap; mid = 0;
               while (low <= high)
               {
                  mid = (high + low) / 2;
                  if (max_locn < V1[mid].locn)
                     high = mid - 1;
                  else if(max_locn > V1[mid].locn)
                     low = mid + 1;
                  else if  (low != mid)
                     high = mid;
                  else
                     break;
               }
               lowerptr_V1[0] = mid;
               if (max_locn > V1[lowerptr_V1[0]].locn)
                  std::cout << "ERROR: max_locn > V1[lowerptr_V1[0]].locn\n";
            }
               max_locn = V1[lowerptr_V1[0]].locn;
         upperptr_V1[0] = lowerptr_V1[0];
         while(upperptr_V1[0]<V1.size()-1 && V1[upperptr_V1[0]+1].locn == max_locn)
         {
            ++upperptr_V1[0];
         }
            if(max_locn > V3[V3.size()-1].locn)
            {
               atEnd[0] = true;
               break;
            }
            else if (max_locn > V3[lowerptr_V3[0]].locn)
            {
               leap = 1;
               high = lowerptr_V3[0];
               while (high <= V3.size()-1 && V3[high].locn < max_locn)
               {
                  high += leap;
                  leap *= 2;
               }
               /* Backtrack with binary search */
               leap /= 2;
               high = std::min(high,V3.size()-1);
               low = high - leap; mid = 0;
               while (low <= high)
               {
                  mid = (high + low) / 2;
                  if (max_locn < V3[mid].locn)
                     high = mid - 1;
                  else if(max_locn > V3[mid].locn)
                     low = mid + 1;
                  else if  (low != mid)
                     high = mid;
                  else
                     break;
               }
               lowerptr_V3[0] = mid;
               if (max_locn > V3[lowerptr_V3[0]].locn)
                  std::cout << "ERROR: max_locn > V3[lowerptr_V3[0]].locn\n";
            }
               if (max_locn < V3[lowerptr_V3[0]].locn)
               {
                  max_locn = V3[lowerptr_V3[0]].locn;
                  found[0] = false;
                  continue;
               }
         upperptr_V3[0] = lowerptr_V3[0];
         while(upperptr_V3[0]<V3.size()-1 && V3[upperptr_V3[0]+1].locn == max_locn)
         {
            ++upperptr_V3[0];
         }
            if(max_locn > Inventory_locn[Inventory.size()-1])
            {
               atEnd[0] = true;
               break;
            }
            else if (max_locn > Inventory_locn[lowerptr_Inventory[0]])
            {
               leap = 1;
               high = lowerptr_Inventory[0];
               while (high <= Inventory.size()-1 && Inventory_locn[high] < max_locn)
               {
                  high += leap;
                  leap *= 2;
               }
               /* Backtrack with binary search */
               leap /= 2;
               high = std::min(high,Inventory.size()-1);
               low = high - leap; mid = 0;
               while (low <= high)
               {
                  mid = (high + low) / 2;
                  if (max_locn < Inventory_locn[mid])
                     high = mid - 1;
                  else if(max_locn > Inventory_locn[mid])
                     low = mid + 1;
                  else if  (low != mid)
                     high = mid;
                  else
                     break;
               }
               lowerptr_Inventory[0] = mid;
               if (max_locn > Inventory_locn[lowerptr_Inventory[0]])
                  std::cout << "ERROR: max_locn > Inventory[lowerptr_Inventory[0]].locn\n";
            }
               if (max_locn < Inventory_locn[lowerptr_Inventory[0]])
               {
                  max_locn = Inventory_locn[lowerptr_Inventory[0]];
                  found[0] = false;
                  continue;
               }
         upperptr_Inventory[0] = lowerptr_Inventory[0];
         while(upperptr_Inventory[0]<Inventory.size()-1 && Inventory_locn[upperptr_Inventory[0]+1] == max_locn)
         {
            ++upperptr_Inventory[0];
         }
         } while (!found[0] && !atEnd[0]);
         // End Seek Value
         // If atEnd break loop
         if(atEnd[0])
            break;
         addTuple[0] = false;
         memset(&aggregateRegister[0], 0, sizeof(double) * 1);
         // Inventory_tuple &InventoryTuple = Inventory[lowerptr_Inventory[0]];
         V1_tuple &V1Tuple = V1[lowerptr_V1[0]];
         double *aggregates_V1 = V1Tuple.aggregates;
         V2_tuple &V2Tuple = V2[lowerptr_V2[0]];
         double *aggregates_V2 = V2Tuple.aggregates;
         V3_tuple &V3Tuple = V3[lowerptr_V3[0]];
         double *aggregates_V3 = V3Tuple.aggregates;
         memset(&aggregateRegister[0], 0, sizeof(double) * 1);
         aggregateRegister[0] += aggregates_V1[0];
         upperptr_Inventory[1] = upperptr_Inventory[0];
         lowerptr_Inventory[1] = lowerptr_Inventory[0];
         upperptr_V1[1] = upperptr_V1[0];
         lowerptr_V1[1] = lowerptr_V1[0];
         upperptr_V2[1] = upperptr_V2[0];
         lowerptr_V2[1] = lowerptr_V2[0];
         upperptr_V3[1] = upperptr_V3[0];
         lowerptr_V3[1] = lowerptr_V3[0];
         atEnd[1] = false;
         memset(&postRegister[1], 0, sizeof(double) * 1);
         while(!atEnd[1])
         {
            max_dateid = V3[lowerptr_V3[1]].dateid;
            // Seek Value
            do
            {
               found[1] = true;
               if(max_dateid > V3[upperptr_V3[0]].dateid)
               {
                  atEnd[1] = true;
                  break;
               }
               else if (max_dateid > V3[lowerptr_V3[1]].dateid)
               {
                  leap = 1;
                  high = lowerptr_V3[1];
                  while (high <= upperptr_V3[0] && V3[high].dateid < max_dateid)
                  {
                     high += leap;
                     leap *= 2;
                  }
                  /* Backtrack with binary search */
                  leap /= 2;
                  high = std::min(high,upperptr_V3[0]);
                  low = high - leap; mid = 0;
                  while (low <= high)
                  {
                     mid = (high + low) / 2;
                     if (max_dateid < V3[mid].dateid)
                        high = mid - 1;
                     else if(max_dateid > V3[mid].dateid)
                        low = mid + 1;
                     else if  (low != mid)
                        high = mid;
                     else
                        break;
                  }
                  lowerptr_V3[1] = mid;
                  if (max_dateid > V3[lowerptr_V3[1]].dateid)
                     std::cout << "ERROR: max_dateid > V3[lowerptr_V3[1]].dateid\n";
               }
                  max_dateid = V3[lowerptr_V3[1]].dateid;
            upperptr_V3[1] = lowerptr_V3[1];
            while(upperptr_V3[1]<upperptr_V3[0] && V3[upperptr_V3[1]+1].dateid == max_dateid)
            {
               ++upperptr_V3[1];
            }
               if(max_dateid > Inventory_dateid[upperptr_Inventory[0]])
               {
                  atEnd[1] = true;
                  break;
               }
               else if (max_dateid > Inventory_dateid[lowerptr_Inventory[1]])
               {
                  leap = 1;
                  high = lowerptr_Inventory[1];
                  while (high <= upperptr_Inventory[0] && Inventory_dateid[high] < max_dateid)
                  {
                     high += leap;
                     leap *= 2;
                  }
                  /* Backtrack with binary search */
                  leap /= 2;
                  high = std::min(high,upperptr_Inventory[0]);
                  low = high - leap; mid = 0;
                  while (low <= high)
                  {
                     mid = (high + low) / 2;
                     if (max_dateid < Inventory_dateid[mid])
                        high = mid - 1;
                     else if(max_dateid > Inventory_dateid[mid])
                        low = mid + 1;
                     else if  (low != mid)
                        high = mid;
                     else
                        break;
                  }
                  lowerptr_Inventory[1] = mid;
                  if (max_dateid > Inventory_dateid[lowerptr_Inventory[1]])
                     std::cout << "ERROR: max_dateid > Inventory[lowerptr_Inventory[1]].dateid\n";
               }
                  if (max_dateid < Inventory_dateid[lowerptr_Inventory[1]])
                  {
                     max_dateid = Inventory_dateid[lowerptr_Inventory[1]];
                     found[1] = false;
                     continue;
                  }
            upperptr_Inventory[1] = lowerptr_Inventory[1];
            while(upperptr_Inventory[1]<upperptr_Inventory[0] && Inventory_dateid[upperptr_Inventory[1]+1] == max_dateid)
            {
               ++upperptr_Inventory[1];
            }
            } while (!found[1] && !atEnd[1]);
            // End Seek Value
            // If atEnd break loop
            if(atEnd[1])
               break;
            addTuple[1] = false;
            memset(&aggregateRegister[1], 0, sizeof(double) * 1);
            // Inventory_tuple &InventoryTuple = Inventory[lowerptr_Inventory[1]];
            V1_tuple &V1Tuple = V1[lowerptr_V1[1]];
            double *aggregates_V1 = V1Tuple.aggregates;
            V2_tuple &V2Tuple = V2[lowerptr_V2[1]];
            double *aggregates_V2 = V2Tuple.aggregates;
            V3_tuple &V3Tuple = V3[lowerptr_V3[1]];
            double *aggregates_V3 = V3Tuple.aggregates;
            memset(&aggregateRegister[1], 0, sizeof(double) * 1);
            aggregateRegister[1] += aggregates_V3[0];
            upperptr_Inventory[2] = upperptr_Inventory[1];
            lowerptr_Inventory[2] = lowerptr_Inventory[1];
            upperptr_V1[2] = upperptr_V1[1];
            lowerptr_V1[2] = lowerptr_V1[1];
            upperptr_V2[2] = upperptr_V2[1];
            lowerptr_V2[2] = lowerptr_V2[1];
            upperptr_V3[2] = upperptr_V3[1];
            lowerptr_V3[2] = lowerptr_V3[1];
            atEnd[2] = false;
            memset(&postRegister[0], 0, sizeof(double) * 1);
            while(!atEnd[2])
            {
               max_ksn = V2[lowerptr_V2[2]].ksn;
               // Seek Value
               do
               {
                  found[2] = true;
                  if(max_ksn > V2[upperptr_V2[1]].ksn)
                  {
                     atEnd[2] = true;
                     break;
                  }
                  else if (max_ksn > V2[lowerptr_V2[2]].ksn)
                  {
                     leap = 1;
                     high = lowerptr_V2[2];
                     while (high <= upperptr_V2[1] && V2[high].ksn < max_ksn)
                     {
                        high += leap;
                        leap *= 2;
                     }
                     /* Backtrack with binary search */
                     leap /= 2;
                     high = std::min(high,upperptr_V2[1]);
                     low = high - leap; mid = 0;
                     while (low <= high)
                     {
                        mid = (high + low) / 2;
                        if (max_ksn < V2[mid].ksn)
                           high = mid - 1;
                        else if(max_ksn > V2[mid].ksn)
                           low = mid + 1;
                        else if  (low != mid)
                           high = mid;
                        else
                           break;
                     }
                     lowerptr_V2[2] = mid;
                     if (max_ksn > V2[lowerptr_V2[2]].ksn)
                        std::cout << "ERROR: max_ksn > V2[lowerptr_V2[2]].ksn\n";
                  }
                     max_ksn = V2[lowerptr_V2[2]].ksn;
               upperptr_V2[2] = lowerptr_V2[2];
               while(upperptr_V2[2]<upperptr_V2[1] && V2[upperptr_V2[2]+1].ksn == max_ksn)
               {
                  ++upperptr_V2[2];
               }
                  if(max_ksn > Inventory_ksn[upperptr_Inventory[1]])
                  {
                     atEnd[2] = true;
                     break;
                  }
                  else if (max_ksn > Inventory_ksn[lowerptr_Inventory[2]])
                  {
                     leap = 1;
                     high = lowerptr_Inventory[2];
                     while (high <= upperptr_Inventory[1] && Inventory_ksn[high] < max_ksn)
                     {
                        high += leap;
                        leap *= 2;
                     }
                     /* Backtrack with binary search */
                     leap /= 2;
                     high = std::min(high,upperptr_Inventory[1]);
                     low = high - leap; mid = 0;
                     while (low <= high)
                     {
                        mid = (high + low) / 2;
                        if (max_ksn < Inventory_ksn[mid])
                           high = mid - 1;
                        else if(max_ksn > Inventory_ksn[mid])
                           low = mid + 1;
                        else if  (low != mid)
                           high = mid;
                        else
                           break;
                     }
                     lowerptr_Inventory[2] = mid;
                     if (max_ksn > Inventory_ksn[lowerptr_Inventory[2]])
                        std::cout << "ERROR: max_ksn > Inventory[lowerptr_Inventory[2]].ksn\n";
                  }
                     if (max_ksn < Inventory_ksn[lowerptr_Inventory[2]])
                     {
                        max_ksn = Inventory_ksn[lowerptr_Inventory[2]];
                        found[2] = false;
                        continue;
                     }
               upperptr_Inventory[2] = lowerptr_Inventory[2];
               while(upperptr_Inventory[2]<upperptr_Inventory[1] && Inventory_ksn[upperptr_Inventory[2]+1] == max_ksn)
               {
                  ++upperptr_Inventory[2];
               }
               } while (!found[2] && !atEnd[2]);
               // End Seek Value
               // If atEnd break loop
               if(atEnd[2])
                  break;
               addTuple[2] = false;
               memset(&aggregateRegister[2], 0, sizeof(double) * 1);
               // Inventory_tuple &InventoryTuple = Inventory[lowerptr_Inventory[2]];
               V1_tuple &V1Tuple = V1[lowerptr_V1[2]];
               double *aggregates_V1 = V1Tuple.aggregates;
               V2_tuple &V2Tuple = V2[lowerptr_V2[2]];
               double *aggregates_V2 = V2Tuple.aggregates;
               V3_tuple &V3Tuple = V3[lowerptr_V3[2]];
               double *aggregates_V3 = V3Tuple.aggregates;
               double count = upperptr_Inventory[2] - lowerptr_Inventory[2] + 1;
               memset(&aggregateRegister[2], 0, sizeof(double) * 1);
               aggregateRegister[2] += aggregates_V2[0]*count;
               memset(addTuple, true, sizeof(bool) * 3);
               postRegister[0] += aggregateRegister[2];
               lowerptr_Inventory[2] = upperptr_Inventory[2] + 1;
               if(lowerptr_Inventory[2] > upperptr_Inventory[1])
                  atEnd[2] = true;
               lowerptr_V2[2] = upperptr_V2[2] + 1;
               if(lowerptr_V2[2] > upperptr_V2[1])
                  atEnd[2] = true;
            }
            if (addTuple[1]) 
            {
               postRegister[1] += aggregateRegister[1]*postRegister[0];
            }
            lowerptr_Inventory[1] = upperptr_Inventory[1] + 1;
            if(lowerptr_Inventory[1] > upperptr_Inventory[0])
               atEnd[1] = true;
            lowerptr_V3[1] = upperptr_V3[1] + 1;
            if(lowerptr_V3[1] > upperptr_V3[0])
               atEnd[1] = true;
         }
         if (addTuple[0]) 
         {
            postRegister[2] += aggregateRegister[0]*postRegister[1];
         }
         lowerptr_Inventory[0] = upperptr_Inventory[0] + 1;
         if(lowerptr_Inventory[0] > Inventory.size()-1)
            atEnd[0] = true;
         lowerptr_V1[0] = upperptr_V1[0] + 1;
         if(lowerptr_V1[0] > V1.size()-1)
            atEnd[0] = true;
         lowerptr_V3[0] = upperptr_V3[0] + 1;
         if(lowerptr_V3[0] > V3.size()-1)
            atEnd[0] = true;
      }
      V4.emplace_back();
      aggregates_V4 = V4.back().aggregates;
      aggregates_V4[0] += postRegister[2];

      int64_t endProcess = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()-startProcess;
      std::cout <<"Compute Group 4 process: "+std::to_string(endProcess)+"ms.\n";
      std::ofstream ofs("times.txt",std::ofstream::out| std::ofstream::app);
      ofs << "\t" << endProcess;
      ofs.close();

   }

}
