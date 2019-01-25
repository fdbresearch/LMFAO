#ifndef INCLUDE_DATAHANDLER_HPP_
#define INCLUDE_DATAHANDLER_HPP_

#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <thread>
#include <unordered_map>
#include <vector>

using namespace std::chrono;

namespace lmfao
{
   const std::string PATH_TO_DATA = "/Users/amir/code/fdb/benchmarking/datasets/retailer";

   struct Inventory_tuple
   {
      int locn;
      int dateid;
      int ksn;
      double inventoryunits;
      Inventory_tuple(const std::string& tuple);
   };

   extern std::vector<Inventory_tuple> Inventory;

   struct Location_tuple
   {
      int locn;
      int zip;
      double rgn_cd;
      double clim_zn_nbr;
      double tot_area_sq_ft;
      double sell_area_sq_ft;
      double avghhi;
      double supertargetdistance;
      double supertargetdrivetime;
      double targetdistance;
      double targetdrivetime;
      double walmartdistance;
      double walmartdrivetime;
      double walmartsupercenterdistance;
      double walmartsupercenterdrivetime;
      Location_tuple(const std::string& tuple);
   };

   extern std::vector<Location_tuple> Location;

   struct Census_tuple
   {
      int zip;
      double population;
      double white;
      double asian;
      double pacific;
      double black;
      double medianage;
      double occupiedhouseunits;
      double houseunits;
      double families;
      double households;
      double husbwife;
      double males;
      double females;
      double householdschildren;
      double hispanic;
      Census_tuple(const std::string& tuple);
   };

   extern std::vector<Census_tuple> Census;

   struct Item_tuple
   {
      int ksn;
      int subcategory;
      int category;
      int categoryCluster;
      double prize;
      Item_tuple(const std::string& tuple);
   };

   extern std::vector<Item_tuple> Item;

   struct Weather_tuple
   {
      int locn;
      int dateid;
      int rain;
      int snow;
      int maxtemp;
      int mintemp;
      double meanwind;
      int thunder;
      Weather_tuple(const std::string& tuple);
   };

   extern std::vector<Weather_tuple> Weather;

   struct V0_tuple
   {
      int zip;
      double aggregates[1] = {};
      V0_tuple(const int& zip);
   };

   extern std::vector<V0_tuple> V0;

   struct V1_tuple
   {
      int locn;
      double aggregates[1] = {};
      V1_tuple(const int& locn);
   };

   extern std::vector<V1_tuple> V1;

   struct V2_tuple
   {
      int ksn;
      double aggregates[1] = {};
      V2_tuple(const int& ksn);
   };

   extern std::vector<V2_tuple> V2;

   struct V3_tuple
   {
      int locn;
      int dateid;
      double aggregates[1] = {};
      V3_tuple(const int& locn,const int& dateid);
   };

   extern std::vector<V3_tuple> V3;

   struct V4_tuple
   {
      double aggregates[1] = {};
      V4_tuple();
   };

   extern std::vector<V4_tuple> V4;

   enum ID {
      Inventory_ID,
      Location_ID,
      Census_ID,
      Item_ID,
      Weather_ID,
      V0_ID,
      V1_ID,
      V2_ID,
      V3_ID,
      V4_ID
   };

   extern void sortInventory();
   extern void sortLocation();
   extern void sortCensus();
   extern void sortItem();
   extern void sortWeather();
}

#endif /* INCLUDE_DATAHANDLER_HPP_*/
