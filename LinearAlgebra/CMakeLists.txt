cmake_minimum_required(VERSION 2.8.9)

include_directories(include)
 
file(GLOB SOURCES "src/*.cpp")
project (hello)
add_executable(hello main.cpp)
