#!/usr/bin/env sh

pacman -S $@ --needed \
  cmake     \
  gcc       \
  lld       \
  glfw      \
  file      \
  mbedtls   \
  python3   \
  freetype2 \
  xdg-desktop-portal
