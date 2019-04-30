cmake_minimum_required(VERSION 3.9)

set(LMFAO_GPU false)
if (LMFAO_GPU)
  project(lmfao_algebra_gpu LANGUAGES CUDA CXX)
else()
  project(lmfao_algebra LANGUAGES CXX)
endif()

set(CMKAKE_CXX_STANDARD 14)
set(CMKAKE_CXX_STANDARD REQUIRED )

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -std=c++14 -pthread -g -Wall -Wextra -Og")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -std=c++14 -pthread -Ofast -mtune=native -fassociative-math -freciprocal-math -fno-signed-zeros -frename-registers -fopenmp")
else()
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -std=c++14 -pthread -g -Wall -Wextra -O0")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -std=c++14  -Ofast")
endif()
set(CMAKE_CXX_FLAGS_BENCHMARK "${CMAKE_CXX_FLAGS_RELEASE} -DBENCH")

# Set to Debug, Benchmark or Release.
set(CMAKE_BUILD_TYPE Benchmark)
#set(LMFAO_RUN true)
set(LMFAO_LIB true)
set(LMFAO_TEST true)

if (LMFAO_GPU)
  find_package(CUDA 10.0 REQUIRED)
  set(CUDA_LINK_LIBRARIES_KEYWORD PUBLIC)
  set(CUDA_LINK_LIBRARIES  ${CUDA_cublas_LIBRARY} ${CUDA_cusolver_LIBRARY} ${CUDA_CUDART_LIBRARY})
endif()

set(LMFAO_LA_ROOT .)
set(LMFAO_LA_TEST ${LMFAO_LA_ROOT}/tests)
set(LMFAO_LA_INCLUDE ${LMFAO_LA_ROOT}/include)

include_directories(${LMFAO_LA_INCLUDE})
if (${LMFAO_GPU})
  include_directories(${CMAKE_CUDA_TOOLKIT_INCLUDE_DIRECTORIES})
endif()
file(GLOB SRCFILES src/*.cpp)

#Build library for svd from cuda files only
set(SRCCUFILES "")
set(LMFAO_CUDA_LIBS "")
if (LMFAO_GPU)
  file(GLOB SRCCUFILES src/*.cu)
  add_library(cublas_svd ${SRCCUFILES})
  set(LMFAO_CUDA_LIBS cublas_svd ${CUDA_LINK_LIBRARIES})
endif()



if (LMFAO_RUN)
  add_executable(lmfaola main.cpp ${SRCFILES})
endif()

if (LMFAO_TEST)
  # Test environment.
  add_executable(lmfaola_test ${LMFAO_LA_TEST}/test.cpp ${SRCFILES})
  target_include_directories(lmfaola_test PRIVATE ${LMFAO_LA_TEST})
endif()

set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost REQUIRED COMPONENTS system thread iostreams program_options)
include_directories(${Boost_INCLUDE_DIRS})

# Add libraries.
if (LMFAO_RUN)
  target_link_libraries(lmfaola ${LMFAO_CUDA_LIBS} ${Boost_LIBRARIES})
endif()

if (LMFAO_TEST)
  # Take care of Gtest library.
  find_package(GTest REQUIRED)
  include_directories(${GTEST_INCLUDE_DIRS})
  target_link_libraries(lmfaola_test ${LMFAO_CUDA_LIBS} ${Boost_LIBRARIES} ${GTEST_LIBRARIES})
endif()


#Builds static library which will be loaded in runtime.
if (LMFAO_LIB)
  #add_library(lmfaolalib STATIC  ${SRCFILES} ${SRCCUFILES})
  add_library(lmfaolalib STATIC ${SRCFILES} )
  #target_link_libraries(lmfaolalib ${Boost_LIBRARIES})
  if (LMFAO_GPU)
    target_link_libraries(lmfaolalib ${CUDA_LINK_LIBRARIES})
  endif()
endif()

#if (LMFAO_LIB)
#  message(${Boost_LIBRARIES})
#  target_link_libraries(lmfaolalib ${Boost_LIBRARIES})
#endif()
