use crate::sys;

#[derive(Clone, Eq, PartialEq, Debug)]
enum FontGlyphRangeData {
    ChineseSimplifiedCommon,
    ChineseFull,
    Cyrillic,
    Default,
    Japanese,
    Korean,
    Thai,
    Vietnamese,
    Custom(*const sys::ImWchar),
}

/// A set of Unicode codepoints
#[derive(Clone, Eq, PartialEq, Debug)]
pub struct FontGlyphRanges(FontGlyphRangeData);
impl FontGlyphRanges {
    /// The default set of glyph ranges used by imgui.
    pub fn default() -> FontGlyphRanges {
        FontGlyphRanges(FontGlyphRangeData::Default)
    }
    /// A set of glyph ranges appropriate for use with simplified common Chinese text.
    pub fn chinese_simplified_common() -> FontGlyphRanges {
        FontGlyphRanges(FontGlyphRangeData::ChineseSimplifiedCommon)
    }
    /// A set of glyph ranges appropriate for use with Chinese text.
    pub fn chinese_full() -> FontGlyphRanges {
        FontGlyphRanges(FontGlyphRangeData::ChineseFull)
    }
    /// A set of glyph ranges appropriate for use with Cyrillic text.
    pub fn cyrillic() -> FontGlyphRanges {
        FontGlyphRanges(FontGlyphRangeData::Cyrillic)
    }
    /// A set of glyph ranges appropriate for use with Japanese text.
    pub fn japanese() -> FontGlyphRanges {
        FontGlyphRanges(FontGlyphRangeData::Japanese)
    }
    /// A set of glyph ranges appropriate for use with Korean text.
    pub fn korean() -> FontGlyphRanges {
        FontGlyphRanges(FontGlyphRangeData::Korean)
    }
    /// A set of glyph ranges appropriate for use with Thai text.
    pub fn thai() -> FontGlyphRanges {
        FontGlyphRanges(FontGlyphRangeData::Thai)
    }
    /// A set of glyph ranges appropriate for use with Vietnamese text.
    pub fn vietnamese() -> FontGlyphRanges {
        FontGlyphRanges(FontGlyphRangeData::Vietnamese)
    }

    /// Creates a glyph range from a static slice. The expected format is a series of pairs of
    /// non-zero codepoints, each representing an inclusive range, followed by a single
    /// zero terminating the range. The ranges must not overlap.
    ///
    /// As the slice is expected to last as long as a font is used, and is written into global
    /// state, it must be `'static`.
    ///
    /// Panics
    /// ======
    ///
    /// This function will panic if the given slice is not a valid font range.
    // TODO(thom): This takes `u32` for now, since I believe it's fine for it to
    // contain surrogates? (It seems plausible that font data can describe what
    // to show for unpaired surrogates) Would be nice to be sure, if so, this
    // should accept `char` (we'd still have to check that the range doesn't
    // fully contain the surrogate range though)
    pub fn from_slice(slice: &'static [u32]) -> FontGlyphRanges {
        assert_eq!(
            slice.len() % 2,
            1,
            "The length of a glyph range must be odd."
        );
        assert_eq!(
            slice.last(),
            Some(&0),
            "A glyph range must be zero-terminated."
        );

        for (i, &glyph) in slice.iter().enumerate().take(slice.len() - 1) {
            assert_ne!(
                glyph, 0,
                "A glyph in a range cannot be zero. \
                 (Glyph is zero at index {})",
                i
            );
            assert!(
                glyph <= core::char::MAX as u32,
                "A glyph in a range cannot exceed the maximum codepoint. \
                 (Glyph is {:#x} at index {})",
                glyph,
                i,
            );
        }

        let mut ranges = Vec::new();
        for i in 0..slice.len() / 2 {
            let (start, end) = (slice[i * 2], slice[i * 2 + 1]);
            assert!(
                start <= end,
                "The start of a range cannot be larger than its end. \
                 (At index {}, {} > {})",
                i * 2,
                start,
                end
            );
            ranges.push((start, end));
        }
        ranges.sort_unstable_by_key(|x| x.0);
        for i in 0..ranges.len() - 1 {
            let (range_a, range_b) = (ranges[i], ranges[i + 1]);
            if range_a.1 >= range_b.0 {
                panic!(
                    "The glyph ranges {:?} and {:?} overlap between {:?}.",
                    range_a,
                    range_b,
                    (range_a.1, range_b.0)
                );
            }
        }

        unsafe { FontGlyphRanges::from_slice_unchecked(slice) }
    }

    /// Creates a glyph range from a static slice without checking its validity.
    ///
    /// See [`FontGlyphRanges::from_slice`] for more information.
    ///
    /// # Safety
    ///
    /// It is up to the caller to guarantee the slice contents are valid.
    pub unsafe fn from_slice_unchecked(slice: &'static [u32]) -> FontGlyphRanges {
        FontGlyphRanges::from_ptr(slice.as_ptr())
    }

    /// Creates a glyph range from a pointer, without checking its validity or enforcing its
    /// lifetime. The memory the pointer points to must be valid for as long as the font is
    /// in use.
    ///
    /// # Safety
    ///
    /// It is up to the caller to guarantee the pointer is not null, remains valid forever, and
    /// points to valid data.
    pub unsafe fn from_ptr(ptr: *const u32) -> FontGlyphRanges {
        FontGlyphRanges(FontGlyphRangeData::Custom(ptr))
    }

    pub(crate) unsafe fn to_ptr(&self, atlas: *mut sys::ImFontAtlas) -> *const sys::ImWchar {
        match self.0 {
            FontGlyphRangeData::ChineseFull => sys::ImFontAtlas_GetGlyphRangesChineseFull(atlas),
            FontGlyphRangeData::ChineseSimplifiedCommon => {
                sys::ImFontAtlas_GetGlyphRangesChineseSimplifiedCommon(atlas)
            }
            FontGlyphRangeData::Cyrillic => sys::ImFontAtlas_GetGlyphRangesCyrillic(atlas),
            FontGlyphRangeData::Default => sys::ImFontAtlas_GetGlyphRangesDefault(atlas),
            FontGlyphRangeData::Japanese => sys::ImFontAtlas_GetGlyphRangesJapanese(atlas),
            FontGlyphRangeData::Korean => sys::ImFontAtlas_GetGlyphRangesKorean(atlas),
            FontGlyphRangeData::Thai => sys::ImFontAtlas_GetGlyphRangesThai(atlas),
            FontGlyphRangeData::Vietnamese => sys::ImFontAtlas_GetGlyphRangesVietnamese(atlas),
            FontGlyphRangeData::Custom(ptr) => ptr,
        }
    }
}
