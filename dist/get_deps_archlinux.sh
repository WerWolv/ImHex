#!/usr/bin/env sh

pacman -S $@ --needed \
  cmake     \
  gcc       \
  lld       \
  glfw      \
  file      \
  python3   \
  freetype2 \
  dbus      \
  xdg-desktop-portal \
  gnutls
