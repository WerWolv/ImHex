FROM debian:bullseye-slim
LABEL maintainer Example <example@example.com>

ARG TAG=master
ARG REPO=https://github.com/WerWolv/ImHex.git

USER root

# Bring packages up to date
RUN apt-get update \
    && apt-get upgrade -y \
    && apt-get autoremove -y \
    && apt-get install -y \
        git \
        cmake \
        curl \
        squashfs-tools

# Fetch source and dependencies
RUN mkdir -p /source \
    && cd /source \
    && git clone $REPO \
    && cd ImHex \
    && git checkout $TAG \
    && git submodule update --init --recursive \
    && cd /source/ImHex/dist \
    && ./get_deps_debian.sh

ARG CXX=g++-10

# Build ImHex
RUN mkdir -p /source/ImHex/build \
    && cd /source/ImHex/build \
    && cmake --install-prefix /usr -DCMAKE_BUILD_TYPE=Release .. \
    && make -j

# Prepare for AppImage
RUN cd /source/ImHex/dist/AppImage \
    && ./package.sh /source/ImHex/build \
    && mv /source/ImHex/build/ImHex-x86_64.AppImage /AppImage/
