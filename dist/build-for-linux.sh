#!/bin/bash

# Go to this scripts folder
cd "$(dirname "$0")"

# Get submodules
git submodule update --init --recursive

# Make and enter build directory
mkdir -p build
cd build

# Build
CC=gcc-12 CXX=g++-12 cmake                    \
    -DCMAKE_BUILD_TYPE=Release                \
    -DCMAKE_INSTALL_PREFIX="/usr" 	          \
    -DCMAKE_C_COMPILER_LAUNCHER=ccache        \
    -DCMAKE_CXX_COMPILER_LAUNCHER=ccache      \
    -DCMAKE_C_FLAGS="-fuse-ld=lld"            \
    -DCMAKE_CXX_FLAGS="-fuse-ld=lld"          \
    -DCMAKE_OBJC_COMPILER_LAUNCHER=ccache     \
    -DCMAKE_OBJCXX_COMPILER_LAUNCHER=ccache   \
    ../..
if [ "$(id -u)" -eq 0 ]; then
  make -j 4 install
else
  sudo make -j 4 install
fi
