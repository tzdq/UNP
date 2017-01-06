cmake_minimum_required(VERSION 3.6)
project(unp)

set(CMAKE_CXX_STANDARD 14)

set(SOURCE_FILES main.cpp utils.h utils.cpp unp.h readn.cpp)
add_executable(unp ${SOURCE_FILES})