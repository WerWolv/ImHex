fn main() {
    println!("cargo:rustc-link-lib=dylib=imhex");
    println!(
        "cargo:rustc-link-search=native={}",
        env!("LIBIMHEX_OUTPUT_DIRECTORY")
    );
}

