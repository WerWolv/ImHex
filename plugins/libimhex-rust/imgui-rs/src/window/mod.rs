use bitflags::bitflags;
use std::f32;
use std::ptr;

use crate::sys;
use crate::{Condition, Ui};

pub(crate) mod child_window;
pub(crate) mod content_region;
pub(crate) mod scroll;

bitflags! {
    /// Window hover check option flags
    #[repr(transparent)]
    pub struct WindowHoveredFlags: u32 {
        /// Return true if any child of the window is hovered
        const CHILD_WINDOWS = sys::ImGuiHoveredFlags_ChildWindows;
        /// Test from root window (top-most parent of the current hierarchy)
        const ROOT_WINDOW = sys::ImGuiHoveredFlags_RootWindow;
        /// Return true if any window is hovered
        const ANY_WINDOW = sys::ImGuiHoveredFlags_AnyWindow;
        /// Return true even if a popup window is blocking access to this window
        const ALLOW_WHEN_BLOCKED_BY_POPUP = sys::ImGuiHoveredFlags_AllowWhenBlockedByPopup;
        /// Return true even if an active item is blocking access to this window
        const ALLOW_WHEN_BLOCKED_BY_ACTIVE_ITEM = sys::ImGuiHoveredFlags_AllowWhenBlockedByActiveItem;
        /// Test from root window, and return true if any child is hovered
        const ROOT_AND_CHILD_WINDOWS = Self::ROOT_WINDOW.bits | Self::CHILD_WINDOWS.bits;
    }
}

bitflags! {
    /// Window focus check option flags
    #[repr(transparent)]
    pub struct WindowFocusedFlags: u32 {
        /// Return true if any child of the window is focused
        const CHILD_WINDOWS = sys::ImGuiFocusedFlags_ChildWindows;
        /// Test from root window (top-most parent of the current hierarchy)
        const ROOT_WINDOW = sys::ImGuiFocusedFlags_RootWindow;
        /// Return true if any window is focused
        const ANY_WINDOW = sys::ImGuiFocusedFlags_AnyWindow;
        /// Test from root window, and return true if any child is focused
        const ROOT_AND_CHILD_WINDOWS = Self::ROOT_WINDOW.bits | Self::CHILD_WINDOWS.bits;
    }
}

