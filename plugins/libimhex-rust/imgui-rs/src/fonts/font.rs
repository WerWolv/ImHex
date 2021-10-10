use std::os::raw::c_int;

use crate::fonts::atlas::{FontAtlas, FontId};
use crate::fonts::glyph::FontGlyph;
use crate::internal::{ImVector, RawCast};
use crate::sys;

/// Runtime data for a single font within a font atlas
#[repr(C)]
pub struct Font {
    index_advance_x: ImVector<f32>,
    pub fallback_advance_x: f32,
    pub font_size: f32,
    index_lookup: ImVector<sys::ImWchar>,
    glyphs: ImVector<FontGlyph>,
    fallback_glyph: *const FontGlyph,
    container_atlas: *mut FontAtlas,
    config_data: *const sys::ImFontConfig,
    pub config_data_count: i16,
    pub fallback_char: sys::ImWchar,
    pub ellipsis_char: sys::ImWchar,
    pub dot_char: sys::ImWchar,
    pub dirty_lookup_tables: bool,
    pub scale: f32,
    pub ascent: f32,
    pub descent: f32,
    pub metrics_total_surface: c_int,
    pub used_4k_pages_map: [u8; 34],
}

unsafe impl RawCast<sys::ImFont> for Font {}

impl Font {
    /// Returns the identifier of this font
    pub fn id(&self) -> FontId {
        FontId(self as *const _)
    }
}

#[test]
fn test_font_memory_layout() {
    use std::mem;
    assert_eq!(mem::size_of::<Font>(), mem::size_of::<sys::ImFont>());
    assert_eq!(mem::align_of::<Font>(), mem::align_of::<sys::ImFont>());
    use sys::ImFont;
    macro_rules! assert_field_offset {
        ($l:ident, $r:ident) => {
            assert_eq!(
                memoffset::offset_of!(Font, $l),
                memoffset::offset_of!(ImFont, $r)
            );
        };
    }
    assert_field_offset!(index_advance_x, IndexAdvanceX);
    assert_field_offset!(fallback_advance_x, FallbackAdvanceX);
    assert_field_offset!(font_size, FontSize);
    assert_field_offset!(index_lookup, IndexLookup);
    assert_field_offset!(glyphs, Glyphs);
    assert_field_offset!(fallback_glyph, FallbackGlyph);
    assert_field_offset!(container_atlas, ContainerAtlas);
    assert_field_offset!(config_data, ConfigData);
    assert_field_offset!(config_data_count, ConfigDataCount);
    assert_field_offset!(fallback_char, FallbackChar);
    assert_field_offset!(ellipsis_char, EllipsisChar);
    assert_field_offset!(dirty_lookup_tables, DirtyLookupTables);
    assert_field_offset!(scale, Scale);
    assert_field_offset!(ascent, Ascent);
    assert_field_offset!(descent, Descent);
    assert_field_offset!(metrics_total_surface, MetricsTotalSurface);
    assert_field_offset!(used_4k_pages_map, Used4kPagesMap);
}
