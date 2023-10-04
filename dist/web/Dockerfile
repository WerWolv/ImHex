FROM emscripten/emsdk:latest as build

# Used to invalidate layer cache but not mount cache
# See https://github.com/moby/moby/issues/41715#issuecomment-733976493
ARG UNIQUEKEY 1

RUN apt update
RUN apt install -y git ccache autoconf automake libtool cmake pkg-config

# Install vcpkg
# Note: we are using my fork of the repository with a libmagic patch
RUN git clone https://github.com/iTrooz/vcpkg --branch libmagic /vcpkg
RUN /vcpkg/bootstrap-vcpkg.sh

# Patch vcpkg build instructions to add -pthread
RUN <<EOF
set -xe

echo '
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
' >> /emsdk/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake
EOF

ENV VCPKG_DEFAULT_BINARY_CACHE /cache/vcpkg
RUN --mount=type=cache,target=/cache <<EOF
# Install dependencies with vcpkg
set -xe

mkdir -p $VCPKG_DEFAULT_BINARY_CACHE

/vcpkg/vcpkg install --triplet=wasm32-emscripten libmagic
/vcpkg/vcpkg install --triplet=wasm32-emscripten freetype
/vcpkg/vcpkg install --triplet=wasm32-emscripten josuttis-jthread
/vcpkg/vcpkg install --triplet=wasm32-emscripten mbedtls
EOF

# Build ImHex
ARG JOBS=4
ENV CCACHE_DIR /cache/ccache

RUN mkdir /build
WORKDIR /build
RUN --mount=type=cache,target=/cache \
    --mount=type=bind,source=.,target=/imhex <<EOF

set -xe
ccache -zs

cmake /imhex                                                                                            \
    -DIMHEX_OFFLINE_BUILD=ON                                                                            \
    -DIMHEX_STATIC_LINK_PLUGINS=ON                                                                      \
    -DNATIVE_CMAKE_C_COMPILER=gcc                                                                       \
    -DNATIVE_CMAKE_CXX_COMPILER=g++                                                                     \
    -DCMAKE_C_COMPILER_LAUNCHER=ccache                                                                  \
    -DCMAKE_CXX_COMPILER_LAUNCHER=ccache                                                                \
    -DCMAKE_TOOLCHAIN_FILE=/vcpkg/scripts/buildsystems/vcpkg.cmake                                      \
    -DVCPKG_CHAINLOAD_TOOLCHAIN_FILE=/emsdk/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake \
    -DCMAKE_BUILD_TYPE=Release

make -j $JOBS

cp /imhex/dist/web/source/* /build
ccache -s
EOF

FROM scratch
COPY --from=build [             \
    # ImHex                     \
    "/build/imhex.wasm",        \
    "/build/imhex.js",          \
    "/build/imhex.worker.js",   \
                                \
    # Static files              \
    "/build/index.html",        \
    "/build/style.css",         \
    "/build/wasm-config.js",    \
    "/build/enable-threads.js", \
    "/build/favicon.ico",       \
                                \
    # Destination               \
    "./"                        \
]