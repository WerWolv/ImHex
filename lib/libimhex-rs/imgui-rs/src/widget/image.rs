use std::os::raw::c_void;

use crate::render::renderer::TextureId;
use crate::sys;
use crate::Ui;

/// Builder for an image widget
#[derive(Copy, Clone, Debug)]
#[must_use]
pub struct Image {
    texture_id: TextureId,
    size: [f32; 2],
    uv0: [f32; 2],
    uv1: [f32; 2],
    tint_col: [f32; 4],
    border_col: [f32; 4],
}

impl Image {
    /// Creates a new image builder with the given texture and size
    #[doc(alias = "Image")]
    pub const fn new(texture_id: TextureId, size: [f32; 2]) -> Image {
        Image {
            texture_id,
            size,
            uv0: [0.0, 0.0],
            uv1: [1.0, 1.0],
            tint_col: [1.0, 1.0, 1.0, 1.0],
            border_col: [0.0, 0.0, 0.0, 0.0],
        }
    }
    /// Sets the image size
    pub const fn size(mut self, size: [f32; 2]) -> Self {
        self.size = size;
        self
    }
    /// Sets uv0 (default `[0.0, 0.0]`)
    pub const fn uv0(mut self, uv0: [f32; 2]) -> Self {
        self.uv0 = uv0;
        self
    }
    /// Sets uv1 (default `[1.0, 1.0]`)
    pub const fn uv1(mut self, uv1: [f32; 2]) -> Self {
        self.uv1 = uv1;
        self
    }
    /// Sets the tint color (default: no tint color)
    pub const fn tint_col(mut self, tint_col: [f32; 4]) -> Self {
        self.tint_col = tint_col;
        self
    }
    /// Sets the border color (default: no border)
    pub const fn border_col(mut self, border_col: [f32; 4]) -> Self {
        self.border_col = border_col;
        self
    }
    /// Builds the image
    pub fn build(self, _: &Ui<'_>) {
        unsafe {
            sys::igImage(
                self.texture_id.id() as *mut c_void,
                self.size.into(),
                self.uv0.into(),
                self.uv1.into(),
                self.tint_col.into(),
                self.border_col.into(),
            );
        }
    }
}

/// Builder for an image button widget
#[derive(Copy, Clone, Debug)]
#[must_use]
pub struct ImageButton {
    texture_id: TextureId,
    size: [f32; 2],
    uv0: [f32; 2],
    uv1: [f32; 2],
    frame_padding: i32,
    bg_col: [f32; 4],
    tint_col: [f32; 4],
}

impl ImageButton {
    /// Creates a new image button builder with the given texture and size
    #[doc(alias = "ImageButton")]
    pub fn new(texture_id: TextureId, size: [f32; 2]) -> ImageButton {
        ImageButton {
            texture_id,
            size,
            uv0: [0.0, 0.0],
            uv1: [1.0, 1.0],
            frame_padding: -1,
            bg_col: [0.0, 0.0, 0.0, 0.0],
            tint_col: [1.0, 1.0, 1.0, 1.0],
        }
    }
    /// Sets the image button size
    pub fn size(mut self, size: [f32; 2]) -> Self {
        self.size = size;
        self
    }
    /// Sets uv0 (default `[0.0, 0.0]`)
    pub fn uv0(mut self, uv0: [f32; 2]) -> Self {
        self.uv0 = uv0;
        self
    }
    /// Sets uv1 (default `[1.0, 1.0]`)
    pub fn uv1(mut self, uv1: [f32; 2]) -> Self {
        self.uv1 = uv1;
        self
    }
    /// Sets the frame padding (default: uses frame padding from style).
    ///
    /// - `< 0`: uses frame padding from style (default)
    /// - `= 0`: no framing
    /// - `> 0`: set framing size
    pub fn frame_padding(mut self, frame_padding: i32) -> Self {
        self.frame_padding = frame_padding;
        self
    }
    /// Sets the background color (default: no background color)
    pub fn background_col(mut self, bg_col: [f32; 4]) -> Self {
        self.bg_col = bg_col;
        self
    }
    /// Sets the tint color (default: no tint color)
    pub fn tint_col(mut self, tint_col: [f32; 4]) -> Self {
        self.tint_col = tint_col;
        self
    }
    /// Builds the image button
    pub fn build(self, _: &Ui<'_>) -> bool {
        unsafe {
            sys::igImageButton(
                self.texture_id.id() as *mut c_void,
                self.size.into(),
                self.uv0.into(),
                self.uv1.into(),
                self.frame_padding,
                self.bg_col.into(),
                self.tint_col.into(),
            )
        }
    }
}
