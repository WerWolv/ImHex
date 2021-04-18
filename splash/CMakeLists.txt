cmake_minimum_required(VERSION 3.16)
include("${CMAKE_SOURCE_DIR}/cmake/build_helpers.cmake")
include("${CMAKE_SOURCE_DIR}/cmake/modules/PostprocessBundle.cmake")

get_target_property(IMHEX_VERSION imhex VERSION)
project(splash VERSION ${IMHEX_VERSION})

addVersionDefines()
configurePackageCreation()

if (WIN32)
    set(application_type "WIN32")
else ()
    set(application_type)
endif ()

add_executable(splash ${application_type}
        source/main.cpp
        source/splash_window.cpp
        ${imhex_icon}
)

target_include_directories(splash PUBLIC include)

add_compile_definitions(IMHEX_FILE_NAME=\"$<TARGET_FILE_NAME:imhex>\")
set_target_properties(splash PROPERTIES OUTPUT_NAME "imhex")
add_dependencies(splash imhex)


if (WIN32)
    target_link_libraries(splash libimhex winpthread)
else ()
    target_link_libraries(splash libimhex pthread)
endif ()