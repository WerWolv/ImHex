#!/usr/bin/env sh

echo "As of 2020-12, Debian stable does not include g++-10, needs debian testing or unstable."

# Tested on 2020-12-09 with Docker image bitnami/minideb:unstable

# Install pkgconf (adds minimum dependencies) only if the equivalent pkf-config is not already installed.
if !which pkg-config
then
    PKGCONF="pkgconf"
fi

apt install -y \
  build-essential       \
  gcc-10                \
  g++-10                \
  ${PKGCONF:-}          \
  cmake                 \
  make                  \
  libglfw3-dev          \
  libglm-dev            \
  libmagic-dev          \
  libmbedtls-dev        \
  libcapstone-dev       \
  python3-dev           \
  libfreetype-dev       \
  libgtk-3-dev          \

echo "Please consider this before running cmake (useful on e.g. Ubuntu 20.04):"
echo "export CXX=g++-10"
