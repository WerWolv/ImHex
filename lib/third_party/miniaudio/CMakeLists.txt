cmake_minimum_required(VERSION 3.16)
project(miniaudio)

add_library(miniaudio STATIC
        source/miniaudio.c
)

target_include_directories(miniaudio PUBLIC include)
target_compile_options(miniaudio PRIVATE -Wno-unused-result)
if (APPLE)
    set_source_files_properties(source/miniaudio.c PROPERTIES LANGUAGE OBJC)
endif ()
