use crate::internal::RawCast;
use crate::sys;

/// A single font glyph
#[derive(Copy, Clone, Debug, PartialEq)]
#[repr(C)]
pub struct FontGlyph {
    bitfields: u32,
    pub advance_x: f32,
    pub x0: f32,
    pub y0: f32,
    pub x1: f32,
    pub y1: f32,
    pub u0: f32,
    pub v0: f32,
    pub u1: f32,
    pub v1: f32,
}

impl FontGlyph {
    pub fn codepoint(&self) -> u32 {
        unsafe { self.raw().Codepoint() }
    }
    pub fn set_codepoint(&mut self, codepoint: u32) {
        unsafe { self.raw_mut().set_Codepoint(codepoint) };
    }
    pub fn visible(&self) -> bool {
        unsafe { self.raw().Visible() != 0 }
    }
    pub fn set_visible(&mut self, visible: bool) {
        unsafe { self.raw_mut().set_Visible(visible as u32) }
    }
}

unsafe impl RawCast<sys::ImFontGlyph> for FontGlyph {}

#[test]
fn test_font_glyph_memory_layout() {
    use std::mem;
    assert_eq!(
        mem::size_of::<FontGlyph>(),
        mem::size_of::<sys::ImFontGlyph>()
    );
    assert_eq!(
        mem::align_of::<FontGlyph>(),
        mem::align_of::<sys::ImFontGlyph>()
    );
    use sys::ImFontGlyph;
    macro_rules! assert_field_offset {
        ($l:ident, $r:ident) => {
            assert_eq!(
                memoffset::offset_of!(FontGlyph, $l),
                memoffset::offset_of!(ImFontGlyph, $r)
            );
        };
    }
    assert_field_offset!(bitfields, _bitfield_1);
    assert_field_offset!(advance_x, AdvanceX);
    assert_field_offset!(x0, X0);
    assert_field_offset!(y0, Y0);
    assert_field_offset!(x1, X1);
    assert_field_offset!(y1, Y1);
    assert_field_offset!(u0, U0);
    assert_field_offset!(v0, V0);
    assert_field_offset!(u1, U1);
    assert_field_offset!(v1, V1);
}
