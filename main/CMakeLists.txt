project(main)

add_executable(main ${APPLICATION_TYPE}
    source/main.cpp
    source/crash_handlers.cpp

    source/window/window.cpp
    source/window/win_window.cpp
    source/window/macos_window.cpp
    source/window/linux_window.cpp

    source/init/splash_window.cpp
    source/init/tasks.cpp

    ${IMHEX_ICON}
)

if(LLVM_VERSION_MAJOR)
    message(STATUS "Adding LLVM_VERSION_MAJOR=${LLVM_VERSION_MAJOR} to source/crash_handlers.cpp for llvm::itaniumDemangle")
    set_property(
        SOURCE source/crash_handlers.cpp
        APPEND
        PROPERTY COMPILE_DEFINITIONS
        "LLVM_VERSION_MAJOR=${LLVM_VERSION_MAJOR}"
    )
endif()

target_include_directories(main PUBLIC include)
setupCompilerWarnings(main)

set(LIBROMFS_RESOURCE_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/romfs)
set(LIBROMFS_PROJECT_NAME imhex)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../lib/external/libromfs ${CMAKE_CURRENT_BINARY_DIR}/main/libromfs EXCLUDE_FROM_ALL)
set_target_properties(${LIBROMFS_LIBRARY} PROPERTIES POSITION_INDEPENDENT_CODE ON)

set_target_properties(main PROPERTIES
        OUTPUT_NAME "imhex"
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/..
        CXX_VISIBILITY_PRESET hidden
        POSITION_INDEPENDENT_CODE ON)

add_compile_definitions(IMHEX_PROJECT_NAME="${PROJECT_NAME}")

target_link_libraries(main PRIVATE libromfs-imhex libimhex ${FMT_LIBRARIES})
if (WIN32)
    target_link_libraries(main PRIVATE usp10 wsock32 ws2_32 Dwmapi.lib)
else ()
    target_link_libraries(main PRIVATE pthread)
endif ()

if (APPLE)
    add_compile_definitions(GL_SILENCE_DEPRECATION)
endif ()
