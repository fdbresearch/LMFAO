#include "DataHandler.h"
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix_core.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>

namespace qi = boost::spirit::qi;
namespace phoenix = boost::phoenix;

namespace lmfao
{
   Inventory_tuple::Inventory_tuple(const std::string& tuple)
   {
      qi::phrase_parse(tuple.begin(),tuple.end(),
         qi::int_[phoenix::ref(locn) = qi::_1]>>
         qi::int_[phoenix::ref(dateid) = qi::_1]>>
         qi::int_[phoenix::ref(ksn) = qi::_1]>>
         qi::double_[phoenix::ref(inventoryunits) = qi::_1],
         '|');
   }

   Location_tuple::Location_tuple(const std::string& tuple)
   {
      qi::phrase_parse(tuple.begin(),tuple.end(),
         qi::int_[phoenix::ref(locn) = qi::_1]>>
         qi::int_[phoenix::ref(zip) = qi::_1]>>
         qi::double_[phoenix::ref(rgn_cd) = qi::_1]>>
         qi::double_[phoenix::ref(clim_zn_nbr) = qi::_1]>>
         qi::double_[phoenix::ref(tot_area_sq_ft) = qi::_1]>>
         qi::double_[phoenix::ref(sell_area_sq_ft) = qi::_1]>>
         qi::double_[phoenix::ref(avghhi) = qi::_1]>>
         qi::double_[phoenix::ref(supertargetdistance) = qi::_1]>>
         qi::double_[phoenix::ref(supertargetdrivetime) = qi::_1]>>
         qi::double_[phoenix::ref(targetdistance) = qi::_1]>>
         qi::double_[phoenix::ref(targetdrivetime) = qi::_1]>>
         qi::double_[phoenix::ref(walmartdistance) = qi::_1]>>
         qi::double_[phoenix::ref(walmartdrivetime) = qi::_1]>>
         qi::double_[phoenix::ref(walmartsupercenterdistance) = qi::_1]>>
         qi::double_[phoenix::ref(walmartsupercenterdrivetime) = qi::_1],
         '|');
   }

   Census_tuple::Census_tuple(const std::string& tuple)
   {
      qi::phrase_parse(tuple.begin(),tuple.end(),
         qi::int_[phoenix::ref(zip) = qi::_1]>>
         qi::double_[phoenix::ref(population) = qi::_1]>>
         qi::double_[phoenix::ref(white) = qi::_1]>>
         qi::double_[phoenix::ref(asian) = qi::_1]>>
         qi::double_[phoenix::ref(pacific) = qi::_1]>>
         qi::double_[phoenix::ref(black) = qi::_1]>>
         qi::double_[phoenix::ref(medianage) = qi::_1]>>
         qi::double_[phoenix::ref(occupiedhouseunits) = qi::_1]>>
         qi::double_[phoenix::ref(houseunits) = qi::_1]>>
         qi::double_[phoenix::ref(families) = qi::_1]>>
         qi::double_[phoenix::ref(households) = qi::_1]>>
         qi::double_[phoenix::ref(husbwife) = qi::_1]>>
         qi::double_[phoenix::ref(males) = qi::_1]>>
         qi::double_[phoenix::ref(females) = qi::_1]>>
         qi::double_[phoenix::ref(householdschildren) = qi::_1]>>
         qi::double_[phoenix::ref(hispanic) = qi::_1],
         '|');
   }

   Item_tuple::Item_tuple(const std::string& tuple)
   {
      qi::phrase_parse(tuple.begin(),tuple.end(),
         qi::int_[phoenix::ref(ksn) = qi::_1]>>
         qi::int_[phoenix::ref(subcategory) = qi::_1]>>
         qi::int_[phoenix::ref(category) = qi::_1]>>
         qi::int_[phoenix::ref(categoryCluster) = qi::_1]>>
         qi::double_[phoenix::ref(prize) = qi::_1],
         '|');
   }

   Weather_tuple::Weather_tuple(const std::string& tuple)
   {
      qi::phrase_parse(tuple.begin(),tuple.end(),
         qi::int_[phoenix::ref(locn) = qi::_1]>>
         qi::int_[phoenix::ref(dateid) = qi::_1]>>
         qi::int_[phoenix::ref(rain) = qi::_1]>>
         qi::int_[phoenix::ref(snow) = qi::_1]>>
         qi::int_[phoenix::ref(maxtemp) = qi::_1]>>
         qi::int_[phoenix::ref(mintemp) = qi::_1]>>
         qi::double_[phoenix::ref(meanwind) = qi::_1]>>
         qi::int_[phoenix::ref(thunder) = qi::_1],
         '|');
   }

   V0_tuple::V0_tuple(const int& zip) : zip(zip){}

   V1_tuple::V1_tuple(const int& locn) : locn(locn){}

   V2_tuple::V2_tuple(const int& ksn) : ksn(ksn){}

   V3_tuple::V3_tuple(const int& locn,const int& dateid) : locn(locn),dateid(dateid){}

   V4_tuple::V4_tuple(){}

}