bitflags! {
    /// Configuration flags for windows
    #[repr(transparent)]
    pub struct WindowFlags: u32 {
        /// Disable the title bar
        const NO_TITLE_BAR = sys::ImGuiWindowFlags_NoTitleBar;
        /// Disable resizing with the lower-right grip
        const NO_RESIZE = sys::ImGuiWindowFlags_NoResize;
        /// Disable moving the window
        const NO_MOVE = sys::ImGuiWindowFlags_NoMove;
        /// Disable scrollbars (scrolling is still possible with the mouse or programmatically)
        const NO_SCROLLBAR = sys::ImGuiWindowFlags_NoScrollbar;
        /// Disable vertical scrolling with the mouse wheel.
        ///
        /// On child window, the mouse wheel will be forwarded to the parent unless `NO_SCROLLBAR`
        /// is also set.
        const NO_SCROLL_WITH_MOUSE = sys::ImGuiWindowFlags_NoScrollWithMouse;
        /// Disable collapsing the window by double-clicking it
        const NO_COLLAPSE = sys::ImGuiWindowFlags_NoCollapse;
        /// Resize the window to its content on every frame
        const ALWAYS_AUTO_RESIZE = sys::ImGuiWindowFlags_AlwaysAutoResize;
        /// Disable drawing of background color and outside border
        const NO_BACKGROUND = sys::ImGuiWindowFlags_NoBackground;
        /// Never load/save settings
        const NO_SAVED_SETTINGS = sys::ImGuiWindowFlags_NoSavedSettings;
        /// Disable catching mouse input. Hovering test will pass through
        const NO_MOUSE_INPUTS = sys::ImGuiWindowFlags_NoMouseInputs;
        /// Show a menu bar
        const MENU_BAR = sys::ImGuiWindowFlags_MenuBar;
        /// Allow horizontal scrollbar to appear
        const HORIZONTAL_SCROLLBAR = sys::ImGuiWindowFlags_HorizontalScrollbar;
        /// Disable taking focus when transitioning from hidden to visible state
        const NO_FOCUS_ON_APPEARING = sys::ImGuiWindowFlags_NoFocusOnAppearing;
        /// Disable bringing window to front when taking focus (e.g. clicking it or
        /// programmatically giving it focus)
        const NO_BRING_TO_FRONT_ON_FOCUS = sys::ImGuiWindowFlags_NoBringToFrontOnFocus;
        /// Always show vertical scrollbar
        const ALWAYS_VERTICAL_SCROLLBAR = sys::ImGuiWindowFlags_AlwaysVerticalScrollbar;
        /// Always show horizontal scrollbar
        const ALWAYS_HORIZONTAL_SCROLLBAR = sys::ImGuiWindowFlags_AlwaysHorizontalScrollbar;
        /// Ensure child windows without border use `style.window_padding`
        const ALWAYS_USE_WINDOW_PADDING = sys::ImGuiWindowFlags_AlwaysUseWindowPadding;
        /// Disable gamepad/keyboard navigation within the window
        const NO_NAV_INPUTS = sys::ImGuiWindowFlags_NoNavInputs;
        /// No focusing toward this window with gamepad/keyboard navigation (e.g. skipped by
        /// CTRL+TAB)
        const NO_NAV_FOCUS = sys::ImGuiWindowFlags_NoNavFocus;
        /// Append '*' to title without affecting the ID, as a convenience
        const UNSAVED_DOCUMENT = sys::ImGuiWindowFlags_UnsavedDocument;
        /// Disable gamepad/keyboard navigation and focusing.
        ///
        /// Shorthand for `WindowFlags::NO_NAV_INPUTS | WindowFlags::NO_NAV_FOCUS`.
        const NO_NAV = sys::ImGuiWindowFlags_NoNav;
        /// Disable all window decorations.
        ///
        /// Shorthand for `WindowFlags::NO_TITLE_BAR | WindowFlags::NO_RESIZE |
        /// WindowFlags::NO_SCROLLBAR | WindowFlags::NO_COLLAPSE`.
        const NO_DECORATION = sys::ImGuiWindowFlags_NoDecoration;
        /// Don't handle input.
        ///
        /// Shorthand for `WindowFlags::NO_MOUSE_INPUTS | WindowFlags::NO_NAV_INPUTS |
        /// WindowFlags::NO_NAV_FOCUS`.
        const NO_INPUTS = sys::ImGuiWindowFlags_NoInputs;
    }
}

/// # Window utilities
impl<'ui> Ui<'ui> {
    /// Returns true if the current window appeared during this frame
    #[doc(alias = "IsWindowAppearing")]
    pub fn is_window_appearing(&self) -> bool {
        unsafe { sys::igIsWindowAppearing() }
    }
    /// Returns true if the current window is in collapsed state (= only the title bar is visible)
    #[doc(alias = "IsWindowCollapsed")]
    pub fn is_window_collapsed(&self) -> bool {
        unsafe { sys::igIsWindowCollapsed() }
    }
    /// Returns true if the current window is focused
    #[doc(alias = "IsWindowFocused")]
    pub fn is_window_focused(&self) -> bool {
        unsafe { sys::igIsWindowFocused(0) }
    }
    /// Returns true if the current window is focused based on the given flags
    #[doc(alias = "IsWindowFocused")]
    pub fn is_window_focused_with_flags(&self, flags: WindowFocusedFlags) -> bool {
        unsafe { sys::igIsWindowFocused(flags.bits() as i32) }
    }
    /// Returns true if the current window is hovered
    #[doc(alias = "IsWindowHovered")]
    pub fn is_window_hovered(&self) -> bool {
        unsafe { sys::igIsWindowHovered(0) }
    }
    /// Returns true if the current window is hovered based on the given flags
    #[doc(alias = "IsWindowHovered")]
    pub fn is_window_hovered_with_flags(&self, flags: WindowHoveredFlags) -> bool {
        unsafe { sys::igIsWindowHovered(flags.bits() as i32) }
    }
    /// Returns the position of the current window (in screen space)
    #[doc(alias = "GetWindowPos")]
    pub fn window_pos(&self) -> [f32; 2] {
        let mut out = sys::ImVec2::zero();
        unsafe { sys::igGetWindowPos(&mut out) };
        out.into()
    }
    /// Returns the size of the current window
    #[doc(alias = "GetWindowPos")]
    pub fn window_size(&self) -> [f32; 2] {
        let mut out = sys::ImVec2::zero();
        unsafe { sys::igGetWindowSize(&mut out) };
        out.into()
    }
}

