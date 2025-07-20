cmake_minimum_required(VERSION 3.16)

include(ImHexPlugin)

if (NOT WIN32)
    find_package(OpenSSL REQUIRED)
endif()
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

if (NOT WIN32)
    target_link_libraries(remote PRIVATE OpenSSL::SSL OpenSSL::Crypto)
endif()
