# syntax=docker/dockerfile:1

FROM ubuntu:24.04 AS build

# Building this image downloads Visual Studio Build Tools and accepts Microsoft's
# license: https://go.microsoft.com/fwlink/?LinkId=2086102
ARG MSVC_WINE_VERSION=514f8ea34842cd6d831804d0e9658d3a32870ae1
ARG MSVC_VERSION=17.14
ARG POWERSHELL_VERSION=7.4.6
ARG TARGET_ARCHITECTURE=x64

RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        autoconf \
        autoconf-archive \
        automake \
        ca-certificates \
        ccache \
        cmake \
        curl \
        g++ \
        gcc \
        git \
        libgssapi-krb5-2 \
        libicu74 \
        libssl3 \
        libtool \
        libunwind8 \
        make \
        msitools \
        nasm \
        ninja-build \
        patch \
        pkg-config \
        procps \
        python3 \
        unzip \
        winbind \
        wine \
        wine64 \
        zip && \
    rm -rf /var/lib/apt/lists/*

# Several Windows vcpkg ports use a native PowerShell process to copy the
# dependencies of build tools into their packages.
RUN <<EOF
set -xe

case "$(uname -m)" in
    x86_64)
        POWERSHELL_ARCHITECTURE=x64
        POWERSHELL_SHA256=6f6015203c47806c5cc444c19d8ed019695e610fbd948154264bf9ca8e157561
        ;;
    aarch64)
        POWERSHELL_ARCHITECTURE=arm64
        POWERSHELL_SHA256=c0159b03e85f44ae1e7697818a011558da6c813d0aae848bf5ac13bf435d8624
        ;;
    *)
        echo "Unsupported Linux build architecture: $(uname -m)" >&2
        exit 1
        ;;
esac

curl -L \
    https://github.com/PowerShell/PowerShell/releases/download/v${POWERSHELL_VERSION}/powershell-${POWERSHELL_VERSION}-linux-${POWERSHELL_ARCHITECTURE}.tar.gz \
    -o /tmp/powershell.tar.gz
echo "${POWERSHELL_SHA256}  /tmp/powershell.tar.gz" | sha256sum --check -

mkdir -p /opt/microsoft/powershell/7
tar -xzf /tmp/powershell.tar.gz -C /opt/microsoft/powershell/7
chmod +x /opt/microsoft/powershell/7/pwsh
ln -s /opt/microsoft/powershell/7/pwsh /usr/local/bin/pwsh
rm /tmp/powershell.tar.gz

pwsh -NoLogo -NoProfile -Command '$PSVersionTable.PSVersion.ToString()'
EOF

# Initialize Wine and wait for its server to stop before committing the layer.
RUN WINE="$(command -v wine64 || command -v wine)" && \
    "$WINE" wineboot --init && \
    while pgrep wineserver > /dev/null; do sleep 1; done

# Download only the compiler and SDK for the requested target. The MSVC files
# are not redistributable and therefore intentionally stay in the build stage.
RUN --mount=type=cache,target=/cache <<EOF
set -xe

case "${TARGET_ARCHITECTURE}" in
    x64|arm64) ;;
    *) echo "TARGET_ARCHITECTURE must be x64 or arm64" >&2; exit 1 ;;
esac
case "$(uname -m)" in
    x86_64) HOST_ARCHITECTURE=x64 ;;
    aarch64) HOST_ARCHITECTURE=arm64 ;;
    *) echo "Unsupported Linux build architecture: $(uname -m)" >&2; exit 1 ;;
esac
if [ "${TARGET_ARCHITECTURE}" != "${HOST_ARCHITECTURE}" ]; then
    echo "TARGET_ARCHITECTURE must match the Linux build architecture so Wine can run ImHex's code generators" >&2
    exit 1
fi

git init /opt/msvc-wine
git -C /opt/msvc-wine remote add origin https://github.com/mstorsjo/msvc-wine.git
git -C /opt/msvc-wine fetch --depth 1 origin ${MSVC_WINE_VERSION}
git -C /opt/msvc-wine checkout --detach FETCH_HEAD

/opt/msvc-wine/vsdownload.py \
    --accept-license \
    --major 17 \
    --msvc-version ${MSVC_VERSION} \
    --architecture host ${TARGET_ARCHITECTURE} \
    --with-default=no \
    --with-msvc=yes \
    --with-sdk=yes \
    --cache /cache/msvc-wine \
    --dest /opt/msvc
/opt/msvc-wine/install.sh /opt/msvc
rm -rf /opt/msvc-wine
EOF

ARG VCPKG_VERSION=66c0373dc7fca549e5803087b9487edfe3aca0a1
RUN git init /vcpkg && \
    git -C /vcpkg remote add origin https://github.com/microsoft/vcpkg.git && \
    git -C /vcpkg fetch --depth 1 origin ${VCPKG_VERSION} && \
    git -C /vcpkg checkout --detach FETCH_HEAD && \
    sed -i 's/if(CMAKE_HOST_WIN32 AND VCPKG_DETECTED_CMAKE_C_COMPILER MATCHES "cl.exe")/if(VCPKG_TARGET_IS_WINDOWS)/' /vcpkg/scripts/cmake/vcpkg_configure_make.cmake && \
    sed -i '/VCPKG_DETECTED_CMAKE_LINKER MATCHES/ s/if(VCPKG_TARGET_IS_WINDOWS AND/if(VCPKG_TARGET_IS_WINDOWS AND NOT arg_USE_WRAPPERS AND/' /vcpkg/scripts/cmake/vcpkg_configure_make.cmake && \
    sed -i 's/set(linker_flag_escape "-Xlinker ")/set(linker_flag_escape "")/' /vcpkg/scripts/cmake/vcpkg_configure_make.cmake && \
    sed -i 's/if(arg_USE_WRAPPERS)/if(CMAKE_HOST_WIN32 AND arg_USE_WRAPPERS)/' /vcpkg/scripts/cmake/vcpkg_configure_make.cmake && \
    sed -i 's/MultiThreaded\$<\$<CONFIG:Debug>:Debug>/MultiThreaded/' /vcpkg/scripts/toolchains/windows.cmake && \
    sed -i 's/\${VCPKG_CRT_LINK_FLAG_PREFIX}d /\${VCPKG_CRT_LINK_FLAG_PREFIX} /g' /vcpkg/scripts/toolchains/windows.cmake && \
    grep -Fq 'if(VCPKG_TARGET_IS_WINDOWS) #only applies to windows (clang-)cl and lib' /vcpkg/scripts/cmake/vcpkg_configure_make.cmake && \
    /vcpkg/bootstrap-vcpkg.sh -disableMetrics

ENV PATH="/opt/msvc/bin/${TARGET_ARCHITECTURE}:${PATH}"
ENV VCPKG_DEFAULT_BINARY_CACHE=/cache/vcpkg
ENV VCPKG_TARGET_TRIPLET=${TARGET_ARCHITECTURE}-windows-msvc
ENV WINEDEBUG=-all

# Source the target environment before running generated Windows tools so Wine
# can also locate the MSVC runtime DLLs.
RUN <<EOF
cat > /usr/local/bin/msvc-wine-emulator <<SCRIPT
#!/usr/bin/env bash
. /opt/msvc/bin/${TARGET_ARCHITECTURE}/msvcenv.sh
exec /opt/msvc/bin/${TARGET_ARCHITECTURE}/wine-msvc.sh "\$@"
SCRIPT

cat > /usr/local/bin/cygpath <<'SCRIPT'
#!/bin/sh
for argument do
    case "${argument}" in
        -*) ;;
        *) path="${argument}" ;;
    esac
done
test -n "${path:-}" || exit 1
winepath -w "${path}" | tr '\\' '/'
SCRIPT

cat > /usr/local/bin/file.exe <<'SCRIPT'
#!/bin/sh
if [ "${1:-}" = "--version" ]; then
    file "$@" | sed 's/^file-//'
    exit $?
fi
exec file "$@"
SCRIPT

chmod +x /usr/local/bin/msvc-wine-emulator
chmod +x /usr/local/bin/cygpath
chmod +x /usr/local/bin/file.exe

for directory in /opt/msvc/VC/Redist/MSVC/*/${TARGET_ARCHITECTURE}/Microsoft.VC*.CRT; do
    if [ -d "${directory}" ]; then
        CRT_DIRECTORY="${directory}"
    fi
