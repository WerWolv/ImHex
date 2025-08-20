#!/usr/bin/env sh

pacman -S --needed --noconfirm pactoys unzip git
pacboy -S --needed --noconfirm  \
  gcc:p             \
  lld:p             \
  cmake:p           \
  ccache:p          \
  glfw:p            \
  file:p            \
  curl-winssl:p     \
  mbedtls:p         \
  freetype:p        \
  dlfcn:p           \
  ninja:p           \
  capstone:p        \
  zlib:p            \
  bzip2:p           \
  xz:p              \
  zstd:p            \
  lz4:p             \
  libssh2-wincng:p  \
  md4c:p