/// Builder for a window
#[derive(Debug)]
#[must_use]
pub struct Window<'a, T> {
    name: T,
    opened: Option<&'a mut bool>,
    flags: WindowFlags,
    pos: [f32; 2],
    pos_cond: Condition,
    pos_pivot: [f32; 2],
    size: [f32; 2],
    size_cond: Condition,
    size_constraints: Option<([f32; 2], [f32; 2])>,
    content_size: [f32; 2],
    collapsed: bool,
    collapsed_cond: Condition,
    focused: bool,
    bg_alpha: f32,
}

impl<'a, T: AsRef<str>> Window<'a, T> {
    /// Creates a new window builder with the given name
    pub fn new(name: T) -> Self {
        Window {
            name,
            opened: None,
            flags: WindowFlags::empty(),
            pos: [0.0, 0.0],
            pos_cond: Condition::Never,
            pos_pivot: [0.0, 0.0],
            size: [0.0, 0.0],
            size_cond: Condition::Never,
            size_constraints: None,
            content_size: [0.0, 0.0],
            collapsed: false,
            collapsed_cond: Condition::Never,
            focused: false,
            bg_alpha: f32::NAN,
        }
    }
    /// Enables the window close button, which sets the passed boolean to false when clicked
    #[inline]
    pub fn opened(mut self, opened: &'a mut bool) -> Self {
        self.opened = Some(opened);
        self
    }
    /// Replace current window flags with the given value
    #[inline]
    pub fn flags(mut self, flags: WindowFlags) -> Self {
        self.flags = flags;
        self
    }
    /// Sets the window position, which is applied based on the given condition value
    #[inline]
    pub fn position(mut self, position: [f32; 2], condition: Condition) -> Self {
        self.pos = position;
        self.pos_cond = condition;
        self
    }
    /// Sets the window position pivot, which can be used to adjust the alignment of the window
    /// relative to the position.
    ///
    /// For example, pass [0.5, 0.5] to center the window on the position.
    /// Does nothing if window position is not also set with `position()`.
    #[inline]
    pub fn position_pivot(mut self, pivot: [f32; 2]) -> Self {
        self.pos_pivot = pivot;
        self
    }
    /// Sets the window size, which is applied based on the given condition value
    #[inline]
    pub fn size(mut self, size: [f32; 2], condition: Condition) -> Self {
        self.size = size;
        self.size_cond = condition;
        self
    }
    /// Sets window size constraints.
    ///
    /// Use -1.0, -1.0 on either X or Y axis to preserve current size.
    #[inline]
    pub fn size_constraints(mut self, size_min: [f32; 2], size_max: [f32; 2]) -> Self {
        self.size_constraints = Some((size_min, size_max));
        self
    }
    /// Sets the window content size, which can be used to enforce scrollbars.
    ///
    /// Does not include window decorations (title bar, menu bar, etc.). Set one of the values to
    /// 0.0 to leave the size automatic.
    #[inline]
    pub fn content_size(mut self, size: [f32; 2]) -> Self {
        self.content_size = size;
        self
    }
    /// Sets the window collapse state, which is applied based on the given condition value
    #[inline]
    pub fn collapsed(mut self, collapsed: bool, condition: Condition) -> Self {
        self.collapsed = collapsed;
        self.collapsed_cond = condition;
        self
    }
    /// Sets the window focused state, which can be used to bring the window to front
    #[inline]
    pub fn focused(mut self, focused: bool) -> Self {
        self.focused = focused;
        self
    }
    /// Sets the background color alpha value.
    ///
    /// See also `draw_background`
    #[inline]
    pub fn bg_alpha(mut self, bg_alpha: f32) -> Self {
        self.bg_alpha = bg_alpha;
        self
    }
    /// Enables/disables the title bar.
    ///
    /// Enabled by default.
    #[inline]
    pub fn title_bar(mut self, value: bool) -> Self {
        self.flags.set(WindowFlags::NO_TITLE_BAR, !value);
        self
    }
    /// Enables/disables resizing with the lower-right grip.
    ///
    /// Enabled by default.
    #[inline]
    pub fn resizable(mut self, value: bool) -> Self {
        self.flags.set(WindowFlags::NO_RESIZE, !value);
        self
    }
    /// Enables/disables moving the window.
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
    /// Enables/disables collapsing the window by double-clicking it.
    ///
    /// Enabled by default.
    #[inline]
    pub fn collapsible(mut self, value: bool) -> Self {
        self.flags.set(WindowFlags::NO_COLLAPSE, !value);
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
    /// Enables/disables loading and saving of settings (e.g. from/to an .ini file).
    ///
    /// Enabled by default.
    #[inline]
    pub fn save_settings(mut self, value: bool) -> Self {
        self.flags.set(WindowFlags::NO_SAVED_SETTINGS, !value);
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
    /// When enabled, appends '*' to title without affecting the ID, as a convenience.
    ///
    /// Disabled by default.
    #[inline]
    pub fn unsaved_document(mut self, value: bool) -> Self {
        self.flags.set(WindowFlags::UNSAVED_DOCUMENT, value);
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
    /// Disable all window decorations.
    ///
    /// Shorthand for
    /// ```text
    /// .title_bar(false)
    /// .resizable(false)
    /// .scroll_bar(false)
    /// .collapsible(false)
    /// ```
    #[inline]
    pub fn no_decoration(mut self) -> Self {
        self.flags |= WindowFlags::NO_DECORATION;
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
    /// Creates a window and starts appending to it.
    ///
    /// Returns `Some(WindowToken)` if the window is visible. After content has been
    /// rendered, the token must be ended by calling `.end()`.
    ///
    /// Returns `None` if the window is not visible and no content should be rendered.
    #[must_use]
    pub fn begin<'ui>(self, ui: &Ui<'ui>) -> Option<WindowToken<'ui>> {
        if self.pos_cond != Condition::Never {
            unsafe {
                sys::igSetNextWindowPos(
                    self.pos.into(),
                    self.pos_cond as i32,
                    self.pos_pivot.into(),
                )
            };
        }
        if self.size_cond != Condition::Never {
            unsafe { sys::igSetNextWindowSize(self.size.into(), self.size_cond as i32) };
        }
        if let Some((size_min, size_max)) = self.size_constraints {
            // TODO: callback support
            unsafe {
                sys::igSetNextWindowSizeConstraints(
                    size_min.into(),
                    size_max.into(),
                    None,
                    ptr::null_mut(),
                )
            };
        }
        if self.content_size[0] != 0.0 || self.content_size[1] != 0.0 {
            unsafe { sys::igSetNextWindowContentSize(self.content_size.into()) };
        }
        if self.collapsed_cond != Condition::Never {
            unsafe { sys::igSetNextWindowCollapsed(self.collapsed, self.collapsed_cond as i32) };
        }
        if self.focused {
            unsafe { sys::igSetNextWindowFocus() };
        }
        if self.bg_alpha.is_finite() {
            unsafe { sys::igSetNextWindowBgAlpha(self.bg_alpha) };
        }
        let should_render = unsafe {
            sys::igBegin(
                ui.scratch_txt(self.name),
                self.opened
                    .map(|x| x as *mut bool)
                    .unwrap_or(ptr::null_mut()),
                self.flags.bits() as i32,
            )
        };
        if should_render {
            Some(WindowToken::new(ui))
        } else {
            unsafe { sys::igEnd() };
            None
        }
    }
    /// Creates a window and runs a closure to construct the contents.
    /// Returns the result of the closure, if it is called.
    ///
    /// Note: the closure is not called if no window content is visible (e.g. window is collapsed
    /// or fully clipped).
    pub fn build<R, F: FnOnce() -> R>(self, ui: &Ui<'_>, f: F) -> Option<R> {
        self.begin(ui).map(|_window| f())
    }
}

create_token!(
    /// Tracks a window that can be ended by calling `.end()`
    /// or by dropping.
    pub struct WindowToken<'ui>;

    /// Ends a window
    drop { sys::igEnd() }
);
