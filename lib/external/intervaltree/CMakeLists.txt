cmake_minimum_required(VERSION 3.16)
project(intervaltree)

set(CMAKE_CXX_STANDARD20)

add_library(intervaltree INTERFACE)

target_include_directories(intervaltree INTERFACE include)
target_compile_options(intervaltree INTERFACE "-DUSE_INTERVAL_TREE_NAMESPACE")