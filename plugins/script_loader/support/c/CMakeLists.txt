project(c_api)

add_library(c_api OBJECT
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