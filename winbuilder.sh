#! THIS SCRIPT MUST BE RUN IN A FULL MINGW64 ENVIRONMENT!
#! IT WON'T WORK OTHERWISE!
#! You should also probably make sure that everything is up-to-date (pacman -Syu).

#* Also, the script doesn't check anything so if it fail you may have to delete the build folder and start over.

#* This script reproduct the installation process for windows of the github action workflow file.
#* You can check if it correspond on the github repository at:
#* https://github.com/WerWolv/ImHex/actions
#* Find the right commit; click on the "..." button and select "View workflow file".
#* You normally don't need a shebang on windows so I didn't add it.

# install and/or update packages
./dist/get_deps_msys2.sh;

# Create build folder
mkdir build
cd build

# Get path to mingw python library
PYTHON_LIB_NAME=$(pkg-config --libs-only-l python3 | sed 's/^-l//' | tr -d " ")
PYTHON_LIB_PATH=$(cygpath -m $(which lib${PYTHON_LIB_NAME}.dll))

# Get path to magic db
MAGICDB_PATH=$(cygpath -m $(file --version | grep -oP "(?<=magic file from ).+"))

echo Python_LIBRARY: $PYTHON_LIB_PATH
echo MagicDB Path: $MAGICDB_PATH

# Generate the Makefile
cmake -G "MinGW Makefiles" \
  -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
  -DCMAKE_INSTALL_PREFIX="$PWD/install" \
  -DCREATE_PACKAGE=ON \
  -DPython_LIBRARY="$PYTHON_LIB_PATH" \
  -DEXTRA_MAGICDBS="$MAGICDB_PATH" \
  ..

# MAKE. The option describes the number of threads used so it can be faster ("-j" is unlimited).
mingw32-make -j4

#* I don't really know what this does so I put it in comment since it already work at this point.
#* mingw32-make install
# And that too since everyone won't make a package.
# cpack

cp $PYTHON_LIB_PATH ./
# cp $(which libwinpthread-1.dll) ./
# I don't know if winpthread is still included in the project but I still put it in comment just in case.