done
test -n "${CRT_DIRECTORY:-}"
cp "${CRT_DIRECTORY}"/*.dll /root/.wine/drive_c/windows/system32/
EOF

# Install the repository's Windows dependencies.
RUN mkdir /build
RUN --mount=type=cache,target=/cache \
    --mount=type=bind,source=.,target=/imhex <<EOF
set -xe
mkdir -p ${VCPKG_DEFAULT_BINARY_CACHE}
mkdir /manifest
# OpenSSL's Windows port requires a Windows-hosted Perl/NMake pipeline. Use
# libssh2's native Windows crypto backend in this Linux-hosted build instead.
sed 's/"libssh2"/{"name":"libssh2","default-features":false,"features":["zlib"]}/' \
    /imhex/dist/vcpkg.json > /manifest/vcpkg.json
wineserver -p
/vcpkg/vcpkg install \
    --triplet=${VCPKG_TARGET_TRIPLET} \
    --overlay-triplets=/imhex/dist/msvc/vcpkg-triplets \
    --x-manifest-root=/manifest \
    --x-install-root=/build/vcpkg_installed
wineserver -k
EOF

# Keep the repository bind mount read-only and persist its contents for the
# configure and build stages.
RUN --mount=type=bind,source=.,target=/source <<EOF
set -xe
mkdir /imhex
cp -a /source/. /imhex/
EOF

ARG BUILD_TYPE=RelWithDebInfo
ARG JOBS=4
ARG GIT_COMMIT_HASH=""
ARG GIT_BRANCH=""
ENV CCACHE_DIR=/cache/ccache

WORKDIR /build

# Build ImHex with MSVC through Wine. ImHex's code generators are Windows
# executables too, so CMake runs them through Wine while cross-compiling.
RUN --mount=type=cache,target=/cache <<EOF
set -xe
ccache -z
wineserver -p

cmake -S /imhex -B /build -G Ninja \
    -DCMAKE_SYSTEM_NAME=Windows \
    -DCMAKE_C_COMPILER=/opt/msvc/bin/${TARGET_ARCHITECTURE}/cl.exe \
    -DCMAKE_CXX_COMPILER=/opt/msvc/bin/${TARGET_ARCHITECTURE}/cl.exe \
    -DCMAKE_CROSSCOMPILING_EMULATOR=/usr/local/bin/msvc-wine-emulator \
    -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL \
    -DCMAKE_C_FLAGS_INIT=/wd4005 \
    -DCMAKE_CXX_FLAGS_INIT=/wd4005 \
    -DCMAKE_C_COMPILER_LAUNCHER=ccache \
    -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
    -DCMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded \
    -DCMAKE_C_USE_RESPONSE_FILE_FOR_OBJECTS=ON \
    -DCMAKE_CXX_USE_RESPONSE_FILE_FOR_OBJECTS=ON \
    -DCMAKE_JOB_POOLS=link_pool=1 \
    -DCMAKE_JOB_POOL_LINK=link_pool \
    -DCMAKE_TOOLCHAIN_FILE=/vcpkg/scripts/buildsystems/vcpkg.cmake \
    -DVCPKG_TARGET_TRIPLET=${VCPKG_TARGET_TRIPLET} \
    -DVCPKG_OVERLAY_TRIPLETS=/imhex/dist/msvc/vcpkg-triplets \
    -DVCPKG_INSTALLED_DIR=/build/vcpkg_installed \
    -DVCPKG_MANIFEST_DIR=/imhex/dist \
    -DVCPKG_MANIFEST_INSTALL=OFF \
    -DVCPKG_APPLOCAL_DEPS=OFF \
    -DNATIVE_CMAKE_C_COMPILER=/opt/msvc/bin/${TARGET_ARCHITECTURE}/cl.exe \
    -DNATIVE_CMAKE_CXX_COMPILER=/opt/msvc/bin/${TARGET_ARCHITECTURE}/cl.exe \
    -DLIBROMFS_GENERATOR_EMULATOR=/usr/local/bin/msvc-wine-emulator \
    -DLIBROMFS_COMPRESS_RESOURCES=OFF \
    -DCMAKE_INSTALL_PREFIX=/build/install \
    -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
    -DIMHEX_GENERATE_PACKAGE=ON \
    -DIMHEX_RESOLVE_RUNTIME_DEPS=OFF \
    -DIMHEX_PATTERNS_PULL_MASTER=ON \
    -DIMHEX_COMMIT_HASH_LONG=${GIT_COMMIT_HASH} \
    -DIMHEX_COMMIT_BRANCH=${GIT_BRANCH}

cmake --build /build --parallel ${JOBS}

wineserver -k
ccache -s
EOF

RUN <<EOF
set -xe
cmake --install /build

for directory in /opt/msvc/VC/Redist/MSVC/*/${TARGET_ARCHITECTURE}/Microsoft.VC*.CRT; do
    if [ -d "${directory}" ]; then
        CRT_DIRECTORY="${directory}"
    fi
done
test -n "${CRT_DIRECTORY:-}"
cp "${CRT_DIRECTORY}"/*.dll /build/install/
EOF

FROM scratch
COPY --from=build /build/install/ /
