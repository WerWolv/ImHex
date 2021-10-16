use crate::sys;
use crate::Ui;

/// # Window scrolling
impl<'ui> Ui<'ui> {
    /// Returns the horizontal scrolling position.
    ///
    /// Value is between 0.0 and self.scroll_max_x().
    #[doc(alias = "GetScrollX")]
    pub fn scroll_x(&self) -> f32 {
        unsafe { sys::igGetScrollX() }
    }
    /// Returns the vertical scrolling position.
    ///
    /// Value is between 0.0 and self.scroll_max_y().
    #[doc(alias = "GetScrollY")]
    pub fn scroll_y(&self) -> f32 {
        unsafe { sys::igGetScrollY() }
    }
    /// Returns the maximum horizontal scrolling position.
    ///
    /// Roughly equal to content size X - window size X.
    #[doc(alias = "GetScrollMaxX")]
    pub fn scroll_max_x(&self) -> f32 {
        unsafe { sys::igGetScrollMaxX() }
    }
    /// Returns the maximum vertical scrolling position.
    ///
    /// Roughly equal to content size Y - window size Y.
    #[doc(alias = "GetScrollMaxY")]
    pub fn scroll_max_y(&self) -> f32 {
        unsafe { sys::igGetScrollMaxY() }
    }
    /// Sets the horizontal scrolling position
    #[doc(alias = "SetScrollX")]
    pub fn set_scroll_x(&self, scroll_x: f32) {
        unsafe { sys::igSetScrollX(scroll_x) };
    }
    /// Sets the vertical scroll position
    #[doc(alias = "SetScrollY")]
    pub fn set_scroll_y(&self, scroll_y: f32) {
        unsafe { sys::igSetScrollY(scroll_y) };
    }
    /// Adjusts the horizontal scroll position to make the current cursor position visible.
    ///
    /// This is the same as [set_scroll_here_x_with_ratio](Self::set_scroll_here_x_with_ratio) but with `ratio` at 0.5.
    #[doc(alias = "SetScrollHereX")]
    pub fn set_scroll_here_x(&self) {
        self.set_scroll_here_x_with_ratio(0.5);
    }
    /// Adjusts the horizontal scroll position to make the current cursor position visible.
    ///
    /// center_x_ratio:
    ///
    /// - `0.0`: left
    /// - `0.5`: center
    /// - `1.0`: right
    #[doc(alias = "SetScrollHereX")]
    pub fn set_scroll_here_x_with_ratio(&self, center_x_ratio: f32) {
        unsafe { sys::igSetScrollHereX(center_x_ratio) };
    }
    /// Adjusts the vertical scroll position to make the current cursor position visible
    ///
    /// This is the same as [set_scroll_here_y_with_ratio](Self::set_scroll_here_y_with_ratio) but with `ratio` at 0.5.
    #[doc(alias = "SetScrollHereY")]
    pub fn set_scroll_here_y(&self) {
        self.set_scroll_here_y_with_ratio(0.5);
    }
    /// Adjusts the vertical scroll position to make the current cursor position visible.
    ///
    /// center_y_ratio:
    ///
    /// - `0.0`: top
    /// - `0.5`: center
    /// - `1.0`: bottom
    #[doc(alias = "SetScrollHereY")]
    pub fn set_scroll_here_y_with_ratio(&self, center_y_ratio: f32) {
        unsafe { sys::igSetScrollHereY(center_y_ratio) };
    }
    #[doc(alias = "SetScrollFromPosX")]
    /// Adjusts the horizontal scroll position to make the given position visible
    ///
    /// This is the same as [set_scroll_from_pos_x_with_ratio](Self::set_scroll_from_pos_x_with_ratio)
    /// but with `ratio` at 0.5.
    pub fn set_scroll_from_pos_x(&self, local_x: f32) {
        self.set_scroll_from_pos_x_with_ratio(local_x, 0.5);
    }
    /// Adjusts the horizontal scroll position to make the given position visible.
    ///
    /// center_x_ratio:
    ///
    /// - `0.0`: left
    /// - `0.5`: center
    /// - `1.0`: right
    #[doc(alias = "SetScrollFromPosX")]
    pub fn set_scroll_from_pos_x_with_ratio(&self, local_x: f32, center_x_ratio: f32) {
        unsafe { sys::igSetScrollFromPosX(local_x, center_x_ratio) };
    }
    /// Adjusts the vertical scroll position to make the given position visible
    ///
    /// This is the same as [set_scroll_from_pos_y_with_ratio](Self::set_scroll_from_pos_y_with_ratio)
    /// but with `ratio` at 0.5.
    #[doc(alias = "SetScrollFromPosY")]
    pub fn set_scroll_from_pos_y(&self, local_y: f32) {
        self.set_scroll_from_pos_y_with_ratio(local_y, 0.5);
    }
    /// Adjusts the vertical scroll position to make the given position visible.
    ///
    /// center_y_ratio:
    ///
    /// - `0.0`: top
    /// - `0.5`: center
    /// - `1.0`: bottom
    #[doc(alias = "SetScrollFromPosY")]
    pub fn set_scroll_from_pos_y_with_ratio(&self, local_y: f32, center_y_ratio: f32) {
        unsafe { sys::igSetScrollFromPosY(local_y, center_y_ratio) };
    }
}
