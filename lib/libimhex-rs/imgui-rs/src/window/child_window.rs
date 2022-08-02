use std::f32;
use std::os::raw::{c_char, c_void};

use crate::sys;
use crate::window::WindowFlags;
use crate::{Id, Ui};

/// Builder for a child window
#[derive(Copy, Clone, Debug)]
#[must_use]
pub struct ChildWindow<'a> {
    id: Id<'a>,
    flags: WindowFlags,
    size: [f32; 2],
    content_size: [f32; 2],
    focused: bool,
    bg_alpha: f32,
    border: bool,
}

impl<'a> ChildWindow<'a> {
    /// Creates a new child window builder with the given ID
    #[doc(alias = "BeginChildID")]
    pub fn new<T: Into<Id<'a>>>(id: T) -> ChildWindow<'a> {
        ChildWindow {
            id: id.into(),
            flags: WindowFlags::empty(),
            size: [0.0, 0.0],
            content_size: [0.0, 0.0],
            focused: false,
            bg_alpha: f32::NAN,
            border: false,
        }
    }
    /// Replace current window flags with the given value
    #[inline]
    pub fn flags(mut self, flags: WindowFlags) -> Self {
        self.flags = flags;
        self
    }
    /// Sets the child window size.
    ///
    /// For each independent axis of size:
    ///
    /// - `> 0.0`: fixed size
    /// - `= 0.0`: use remaining host window size
    /// - `< 0.0`: use remaining host window size minus abs(size)
    #[inline]
    pub fn size(mut self, size: [f32; 2]) -> Self {
        self.size = size;
        self
    }
    /// Sets the window content size, which can be used to enforce scrollbars.
    ///
    /// Does not include window decorations (title bar, menu bar, etc.). Set one of the values to
    /// 0.0 to leave the size automatic.
    #[inline]
    #[doc(alias = "SetNextWindowContentSize")]
    pub fn content_size(mut self, size: [f32; 2]) -> Self {
        self.content_size = size;
        self
    }
    /// Sets the window focused state, which can be used to bring the window to front
    #[inline]
    #[doc(alias = "SetNextWindowFocus")]
    pub fn focused(mut self, focused: bool) -> Self {
        self.focused = focused;
        self
    }
    /// Sets the background color alpha value.
    ///
    /// See also `draw_background`
    #[inline]
    #[doc(alias = "SetNextWindowContentBgAlpha")]
    pub fn bg_alpha(mut self, bg_alpha: f32) -> Self {
        self.bg_alpha = bg_alpha;
        self
    }
    /// Enables/disables the child window border.
    ///
    /// Disabled by default.
    #[inline]
    pub fn border(mut self, border: bool) -> Self {
        self.border = border;
        self
    }
    /// Enables/disables moving the window when child window is dragged.
    ///
    /// Enabled by default.
    #[inline]
    pub fn movable(mut self, value: bool) -> Self {
        self.flags.set(WindowFlags::NO_MOVE, !value);
        self
    }
    /// Enables/disables scrollbars (scrolling is still possible with the mouse or
    /// programmatically).
    ///
    /// Enabled by default.
    #[inline]
    pub fn scroll_bar(mut self, value: bool) -> Self {
        self.flags.set(WindowFlags::NO_SCROLLBAR, !value);
        self
    }
    /// Enables/disables vertical scrolling with the mouse wheel.
    ///
    /// Enabled by default.
    /// When enabled, child windows forward the mouse wheel to the parent unless `NO_SCROLLBAR`
    /// is also set.
    #[inline]
    pub fn scrollable(mut self, value: bool) -> Self {
        self.flags.set(WindowFlags::NO_SCROLL_WITH_MOUSE, !value);
        self
    }
    /// Enables/disables resizing the window to its content on every frame.
    ///
    /// Disabled by default.
    #[inline]
    pub fn always_auto_resize(mut self, value: bool) -> Self {
        self.flags.set(WindowFlags::ALWAYS_AUTO_RESIZE, value);
        self
    }
    /// Enables/disables drawing of background color and outside border.
    ///
    /// Enabled by default.
    #[inline]
    pub fn draw_background(mut self, value: bool) -> Self {
        self.flags.set(WindowFlags::NO_BACKGROUND, !value);
        self
    }
    /// Enables/disables catching mouse input.
    ///
    /// Enabled by default.
    /// Note: Hovering test will pass through when disabled
    #[inline]
    pub fn mouse_inputs(mut self, value: bool) -> Self {
        self.flags.set(WindowFlags::NO_MOUSE_INPUTS, !value);
        self
    }
    /// Enables/disables the menu bar.
    ///
    /// Disabled by default.
    #[inline]
    pub fn menu_bar(mut self, value: bool) -> Self {
        self.flags.set(WindowFlags::MENU_BAR, value);
        self
    }
    /// Enables/disables the horizontal scrollbar.
    ///
    /// Disabled by default.
    #[inline]
    pub fn horizontal_scrollbar(mut self, value: bool) -> Self {
        self.flags.set(WindowFlags::HORIZONTAL_SCROLLBAR, value);
        self
    }
    /// Enables/disables taking focus when transitioning from hidden to visible state.
    ///
    /// Enabled by default.
    #[inline]
    pub fn focus_on_appearing(mut self, value: bool) -> Self {
        self.flags.set(WindowFlags::NO_FOCUS_ON_APPEARING, !value);
        self
    }
    /// Enables/disables bringing the window to front when taking focus (e.g. clicking it or
    /// programmatically giving it focus).
    ///
    /// Enabled by default.
    #[inline]
    pub fn bring_to_front_on_focus(mut self, value: bool) -> Self {
        self.flags
            .set(WindowFlags::NO_BRING_TO_FRONT_ON_FOCUS, !value);
        self
    }
    /// When enabled, forces the vertical scrollbar to render regardless of the content size.
    ///
    /// Disabled by default.
    #[inline]
    pub fn always_vertical_scrollbar(mut self, value: bool) -> Self {
        self.flags
            .set(WindowFlags::ALWAYS_VERTICAL_SCROLLBAR, value);
        self
    }
    /// When enabled, forces the horizontal scrollbar to render regardless of the content size.
    ///
    /// Disabled by default.
    #[inline]
    pub fn always_horizontal_scrollbar(mut self, value: bool) -> Self {
        self.flags
            .set(WindowFlags::ALWAYS_HORIZONTAL_SCROLLBAR, value);
        self
    }
    /// When enabled, ensures child windows without border use `style.window_padding`.
    ///
    /// Disabled by default.
    #[inline]
    pub fn always_use_window_padding(mut self, value: bool) -> Self {
        self.flags
            .set(WindowFlags::ALWAYS_USE_WINDOW_PADDING, value);
        self
    }
    /// Enables/disables gamepad/keyboard navigation within the window.
    ///
    /// Enabled by default.
    #[inline]
    pub fn nav_inputs(mut self, value: bool) -> Self {
        self.flags.set(WindowFlags::NO_NAV_INPUTS, !value);
        self
    }
    /// Enables/disables focusing toward this window with gamepad/keyboard navigation (e.g.
    /// CTRL+TAB).
    ///
    /// Enabled by default.
    #[inline]
    pub fn nav_focus(mut self, value: bool) -> Self {
        self.flags.set(WindowFlags::NO_NAV_FOCUS, !value);
        self
    }
    /// Disable gamepad/keyboard navigation and focusing.
    ///
    /// Shorthand for
    /// ```text
    /// .nav_inputs(false)
    /// .nav_focus(false)
    /// ```
    #[inline]
    pub fn no_nav(mut self) -> Self {
        self.flags |= WindowFlags::NO_NAV;
        self
    }
    /// Don't handle input.
    ///
    /// Shorthand for
    /// ```text
    /// .mouse_inputs(false)
    /// .nav_inputs(false)
    /// .nav_focus(false)
    /// ```
    #[inline]
    pub fn no_inputs(mut self) -> Self {
        self.flags |= WindowFlags::NO_INPUTS;
        self
    }
    /// Creates a child window and starts append to it.
    ///
    /// Returns `Some(ChildWindowToken)` if the window is visible. After content has been
    /// rendered, the token must be ended by calling `.end()`.
    ///
    /// Returns `None` if the window is not visible and no content should be rendered.
    pub fn begin<'ui>(self, ui: &Ui<'ui>) -> Option<ChildWindowToken<'ui>> {
        if self.content_size[0] != 0.0 || self.content_size[1] != 0.0 {
            unsafe { sys::igSetNextWindowContentSize(self.content_size.into()) };
        }
        if self.focused {
            unsafe { sys::igSetNextWindowFocus() };
        }
        if self.bg_alpha.is_finite() {
            unsafe { sys::igSetNextWindowBgAlpha(self.bg_alpha) };
        }
        let id = unsafe {
            match self.id {
                Id::Int(i) => sys::igGetID_Ptr(i as *const c_void),
                Id::Ptr(p) => sys::igGetID_Ptr(p),
                Id::Str(s) => {
                    let start = s.as_ptr() as *const c_char;
                    let end = start.add(s.len());
                    sys::igGetID_StrStr(start, end)
                }
            }
        };
        let should_render = unsafe {
            sys::igBeginChild_ID(id, self.size.into(), self.border, self.flags.bits() as i32)
        };
        if should_render {
            Some(ChildWindowToken::new(ui))
        } else {
            unsafe { sys::igEndChild() };
            None
        }
    }
    /// Creates a child window and runs a closure to construct the contents.
    /// Returns the result of the closure, if it is called.
    ///
    /// Note: the closure is not called if no window content is visible (e.g. window is collapsed
    /// or fully clipped).
    pub fn build<T, F: FnOnce() -> T>(self, ui: &Ui<'_>, f: F) -> Option<T> {
        self.begin(ui).map(|_window| f())
    }
}

create_token!(
    /// Tracks a child window that can be ended by calling `.end()`
    /// or by dropping
    pub struct ChildWindowToken<'ui>;

    /// Ends a window
    drop { sys::igEndChild() }
);
