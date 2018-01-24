objects = lmfao-engine.o

BIN_DIR     = ./bin
CXXFLAG     = -std=c++11  -pthread -O2 -mtune=native -fassociative-math -freciprocal-math -fno-signed-zeros

lmfao : lmfao-engine.cpp Engine.hpp
	$(CXX) $(MULTI) $(CXXFLAG) -o lmfao lmfao-engine.cpp

#the below is not used 
# lmfao-engine.o : lmfao-engine.cpp Engine.hpp
# 	$(CXX) -DMULTITHREAD -c $(CXXFLAG) lmfao-engine.cpp

.PHONY : multithread
multi : MULTI = -DMULTITHREAD
multi : lmfao

.PHONY : clean
clean :
	-rm lmfao
