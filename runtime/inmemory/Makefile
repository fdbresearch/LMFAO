objects = lmfao-engine.o

BIN_DIR     = ./bin
CXXFLAG     += -std=c++11 -O2
#  -pthread -O2 -mtune=native -fassociative-math -freciprocal-math -fno-signed-zeros -v -ftime-report -fno-stack-protector

lmfao : lmfao-engine.o
	$(CXX) $(CXXFLAG) lmfao.o -o lmfao

lmfao-engine.o : lmfao.i 
	$(CXX) $(MULTI) $(CXXFLAG) -xc++ -c lmfao.i -o lmfao.o

lmfao.i : lmfao-engine.cpp Engine.hpp
	$(CXX) $(MULTI) $(CXXFLAG) -xc++ -E lmfao-engine.cpp -o lmfao.i

.PHONY : multithread
multi : MULTI = -DMULTITHREAD
multi : lmfao

.PHONY : clean
clean :
	rm lmfao
