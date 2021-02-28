#!/usr/bin/env sh

pacman -S --needed --noconfirm \
  mingw-w64-x86_64-gcc \
  mingw-w64-x86_64-cmake \
  mingw-w64-x86_64-make \
  mingw-w64-x86_64-capstone \
  mingw-w64-x86_64-glfw \
  mingw-w64-x86_64-glm \
  mingw-w64-x86_64-file \
  mingw-w64-x86_64-mbedtls \
  mingw-w64-x86_64-python \
  mingw-w64-x86_64-freetype \
  mingw-w64-x86_64-dlfcn
