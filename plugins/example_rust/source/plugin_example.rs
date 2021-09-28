extern crate hex;

#[hex::plugin_setup("Hello Rust", "WerWolv", "Test Description")]
fn init() {
    println!("Hello from Rust!");
}