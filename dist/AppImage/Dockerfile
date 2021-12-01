FROM debian:bullseye-slim
LABEL maintainer Example <example@example.com>

ARG TAG=master

USER root

# Bring packages up to date
RUN apt-get update \
    && apt-get upgrade -y \
    && apt-get autoremove -y \
    && apt-get install -y \
        git \
        cmake \
        curl \
        libfuse2 \
        file

# Fetch source and dependencies
RUN mkdir -p /source \
    && cd /source \
    && git clone https://github.com/WerWolv/ImHex.git \
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
RUN mkdir -p /source/ImHex.AppDir/usr/bin \
    && mkdir -p /source/ImHex.AppDir/usr/lib \
    && mkdir -p /source/ImHex.AppDir/usr/share/imhex/plugins \
    && cp /source/ImHex/build/imhex /source/ImHex.AppDir/usr/bin/imhex \
    && cp /source/ImHex/build/plugins/builtin/builtin.hexplug /source/ImHex.AppDir/usr/share/imhex/plugins

COPY AppRun ImHex.desktop imhex.png /source/ImHex.AppDir/
#RUN inkscape -z -o /source/ImHex.AppDir/imhex.png -w 128 -h 128 /source/ImHex/res/icon.svg

# Gather the needed libraries
RUN chmod a+x /source/ImHex.AppDir/AppRun \
    && ldd /source/ImHex/build/imhex | awk '/ => /{print $3}' | xargs -I '{}' cp '{}' /source/ImHex.AppDir/usr/lib

# Package the prepared AppDir
RUN cd /source \
    && curl -L https://github.com/AppImage/AppImageKit/releases/download/13/appimagetool-x86_64.AppImage -o appimagetool-x86_64.AppImage \
    && chmod a+x appimagetool-x86_64.AppImage

# All that remains is the following. However, due to docker limitations with fuse
# this cannot be done in a build container.
#
# /source/appimagetool-x86_64.AppImage ImHex.AppDir

ENTRYPOINT sh -c '/source/appimagetool-x86_64.AppImage /source/ImHex.AppDir; sleep 60'
