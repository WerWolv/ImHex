#!/usr/bin/env sh

pacman -S $@ --needed \
  cmake         \
  gcc           \
  lld           \
  glfw          \
  fontconfig    \
  file          \
  mbedtls       \
  freetype2     \
  dbus          \
  gtk3          \
  curl          \
  fmt           \
  yara          \
  nlohmann-json \
  ninja         \
  zlib          \
  bzip2         \
  xz            \
  zstd          \
  lz4
