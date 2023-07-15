#!/usr/bin/env sh

pacman -S $@ --needed \
  cmake     \
  gcc       \
  lld       \
  glfw      \
  file      \
  mbedtls   \
  freetype2 \
  dbus      \
  gtk3      \
  curl      \
  fmt       \
  yara      \
  nlohmann-json \
  ninja
