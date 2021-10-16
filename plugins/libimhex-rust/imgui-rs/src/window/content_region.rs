use crate::sys;
use crate::Ui;

/// # Content region
impl<'ui> Ui<'ui> {
    /// Returns the current content boundaries (in *window coordinates*)
    #[doc(alias = "GetContentRegionMax")]
    pub fn content_region_max(&self) -> [f32; 2] {
        let mut out = sys::ImVec2::zero();
        unsafe { sys::igGetContentRegionMax(&mut out) };
        out.into()
    }
    /// Equal to `ui.content_region_max()` - `ui.cursor_pos()`
    #[doc(alias = "GetContentRegionAvail")]
    pub fn content_region_avail(&self) -> [f32; 2] {
        let mut out = sys::ImVec2::zero();
        unsafe { sys::igGetContentRegionAvail(&mut out) };
        out.into()
    }
    /// Content boundaries min (in *window coordinates*).
    ///
    /// Roughly equal to [0.0, 0.0] - scroll.
    #[doc(alias = "GetContentRegionMin")]
    pub fn window_content_region_min(&self) -> [f32; 2] {
        let mut out = sys::ImVec2::zero();
        unsafe { sys::igGetWindowContentRegionMin(&mut out) };
        out.into()
    }
    /// Content boundaries max (in *window coordinates*).
    ///
    /// Roughly equal to [0.0, 0.0] + size - scroll.
    #[doc(alias = "GetContentRegionMax")]
    pub fn window_content_region_max(&self) -> [f32; 2] {
        let mut out = sys::ImVec2::zero();
        unsafe { sys::igGetWindowContentRegionMax(&mut out) };
        out.into()
    }
    #[doc(alias = "GetContentRegionWidth")]
    pub fn window_content_region_width(&self) -> f32 {
        unsafe { sys::igGetWindowContentRegionWidth() }
    }
}
