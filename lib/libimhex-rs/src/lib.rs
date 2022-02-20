use autocxx::prelude::*;

include_cpp! {
    #include "hex/api/imhex_api.hpp"

    safety!(unsafe)

    generate!("hex::ImHexApi::Common::closeImHex")
    generate!("hex::ImHexApi::Common::restartImHex")
}

//pub use crate::ffi::*;

/// API for working with imhex bookmarks/highlights
pub mod bookmarks;

/// API for working with imhex itself
pub mod imhex;

mod imhex_api;

pub use imgui;

#[doc(inline)]
pub use imhex_api::Color;

/// A macro for declaring the init function for your plugin
///
/// ## Example
///
/// ```
/// #[hex::plugin_setup(
///     /* Display name*/ "Example Rust",
///     /* Author */ "WerWolv",
///     /* Description */ "Example Rust plugin used as template for plugin devs"
/// )]
/// fn init() {
///     // plugin initialization logic here
/// }
/// ```
pub use imhex_macros::plugin_setup;
