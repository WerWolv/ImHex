#!/usr/bin/env sh

pacman -S $@ --needed \
  cmake         \
  gcc           \
  lld           \
  libglvnd      \
  wayland       \
  libxkbcommon  \
  libx11        \
  libxrandr     \
  libxinerama   \
  libxcursor    \
  libxi         \
  libxext       \
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
  lz4           \
  libssh2       \
  md4c
