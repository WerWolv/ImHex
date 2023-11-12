cmake_minimum_required(VERSION 3.16)
project(LLVMDemangle)

set(CMAKE_CXX_STANDARD 17)

add_library(LLVMDemangle STATIC
    source/Demangle.cpp
    source/DLangDemangle.cpp
    source/ItaniumDemangle.cpp
    source/MicrosoftDemangle.cpp
    source/MicrosoftDemangleNodes.cpp
    source/RustDemangle.cpp
)

target_include_directories(LLVMDemangle PUBLIC include)
