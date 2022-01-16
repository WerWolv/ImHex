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
APPDIR=${BUILDDIR}/ImHex.AppDir
APPIMAGE=${BUILDDIR}/ImHex-x86_64.AppImage

# Prepare for AppImage
## Fetch the needed AppImage binaries
curl -L https://github.com/AppImage/AppImageKit/releases/download/13/AppRun-x86_64 -o ${MYDIR}/AppRun-x86_64
curl -L https://github.com/AppImage/AppImageKit/releases/download/13/runtime-x86_64 -o ${MYDIR}/runtime-x86_64

## Setup directory structure
mkdir -p ${BUILDDIR}/ImHex.AppDir/usr/{bin,lib} ${BUILDDIR}/ImHex.AppDir/usr/share/imhex/plugins

## Add ImHex files to structure
cp ${BUILDDIR}/imhex ${APPDIR}/usr/bin
cp ${BUILDDIR}/plugins/builtin/builtin.hexplug ${APPDIR}/usr/share/imhex/plugins
cp ${MYDIR}/{AppRun-x86_64,ImHex.desktop,imhex.png} ${APPDIR}/
mv ${BUILDDIR}/ImHex.AppDir/AppRun-x86_64 ${APPDIR}/AppRun
chmod a+x ${BUILDDIR}/ImHex.AppDir/AppRun

## Add all dependencies
ldd ${BUILDDIR}/imhex | awk '/ => /{print $3}' | awk '!/(libc|libstdc++|libc++|libdl|libpthread|libselinux|ld-linux|libgdk)/' | xargs -I '{}' cp '{}' ${APPDIR}/usr/lib
ldd ${BUILDDIR}/plugins/builtin/builtin.hexplug | awk '/ => /{print $3}' | awk '!/(libc|libstdc++|libc++|libdl|libpthread|libselinux|ld-linux|libgdk)/' | xargs -I '{}' cp '{}' ${APPDIR}/usr/lib
ldd ${BUILDDIR}/lib/libimhex/libimhex.so | awk '/ => /{print $3}' | awk '!/(libc|libstdc++|libc++|libdl|libpthread|libselinux|ld-linux|libgdk)/' | xargs -I '{}' cp '{}' ${APPDIR}/usr/lib

# Package it up as described here:
# https://github.com/AppImage/AppImageKit#appimagetool-usage
# under 'If you want to generate an AppImage manually'
# This builds a v2 AppImage according to
# https://github.com/AppImage/AppImageSpec/blob/master/draft.md#type-2-image-format
mksquashfs ${APPDIR} ${BUILDDIR}/ImHex.squashfs -root-owned -noappend
cat ${MYDIR}/runtime-x86_64 > ${APPIMAGE}
cat ${BUILDDIR}/ImHex.squashfs >> ${APPIMAGE}
chmod a+x ${APPIMAGE}

if [ ! -f /.dockerenv ]; then
    echo -e "\nThe created AppImage can be found here:\n  ${APPIMAGE}\n\n"
fi
