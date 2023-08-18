cmake_minimum_required(VERSION 3.11)

project(Consistency CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/)

include_directories(${CMAKE_SOURCE_DIR}/include)

add_executable(exp_con exp.cpp)

target_compile_options(exp_con PUBLIC -O3)