#!/bin/bash

# Set the TAG environment variable to move to a versioned name while extracting

# Make sure we're in the same directory as this script
pushd $(dirname "$(realpath "$0")")

SUFFIX=""
[ -n "$TAG" ] && SUFFIX=":$TAG"

# Remove old containers
docker rm imhex 2>&1 > /dev/null

docker run -d --name imhex imhex-appimage-build${SUFFIX} sleep 30 &
sleep 5
docker cp imhex:/ImHex-x86_64.AppImage .

# Move to tagged name if $TAG set
if [ -n "$TAG" ]; then
    mv ImHex-x86_64.AppImage ImHex-${TAG}-x86_64.AppImage
    echo -e "\nThe created AppImage can be found here:\n  $(pwd)/ImHex-${TAG}-x86_64.AppImage\n\n"
else
    echo -e "\nThe created AppImage can be found here:\n  $(pwd)/ImHex-x86_64.AppImage\n\n"
fi

popd
