project(OmniWindow-sim)

cmake_minimum_required(VERSION 3.5)

add_compile_options(-march=native)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY bin)
set(CMAKE_CXX_STANDARD 17)

include_directories(include)

add_subdirectory(lib)
add_subdirectory(apps)
