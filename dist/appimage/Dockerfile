FROM ubuntu:24.10 as build

# Used to invalidate layer cache but not mount cache
# See https://github.com/moby/moby/issues/41715#issuecomment-733976493
ARG UNIQUEKEY 1

COPY dist/get_deps_debian.sh /tmp
RUN --mount=type=cache,target=/var/apt/cache <<EOF
# Install apt dependencies
set -xe

apt update

# general deps
apt install -y ccache git wget
# appimage tools deps
apt install -y python3-pip python3-venv python3-setuptools desktop-file-utils libgdk-pixbuf2.0-dev fuse ninja-build
apt install -y squashfs-tools zsync

# imhex deps
/tmp/get_deps_debian.sh
EOF

ENV PATH="/cache/bin/:${PATH}"

# Copy Imhex source
COPY . /imhex

ARG LTO=ON
ARG BUILD_TYPE=RelWithDebInfo
ARG GIT_COMMIT_HASH
ARG GIT_BRANCH
WORKDIR /build

# Ubuntu sh doesnt support string substitution
SHELL ["bash", "-c"]

RUN <<EOF
# Prepare ImHex build
set -xe

CC=gcc-14 CXX=g++-14 cmake -G "Ninja"                  \
    -DCMAKE_BUILD_TYPE=${BUILD_TYPE}                   \
    -DCMAKE_INSTALL_PREFIX="/usr"                      \
    -DCMAKE_C_COMPILER_LAUNCHER=ccache                 \
    -DCMAKE_CXX_COMPILER_LAUNCHER=ccache               \
    -DIMHEX_PATTERNS_PULL_MASTER=ON                    \
    -DIMHEX_COMMIT_HASH_LONG="${GIT_COMMIT_HASH}"      \
    -DIMHEX_COMMIT_BRANCH="${GIT_BRANCH}"              \
    -DIMHEX_ENABLE_LTO=${LTO}                          \
    -DIMHEX_PLUGINS_IN_SHARE=ON                        \
    /imhex
EOF

ENV CCACHE_DIR /cache/ccache
RUN --mount=type=cache,target=/cache <<EOF
# Build Imhex
set -xe

ccache -zs
DESTDIR=AppDir ninja install
ccache -s
EOF

RUN <<EOF
# Download appimage-builder
set -xe

mkdir -p /cache/bin
wget -nc https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage -O /cache/bin/appimagetool || true
chmod +x /cache/bin/appimagetool

python3 -m venv venv
. venv/bin/activate
pip3 install git+https://github.com/AppImageCrafters/appimage-builder@f38699e

# Package ImHex as AppImage
export VERSION=$(cat /imhex/VERSION)
appimage-builder --recipe /imhex/dist/AppImageBuilder.yml
EOF

FROM scratch

# Copy build artifact
COPY --from=build /build/*.AppImage /build/*.AppImage.zsync ./
