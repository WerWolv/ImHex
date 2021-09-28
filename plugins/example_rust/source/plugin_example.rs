extern crate libimhex;
use libimhex::macros::plugin_setup;

#[plugin_setup("Hello Rust", "WerWolv", "Test Description")]
fn init() {
    println!("Hello from Rust!");
}