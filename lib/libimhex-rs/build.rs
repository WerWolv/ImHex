fn main() {
    println!("cargo:rustc-link-lib=dylib=imhex");
    println!(
        "cargo:rustc-link-search=all={}",
        env!("LIBIMHEX_OUTPUT_DIRECTORY")
    );

    println!("cargo:rerun-if-changed=src/lib.rs");
    println!("cargo:rerun-if-changed=src/imhex_api.rs");

    let include = format!("-I{}/include", env!("LIBIMHEX_SOURCE_DIRECTORY"));

    let path = std::path::PathBuf::from("src");
    let mut build = autocxx_build::Builder::new("src/lib.rs", &[&path])
        .extra_clang_args(&[&include, "-x", "c++", "-std=gnu++20"])
        .auto_allowlist(true)
        .expect_build();

    build
        .include(format!("{}/include", env!("LIBIMHEX_SOURCE_DIRECTORY")))
        .flag_if_supported("-std=gnu++20")
        .flag_if_supported("-std=gnu++2a")
        .flag_if_supported("-fconcepts")
        .compiler(env!("CXX_COMPILER"))
        .compile("libimhex-bridge");
}
