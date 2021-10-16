# imgui-sys: Low level bindings

This crate contains the raw FFI bindings to the Dear ImGui C++
library, by using the [cimgui](https://github.com/cimgui/cimgui) (a C
API wrapper project for Dear ImGui), then creating Rust bindings using
[bindgen](https://github.com/rust-lang/rust-bindgen).

These low level, mostly `unsafe` bindings are then used by `imgui-rs`
which wraps them in a nice to use, mostly safe API. Therefore most
users should not need to interact with this crate directly.
