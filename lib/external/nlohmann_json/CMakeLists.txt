cmake_minimum_required(VERSION 3.16)
project(nlohmann_json)

set(CMAKE_CXX_STANDARD 17)

add_library(nlohmann_json INTERFACE)

target_include_directories(nlohmann_json INTERFACE include)
