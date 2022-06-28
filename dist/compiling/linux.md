### Compiling ImHex on Linux

Dependency installation scripts are available for many common Linux distributions in the [/dist](dist) folder.
After all the dependencies are installed, run the following commands to build ImHex:

```sh
mkdir -p build
cd build
CC=gcc-12 CXX=g++-12 cmake                    \
    -DCMAKE_BUILD_TYPE=Release                \
    -DCMAKE_INSTALL_PREFIX="/usr" 	          \
    -DCMAKE_C_COMPILER_LAUNCHER=ccache        \
    -DCMAKE_CXX_COMPILER_LAUNCHER=ccache      \
    -DCMAKE_C_FLAGS="-fuse-ld=lld"            \
    -DCMAKE_CXX_FLAGS="-fuse-ld=lld"          \
    -DCMAKE_OBJC_COMPILER_LAUNCHER=ccache     \
    -DCMAKE_OBJCXX_COMPILER_LAUNCHER=ccache   \
    -DRUST_PATH="$HOME/.cargo/bin/"           \
    ..
make -j 4 install
```

---

Put the ImHex executable into the `/usr/bin` folder.
Put libimhex.so into the `/usr/lib` folder.
Configuration files go to `/usr/etc/imhex` or `~/.config/imhex`.
All other files belong in `/usr/share/imhex` or `~/.local/share/imhex`:

```
Patterns: /usr/share/imhex/patterns
Pattern Includes: /usr/share/imhex/includes
Magic files: /usr/share/imhex/magic
Python: /usr/share/imhex/lib/pythonX.X
Plugins: /usr/share/imhex/plugins
Configuration: /etc/xdg/imhex/config
```

All paths follow the XDG Base Directories standard, and can thus be modified
with the environment variables `XDG_CONFIG_HOME`, `XDG_CONFIG_DIRS`,
`XDG_DATA_HOME` and `XDG_DATA_DIRS`.