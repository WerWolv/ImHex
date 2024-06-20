cmake_minimum_required(VERSION 3.16)

include(ImHexPlugin)

if (NOT USE_SYSTEM_MINIAUDIO)
    add_subdirectory(${THIRD_PARTY_LIBS_FOLDER}/miniaudio ${CMAKE_CURRENT_BINARY_DIR}/miniaudio EXCLUDE_FROM_ALL)
    set(MINIAUDIO_LIBRARIES miniaudio)
else()
    find_package(miniaudio REQUIRED)
endif()

add_imhex_plugin(
    NAME
        visualizers
    SOURCES
        source/plugin_visualizers.cpp

        source/content/pl_visualizers.cpp
        source/content/pl_inline_visualizers.cpp

        source/content/pl_visualizers/line_plot.cpp
        source/content/pl_visualizers/scatter_plot.cpp
        source/content/pl_visualizers/image.cpp
        source/content/pl_visualizers/3d_model.cpp
        source/content/pl_visualizers/sound.cpp
        source/content/pl_visualizers/coordinates.cpp
        source/content/pl_visualizers/timestamp.cpp
        source/content/pl_visualizers/table.cpp
        source/content/pl_visualizers/digital_signal.cpp
    INCLUDES
        include
        ${MINIAUDIO_INCLUDE_DIRS}

        LIBRARIES
        ${JTHREAD_LIBRARIES}
        ${MINIAUDIO_LIBRARIES}
    LIBRARIES
        fonts
)
