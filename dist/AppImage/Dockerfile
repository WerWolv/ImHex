ARG ARCHITECTURE_APPIMAGE_BUILDER=x86_64
ARG APPIMAGE_BUILDER_IMAGE=condaforge/linux-anvil-x86_64:alma8@sha256:0fab560017f2b4eda27cf0965f4547bac9fdde51508aa355443dd656576a6338
FROM ${APPIMAGE_BUILDER_IMAGE} AS build

ARG ARCHITECTURE_APPIMAGE_BUILDER

# Use Bash for heredoc scripts, pipefail, and string substitution.
SHELL ["/bin/bash", "-o", "pipefail", "-c"]

# Used to invalidate layer cache but not mount cache
# See https://github.com/moby/moby/issues/41715#issuecomment-733976493
ARG UNIQUEKEY 1

RUN --mount=type=cache,target=/var/cache/dnf <<EOF
# Install build and AppImage dependencies
set -xe

dnf install -y dnf-plugins-core epel-release
dnf config-manager --set-enabled powertools
dnf install -y \
    autoconf automake autoconf-archive bison bzip2 ccache curl dbus-devel desktop-file-utils diffutils file fontconfig-devel fuse gettext git glm-devel gtk3-devel gzip kernel-headers libtool make patch patchelf python3 tar xz \
    libX11-devel libXcursor-devel libXext-devel libXi-devel libXinerama-devel libXrandr-devel libxkbcommon-devel lz4-devel \
    mesa-libGL-devel mesa-libGLU-devel perl-IPC-Cmd perl-Time-Piece pkgconf-pkg-config squashfs-tools unzip wayland-devel wget zip
EOF

ARG GCC_VERSION=16.2.0
ENV GCC_ROOT=/opt/gcc-16
RUN --mount=type=cache,target=/opt/micromamba/pkgs <<EOF
# Install precompiled GCC and its glibc 2.28 sysroot
set -xe

case "${ARCHITECTURE_APPIMAGE_BUILDER}" in
    x86_64)
        conda_arch=64
        gxx_build=h38dc33c_1
        sysroot_build=h4ee821c_9
        target=x86_64-conda-linux-gnu
        ;;
    aarch64)
        conda_arch=aarch64
        gxx_build=h6eb44ee_1
        sysroot_build=h585391f_9
        target=aarch64-conda-linux-gnu
        ;;
    *) echo "Unsupported AppImage architecture: ${ARCHITECTURE_APPIMAGE_BUILDER}" >&2; exit 1 ;;
esac

MAMBA_ROOT_PREFIX=/opt/micromamba /opt/conda/bin/micromamba create -y -p ${GCC_ROOT} -c conda-forge \
    "gxx_linux-${conda_arch}=${GCC_VERSION}=${gxx_build}" \
    "sysroot_linux-${conda_arch}=2.28=${sysroot_build}" \
    cmake=4.4.3 \
    ninja=1.13.2

for tool in gcc g++ gcc-ar gcc-nm gcc-ranlib readelf; do
    ln -s "${target}-${tool}" "${GCC_ROOT}/bin/${tool}"
done

test "$("${GCC_ROOT}/bin/gcc" -dumpfullversion)" = "${GCC_VERSION}"
test "$(rpm --queryformat '%{VERSION}' -q glibc)" = "2.28"
EOF

ENV PATH="${GCC_ROOT}/bin:/cache/bin/:/vcpkg:${PATH}"
ENV CC="${GCC_ROOT}/bin/gcc"
ENV CXX="${GCC_ROOT}/bin/g++"

ARG VCPKG_BASELINE=9e593bb18ea69cc5095e012465dcd675a822ed0d
RUN <<EOF
# Install vcpkg
set -xe

git clone https://github.com/microsoft/vcpkg.git /vcpkg
git -C /vcpkg checkout ${VCPKG_BASELINE}
/vcpkg/bootstrap-vcpkg.sh -disableMetrics
EOF

ENV VCPKG_ROOT=/vcpkg
ENV VCPKG_DEFAULT_BINARY_CACHE=/cache/vcpkg

