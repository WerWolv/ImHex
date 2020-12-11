#!/usr/bin/env sh

echo "As of 2020-12, Debian stable does not include g++-10, needs debian testing or unstable."

# Tested on 2020-12-09 with Docker image bitnami/minideb:unstable

# Install pkgconf (adds minimum dependencies) only if the equivalent pkf-config is not already installed.
if !which pkg-config
then
    PKGCONF="pkgconf"
fi

apt install \
  cmake \
  g++-10 \
  ${PKGCONF:-} \
  nlohmann-json3-dev \
  libcapstone-dev \
  libmagic-dev \
  libglfw3-dev \
  libglm-dev \
  libjsoncpp-dev \
  llvm-dev \
  libssl-dev \
  libstdc++-10-dev \
  python3-dev \
  libfreetype-dev

echo "Please consider this before running cmake (useful on e.g. Ubuntu 20.04):"
echo "export CXX=g++-10"
