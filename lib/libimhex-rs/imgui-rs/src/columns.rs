use crate::sys;
use crate::Ui;

/// # Columns
impl<'ui> Ui<'ui> {
    #[doc(alias = "Columns")]
    pub fn columns(&self, count: i32, id: impl AsRef<str>, border: bool) {
        unsafe { sys::igColumns(count, self.scratch_txt(id), border) }
    }
    /// Switches to the next column.
    ///
    /// If the current row is finished, switches to first column of the next row
    #[doc(alias = "NextColumn")]
    pub fn next_column(&self) {
        unsafe { sys::igNextColumn() }
    }
    /// Returns the index of the current column
    #[doc(alias = "GetColumnIndex")]
    pub fn current_column_index(&self) -> i32 {
        unsafe { sys::igGetColumnIndex() }
    }
    /// Returns the width of the current column (in pixels)
    #[doc(alias = "GetColumnWidth")]
    pub fn current_column_width(&self) -> f32 {
        unsafe { sys::igGetColumnWidth(-1) }
    }
    #[doc(alias = "GetColumnWidth")]
    /// Returns the width of the given column (in pixels)
    pub fn column_width(&self, column_index: i32) -> f32 {
        unsafe { sys::igGetColumnWidth(column_index) }
    }
    #[doc(alias = "SetColumnWidth")]
    /// Sets the width of the current column (in pixels)
    pub fn set_current_column_width(&self, width: f32) {
        unsafe { sys::igSetColumnWidth(-1, width) };
    }
    #[doc(alias = "SetColumnWidth")]
    /// Sets the width of the given column (in pixels)
    pub fn set_column_width(&self, column_index: i32, width: f32) {
        unsafe { sys::igSetColumnWidth(column_index, width) };
    }
    /// Returns the offset of the current column (in pixels from the left side of the content
    /// region)
    #[doc(alias = "GetColumnOffset")]
    pub fn current_column_offset(&self) -> f32 {
        unsafe { sys::igGetColumnOffset(-1) }
    }
    /// Returns the offset of the given column (in pixels from the left side of the content region)
    #[doc(alias = "GetColumnOffset")]
    pub fn column_offset(&self, column_index: i32) -> f32 {
        unsafe { sys::igGetColumnOffset(column_index) }
    }
    /// Sets the offset of the current column (in pixels from the left side of the content region)
    #[doc(alias = "SetColumnOffset")]
    pub fn set_current_column_offset(&self, offset_x: f32) {
        unsafe { sys::igSetColumnOffset(-1, offset_x) };
    }
    /// Sets the offset of the given column (in pixels from the left side of the content region)
    #[doc(alias = "SetColumnOffset")]
    pub fn set_column_offset(&self, column_index: i32, offset_x: f32) {
        unsafe { sys::igSetColumnOffset(column_index, offset_x) };
    }
    /// Returns the current amount of columns
    #[doc(alias = "GetColumnCount")]
    pub fn column_count(&self) -> i32 {
        unsafe { sys::igGetColumnsCount() }
    }
}
