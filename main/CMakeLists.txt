if (WIN32)
    set(IMHEX_APPLICATION_NAME "imhex-gui")
else ()
    set(IMHEX_APPLICATION_NAME "imhex")
endif ()

add_subdirectory(gui)
if (WIN32)
    add_subdirectory(forwarder)

    if (NOT MSVC)
        add_subdirectory(version_stripper)
    endif()
endif ()

if (NOT EMSCRIPTEN)
    add_subdirectory(updater)
endif ()