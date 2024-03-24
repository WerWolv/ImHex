cmake_minimum_required(VERSION 3.16)

include(ImHexPlugin)

find_package(CoreClrEmbed)
if (CoreClrEmbed_FOUND)
    set(IMHEX_DOTNET_SCRIPT_SUPPORT ON)

    add_library(nethost SHARED IMPORTED)
    target_include_directories(nethost INTERFACE "${CoreClrEmbed_INCLUDE_DIRS}")
    get_filename_component(CoreClrEmbed_FOLDER ${CoreClrEmbed_SHARED_LIBRARIES} DIRECTORY)
    set_target_properties(nethost
            PROPERTIES
            IMPORTED_IMPLIB     ${CoreClrEmbed_SHARED_LIBRARIES}
            IMPORTED_LOCATION   ${CoreClrEmbed_LIBRARIES}
            BUILD_RPATH         ${CoreClrEmbed_FOLDER}
            INSTALL_RPATH       ${CoreClrEmbed_FOLDER})

    set(EXTRA_BUNDLE_LIBRARY_PATHS "${CoreClrEmbed_FOLDER}" PARENT_SCOPE)

    if (IMHEX_BUNDLE_DOTNET)
        install(FILES ${CoreClrEmbed_SHARED_LIBRARIES} DESTINATION ${CMAKE_INSTALL_LIBDIR})
    endif()
endif()

find_package(Python3 COMPONENTS Interpreter Development.Embed)
if (Python3_FOUND)
    set(IMHEX_PYTHON_SCRIPT_SUPPORT ON)

    get_target_property(PYTHON_LIBRARY Python3::Python IMPORTED_LOCATION)
    get_target_property(PYTHON_INCLUDE_DIR Python3::Python INTERFACE_INCLUDE_DIRECTORIES)
endif()

add_subdirectory(support/c)



add_imhex_plugin(
    NAME
        script_loader

    SOURCES
        source/plugin_script_loader.cpp
    INCLUDES
        include

    LIBRARIES
        c_api
        fonts

    FEATURES
        DOTNET
        PYTHON
)



if (IMHEX_DOTNET_SCRIPT_SUPPORT)
    message(STATUS "Enabling .NET Scripting support!")

    target_link_directories(script_loader PRIVATE ${CoreClrEmbed_FOLDER})
    target_include_directories(script_loader PRIVATE ${CoreClrEmbed_INCLUDE_DIRS})
    target_compile_definitions(script_loader PRIVATE IMHEX_DOTNET_SCRIPT_SUPPORT=1)
    target_sources(script_loader PRIVATE
            source/loaders/dotnet/dotnet_loader.cpp
    )

    add_subdirectory(support/dotnet)
    add_dependencies(script_loader AssemblyLoader)
    enable_plugin_feature(DOTNET)
endif()

if (IMHEX_PYTHON_SCRIPT_SUPPORT)
    message(STATUS "Enabling Python Scripting support!")

    target_compile_definitions(script_loader PRIVATE IMHEX_PYTHON_SCRIPT_SUPPORT=1)
    target_sources(script_loader PRIVATE
            source/loaders/python/python_loader.cpp
            source/loaders/python/library_wrapper.cpp
    )

    target_include_directories(script_loader PRIVATE ${PYTHON_INCLUDE_DIR})
    target_compile_definitions(script_loader PRIVATE PYTHON_LIBRARY_PATH="${PYTHON_LIBRARY}")
    enable_plugin_feature(PYTHON)
endif()