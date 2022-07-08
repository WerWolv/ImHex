#!/usr/bin/env sh

# Install pkgconf (adds minimum dependencies) only if the equivalent pkf-config is not already installed.
if ! which pkg-config
then
    PKGCONF="pkgconf"
fi

apt install -y \
  build-essential       \
  gcc-12                \
  g++-12                \
  lld                   \
  ${PKGCONF:-}          \
  cmake                 \
  make                  \
  ccache                \
  libglfw3-dev          \
  libglm-dev            \
  libmagic-dev          \
  libmbedtls-dev        \
  python3-dev           \
  libfreetype-dev       \
  libdbus-1-dev         \
  xdg-desktop-portal
