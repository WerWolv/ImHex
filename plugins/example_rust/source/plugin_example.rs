extern crate hex;

#[hex::plugin_setup("Hello Rust", "WerWolv", "Test Description")]
fn init() {
    println!("Hello from Rust!");

    hex::ImHexApi::Bookmarks::add(
        hex::Region {
            address: 0x00,
            size: 0x10,
        },
        "Test",
        "123",
        None,
    );

    println!("Rust ImGui Version: {}", hex::imgui::dear_imgui_version());
    println!("Rust ImGui Context: {:?}", hex::imgui::Context::current());
}

