cmake_minimum_required(VERSION 3.16)
project(LLVMDemangle)

set(CMAKE_CXX_STANDARD 17)

add_library(LLVMDemangle
  Demangle.cpp
  ItaniumDemangle.cpp
  MicrosoftDemangle.cpp
  MicrosoftDemangleNodes.cpp
)

target_include_directories(LLVMDemangle PUBLIC ../include)
