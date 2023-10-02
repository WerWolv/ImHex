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
RUN printf 'set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")\nset(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")' >> /emsdk/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake

# Install dependencies with vcpkg
RUN /vcpkg/vcpkg install --triplet=wasm32-emscripten libmagic
RUN /vcpkg/vcpkg install --triplet=wasm32-emscripten freetype
RUN /vcpkg/vcpkg install --triplet=wasm32-emscripten josuttis-jthread
RUN /vcpkg/vcpkg install --triplet=wasm32-emscripten mbedtls

# Build ImHex
ARG JOBS=4
ENV CCACHE_DIR /cache/ccache

RUN mkdir /build
WORKDIR /build
RUN --mount=type=cache,target=/cache \
    --mount=type=bind,source=.,target=/imhex \
ccache -zs && \
cmake /imhex                                                                                            \
    -DIMHEX_OFFLINE_BUILD=ON                                                                            \
    -DIMHEX_STRICT_WARNINGS=OFF                                                                         \
    -DNATIVE_CMAKE_C_COMPILER=gcc                                                                       \
    -DNATIVE_CMAKE_CXX_COMPILER=g++                                                                     \
    -DCMAKE_C_COMPILER_LAUNCHER=ccache                                                                  \
    -DCMAKE_CXX_COMPILER_LAUNCHER=ccache                                                                \
    -DCMAKE_TOOLCHAIN_FILE=/vcpkg/scripts/buildsystems/vcpkg.cmake                                      \
    -DVCPKG_CHAINLOAD_TOOLCHAIN_FILE=/emsdk/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake \
    -DCMAKE_BUILD_TYPE=Release && \
make -j $JOBS && \
cp /imhex/dist/web/source/index.html /build/index.html && \
wget "https://raw.githubusercontent.com/josephrocca/clip-image-sorter/main/enable-threads.js" -O enable-threads.js -P /build && \
ccache -s

FROM scratch
COPY --from=build /build/imhex.wasm /build/imhex.js /build/imhex.worker.js /build/index.html /build/enable-threads.js .
