## You can set costum flags by adding: FLAG=-D... 

CXXFLAG  += -std=c++11 -O3 -pthread -mtune=native -ftree-vectorize
# -pthread -O2 -mtune=native -fassociative-math -freciprocal-math -fno-signed-zeros -v -ftime-report -fno-stack-protector

lmfao : main.o datahandler.o computegroup0.o computegroup1.o computegroup2.o computegroup3.o computegroup4.o 
	$(CXX) $(CXXFLAG) main.o datahandler.o computegroup0.o computegroup1.o computegroup2.o computegroup3.o computegroup4.o -o lmfao

main.o : main.cpp
	$(CXX) $(FLAG) $(CXXFLAG) -c main.cpp -o main.o

datahandler.o : DataHandler.cpp
	$(CXX) $(FLAG) $(CXXFLAG) -c DataHandler.cpp -o datahandler.o

computegroup0.o : ComputeGroup0.cpp
	$(CXX) $(FLAG) $(CXXFLAG) -c ComputeGroup0.cpp -o computegroup0.o

computegroup1.o : ComputeGroup1.cpp
	$(CXX) $(FLAG) $(CXXFLAG) -c ComputeGroup1.cpp -o computegroup1.o

computegroup2.o : ComputeGroup2.cpp
	$(CXX) $(FLAG) $(CXXFLAG) -c ComputeGroup2.cpp -o computegroup2.o

computegroup3.o : ComputeGroup3.cpp
	$(CXX) $(FLAG) $(CXXFLAG) -c ComputeGroup3.cpp -o computegroup3.o

computegroup4.o : ComputeGroup4.cpp
	$(CXX) $(FLAG) $(CXXFLAG) -c ComputeGroup4.cpp -o computegroup4.o

.PHONY : multithread
multi : FLAG = -DMULTITHREAD
multi : lmfao

.PHONY : precompilation
precomp : main.o
precomp : datahandler.o
precomp : application.o

.PHONY : testing
test : FLAG = -DTESTING
test : lmfao

.PHONY : dump
dump : FLAG = -DDUMP_OUTPUT
dump : lmfao

.PHONY : debug
debug : CXXFLAG += -std=c++11 -g
debug : lmfao

.PHONY : clean
clean :
	rm *.o lmfao