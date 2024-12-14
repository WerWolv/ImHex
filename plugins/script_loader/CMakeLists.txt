cmake_minimum_required(VERSION 3.16)

include(ImHexPlugin)

find_package(CoreClrEmbed)
add_library(dotnet INTERFACE)
if (CoreClrEmbed_FOUND)
    set(IMHEX_DOTNET_SCRIPT_SUPPORT ON)

    target_compile_definitions(dotnet INTERFACE IMHEX_DOTNET_RUNTIME_VERSION="${CoreClrEmbed_VERSION}")

    add_library(nethost SHARED IMPORTED)
    target_include_directories(nethost INTERFACE "${CoreClrEmbed_INCLUDE_DIRS}")
    get_filename_component(CoreClrEmbed_FOLDER ${CoreClrEmbed_SHARED_LIBRARIES} DIRECTORY)
    set_target_properties(nethost
            PROPERTIES
            IMPORTED_IMPLIB     ${CoreClrEmbed_SHARED_LIBRARIES}
            IMPORTED_LOCATION   ${CoreClrEmbed_LIBRARIES}
            BUILD_RPATH         ${CoreClrEmbed_FOLDER}
            INSTALL_RPATH       ${CoreClrEmbed_FOLDER})

    set(EXTRA_BUNDLE_LIBRARY_PATHS ${EXTRA_BUNDLE_LIBRARY_PATHS} "${CoreClrEmbed_FOLDER}" PARENT_SCOPE)

    if (IMHEX_BUNDLE_DOTNET)
        install(FILES ${CoreClrEmbed_SHARED_LIBRARIES} DESTINATION ${CMAKE_INSTALL_LIBDIR})
    endif()

    message(STATUS "Enabling .NET Scripting support!")

    target_link_directories(dotnet INTERFACE ${CoreClrEmbed_FOLDER})
    target_include_directories(dotnet INTERFACE ${CoreClrEmbed_INCLUDE_DIRS})
    target_compile_definitions(dotnet INTERFACE IMHEX_DOTNET_SCRIPT_SUPPORT=1)
    target_sources(dotnet INTERFACE
            source/loaders/dotnet/dotnet_loader.cpp
    )

    add_subdirectory(support/dotnet)
    add_dependencies(dotnet AssemblyLoader)
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
        ui
        dotnet
)