ARG VCPKG_TARGET_TRIPLET=x64-linux-release
COPY dist/vcpkg.json /vcpkg-manifest/vcpkg.json
COPY dist/AppImage/vcpkg-triplets /vcpkg-triplets
RUN --mount=type=cache,target=/cache <<EOF
# Build dependencies with GCC 16 and its glibc 2.28 sysroot
set -xe

mkdir -p ${VCPKG_DEFAULT_BINARY_CACHE}
vcpkg install \
    --x-manifest-root=/vcpkg-manifest \
    --x-install-root=/vcpkg_installed \
    --overlay-triplets=/vcpkg-triplets \
    --triplet=${VCPKG_TARGET_TRIPLET}
EOF

# Prefer the conda sysroot's patched glibc headers, then use AlmaLinux for
# desktop-library headers and libraries that are not part of that sysroot.
ENV CFLAGS="-idirafter /usr/include"
ENV CXXFLAGS="-idirafter /usr/include"
ENV LIBRARY_PATH="/usr/lib64:/usr/lib"
ENV LDFLAGS="-Wl,-rpath-link,/usr/lib64"

# Copy Imhex source
COPY . /imhex

ARG LTO=ON
ARG BUILD_TYPE=RelWithDebInfo
ARG GIT_COMMIT_HASH
ARG GIT_BRANCH
ARG ARCHITECTURE_FILE_NAME=x86_64
WORKDIR /build

RUN <<EOF
# Prepare ImHex build
set -xe

# libdecor rejects its GTK plugin when static libpng symbols are globally visible.
cmake -G "Ninja"                                                     \
    -DCMAKE_BUILD_TYPE=${BUILD_TYPE}                                 \
    -DCMAKE_INSTALL_PREFIX="/usr"                                    \
    -DCMAKE_C_COMPILER=${CC}                                         \
    -DCMAKE_CXX_COMPILER=${CXX}                                      \
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
install -Dm755 "$(g++ -print-file-name=libstdc++.so.6)" AppDir/usr/lib/libstdc++.so.6
install -Dm755 "$(gcc -print-file-name=libgcc_s.so.1)" AppDir/usr/lib/libgcc_s.so.1
readelf --version-info AppDir/usr/lib/libstdc++.so.6 | grep GLIBCXX_3.4.35 >/dev/null
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
if [[ "${VERSION}" = *.WIP ]]; then
    release_tag=nightly
else
    release_tag=latest
fi

# Remove conda and vcpkg build paths before linuxdeploy preserves the RPATHs.
shopt -s globstar nullglob
for file in AppDir/usr/bin/* AppDir/usr/lib/**/*; do
    case "${file}" in
        AppDir/usr/lib/libgcc_s.so.1|AppDir/usr/lib/libstdc++.so.6) continue ;;
    esac
    if patchelf --print-rpath "${file}" >/dev/null 2>&1; then
        patchelf --shrink-rpath --allowed-rpath-prefixes '$ORIGIN' "${file}"
    fi
done

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

readelf --file-header AppDir/usr/lib/libstdc++.so.6 >/dev/null
readelf --file-header AppDir/usr/lib/libgcc_s.so.1 >/dev/null

max_glibc=$(
    for file in AppDir/usr/bin/* AppDir/usr/lib/**/*; do
        if readelf --file-header "${file}" >/dev/null 2>&1; then
            readelf --version-info "${file}"
        fi
    done | grep -oE 'GLIBC_[0-9]+(\.[0-9]+)+' | cut -d_ -f2 | sort -Vu | tail -n 1
)
if [[ "$(printf '2.28\n%s\n' "${max_glibc}" | sort -V | tail -n 1)" != 2.28 ]]; then
    echo "AppImage requires GLIBC_${max_glibc}, newer than GLIBC_2.28" >&2
    exit 1
fi

for file in AppDir/usr/bin/* AppDir/usr/lib/**/*; do
    if rpath=$(patchelf --print-rpath "${file}" 2>/dev/null); then
        case "${rpath}" in
            *"${GCC_ROOT}"*|*/vcpkg*|*/build*|*/imhex*) echo "Build path remains in RPATH of ${file}: ${rpath}" >&2; exit 1 ;;
        esac
    fi
done
EOF

FROM scratch

# Copy build artifact
COPY --from=build /build/*.AppImage /build/*.AppImage.zsync ./
