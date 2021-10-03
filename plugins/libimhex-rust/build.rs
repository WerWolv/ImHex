fn main() {
    println!("cargo:rustc-link-lib=dylib=libimhex");
    println!("cargo:rustc-link-search=native={}", env!("LIBIMHEX_OUTPUT_DIRECTORY"));

    println!("cargo:rerun-if-changed=src/lib.rs");
    println!("cargo:rerun-if-changed=src/imhex_api.rs");

    cxx_build::bridge("src/imhex_api.rs")
        .include(format!("{}/include", env!("LIBIMHEX_SOURCE_DIRECTORY")))
        .flag("-std=gnu++20")
        .compile("libimhex-bridge");
}