#!/bin/bash

# Set the TAG environment variable to build a specific tag

# Make sure we're in the same direcotry as this script
pushd $(dirname "$(realpath "$0")")

if [ -z "$TAG" ]; then
    docker build -t imhex-appimage-build .
else
    docker build --build-arg=TAG=$TAG -t imhex-appimage-build-$TAG .
fi

popd
