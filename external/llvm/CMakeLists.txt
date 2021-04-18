cmake_minimum_required(VERSION 3.16)
project(LLVMDemangle)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")

add_library(LLVMDemangle STATIC
        Demangle/Demangle.cpp
        Demangle/ItaniumDemangle.cpp
        Demangle/MicrosoftDemangle.cpp
        Demangle/MicrosoftDemangleNodes.cpp
)

target_include_directories(LLVMDemangle PUBLIC include)
