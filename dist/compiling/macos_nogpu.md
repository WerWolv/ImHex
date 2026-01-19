### Compiling and running ImHex on macOS without a GPU

In order to run ImHex on a macOS installation without a GPU, you need a custom build of GLFW. You can build it this way:

Note: only tested on macOS x86

1. `git clone --depth 1 https://github.com/glfw/glfw`
2. `git apply {IMHEX_DIR}/dist/macOS/0001-glfw-SW.patch` (file is [here](../macOS/0001-glfw-SW.patch) in the ImHex repository. [Source](https://github.com/glfw/glfw/issues/2080).)
3. `cmake -G "Ninja" -DBUILD_SHARED_LIBS=ON ..`
4. `ninja install`, or `ninja` and figure out how to make ImHex detect the shared library
