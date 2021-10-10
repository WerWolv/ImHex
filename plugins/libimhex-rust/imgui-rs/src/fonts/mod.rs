use crate::fonts::font::Font;
use crate::internal::RawCast;
use crate::Ui;

pub mod atlas;
pub mod font;
pub mod glyph;
pub mod glyph_ranges;

/// # Fonts
impl<'ui> Ui<'ui> {
    /// Returns the current font
    #[doc(alias = "GetFont")]
    pub fn current_font(&self) -> &Font {
        unsafe { Font::from_raw(&*sys::igGetFont()) }
    }
    /// Returns the current font size (= height in pixels) with font scale applied
    #[doc(alias = "GetFontSize")]
    pub fn current_font_size(&self) -> f32 {
        unsafe { sys::igGetFontSize() }
    }
    /// Returns the UV coordinate for a white pixel.
    ///
    /// Useful for drawing custom shapes with the draw list API.
    #[doc(alias = "FontTexUvWhitePixel")]
    pub fn font_tex_uv_white_pixel(&self) -> [f32; 2] {
        let mut out = sys::ImVec2::zero();
        unsafe { sys::igGetFontTexUvWhitePixel(&mut out) };
        out.into()
    }
    /// Sets the font scale of the current window
    #[doc(alias = "SetWindowFontScale")]
    pub fn set_window_font_scale(&self, scale: f32) {
        unsafe { sys::igSetWindowFontScale(scale) }
    }
}
