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
  xdg-desktop-portal \
  curl      \
  fmt       \
  yara      \
  nlohmann-json
