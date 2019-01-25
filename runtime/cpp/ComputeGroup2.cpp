#include "ComputeGroup2.h"
namespace lmfao
{
   void computeGroup2()
   {
      int64_t startProcess = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();

      int max_locn;
      int max_dateid;
      bool found[2] = {}, atEnd[2] = {}, addTuple[2] = {};
      size_t leap, low, mid, high;
      size_t lowerptr_Weather[2] = {}, upperptr_Weather[2] = {}; 
      upperptr_Weather[0] = Weather.size()-1;
      size_t ptr_Weather = 0;
      double *aggregates_V3 = nullptr;
      double aggregateRegister[1] = {};
      atEnd[0] = false;
      while(!atEnd[0])
      {
         max_locn = Weather[lowerptr_Weather[0]].locn;
         upperptr_Weather[0] = lowerptr_Weather[0];
         while(upperptr_Weather[0]<Weather.size()-1 && Weather[upperptr_Weather[0]+1].locn == max_locn)
         {
            ++upperptr_Weather[0];
         }
         addTuple[0] = false;
         Weather_tuple &WeatherTuple = Weather[lowerptr_Weather[0]];
         upperptr_Weather[1] = upperptr_Weather[0];
         lowerptr_Weather[1] = lowerptr_Weather[0];
         atEnd[1] = false;
         while(!atEnd[1])
         {
            max_dateid = Weather[lowerptr_Weather[1]].dateid;
            upperptr_Weather[1] = lowerptr_Weather[1];
            while(upperptr_Weather[1]<upperptr_Weather[0] && Weather[upperptr_Weather[1]+1].dateid == max_dateid)
            {
               ++upperptr_Weather[1];
            }
            addTuple[1] = false;
            memset(&aggregateRegister[0], 0, sizeof(double) * 1);
            Weather_tuple &WeatherTuple = Weather[lowerptr_Weather[1]];
            double count = upperptr_Weather[1] - lowerptr_Weather[1] + 1;
            memset(&aggregateRegister[0], 0, sizeof(double) * 1);
            aggregateRegister[0] += count;
            memset(addTuple, true, sizeof(bool) * 2);
            V3.emplace_back(WeatherTuple.locn,WeatherTuple.dateid);
            aggregates_V3 = V3.back().aggregates;
            aggregates_V3[0] += aggregateRegister[0];
            lowerptr_Weather[1] = upperptr_Weather[1] + 1;
            if(lowerptr_Weather[1] > upperptr_Weather[0])
               atEnd[1] = true;
         }
         lowerptr_Weather[0] = upperptr_Weather[0] + 1;
         if(lowerptr_Weather[0] > Weather.size()-1)
            atEnd[0] = true;
      }

      int64_t endProcess = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()-startProcess;
      std::cout <<"Compute Group 2 process: "+std::to_string(endProcess)+"ms.\n";
      std::ofstream ofs("times.txt",std::ofstream::out| std::ofstream::app);
      ofs << "\t" << endProcess;
      ofs.close();

   }

}
