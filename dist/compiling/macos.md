### Compiling ImHex on macOS

On macOS, ImHex is built through regular GCC and LLVM clang.

1. Clone the repo using `git clone https://github.com/WerWolv/ImHex --recurse-submodules`
2. Install all the dependencies using `brew bundle --no-lock --file dist/Brewfile`
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
  ..
ninja install
```