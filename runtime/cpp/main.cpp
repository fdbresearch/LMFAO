#ifdef TESTING 
   #include <iomanip>
#endif
#include "DataHandler.h"
#include "ComputeGroup0.h"
#include "ComputeGroup1.h"
#include "ComputeGroup2.h"
#include "ComputeGroup3.h"
#include "ComputeGroup4.h"

namespace lmfao
{
   std::vector<Inventory_tuple> Inventory;
   size_t Inventory_size;
   int* Inventory_locn;
   int* Inventory_dateid;
   int* Inventory_ksn;
   double* Inventory_inventoryunits;
   std::vector<Location_tuple> Location;
   std::vector<Census_tuple> Census;
   std::vector<Item_tuple> Item;
   std::vector<Weather_tuple> Weather;
   std::vector<V0_tuple> V0;
   std::vector<V1_tuple> V1;
   std::vector<V2_tuple> V2;
   std::vector<V3_tuple> V3;
   std::vector<V4_tuple> V4;
   void loadRelations()
   {
      std::ifstream input;
      std::string line;

      Inventory.clear();
      input.open(PATH_TO_DATA + "/Inventory.tbl");
      if (!input)
      {
         std::cerr << PATH_TO_DATA + "Inventory.tbl does is not exist. \n";
         exit(1);
      }
      while(getline(input, line))
         Inventory.push_back(Inventory_tuple(line));
      input.close();

      Location.clear();
      input.open(PATH_TO_DATA + "/Location.tbl");
      if (!input)
      {
         std::cerr << PATH_TO_DATA + "Location.tbl does is not exist. \n";
         exit(1);
      }
      while(getline(input, line))
         Location.push_back(Location_tuple(line));
      input.close();

      Census.clear();
      input.open(PATH_TO_DATA + "/Census.tbl");
      if (!input)
      {
         std::cerr << PATH_TO_DATA + "Census.tbl does is not exist. \n";
         exit(1);
      }
      while(getline(input, line))
         Census.push_back(Census_tuple(line));
      input.close();

      Item.clear();
      input.open(PATH_TO_DATA + "/Item.tbl");
      if (!input)
      {
         std::cerr << PATH_TO_DATA + "Item.tbl does is not exist. \n";
         exit(1);
      }
      while(getline(input, line))
         Item.push_back(Item_tuple(line));
      input.close();

      Weather.clear();
      input.open(PATH_TO_DATA + "/Weather.tbl");
      if (!input)
      {
         std::cerr << PATH_TO_DATA + "Weather.tbl does is not exist. \n";
         exit(1);
      }
      while(getline(input, line))
         Weather.push_back(Weather_tuple(line));
      input.close();

      V0.reserve(1000000);
      V1.reserve(1000000);
      V2.reserve(1000000);
      V3.reserve(1000000);
      V4.reserve(1000000);
   }

   void sortInventory()
   {
      int64_t startProcess = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();

      std::sort(Inventory.begin(),Inventory.end(),[ ](const Inventory_tuple& lhs, const Inventory_tuple& rhs)
      {
         if(lhs.locn != rhs.locn)
            return lhs.locn < rhs.locn;
         if(lhs.dateid != rhs.dateid)
            return lhs.dateid < rhs.dateid;
         if(lhs.ksn != rhs.ksn)
            return lhs.ksn < rhs.ksn;
         if(lhs.inventoryunits != rhs.inventoryunits)
            return lhs.inventoryunits < rhs.inventoryunits;
         return false;
      });

      int64_t endProcess = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()-startProcess;
      std::cout << "Sort Relation Inventory: "+std::to_string(endProcess)+"ms.\n";
      startProcess = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
      Inventory_size = Inventory.size();
      Inventory_locn = (int*)malloc(sizeof(int) * Inventory_size);
      Inventory_dateid = (int*)malloc(sizeof(int) * Inventory_size);
      Inventory_ksn = (int*)malloc(sizeof(int) * Inventory_size);
      Inventory_inventoryunits = (double*)malloc(sizeof(double) * Inventory_size);
      for(int i = 0; i<Inventory_size; i++) {
         Inventory_locn[i] = Inventory[i].locn;
         Inventory_dateid[i] = Inventory[i].dateid;
         Inventory_ksn[i] = Inventory[i].ksn;
         Inventory_inventoryunits[i] = Inventory[i].inventoryunits;
      }
      endProcess = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()-startProcess;
      std::cout << "CStoring Relation Inventory: "+std::to_string(endProcess)+"ms.\n";

   }

