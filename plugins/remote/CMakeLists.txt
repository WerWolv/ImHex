cmake_minimum_required(VERSION 3.16)

include(ImHexPlugin)

find_package(libssh2 REQUIRED)

add_imhex_plugin(
    NAME
        remote
    SOURCES
        source/plugin_remote.cpp
        source/content/helpers/sftp_client.cpp
        source/content/providers/ssh_provider.cpp

    INCLUDES
        include
        ${LIBSSH2_INCLUDE_DIR}

    LIBRARIES
        ui
        fonts
        ${LIBSSH2_LIBRARY}
)
