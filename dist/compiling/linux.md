### Compiling ImHex on Linux

On Linux, ImHex is built through regular GCC (or optionally Clang).

1. Clone the repo using `git clone https://github.com/WerWolv/ImHex`
2. Install the dependencies using one of the `dist/get_deps_*.sh` scripts. Choose the one that matches your distro.
3. Build ImHex itself using `dist/build-for-linux.sh`

All paths follow the XDG Base Directories standard, and can thus be modified
with the environment variables `XDG_CONFIG_HOME`, `XDG_CONFIG_DIRS`,
`XDG_DATA_HOME` and `XDG_DATA_DIRS`.
