cmake_minimum_required (VERSION 2.4)

project (multifaq)

# Specialised compiler flags such as WORKERS, FEATURES or INTERACTIONS, TABLES or ATTRIBUTES can be set. For instance "-DWORKERS=2". These must be consistent with the values in the configuration files.
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -std=c++17 -pthread -g -Wall -Wextra -Og")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -std=c++17 -pthread -Ofast -mtune=native -fassociative-math -freciprocal-math -fno-signed-zeros -frename-registers -fopenmp")
else()
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -std=c++17 -pthread -g -Wall -Wextra -O0")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -std=c++17")
endif()
set(CMAKE_CXX_FLAGS_BENCHMARK "${CMAKE_CXX_FLAGS_RELEASE} -DBENCH")

# Set to Debug, Benchmark or Release.
set(CMAKE_BUILD_TYPE Benchmark)
#set(LMFAO_TEST false)
#set(LMFAO_RUN true)
#set(LMFAO_LIB true)

include_directories(include/compiler include/treeDecomposition include/application include/launcher include/templates include/codegen include/utils /usr/local/include)

add_subdirectory(src/treeDecomposition)
add_subdirectory(src/compiler)
add_subdirectory(src/codegen)
add_subdirectory(src/application)
add_subdirectory(src/launcher)
#add_subdirectory(LA-F)

add_executable (multifaq multiFAQ.cpp)
target_link_libraries (multifaq launcher application codegen compiler treeDecomposition)

# Take care of Boost libraries.
set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost COMPONENTS system iostreams filesystem program_options)

if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
    target_link_libraries(multifaq ${Boost_LIBRARIES})
endif()

