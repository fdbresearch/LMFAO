cmake_minimum_required(VERSION 2.8.9)

project(lmfao_algebra)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -std=c++14 -pthread -g -Wall -Wextra -Og")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -std=c++14 -pthread -Ofast -mtune=native -fassociative-math -freciprocal-math -fno-signed-zeros -frename-registers -fopenmp")
  message("Hey")
else()
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -std=c++14 -pthread -g -Wall -Wextra -O0")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -std=c++14  -Ofast")
endif()
set(CMAKE_CXX_FLAGS_BENCHMARK "${CMAKE_CXX_FLAGS_RELEASE} -DBENCH")

# Set to Debug, Benchmark or Release.
set(CMAKE_BUILD_TYPE Benchmark)
#set(LMFAO_RUN false)
#set(LMFAO_TEST false)

set(LMFAO_LA_ROOT .)
set(LMFAO_LA_TEST ${LMFAO_LA_ROOT}/tests)
set(LMFAO_LA_INCLUDE ${LMFAO_LA_ROOT}/include)

include_directories(${LMFAO_LA_INCLUDE})

file(GLOB SRCFILES src/*.cpp)

if (LMFAO_LIB)
  add_library(lmfaolalib STATIC ${SRCFILES})
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
find_package(Boost REQUIRED COMPONENTS system iostreams program_options thread)
include_directories(${Boost_INCLUDE_DIRS})

if (LMFAO_TEST)
  # Take care of Gtest library.
  find_package(GTest REQUIRED)
  include_directories(${GTEST_INCLUDE_DIRS})
endif()

# Add libraries.
if (LMFAO_RUN)
  target_link_libraries(lmfaola ${Boost_LIBRARIES})
endif()
if (LMFAO_TEST)
  target_link_libraries(lmfaola_test ${Boost_LIBRARIES} ${GTEST_LIBRARIES})
endif()
