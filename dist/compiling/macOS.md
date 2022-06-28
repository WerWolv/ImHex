### Compiling ImHex on macOS

To build ImHex on macOS, run the following commands:

```sh
brew bundle --no-lock --file dist/Brewfile
mkdir -p build
cd build
CC=$(brew --prefix gcc@12)/bin/gcc-12     \
CXX=$(brew --prefix gcc@12)/bin/g++-12    \
OBJC=$(brew --prefix llvm)/bin/clang      \
OBJCXX=$(brew --prefix llvm)/bin/clang++  \
PKG_CONFIG_PATH="$(brew --prefix openssl)/lib/pkgconfig":"$(brew --prefix)/lib/pkgconfig" \
MACOSX_DEPLOYMENT_TARGET="10.15"          \
cmake                                     \
  -DCMAKE_BUILD_TYPE=$BUILD_TYPE          \
  -DCREATE_BUNDLE=ON                      \
  -DCREATE_PACKAGE=ON                     \
  -DCMAKE_C_COMPILER_LAUNCHER=ccache      \
  -DCMAKE_CXX_COMPILER_LAUNCHER=ccache    \
  -DCMAKE_OBJC_COMPILER_LAUNCHER=ccache   \
  -DCMAKE_OBJCXX_COMPILER_LAUNCHER=ccache \
  ..
make -j4 package
```

Open the generated .dmg file and drag-n-drop the ImHex executable to the Applications folder

All other files belong in `~/Library/Application Support/imhex`:
```
Patterns: ~/Library/Application Support/imhex/patterns
Pattern Includes: ~/Library/Application Support/imhex/includes
Magic files: ~/Library/Application Support/imhex/magic
Python: ~/Library/Application Support/imhex/lib/pythonX.X
Plugins: ~/Library/Application Support/imhex/plugins
Configuration: ~/Library/Application Support/imhex/config
```

If the build fails while trying to find the macOS libraries, make sure you have
XCode installed with `xcode-select --install`. Homebrew will also help get the
most recent SDK installed and configured with `brew doctor`.