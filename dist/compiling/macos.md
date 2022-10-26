### Compiling ImHex on macOS

On macOS, ImHex is built through regular GCC and AppleClang.

1. Clone the repo using `git clone https://github.com/WerWolv/ImHex --recurse-submodules`
2. Install all the dependencies using `brew bundle --no-lock --file dist/Brewfile`
3. Build ImHex itself using the following commands:
```sh
cd ImHex
mkdir -p build
cd build
CC=$(brew --prefix gcc@12)/bin/gcc-12     \
CXX=$(brew --prefix gcc@12)/bin/g++-12    \
OBJC=$(brew --prefix llvm)/bin/clang      \
OBJCXX=$(brew --prefix llvm)/bin/clang++  \
PKG_CONFIG_PATH="$(brew --prefix openssl)/lib/pkgconfig":"$(brew --prefix)/lib/pkgconfig" \
MACOSX_DEPLOYMENT_TARGET="10.15"          \
cmake                                     \
  -DCMAKE_BUILD_TYPE=Release              \
  -DCREATE_BUNDLE=ON                      \
  -DCREATE_PACKAGE=ON                     \
  -DCMAKE_C_COMPILER_LAUNCHER=ccache      \
  -DCMAKE_CXX_COMPILER_LAUNCHER=ccache    \
  -DCMAKE_OBJC_COMPILER_LAUNCHER=ccache   \
  -DCMAKE_OBJCXX_COMPILER_LAUNCHER=ccache \
  ..
make -j4 package
```

If the build fails while trying to find the macOS libraries, make sure you have
Xcode installed with `xcode-select --install`. Homebrew will also help get the
most recent SDK installed and configured with `brew doctor`.
