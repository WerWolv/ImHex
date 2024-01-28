cmake_minimum_required(VERSION 3.16)

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
        INCLUDES
            include
        LIBRARIES
            ui
            fonts
            ${JTHREAD_LIBRARIES}
    )

endif ()
