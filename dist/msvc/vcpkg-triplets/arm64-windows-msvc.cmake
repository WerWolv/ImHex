set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)

if(NOT CMAKE_HOST_WIN32)
    set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "${VCPKG_ROOT_DIR}/scripts/toolchains/windows.cmake")
    set(VCPKG_C_FLAGS "-I/build/vcpkg_installed/arm64-windows-msvc/include")
    set(VCPKG_CXX_FLAGS "-I/build/vcpkg_installed/arm64-windows-msvc/include")
    set(VCPKG_CONFIGURE_MAKE_OPTIONS lt_cv_deplibs_check_method=pass_all)

    set(ENV{CC} cl.exe)
    set(ENV{CXX} cl.exe)
    set(ENV{PATH} "/usr/share/automake-1.16:/vcpkg/scripts/buildsystems/make_wrapper:/opt/msvc/bin/arm64:$ENV{PATH}")
endif()
