use crate::Color;
use std::ops::Range;

#[cxx::bridge]
mod ffi {
    #[namespace = "hex::ImHexApi::Bookmarks"]
    extern "C++" {
        include!("hex/api/imhex_api.hpp");

        pub unsafe fn add(addr: u64, size: u64, name: &CxxString, comment: &CxxString, color: u32);
    }
}

/// Add a bookmark to a region of the current imhex view with an optionally provided color
///
/// ```rust,ignore
/// use hex::bookmarks;
///
/// bookmarks::add(0..0x10, "header", "this is the header of the file", None);
///
/// let red = Color::new(0xFF, 0, 0, 0xFF);
/// bookmarks::add(0x10..0x30, "table", "this is the table of the file", red);
/// ```
pub fn add(region: Range<u64>, name: &str, comment: &str, color: impl Into<Option<Color>>) {
    cxx::let_cxx_string!(cpp_name = name);
    cxx::let_cxx_string!(cpp_comment = comment);

    unsafe {
        ffi::add(
            region.start,
            region.end.saturating_sub(region.start),
            &cpp_name,
            &cpp_comment,
            color.into().unwrap_or(crate::Color::new(0, 0, 0, 0)).rgba(),
        );
    }
}
