/// A highlight color for use with the bookmarks API
pub struct Color {
    pub a: u8,
    pub g: u8,
    pub b: u8,
    pub r: u8,
}

impl Color {
    pub const fn new(r: u8, g: u8, b: u8, a: u8) -> Self {
        Color { a, g, b, r }
    }

    pub const fn rgba(self) -> u32 {
        (self.a as u32) << 24 | (self.b as u32) << 16 | (self.g as u32) << 8 | (self.r as u32) << 0
    }
}
