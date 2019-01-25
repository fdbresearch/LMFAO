#include "ApplicationHandler.h"
namespace lmfao
{
   void runApplication()
   {
#ifdef DUMP_OUTPUT
      std::ofstream ofs("output/covarianceMatrix.out");
      ofs << "3,0\n3,1\n3,2\n3,3\n3,4\n3,5\n3,6\n3,7\n3,8\n3,9\n3,10\n3,11\n3,12\n3,13\n3,14\n3,15\n3,16\n3,17\n3,18\n3,19\n3,20\n3,21\n3,22\n3,23\n3,24\n3,25\n3,26\n3,27\n";
      ofs.close();

#endif /* DUMP_OUTPUT */ 
   }

}
