#!/bin/bash
set -e # Exit on error
set -o pipefail # Bash specific

usage() {
    echo "Tool to package an ImHex build into an AppImage"
    echo
    echo "Usage:"
    echo "$0 <build dir>"
    echo
    exit
}

MYDIR=$(dirname "$(realpath "$0")")

# Check is a build dir has been specified and it's a dir
[ -z "$1" ] && usage
[ -d "$1" ] || usage

set -u # Throw errors when unset variables are used

BUILDDIR=$1

# Prepare for AppImage
## Setup directory structure
mkdir -p ${BUILDDIR}/ImHex.AppDir/usr/{bin,lib} ${BUILDDIR}/ImHex.AppDir/usr/share/imhex/plugins

## Add ImHex files to structure
cp ${BUILDDIR}/imhex ${BUILDDIR}/ImHex.AppDir/usr/bin
cp ${BUILDDIR}/plugins/builtin/builtin.hexplug ${BUILDDIR}/ImHex.AppDir/usr/share/imhex/plugins
cp ${MYDIR}/{AppRun,ImHex.desktop,imhex.png} ${BUILDDIR}/ImHex.AppDir/
chmod a+x ${BUILDDIR}/ImHex.AppDir/AppRun

## Add all dependencies
ldd ${BUILDDIR}/imhex | awk '/ => /{print $3}' | xargs -I '{}' cp '{}' ${BUILDDIR}/ImHex.AppDir/usr/lib

# Package it up as described here:
# https://github.com/AppImage/AppImageKit#appimagetool-usage
# under 'If you want to generate an AppImage manually'
# `runtime` is taken from https://github.com/AppImage/AppImageKit/releases
# This builds a v2 AppImage according to
# https://github.com/AppImage/AppImageSpec/blob/master/draft.md#type-2-image-format
mksquashfs ${BUILDDIR}/ImHex.AppDir ${BUILDDIR}/ImHex.squashfs -root-owned -noappend
cat ${MYDIR}/runtime-x86_64 > ${BUILDDIR}/ImHex-x86_64.AppImage
cat ${BUILDDIR}/ImHex.squashfs >> ${BUILDDIR}/ImHex-x86_64.AppImage
chmod a+x ${BUILDDIR}/ImHex-x86_64.AppImage
