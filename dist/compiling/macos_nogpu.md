### Compiling and running ImHex on macOS without a GPU

In order to run ImHex on a macOS installation without a GPU, the vendored GLFW needs an additional patch:

Note: only tested on macOS x86

1. `cd {IMHEX_DIR}/lib/third_party/glfw`
2. `git apply ../../../dist/macOS/0001-glfw-SW.patch` (file is [here](../macOS/0001-glfw-SW.patch) in the ImHex repository. [Source](https://github.com/glfw/glfw/issues/2080).)
3. Configure and build ImHex normally.
