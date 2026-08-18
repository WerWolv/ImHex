#!/usr/bin/env sh

# Install pkgconf (adds minimum dependencies) only if the equivalent pkf-config is not already installed.
if ! which pkg-config
then
    PKGCONF="pkgconf"
fi

apt install -y \
  build-essential       \
  gcc-16                \
  g++-16                \
  lld                   \
  ${PKGCONF:-}          \
  cmake                 \
  ccache                \
  libgl-dev             \
  libglu1-mesa-dev      \
  libwayland-dev        \
  libwayland-bin        \
  libxkbcommon-dev      \
  libx11-dev            \
  libxrandr-dev         \
  libxinerama-dev       \
  libxcursor-dev        \
  libxi-dev             \
  libxext-dev           \
  libglm-dev            \
  libmagic-dev          \
  libmbedtls-dev        \
  libfontconfig-dev     \
  libfreetype-dev       \
  libdbus-1-dev         \
  libcurl4-gnutls-dev   \
  libgtk-3-dev          \
  ninja-build           \
  zlib1g-dev            \
  libbz2-dev            \
  liblzma-dev           \
  libzstd-dev           \
  liblz4-dev            \
  libssh2-1-dev         \
  libmd4c-dev           \
  libmd4c-html0-dev
