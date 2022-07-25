### Compiling ImHex on Windows

On Windows, ImHex is built through msys2 / mingw. To install all dependencies, open a msys2 window and run the PKGCONFIG script in the [dist/msys2](dist/msys2) folder.
After all the dependencies are installed, run the following commands to build ImHex:

```sh
mkdir build
cd build
cmake -G "MinGW Makefiles"                \
  -DCMAKE_BUILD_TYPE=Release              \
  -DCMAKE_INSTALL_PREFIX="$PWD/install"   \
  -DCMAKE_C_COMPILER_LAUNCHER=ccache      \
  -DCMAKE_CXX_COMPILER_LAUNCHER=ccache    \
  -DCMAKE_C_FLAGS="-fuse-ld=lld"          \
  -DCMAKE_CXX_FLAGS="-fuse-ld=lld"        \
  -DCMAKE_OBJC_COMPILER_LAUNCHER=ccache   \
  -DCMAKE_OBJCXX_COMPILER_LAUNCHER=ccache \
  -DRUST_PATH="$USERPROFILE/.cargo/bin/"  \
  ..
mingw32-make -j install
```

ImHex will look for any extra resources either in various folders directly next to the executable or in `%localappdata%/imhex`
