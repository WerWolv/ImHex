#!/bin/bash

# Set the TAG environment variable to build a specific tag
# Set the REPO environment variable to point at a different git repository

# Make sure we're in the same directory as this script
pushd $(dirname "$(realpath "$0")")

BUILDARG=""
SUFFIX=""
[ -n "${TAG}" ] && BUILDARG="${BUILDARG} --build-arg=TAG=${TAG}" && SUFFIX=":${TAG}"
[ -n "${REPO}" ] && BUILDARG="${BUILDARG} --build-arg=REPO=${REPO}"

mkdir -p ../../build/AppImage
docker build ${BUILDARG} -v "../../build/AppImage:/AppImage" -t imhex-appimage-build${SUFFIX} .

popd
