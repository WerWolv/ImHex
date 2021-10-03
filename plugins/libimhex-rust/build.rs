fn main() {
    println!("cargo:rustc-link-lib=dylib=imhex");
    println!("cargo:rustc-link-search=all={}", env!("LIBIMHEX_OUTPUT_DIRECTORY"));

    println!("cargo:rerun-if-changed=src/lib.rs");
    println!("cargo:rerun-if-changed=src/imhex_api.rs");

    cxx_build::bridge("src/imhex_api.rs")
        .include(format!("{}/include", env!("LIBIMHEX_SOURCE_DIRECTORY")))
        .flag_if_supported("-std=gnu++20")
        .flag_if_supported("-std=gnu++2a")
        .flag_if_supported("-fconcepts")
        .compile("libimhex-bridge");
}