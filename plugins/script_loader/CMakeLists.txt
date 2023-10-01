cmake_minimum_required(VERSION 3.16)

if (NOT EMSCRIPTEN)

    include(ImHexPlugin)
    find_package(CoreClrEmbed)

    add_imhex_plugin(
        NAME
            script_loader

        SOURCES
            source/plugin_script_loader.cpp

        INCLUDES
            include
    )

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

        target_link_directories(script_loader PRIVATE ${CoreClrEmbed_FOLDER})
        target_include_directories(script_loader PRIVATE ${CoreClrEmbed_INCLUDE_DIRS})
        target_compile_definitions(script_loader PRIVATE DOTNET_PLUGINS=1)
        target_sources(script_loader PRIVATE
                source/loaders/dotnet/dotnet_loader.cpp

                source/script_api/v1/mem.cpp
                source/script_api/v1/bookmarks.cpp
                source/script_api/v1/ui.cpp
        )

        set(EXTRA_BUNDLE_LIBRARY_PATHS "${CoreClrEmbed_FOLDER}" PARENT_SCOPE)

        if (IMHEX_BUNDLE_DOTNET)
            install(FILES ${CoreClrEmbed_SHARED_LIBRARIES} DESTINATION ${CMAKE_INSTALL_LIBDIR})
        endif ()

        add_subdirectory(dotnet)
        add_dependencies(script_loader AssemblyLoader)

    endif ()

endif ()