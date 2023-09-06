### Compiling ImHex on Windows

On Windows, ImHex is built through [msys2 / mingw](https://www.msys2.org/)'s gcc.

1. Download and install msys2 from their [website](https://www.msys2.org/).
2. Open the `MSYS2 MinGW x64` shell
3. Clone the repo using `git clone https://github.com/WerWolv/ImHex --recurse-submodules`
4. Install all the dependencies using `./ImHex/dist/get_deps_msys2.sh`
5. Build ImHex itself using the following commands:
```sh
cd ImHex
mkdir build
cd build
cmake -G "Ninja"                          \
  -DCMAKE_BUILD_TYPE=Release              \
  -DCMAKE_INSTALL_PREFIX="$PWD/install"   \
  -DIMHEX_USE_DEFAULT_BUILD_SETTINGS=ON   \
  ..
ninja install
```

ImHex will look for any extra resources either in various folders directly next to the executable or in `%localappdata%/imhex`
