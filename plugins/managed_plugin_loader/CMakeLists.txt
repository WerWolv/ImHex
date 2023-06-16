cmake_minimum_required(VERSION 3.16)

# Change this to the name of your plugin #
project(managed_plugin_loader)

# Add your source files here #

find_package(CoreClrEmbed)

add_library(${PROJECT_NAME} SHARED
        source/plugin_managed_plugin_loader.cpp
)
# Add additional include directories here #
target_include_directories(${PROJECT_NAME} PRIVATE include)
# Add additional libraries here #
target_link_libraries(${PROJECT_NAME} PRIVATE libimhex ${FMT_LIBRARIES} nethost)

if (CoreClrEmbed_FOUND)
    add_library(nethost SHARED IMPORTED)
    target_include_directories(nethost INTERFACE "${CoreClrEmbed_INCLUDE_DIRS}")
    get_filename_component(CoreClrEmbed_FOLDER ${CoreClrEmbed_SHARED_LIBRARIES} DIRECTORY)
    set_target_properties(nethost
            PROPERTIES
            IMPORTED_IMPLIB     ${CoreClrEmbed_SHARED_LIBRARIES}
            IMPORTED_LOCATION   ${CoreClrEmbed_LIBRARIES}
            BUILD_RPATH         ${CoreClrEmbed_FOLDER}
            INSTALL_RPATH       ${CoreClrEmbed_FOLDER})

    target_link_directories(${PROJECT_NAME} PRIVATE ${CoreClrEmbed_FOLDER})
    target_compile_definitions(${PROJECT_NAME} PRIVATE DOTNET_PLUGINS=1)
    target_sources(${PROJECT_NAME} PRIVATE source/loaders/dotnet/dotnet_loader.cpp)

    set(EXTRA_BUNDLE_LIBRARY_PATHS "${CoreClrEmbed_FOLDER}" PARENT_SCOPE)

    if (APPLE)
        install(FILES ${CoreClrEmbed_LIBRARIES} DESTINATION ${CMAKE_INSTALL_LIBDIR})
    endif ()
endif ()


# ---- No need to change anything from here downwards unless you know what you're doing ---- #

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_SHARED_LIBRARY_PREFIX "")
set(CMAKE_SHARED_LIBRARY_SUFFIX ".hexplug")

if (WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,--allow-multiple-definition -fvisibility=hidden")
endif()

add_compile_definitions(IMHEX_PROJECT_NAME="${PROJECT_NAME}")
set_target_properties(${PROJECT_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
setupCompilerWarnings(${PROJECT_NAME})

set(LIBROMFS_RESOURCE_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/romfs)
set(LIBROMFS_PROJECT_NAME ${PROJECT_NAME})
add_subdirectory(../../lib/external/libromfs ${CMAKE_CURRENT_BINARY_DIR}/libromfs)
set_target_properties(${LIBROMFS_LIBRARY} PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBROMFS_LIBRARY})