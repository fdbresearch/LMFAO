cmake_minimum_required(VERSION 2.8.9)

project(lmfao_algebra)

# Specialised compiler flags such as WORKERS, FEATURES or INTERACTIONS, TABLES or ATTRIBUTES can be set. For instance "-DWORKERS=2". These must be consistent with the values in the configuration files.
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -std=c++14 -pthread -g -Wall -Wextra -Og")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -std=c++14 -pthread -Ofast -mtune=native -fassociative-math -freciprocal-math -fno-signed-zeros -frename-registers -fopenmp")
else()
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -std=c++14 -pthread -g -Wall -Wextra -O0")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -std=c++14  -Ofast")
endif()
set(CMAKE_CXX_FLAGS_BENCHMARK "${CMAKE_CXX_FLAGS_RELEASE} -DBENCH")

set(CMAKE_BUILD_TYPE Debug)

set(LMFAO_LA_ROOT .)
set(LMFAO_LA_TEST ${LMFAO_LA_ROOT}/tests)
set(LMFAO_LA_INCLUDE ${LMFAO_LA_ROOT}/include)

include_directories(${LMFAO_LA_INCLUDE})
 
file(GLOB SRCFILES src/*.cpp)
add_executable(lmfaola main.cpp ${SRCFILES})

# Test environment. 
add_executable(lmfaola_test ${LMFAO_LA_TEST}/test.cpp)
target_include_directories(lmfaola_test PRIVATE ${LMFAO_LA_TEST})

set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost REQUIRED COMPONENTS system iostreams program_options)
include_directories(${Boost_INCLUDE_DIRS})

# Take care of Gtest library.
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

# Add libraries.
target_link_libraries(lmfaola ${Boost_LIBRARIES})
target_link_libraries(lmfaola_test ${Boost_LIBRARIES} ${GTEST_LIBRARIES})
