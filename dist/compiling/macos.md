### Compiling ImHex on macOS

On macOS, ImHex is built through regular GCC and LLVM clang.

1. Clone the repo using `git clone https://github.com/WerWolv/ImHex --recurse-submodules`
2. Install all the dependencies using `brew bundle --file dist/macOS/Brewfile`
3. Build ImHex itself using the following commands:
```sh
cd ImHex
mkdir -p build
cd build
CC=$(brew --prefix llvm)/bin/clang        \
CXX=$(brew --prefix llvm)/bin/clang++     \
OBJC=$(brew --prefix llvm)/bin/clang      \
OBJCXX=$(brew --prefix llvm)/bin/clang++  \
cmake -G "Ninja"                          \
  -DCMAKE_BUILD_TYPE=Release              \
  -DCMAKE_INSTALL_PREFIX="./install"      \
  -DIMHEX_GENERATE_PACKAGE=ON             \
  -DIMHEX_SYSTEM_LIBRARY_PATH="$(brew --prefix llvm)/lib;$(brew --prefix llvm)/lib/unwind;$(brew --prefix llvm)/lib/c++;$(brew --prefix)/lib" \
  -DIMHEX_RESIGN_BUNDLE=ON             \
  ..
ninja install
```

If your MacOS installation doesn't have graphic acceleration, you can check the [MacOS NoGPU guide](./macos_nogpu.md)