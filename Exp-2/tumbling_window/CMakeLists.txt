cmake_minimum_required(VERSION 3.11)

project(TumblingWindow CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/)

include_directories(${CMAKE_SOURCE_DIR}/include)

add_executable(exp_tumbling exp.cpp)

target_compile_options(exp_tumbling PUBLIC -O3)
