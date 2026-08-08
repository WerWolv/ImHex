ARG ARCHITECTURE_APPIMAGE_BUILDER=x86_64
FROM quay.io/pypa/manylinux_2_28_${ARCHITECTURE_APPIMAGE_BUILDER}:latest AS build

ARG ARCHITECTURE_APPIMAGE_BUILDER

# Used to invalidate layer cache but not mount cache
# See https://github.com/moby/moby/issues/41715#issuecomment-733976493
ARG UNIQUEKEY 1

RUN --mount=type=cache,target=/var/cache/dnf <<EOF
# Install build and AppImage dependencies
set -xe

dnf install -y \
    autoconf automake ccache curl dbus-devel desktop-file-utils file fontconfig-devel fuse gettext git glm-devel gtk3-devel kernel-headers libtool \
    libX11-devel libXcursor-devel libXext-devel libXi-devel libXinerama-devel libXrandr-devel libxkbcommon-devel lz4-devel \
    mesa-libGL-devel mesa-libGLU-devel ninja-build perl-IPC-Cmd perl-Time-Piece pkgconf-pkg-config squashfs-tools unzip wayland-devel wget zip
EOF

ARG VCPKG_BASELINE=66c0373dc7fca549e5803087b9487edfe3aca0a1
RUN <<EOF
# Install vcpkg
set -xe

git clone https://github.com/microsoft/vcpkg.git /vcpkg
git -C /vcpkg checkout ${VCPKG_BASELINE}
/vcpkg/bootstrap-vcpkg.sh -disableMetrics
EOF

ENV PATH="/cache/bin/:/vcpkg:${PATH}"
ENV VCPKG_ROOT=/vcpkg
ENV VCPKG_DEFAULT_BINARY_CACHE=/cache/vcpkg

ARG VCPKG_TARGET_TRIPLET=x64-linux-release
COPY dist/vcpkg.json /vcpkg-manifest/vcpkg.json
COPY dist/AppImage/vcpkg-triplets /vcpkg-triplets
RUN --mount=type=cache,target=/cache <<EOF
# Build dependencies with the manylinux toolchain
set -xe

mkdir -p ${VCPKG_DEFAULT_BINARY_CACHE}
vcpkg install \
    --x-manifest-root=/vcpkg-manifest \
    --x-install-root=/vcpkg_installed \
    --overlay-triplets=/vcpkg-triplets \
    --triplet=${VCPKG_TARGET_TRIPLET}
EOF

# Copy Imhex source
COPY . /imhex

ARG LTO=ON
ARG BUILD_TYPE=RelWithDebInfo
ARG GIT_COMMIT_HASH
ARG GIT_BRANCH
ARG ARCHITECTURE_FILE_NAME=x86_64
WORKDIR /build

# Use Bash for heredoc scripts and string substitution
SHELL ["bash", "-c"]

RUN <<EOF
# Prepare ImHex build
set -xe

# libdecor rejects its GTK plugin when static libpng symbols are globally visible.
cmake -G "Ninja"                                                     \
    -DCMAKE_BUILD_TYPE=${BUILD_TYPE}                                 \
    -DCMAKE_INSTALL_PREFIX="/usr"                                    \
    -DCMAKE_C_COMPILER_LAUNCHER=ccache                               \
    -DCMAKE_CXX_COMPILER_LAUNCHER=ccache                             \
    -DCMAKE_TOOLCHAIN_FILE=/vcpkg/scripts/buildsystems/vcpkg.cmake   \
    -DVCPKG_TARGET_TRIPLET=${VCPKG_TARGET_TRIPLET}                   \
    -DVCPKG_INSTALLED_DIR=/vcpkg_installed                           \
    -DVCPKG_MANIFEST_DIR=/imhex/dist                                 \
    -DVCPKG_MANIFEST_INSTALL=OFF                                     \
    -DVCPKG_OVERLAY_TRIPLETS=/vcpkg-triplets                         \
    -DCMAKE_SHARED_LINKER_FLAGS="-Wl,--exclude-libs,libpng16.a"      \
    -DCMAKE_MODULE_LINKER_FLAGS="-Wl,--exclude-libs,libpng16.a"      \
    -DIMHEX_PATTERNS_PULL_MASTER=ON                                  \
    -DIMHEX_COMMIT_HASH_LONG="${GIT_COMMIT_HASH}"                    \
    -DIMHEX_COMMIT_BRANCH="${GIT_BRANCH}"                            \
    -DIMHEX_ENABLE_LTO=${LTO}                                        \
    -DIMHEX_BUNDLE_PLUGIN_SDK=OFF                                    \
    `# To prevent using a libdir with an architecture-specific name` \
    -DCMAKE_INSTALL_LIBDIR="lib"                                     \
    /imhex
EOF

ENV CCACHE_DIR=/cache/ccache
RUN --mount=type=cache,target=/cache <<EOF
# Build Imhex
set -xe

ccache -zs
DESTDIR=AppDir ninja install
ccache -s
EOF

ARG LINUXDEPLOY_VERSION=1-alpha-20251107-1
RUN <<EOF
# Package ImHex as an AppImage
set -xe

mkdir -p /cache/bin
case "${ARCHITECTURE_APPIMAGE_BUILDER}" in
    x86_64) linuxdeploy_sha256=c20cd71e3a4e3b80c3483cef793cda3f4e990aca14014d23c544ca3ce1270b4d ;;
    aarch64) linuxdeploy_sha256=620095110d693282b8ebeb244a95b5e911cf8f65f76c88b4b47d16ae6346fcff ;;
    *) echo "Unsupported AppImage architecture: ${ARCHITECTURE_APPIMAGE_BUILDER}" >&2; exit 1 ;;
esac
wget https://github.com/linuxdeploy/linuxdeploy/releases/download/${LINUXDEPLOY_VERSION}/linuxdeploy-${ARCHITECTURE_APPIMAGE_BUILDER}.AppImage -O /cache/bin/linuxdeploy
echo "${linuxdeploy_sha256}  /cache/bin/linuxdeploy" | sha256sum --check -
chmod +x /cache/bin/linuxdeploy

envsubst '${ARCHITECTURE_APPIMAGE_BUILDER}' < /imhex/dist/AppImage/AppRun.env.in > AppDir/AppRun.env

VERSION=$(< /imhex/VERSION)
if [[ "${VERSION}" == *.WIP ]]; then
    release_tag=nightly
else
    release_tag=latest
fi

# Keep host GTK/GIO and libmount from the same system stack.
ARCH=${ARCHITECTURE_APPIMAGE_BUILDER} \
PATCHELF=$(command -v patchelf) \
LD_LIBRARY_PATH=AppDir/usr/lib \
LINUXDEPLOY_OUTPUT_VERSION=${VERSION} \
LDAI_OUTPUT=imhex-${VERSION}-${ARCHITECTURE_FILE_NAME}.AppImage \
LDAI_UPDATE_INFORMATION="gh-releases-zsync|WerWolv|ImHex|${release_tag}|imhex-*-${ARCHITECTURE_FILE_NAME}.AppImage.zsync" \
    /cache/bin/linuxdeploy --appimage-extract-and-run \
        --appdir AppDir \
        --output appimage \
        --exclude-library 'libmount.so*' \
        --desktop-file AppDir/usr/share/applications/imhex.desktop \
        --custom-apprun /imhex/dist/AppImage/AppRun.sh
EOF

FROM scratch

# Copy build artifact
COPY --from=build /build/*.AppImage /build/*.AppImage.zsync ./
