# This base image is also known as "crosscompile". See arm64.crosscompile.Dockerfile
FROM ghcr.io/itrooz/macos-crosscompile:clang17-nosdk as build

ENV MACOSX_DEPLOYMENT_TARGET 12.1

# -- DOWNLOADING STUFF

## Install make
RUN --mount=type=cache,target=/var/lib/apt/lists/ apt update && apt install -y make

## fix environment
### add install_name_tool for cmake command that won't have the right env set (see PostprocessBundle.cmake function postprocess_bundle())
RUN cp /osxcross/build/cctools-port/cctools/misc/install_name_tool /usr/bin/install_name_tool
### a cmake thing wants 'otool' and not '' apparently
RUN cp /osxcross/target/bin/aarch64-apple-darwin23-otool /usr/bin/otool

## Clone glfw
RUN <<EOF
set -xe
if [ "$CUSTOM_GLFW" ]; then
    git clone https://github.com/glfw/glfw /mnt/glfw
fi
EOF

## Assume the SDK has been removed from the image, and copy it again
COPY SDK /osxcross/target/SDK

## Download libmagic
### Clone libmagic
RUN git clone https://github.com/file/file /mnt/file
### Download libmagic dependencies
RUN --mount=type=cache,target=/var/lib/apt/lists/ apt install -y libtool autoconf

# -- DOWNLOADING + BUILDING STUFF
## Install libcurl dep
RUN vcpkg install --triplet=arm-osx-mytriplet curl
## Install mbedtls dep
RUN vcpkg install --triplet=arm-osx-mytriplet mbedtls
## Install freetype dep
RUN vcpkg install --triplet=arm-osx-mytriplet freetype
## Install jthread external library
RUN vcpkg install --triplet=arm-osx-mytriplet josuttis-jthread

## Install glfw3 dep
ARG CUSTOM_GLFW
RUN <<EOF
set -xe
if [ "$CUSTOM_GLFW" ]; then
    echo "Flag confirmation: using custom GLFW for software rendering"
else
    vcpkg install --triplet=arm-osx-mytriplet glfw3
fi
EOF

# -- BUILDING STUFF
ARG JOBS 1
ARG BUILD_TYPE Debug

## Build libmagic
RUN --mount=type=cache,target=/cache <<EOF
    ccache -zs
    set -xe
    
    cd /mnt/file
    autoreconf -is

    # when cross-compiling, libmagic needs to have an the same version installed in the system.
    # So we install it normally first
    ./configure --prefix /usr
    make -j $JOBS install

    # Now, we cross-compile it and install it in the libraries folder
    CC=/osxcross/target/bin/aarch64-apple-darwin23-clang CXX=/osxcross/target/bin/aarch64-apple-darwin23-clang++ ./configure --prefix /vcpkg/installed/arm-osx-mytriplet --host $OSXCROSS_HOST
    make -j $JOBS
    make install

    ccache -s
    
EOF

## Patch glfw
COPY --from=imhex /dist/macOS/0001-glfw-SW.patch /tmp
RUN <<EOF
set -xe
if [ "$CUSTOM_GLFW" ]; then
    cd /mnt/glfw
    git apply /tmp/0001-glfw-SW.patch
fi
EOF

## Build glfw
RUN --mount=type=cache,target=/cache <<EOF
set -xe
if [ "$CUSTOM_GLFW" ]; then
    ccache -zs

    cd /mnt/glfw
    mkdir build
    cd build
    CC=o64-gcc CXX=o64-g++ cmake -G "Ninja"             \
          -DCMAKE_BUILD_TYPE=$BUILD_TYPE                \
          -DBUILD_SHARED_LIBS=ON                        \
          -DCMAKE_C_COMPILER_LAUNCHER=ccache            \
          -DCMAKE_CXX_COMPILER_LAUNCHER=ccache          \
          -DCMAKE_OBJC_COMPILER_LAUNCHER=ccache         \
          -DCMAKE_OBJCXX_COMPILER_LAUNCHER=ccache       \
          -DCMAKE_INSTALL_PREFIX=/vcpkg/installed/arm-osx-mytriplet \
          -DVCPKG_TARGET_TRIPLET=arm-osx-mytriplet -DCMAKE_TOOLCHAIN_FILE=/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_CHAINLOAD_TOOLCHAIN_FILE=/osxcross/target/toolchain.cmake -DCMAKE_OSX_SYSROOT=/osxcross/target/SDK/MacOSX14.0.sdk -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 \
        ..
    ninja -j $JOBS install

    ccache -s
fi
EOF

# Build ImHex
## Copy ImHex
COPY --from=imhex / /mnt/ImHex
## Patch ImHex with hacks
# COPY toolchain.cmake.2 /osxcross/target/toolchain.cmake
# Configure ImHex build
RUN --mount=type=cache,target=/cache --mount=type=cache,target=/mnt/ImHex/build/_deps \
    cd /mnt/ImHex && \
    # compilers
    CC=o64-clang CXX=o64-clang++ OBJC=/osxcross/target/bin/aarch64-apple-darwin23-clang OBJCXX=/osxcross/target/bin/aarch64-apple-darwin23-clang++ \
        cmake -G "Ninja" \
        `# ccache flags` \
        -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache -DCMAKE_OBJC_COMPILER_LAUNCHER=ccache -DCMAKE_OBJCXX_COMPILER_LAUNCHER=ccache \
        `# MacOS cross-compiling flags` \
        -DVCPKG_TARGET_TRIPLET=arm-osx-mytriplet -DCMAKE_TOOLCHAIN_FILE=/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_CHAINLOAD_TOOLCHAIN_FILE=/osxcross/target/toolchain.cmake -DCMAKE_OSX_SYSROOT=/osxcross/target/SDK/MacOSX14.0.sdk -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 \
        `# Override compilers for code generators` \
        -DNATIVE_CMAKE_C_COMPILER=/usr/bin/clang -DNATIVE_CMAKE_CXX_COMPILER=/usr/bin/clang++ \
        `# Normal ImHex flags` \
        -DIMHEX_GENERATE_PACKAGE=ON -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
        `# other flags` \
        -DIMHEX_STRICT_WARNINGS=OFF \
        -B build
## Build ImHex
RUN --mount=type=cache,target=/cache --mount=type=cache,target=/mnt/ImHex/build/_deps <<EOF
    ccache -zs
    set -xe

    cd /mnt/ImHex
    cmake --build build --parallel $JOBS

    ccache -s
EOF


FROM scratch
COPY --from=build /mnt/ImHex/build/imhex.app .
