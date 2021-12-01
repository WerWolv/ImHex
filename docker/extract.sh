#!/bin/bash

# Set the TAG environment variable to move to a versioned name while extracting

# Make sure we're in the same direcotry as this script
pushd $(dirname "$(realpath "$0")")

# Remove old containers
docker rm imhex 2>&1 > /dev/null

# AppImage uses FUSE, which makes --device /dev/fuse --cap-add SYS_ADMIN necessary here
# on Debian --security-opt apparmor:unconfined is also needed
if [ -z "$TAG" ]; then
    docker run -d --device /dev/fuse --cap-add SYS_ADMIN --security-opt apparmor:unconfined --name imhex imhex-appimage-build sh -c '/source/appimagetool-x86_64.AppImage /source/ImHex.AppDir; sleep 60'
else
    docker run -d --device /dev/fuse --cap-add SYS_ADMIN --security-opt apparmor:unconfined --name imhex imhex-appimage-build-$TAG sh -c '/source/appimagetool-x86_64.AppImage /source/ImHex.AppDir; sleep 60'
fi
sleep 10
docker cp imhex:/ImHex-x86_64.AppImage .

# Move to tagged name if $TAG set
if [ -n "$TAG" ]; then
    mv ImHex-x86_64.AppImage Imhex-${TAG}-x86_64.AppImage
fi

popd
