use std::slice;

use crate::internal::{RawCast, RawWrapper};
use crate::render::renderer::TextureId;
use crate::sys;

/// All draw data to render a Dear ImGui frame.
#[repr(C)]
pub struct DrawData {
    /// Only valid after render() is called and before the next new frame() is called.
    valid: bool,
    /// Number of DrawList to render.
    cmd_lists_count: i32,
    /// For convenience, sum of all draw list index buffer sizes.
    pub total_idx_count: i32,
    /// For convenience, sum of all draw list vertex buffer sizes.
    pub total_vtx_count: i32,
    // Array of DrawList.
    cmd_lists: *mut *mut DrawList,
    /// Upper-left position of the viewport to render.
    ///
    /// (= upper-left corner of the orthogonal projection matrix to use)
    pub display_pos: [f32; 2],
    /// Size of the viewport to render.
    ///
    /// (= display_pos + display_size == lower-right corner of the orthogonal matrix to use)
    pub display_size: [f32; 2],
    /// Amount of pixels for each unit of display_size.
    ///
    /// Based on io.display_frame_buffer_scale. Typically [1.0, 1.0] on normal displays, and
    /// [2.0, 2.0] on Retina displays, but fractional values are also possible.
    pub framebuffer_scale: [f32; 2],
}

unsafe impl RawCast<sys::ImDrawData> for DrawData {}

impl DrawData {
    /// Returns an iterator over the draw lists included in the draw data.
    #[inline]
    pub fn draw_lists(&self) -> DrawListIterator<'_> {
        unsafe {
            DrawListIterator {
                iter: self.cmd_lists().iter(),
            }
        }
    }
    /// Returns the number of draw lists included in the draw data.
    #[inline]
    pub fn draw_lists_count(&self) -> usize {
        use std::convert::TryInto;
        self.cmd_lists_count.try_into().unwrap()
    }
    #[inline]
    pub(crate) unsafe fn cmd_lists(&self) -> &[*const DrawList] {
        slice::from_raw_parts(
            self.cmd_lists as *const *const DrawList,
            self.cmd_lists_count as usize,
        )
    }
    /// Converts all buffers from indexed to non-indexed, in case you cannot render indexed
    /// buffers.
    ///
    /// **This is slow and most likely a waste of resources. Always prefer indexed rendering!**
    #[doc(alias = "DeIndexAllBuffers")]
    pub fn deindex_all_buffers(&mut self) {
        unsafe {
            sys::ImDrawData_DeIndexAllBuffers(self.raw_mut());
        }
    }
    /// Scales the clip rect of each draw command.
    ///
    /// Can be used if your final output buffer is at a different scale than imgui-rs expects, or
    /// if there is a difference between your window resolution and framebuffer resolution.
    #[doc(alias = "ScaleClipRects")]
    pub fn scale_clip_rects(&mut self, fb_scale: [f32; 2]) {
        unsafe {
            sys::ImDrawData_ScaleClipRects(self.raw_mut(), fb_scale.into());
        }
    }
}

/// Iterator over draw lists
pub struct DrawListIterator<'a> {
    iter: std::slice::Iter<'a, *const DrawList>,
}

impl<'a> Iterator for DrawListIterator<'a> {
    type Item = &'a DrawList;

    fn next(&mut self) -> Option<Self::Item> {
        self.iter.next().map(|&ptr| unsafe { &*ptr })
    }
}

#[test]
#[cfg(test)]
fn test_drawdata_memory_layout() {
    use std::mem;
    assert_eq!(
        mem::size_of::<DrawData>(),
        mem::size_of::<sys::ImDrawData>()
    );
    assert_eq!(
        mem::align_of::<DrawData>(),
        mem::align_of::<sys::ImDrawData>()
    );
    use sys::ImDrawData;
    macro_rules! assert_field_offset {
        ($l:ident, $r:ident) => {
            assert_eq!(
                memoffset::offset_of!(DrawData, $l),
                memoffset::offset_of!(ImDrawData, $r)
            );
        };
    }
    assert_field_offset!(valid, Valid);
    assert_field_offset!(cmd_lists, CmdLists);
    assert_field_offset!(cmd_lists_count, CmdListsCount);
    assert_field_offset!(total_idx_count, TotalIdxCount);
    assert_field_offset!(total_vtx_count, TotalVtxCount);
    assert_field_offset!(display_pos, DisplayPos);
    assert_field_offset!(display_size, DisplaySize);
    assert_field_offset!(framebuffer_scale, FramebufferScale);
}

/// Draw command list
#[repr(transparent)]
pub struct DrawList(sys::ImDrawList);

