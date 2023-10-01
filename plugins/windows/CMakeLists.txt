cmake_minimum_required(VERSION 3.16)
# Add your source files here #
if (WIN32)

    include(ImHexPlugin)
    add_imhex_plugin(
        NAME
            windows
        SOURCES
            source/plugin_windows.cpp

            source/views/view_tty_console.cpp

            source/content/ui_items.cpp
            source/content/settings_entries.cpp
            source/content/providers.cpp

            source/content/providers/process_memory_provider.cpp
        INCLUDES
            include
    )

endif ()
