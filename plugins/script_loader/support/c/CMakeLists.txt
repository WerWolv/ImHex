project(c_api)

if (IMHEX_STATIC_LINK_PLUGINS)
    set(C_API_LIBRARY_TYPE STATIC)
else()
    set(C_API_LIBRARY_TYPE OBJECT)
endif()

add_library(c_api ${C_API_LIBRARY_TYPE}
    source/script_api/v1/bookmarks.cpp
    source/script_api/v1/logger.cpp
    source/script_api/v1/mem.cpp
    source/script_api/v1/ui.cpp
)

target_include_directories(c_api PUBLIC
    include
)
target_link_libraries(c_api PRIVATE libimhex ui)
target_compile_definitions(c_api PRIVATE IMHEX_PROJECT_NAME="Script")