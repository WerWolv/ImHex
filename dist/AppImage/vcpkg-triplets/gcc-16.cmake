set(CMAKE_C_COMPILER "$ENV{GCC_ROOT}/bin/gcc")
set(CMAKE_CXX_COMPILER "$ENV{GCC_ROOT}/bin/g++")
set(CMAKE_AR "$ENV{GCC_ROOT}/bin/gcc-ar")
set(CMAKE_NM "$ENV{GCC_ROOT}/bin/gcc-nm")
set(CMAKE_RANLIB "$ENV{GCC_ROOT}/bin/gcc-ranlib")
list(APPEND CMAKE_IGNORE_PREFIX_PATH "$ENV{GCC_ROOT}")

include("$ENV{VCPKG_ROOT}/scripts/toolchains/linux.cmake")
