use autocxx::c_ulong;

use crate::Color;
use std::ops::Range;

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

    crate::ffi::hex::ImHexApi::Bookmarks::add(
        region.start,
        c_ulong::from(region.end.saturating_sub(region.start)),
        &cpp_name,
        &cpp_comment,
        color.into().unwrap_or(crate::Color::new(0, 0, 0, 0)).rgba(),
    );
}
