FROM archlinux:latest

MAINTAINER WerWolv "hey@werwolv.net"

# Install dependencies
RUN pacman -Syy --needed --noconfirm
RUN pacman -S --needed --noconfirm      \
    git                                 \
    cmake                               \
    base-devel                          \
    gcc                                 \
    pkg-config                          \
    glfw-x11                            \
    file                                \
    mbedtls                             \
    python3                             \
    freetype2                           \
    dbus                                \
    xdg-desktop-portal

# Clone ImHex
RUN git clone https://github.com/WerWolv/ImHex --recurse-submodules /root/ImHex

# Build ImHex
RUN mkdir /root/ImHex/build
WORKDIR /root/ImHex/build
RUN cmake .. && make -j
WORKDIR /root/ImHex