impl RawWrapper for DrawList {
    type Raw = sys::ImDrawList;
    #[inline]
    unsafe fn raw(&self) -> &sys::ImDrawList {
        &self.0
    }
    #[inline]
    unsafe fn raw_mut(&mut self) -> &mut sys::ImDrawList {
        &mut self.0
    }
}

impl DrawList {
    #[inline]
    pub(crate) unsafe fn cmd_buffer(&self) -> &[sys::ImDrawCmd] {
        slice::from_raw_parts(
            self.0.CmdBuffer.Data as *const sys::ImDrawCmd,
            self.0.CmdBuffer.Size as usize,
        )
    }
    #[inline]
    pub fn idx_buffer(&self) -> &[DrawIdx] {
        unsafe {
            slice::from_raw_parts(
                self.0.IdxBuffer.Data as *const DrawIdx,
                self.0.IdxBuffer.Size as usize,
            )
        }
    }
    #[inline]
    pub fn vtx_buffer(&self) -> &[DrawVert] {
        unsafe {
            slice::from_raw_parts(
                self.0.VtxBuffer.Data as *const DrawVert,
                self.0.VtxBuffer.Size as usize,
            )
        }
    }

    /// # Safety
    /// This is equivalent to `transmute(self.vtx_buffer())` with a little more
    /// checking, and thus inherits the safety considerations of `transmute`ing
    /// slices.
    pub unsafe fn transmute_vtx_buffer<VTy: Copy>(&self) -> &[VTy] {
        // these checks are constant and thus are removed from release builds
        assert_eq!(
            core::mem::size_of::<VTy>(),
            core::mem::size_of::<DrawVert>(),
        );
        assert!(core::mem::align_of::<VTy>() <= core::mem::align_of::<DrawVert>());
        slice::from_raw_parts(self.0.VtxBuffer.Data.cast(), self.0.VtxBuffer.Size as usize)
    }

    #[inline]
    pub fn commands(&self) -> DrawCmdIterator<'_> {
        unsafe {
            DrawCmdIterator {
                iter: self.cmd_buffer().iter(),
            }
        }
    }
}

pub struct DrawCmdIterator<'a> {
    iter: std::slice::Iter<'a, sys::ImDrawCmd>,
}

impl<'a> Iterator for DrawCmdIterator<'a> {
    type Item = DrawCmd;

    #[inline]
    fn next(&mut self) -> Option<Self::Item> {
        self.iter.next().map(|cmd| {
            let cmd_params = DrawCmdParams {
                clip_rect: cmd.ClipRect.into(),
                texture_id: TextureId::from(cmd.TextureId),
                vtx_offset: cmd.VtxOffset as usize,
                idx_offset: cmd.IdxOffset as usize,
            };
            match cmd.UserCallback {
                Some(raw_callback) if raw_callback as usize == -1isize as usize => {
                    DrawCmd::ResetRenderState
                }
                Some(raw_callback) => DrawCmd::RawCallback {
                    callback: raw_callback,
                    raw_cmd: cmd,
                },
                None => DrawCmd::Elements {
                    count: cmd.ElemCount as usize,
                    cmd_params,
                },
            }
        })
    }
}

/// A vertex index
pub type DrawIdx = sys::ImDrawIdx;

#[derive(Copy, Clone, Debug, PartialEq)]
pub struct DrawCmdParams {
    /// left, up, right, down
    pub clip_rect: [f32; 4],
    pub texture_id: TextureId,
    pub vtx_offset: usize,
    pub idx_offset: usize,
}

/// A draw command
pub enum DrawCmd {
    Elements {
        /// The number of indices used for this draw command
        count: usize,
        cmd_params: DrawCmdParams,
    },
    ResetRenderState,
    RawCallback {
        callback: unsafe extern "C" fn(*const sys::ImDrawList, cmd: *const sys::ImDrawCmd),
        raw_cmd: *const sys::ImDrawCmd,
    },
}

/// A single vertex
#[repr(C)]
#[derive(Copy, Clone, Debug, PartialEq)]
pub struct DrawVert {
    pub pos: [f32; 2],
    pub uv: [f32; 2],
    pub col: [u8; 4],
}

#[test]
#[cfg(test)]
fn test_drawvert_memory_layout() {
    use std::mem;
    assert_eq!(
        mem::size_of::<DrawVert>(),
        mem::size_of::<sys::ImDrawVert>()
    );
    assert_eq!(
        mem::align_of::<DrawVert>(),
        mem::align_of::<sys::ImDrawVert>()
    );
    use sys::ImDrawVert;
    macro_rules! assert_field_offset {
        ($l:ident, $r:ident) => {
            assert_eq!(
                memoffset::offset_of!(DrawVert, $l),
                memoffset::offset_of!(ImDrawVert, $r)
            );
        };
    }
    assert_field_offset!(pos, pos);
    assert_field_offset!(uv, uv);
    assert_field_offset!(col, col);
}