   void sortLocation()
   {
      int64_t startProcess = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();

      std::sort(Location.begin(),Location.end(),[ ](const Location_tuple& lhs, const Location_tuple& rhs)
      {
         if(lhs.locn != rhs.locn)
            return lhs.locn < rhs.locn;
         if(lhs.zip != rhs.zip)
            return lhs.zip < rhs.zip;
         if(lhs.rgn_cd != rhs.rgn_cd)
            return lhs.rgn_cd < rhs.rgn_cd;
         if(lhs.clim_zn_nbr != rhs.clim_zn_nbr)
            return lhs.clim_zn_nbr < rhs.clim_zn_nbr;
         if(lhs.tot_area_sq_ft != rhs.tot_area_sq_ft)
            return lhs.tot_area_sq_ft < rhs.tot_area_sq_ft;
         if(lhs.sell_area_sq_ft != rhs.sell_area_sq_ft)
            return lhs.sell_area_sq_ft < rhs.sell_area_sq_ft;
         if(lhs.avghhi != rhs.avghhi)
            return lhs.avghhi < rhs.avghhi;
         if(lhs.supertargetdistance != rhs.supertargetdistance)
            return lhs.supertargetdistance < rhs.supertargetdistance;
         if(lhs.supertargetdrivetime != rhs.supertargetdrivetime)
            return lhs.supertargetdrivetime < rhs.supertargetdrivetime;
         if(lhs.targetdistance != rhs.targetdistance)
            return lhs.targetdistance < rhs.targetdistance;
         if(lhs.targetdrivetime != rhs.targetdrivetime)
            return lhs.targetdrivetime < rhs.targetdrivetime;
         if(lhs.walmartdistance != rhs.walmartdistance)
            return lhs.walmartdistance < rhs.walmartdistance;
         if(lhs.walmartdrivetime != rhs.walmartdrivetime)
            return lhs.walmartdrivetime < rhs.walmartdrivetime;
         if(lhs.walmartsupercenterdistance != rhs.walmartsupercenterdistance)
            return lhs.walmartsupercenterdistance < rhs.walmartsupercenterdistance;
         if(lhs.walmartsupercenterdrivetime != rhs.walmartsupercenterdrivetime)
            return lhs.walmartsupercenterdrivetime < rhs.walmartsupercenterdrivetime;
         return false;
      });

      int64_t endProcess = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()-startProcess;
      std::cout << "Sort Relation Location: "+std::to_string(endProcess)+"ms.\n";

   }

   void sortCensus()
   {
      int64_t startProcess = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();

      std::sort(Census.begin(),Census.end(),[ ](const Census_tuple& lhs, const Census_tuple& rhs)
      {
         if(lhs.zip != rhs.zip)
            return lhs.zip < rhs.zip;
         if(lhs.population != rhs.population)
            return lhs.population < rhs.population;
         if(lhs.white != rhs.white)
            return lhs.white < rhs.white;
         if(lhs.asian != rhs.asian)
            return lhs.asian < rhs.asian;
         if(lhs.pacific != rhs.pacific)
            return lhs.pacific < rhs.pacific;
         if(lhs.black != rhs.black)
            return lhs.black < rhs.black;
         if(lhs.medianage != rhs.medianage)
            return lhs.medianage < rhs.medianage;
         if(lhs.occupiedhouseunits != rhs.occupiedhouseunits)
            return lhs.occupiedhouseunits < rhs.occupiedhouseunits;
         if(lhs.houseunits != rhs.houseunits)
            return lhs.houseunits < rhs.houseunits;
         if(lhs.families != rhs.families)
            return lhs.families < rhs.families;
         if(lhs.households != rhs.households)
            return lhs.households < rhs.households;
         if(lhs.husbwife != rhs.husbwife)
            return lhs.husbwife < rhs.husbwife;
         if(lhs.males != rhs.males)
            return lhs.males < rhs.males;
         if(lhs.females != rhs.females)
            return lhs.females < rhs.females;
         if(lhs.householdschildren != rhs.householdschildren)
            return lhs.householdschildren < rhs.householdschildren;
         if(lhs.hispanic != rhs.hispanic)
            return lhs.hispanic < rhs.hispanic;
         return false;
      });

      int64_t endProcess = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()-startProcess;
      std::cout << "Sort Relation Census: "+std::to_string(endProcess)+"ms.\n";

   }

