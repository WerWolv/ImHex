#!/usr/bin/env bash
# Installs what a typical desktop provides, plus the virtual displays used by
# run-test.sh. Nothing ImHex-specific is installed: anything the AppImage needs
# beyond a normal desktop must be bundled inside it.
#
# Both distribution families get the same set:
#   - GTK 3 and libdecor's GTK plugin: on Wayland, GLFW draws window decorations
#     through libdecor, which loads the host's GTK into ImHex. That's how host
#     libraries end up mixed with bundled ones.
#   - Mesa: software OpenGL/EGL, since there is no GPU.
#   - D-Bus: a session bus, like in a desktop session.
#   - libgpg-error: present on every desktop (GnuPG, libgcrypt), but missing
#     from minimal Ubuntu images.
#   - Xvfb + xwininfo: virtual X11 server and a tool to list its windows.
#   - Weston: Wayland compositor, run without a screen.
set -euo pipefail

source /etc/os-release

case "${ID}" in
    ubuntu|debian)
        apt-get update
        DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
            dbus-daemon dbus-bin \
            libgpg-error0 \
            libgtk-3-0t64 libdecor-0-plugin-1-gtk \
            libgl1-mesa-dri libglx-mesa0 libegl-mesa0 libopengl0 fonts-dejavu-core \
            xvfb x11-utils \
            weston \
            procps
        rm -rf /var/lib/apt/lists/*
        ;;
    fedora)
        dnf install -y --setopt=install_weak_deps=False \
            dbus-daemon \
            gtk3 libdecor \
            mesa-dri-drivers mesa-libGL mesa-libEGL libglvnd-opengl dejavu-sans-fonts \
            xorg-x11-server-Xvfb xwininfo \
            weston \
            procps-ng shadow-utils
        dnf clean all
        ;;
    *)
        echo "Unsupported distribution: ${ID}" >&2
        exit 1
        ;;
esac
