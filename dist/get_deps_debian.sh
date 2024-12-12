#!/usr/bin/env sh

# Install pkgconf (adds minimum dependencies) only if the equivalent pkf-config is not already installed.
if ! which pkg-config
then
    PKGCONF="pkgconf"
fi

apt install -y \
  build-essential       \
  gcc-14                \
  g++-14                \
  lld                   \
  ${PKGCONF:-}          \
  cmake                 \
  ccache                \
  libglfw3-dev          \
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
  liblz4-dev