   void sortItem()
   {
      int64_t startProcess = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();

      std::sort(Item.begin(),Item.end(),[ ](const Item_tuple& lhs, const Item_tuple& rhs)
      {
         if(lhs.ksn != rhs.ksn)
            return lhs.ksn < rhs.ksn;
         if(lhs.subcategory != rhs.subcategory)
            return lhs.subcategory < rhs.subcategory;
         if(lhs.category != rhs.category)
            return lhs.category < rhs.category;
         if(lhs.categoryCluster != rhs.categoryCluster)
            return lhs.categoryCluster < rhs.categoryCluster;
         if(lhs.prize != rhs.prize)
            return lhs.prize < rhs.prize;
         return false;
      });

      int64_t endProcess = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()-startProcess;
      std::cout << "Sort Relation Item: "+std::to_string(endProcess)+"ms.\n";

   }

   void sortWeather()
   {
      int64_t startProcess = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();

      std::sort(Weather.begin(),Weather.end(),[ ](const Weather_tuple& lhs, const Weather_tuple& rhs)
      {
         if(lhs.locn != rhs.locn)
            return lhs.locn < rhs.locn;
         if(lhs.dateid != rhs.dateid)
            return lhs.dateid < rhs.dateid;
         if(lhs.rain != rhs.rain)
            return lhs.rain < rhs.rain;
         if(lhs.snow != rhs.snow)
            return lhs.snow < rhs.snow;
         if(lhs.maxtemp != rhs.maxtemp)
            return lhs.maxtemp < rhs.maxtemp;
         if(lhs.mintemp != rhs.mintemp)
            return lhs.mintemp < rhs.mintemp;
         if(lhs.meanwind != rhs.meanwind)
            return lhs.meanwind < rhs.meanwind;
         if(lhs.thunder != rhs.thunder)
            return lhs.thunder < rhs.thunder;
         return false;
      });

      int64_t endProcess = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()-startProcess;
      std::cout << "Sort Relation Weather: "+std::to_string(endProcess)+"ms.\n";

   }

   void run()
   {
      int64_t startLoading = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();

      loadRelations();

      int64_t loadTime = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()-startLoading;
      std::cout << "Data loading: "+std::to_string(loadTime)+"ms.\n";
      std::ofstream ofs("times.txt",std::ofstream::out | std::ofstream::app);
      ofs << loadTime;
      ofs.close();

      int64_t startSorting = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();

      sortInventory();
      sortLocation();
      sortCensus();
      sortItem();
      sortWeather();

      int64_t sortingTime = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()-startSorting;
      std::cout << "Data sorting: " + std::to_string(sortingTime)+"ms.\n";

      ofs.open("times.txt",std::ofstream::out | std::ofstream::app);
      ofs << "\t" << sortingTime;
      ofs.close();

      int64_t startProcess = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();

      computeGroup0();
      computeGroup1();
      computeGroup2();
      computeGroup3();
      computeGroup4();

      int64_t processTime = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()-startProcess;
      std::cout << "Data process: "+std::to_string(processTime)+"ms.\n";
      ofs.open("times.txt",std::ofstream::out | std::ofstream::app);
      ofs << "\t" << processTime;
      std::cout << "V0: " << V0.size() << std::endl;
      std::cout << "V1: " << V1.size() << std::endl;
      std::cout << "V2: " << V2.size() << std::endl;
      std::cout << "V3: " << V3.size() << std::endl;
      std::cout << "V4: " << V4.size() << std::endl;
      ofs << std::endl;
      ofs.close();

   }

#ifdef TESTING
   void ouputViewsForTesting()
   {
      std::ofstream ofs("test.out");
      ofs << std::fixed << std::setprecision(2);
      for (size_t i=0; i < V4.size(); ++i)
      {
         V4_tuple& tuple = V4[i];
         ofs  << tuple.aggregates[0] << "\n";
      }
      ofs.close();
   }
#endif
#ifdef DUMP_OUTPUT
   void dumpOutputViews()
   {
      std::ofstream ofs;
      ofs.open("output/V4.tbl");
      ofs << "0 1\n";
      for (size_t i=0; i < V4.size(); ++i)
      {
         V4_tuple& tuple = V4[i];
         ofs  << tuple.aggregates[0] << "\n";
      }
      ofs.close();
   }
#endif
}

int main(int argc, char *argv[])
{
   std::cout << "run lmfao" << std::endl;
   lmfao::run();
#ifdef TESTING
   lmfao::ouputViewsForTesting();
#endif
#ifdef DUMP_OUTPUT
   lmfao::dumpOutputViews();
#endif
   return 0;
};
