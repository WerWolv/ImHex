use std::mem;
use std::os::raw::{c_char, c_void};
use std::ptr;
use std::thread;

use crate::context::Context;
use crate::fonts::atlas::FontId;
use crate::internal::RawCast;
use crate::style::{StyleColor, StyleVar};
use crate::sys;
use crate::{Id, Ui};

/// # Parameter stacks (shared)
impl<'ui> Ui<'ui> {
    /// Switches to the given font by pushing it to the font stack.
    ///
    /// Returns a `FontStackToken` that must be popped by calling `.pop()`
    ///
    /// # Panics
    ///
    /// Panics if the font atlas does not contain the given font
    ///
    /// # Examples
    ///
    /// ```no_run
    /// # use imgui::*;
    /// # let mut ctx = Context::create();
    /// # let font_data_sources = [];
    /// // At initialization time
    /// let my_custom_font = ctx.fonts().add_font(&font_data_sources);
    /// # let ui = ctx.frame();
    /// // During UI construction
    /// let font = ui.push_font(my_custom_font);
    /// ui.text("I use the custom font!");
    /// font.pop();
    /// ```
    #[doc(alias = "PushFont")]
    pub fn push_font(&self, id: FontId) -> FontStackToken<'_> {
        let fonts = self.fonts();
        let font = fonts
            .get_font(id)
            .expect("Font atlas did not contain the given font");
        unsafe { sys::igPushFont(font.raw() as *const _ as *mut _) };
        FontStackToken::new(self)
    }
    /// Changes a style color by pushing a change to the color stack.
    ///
    /// Returns a `ColorStackToken` that must be popped by calling `.pop()`
    ///
    /// # Examples
    ///
    /// ```no_run
    /// # use imgui::*;
    /// # let mut ctx = Context::create();
    /// # let ui = ctx.frame();
    /// const RED: [f32; 4] = [1.0, 0.0, 0.0, 1.0];
    /// let color = ui.push_style_color(StyleColor::Text, RED);
    /// ui.text("I'm red!");
    /// color.pop();
    /// ```
    #[doc(alias = "PushStyleColorVec4")]
    pub fn push_style_color(
        &self,
        style_color: StyleColor,
        color: [f32; 4],
    ) -> ColorStackToken<'_> {
        unsafe { sys::igPushStyleColor_Vec4(style_color as i32, color.into()) };
        ColorStackToken::new(self)
    }

    /// Changes style colors by pushing several changes to the color stack.
    ///
    /// Returns a `ColorStackToken` that must be popped by calling `.pop()`
    ///
    /// # Examples
    ///
    /// ```no_run
    /// # use imgui::*;
    /// # let mut ctx = Context::create();
    /// # let ui = ctx.frame();
    /// const RED: [f32; 4] = [1.0, 0.0, 0.0, 1.0];
    /// const GREEN: [f32; 4] = [0.0, 1.0, 0.0, 1.0];
    /// let colors = ui.push_style_colors(&[
    ///     (StyleColor::Text, RED),
    ///     (StyleColor::TextDisabled, GREEN),
    /// ]);
    /// ui.text("I'm red!");
    /// ui.text_disabled("I'm green!");
    /// colors.pop(&ui);
    /// ```
    #[deprecated = "deprecated in 0.7.0. Use `push_style_color` multiple times for similar effect."]
    pub fn push_style_colors<'a, I>(&self, style_colors: I) -> MultiColorStackToken
    where
        I: IntoIterator<Item = &'a (StyleColor, [f32; 4])>,
    {
        let mut count = 0;
        for &(style_color, color) in style_colors {
            unsafe { sys::igPushStyleColor_Vec4(style_color as i32, color.into()) };
            count += 1;
        }
        MultiColorStackToken {
            count,
            ctx: self.ctx,
        }
    }
    /// Changes a style variable by pushing a change to the style stack.
    ///
    /// Returns a `StyleStackToken` that can be popped by calling `.end()`
    /// or by allowing to drop.
    ///
    /// # Examples
    ///
    /// ```no_run
    /// # use imgui::*;
    /// # let mut ctx = Context::create();
    /// # let ui = ctx.frame();
    /// let style = ui.push_style_var(StyleVar::Alpha(0.2));
    /// ui.text("I'm transparent!");
    /// style.pop();
    /// ```
    #[doc(alias = "PushStyleVar")]
    pub fn push_style_var(&self, style_var: StyleVar) -> StyleStackToken<'_> {
        unsafe { push_style_var(style_var) };
        StyleStackToken::new(self)
    }
    /// Changes style variables by pushing several changes to the style stack.
    ///
    /// Returns a `StyleStackToken` that must be popped by calling `.pop()`
    ///
    /// # Examples
    ///
    /// ```no_run
    /// # use imgui::*;
    /// # let mut ctx = Context::create();
    /// # let ui = ctx.frame();
    /// let styles = ui.push_style_vars(&[
    ///     StyleVar::Alpha(0.2),
    ///     StyleVar::ItemSpacing([50.0, 50.0])
    /// ]);
    /// ui.text("We're transparent...");
    /// ui.text("...with large spacing as well");
    /// styles.pop(&ui);
    /// ```
    #[deprecated = "deprecated in 0.7.0. Use `push_style_var` multiple times for similar effect."]
    pub fn push_style_vars<'a, I>(&self, style_vars: I) -> MultiStyleStackToken
    where
        I: IntoIterator<Item = &'a StyleVar>,
    {
        let mut count = 0;
        for &style_var in style_vars {
            unsafe { push_style_var(style_var) };
            count += 1;
        }
        MultiStyleStackToken {
            count,
            ctx: self.ctx,
        }
    }
}

create_token!(
    /// Tracks a font pushed to the font stack that can be popped by calling `.end()`
    /// or by dropping.
    pub struct FontStackToken<'ui>;

    /// Pops a change from the font stack.
    drop { sys::igPopFont() }
);

impl FontStackToken<'_> {
    /// Pops a change from the font stack.
    pub fn pop(self) {
        self.end()
    }
}

create_token!(
    /// Tracks a color pushed to the color stack that can be popped by calling `.end()`
    /// or by dropping.
    pub struct ColorStackToken<'ui>;

    /// Pops a change from the color stack.
    drop { sys::igPopStyleColor(1) }
);

impl ColorStackToken<'_> {
    /// Pops a change from the color stack.
    pub fn pop(self) {
        self.end()
    }
}

/// Tracks one or more changes pushed to the color stack that must be popped by calling `.pop()`
#[must_use]
pub struct MultiColorStackToken {
    count: usize,
    ctx: *const Context,
}

impl MultiColorStackToken {
    /// Pops changes from the color stack
    #[doc(alias = "PopStyleColor")]
    pub fn pop(mut self, _: &Ui<'_>) {
        self.ctx = ptr::null();
        unsafe { sys::igPopStyleColor(self.count as i32) };
    }
}

impl Drop for MultiColorStackToken {
    fn drop(&mut self) {
        if !self.ctx.is_null() && !thread::panicking() {
            panic!("A ColorStackToken was leaked. Did you call .pop()?");
        }
    }
}

create_token!(
    /// Tracks a style pushed to the style stack that can be popped by calling `.end()`
    /// or by dropping.
    pub struct StyleStackToken<'ui>;

    /// Pops a change from the style stack.
    drop { sys::igPopStyleVar(1) }
);

impl StyleStackToken<'_> {
    /// Pops a change from the style stack.
    pub fn pop(self) {
        self.end()
    }
}

/// Tracks one or more changes pushed to the style stack that must be popped by calling `.pop()`
#[must_use]
pub struct MultiStyleStackToken {
    count: usize,
    ctx: *const Context,
}

impl MultiStyleStackToken {
    /// Pops changes from the style stack
    #[doc(alias = "PopStyleVar")]
    pub fn pop(mut self, _: &Ui<'_>) {
        self.ctx = ptr::null();
        unsafe { sys::igPopStyleVar(self.count as i32) };
    }
}

impl Drop for MultiStyleStackToken {
    fn drop(&mut self) {
        if !self.ctx.is_null() && !thread::panicking() {
            panic!("A StyleStackToken was leaked. Did you call .pop()?");
        }
    }
}

#[inline]
unsafe fn push_style_var(style_var: StyleVar) {
    use crate::style::StyleVar::*;
    use crate::sys::{igPushStyleVar_Float, igPushStyleVar_Vec2};
    match style_var {
        Alpha(v) => igPushStyleVar_Float(sys::ImGuiStyleVar_Alpha as i32, v),
        WindowPadding(v) => igPushStyleVar_Vec2(sys::ImGuiStyleVar_WindowPadding as i32, v.into()),
        WindowRounding(v) => igPushStyleVar_Float(sys::ImGuiStyleVar_WindowRounding as i32, v),
        WindowBorderSize(v) => igPushStyleVar_Float(sys::ImGuiStyleVar_WindowBorderSize as i32, v),
        WindowMinSize(v) => igPushStyleVar_Vec2(sys::ImGuiStyleVar_WindowMinSize as i32, v.into()),
        WindowTitleAlign(v) => {
            igPushStyleVar_Vec2(sys::ImGuiStyleVar_WindowTitleAlign as i32, v.into())
        }
        ChildRounding(v) => igPushStyleVar_Float(sys::ImGuiStyleVar_ChildRounding as i32, v),
        ChildBorderSize(v) => igPushStyleVar_Float(sys::ImGuiStyleVar_ChildBorderSize as i32, v),
        PopupRounding(v) => igPushStyleVar_Float(sys::ImGuiStyleVar_PopupRounding as i32, v),
        PopupBorderSize(v) => igPushStyleVar_Float(sys::ImGuiStyleVar_PopupBorderSize as i32, v),
        FramePadding(v) => igPushStyleVar_Vec2(sys::ImGuiStyleVar_FramePadding as i32, v.into()),
        FrameRounding(v) => igPushStyleVar_Float(sys::ImGuiStyleVar_FrameRounding as i32, v),
        FrameBorderSize(v) => igPushStyleVar_Float(sys::ImGuiStyleVar_FrameBorderSize as i32, v),
        ItemSpacing(v) => igPushStyleVar_Vec2(sys::ImGuiStyleVar_ItemSpacing as i32, v.into()),
        ItemInnerSpacing(v) => {
            igPushStyleVar_Vec2(sys::ImGuiStyleVar_ItemInnerSpacing as i32, v.into())
        }
        IndentSpacing(v) => igPushStyleVar_Float(sys::ImGuiStyleVar_IndentSpacing as i32, v),
        ScrollbarSize(v) => igPushStyleVar_Float(sys::ImGuiStyleVar_ScrollbarSize as i32, v),
        ScrollbarRounding(v) => {
            igPushStyleVar_Float(sys::ImGuiStyleVar_ScrollbarRounding as i32, v)
        }
        GrabMinSize(v) => igPushStyleVar_Float(sys::ImGuiStyleVar_GrabMinSize as i32, v),
        GrabRounding(v) => igPushStyleVar_Float(sys::ImGuiStyleVar_GrabRounding as i32, v),
        TabRounding(v) => igPushStyleVar_Float(sys::ImGuiStyleVar_TabRounding as i32, v),
        ButtonTextAlign(v) => {
            igPushStyleVar_Vec2(sys::ImGuiStyleVar_ButtonTextAlign as i32, v.into())
        }
        SelectableTextAlign(v) => {
            igPushStyleVar_Vec2(sys::ImGuiStyleVar_SelectableTextAlign as i32, v.into())
        }
    }
}

/// # Parameter stacks (current window)
impl<'ui> Ui<'ui> {
    /// Changes the item width by pushing a change to the item width stack.
    ///
    /// Returns an `ItemWidthStackToken` that may be popped by calling `.pop()`
    ///
    /// - `> 0.0`: width is `item_width` pixels
    /// - `= 0.0`: default to ~2/3 of window width
    /// - `< 0.0`: `item_width` pixels relative to the right of window (-1.0 always aligns width to
    /// the right side)
    #[doc(alias = "PushItemWith")]
    pub fn push_item_width(&self, item_width: f32) -> ItemWidthStackToken {
        unsafe { sys::igPushItemWidth(item_width) };
        ItemWidthStackToken { _ctx: self.ctx }
    }
    /// Sets the width of the next item.
    ///
    /// - `> 0.0`: width is `item_width` pixels
    /// - `= 0.0`: default to ~2/3 of window width
    /// - `< 0.0`: `item_width` pixels relative to the right of window (-1.0 always aligns width to
    /// the right side)
    #[doc(alias = "SetNextItemWidth")]
    pub fn set_next_item_width(&self, item_width: f32) {
        unsafe { sys::igSetNextItemWidth(item_width) };
    }
    /// Returns the width of the item given the pushed settings and the current cursor position.
    ///
    /// This is NOT necessarily the width of last item.
    #[doc(alias = "CalcItemWidth")]
    pub fn calc_item_width(&self) -> f32 {
        unsafe { sys::igCalcItemWidth() }
    }

    /// Changes the text wrapping position to the end of window (or column), which
    /// is generally the default.
    ///
    /// This is the same as calling [push_text_wrap_pos_with_pos](Self::push_text_wrap_pos_with_pos)
    /// with `wrap_pos_x` set to 0.0.
    ///
    /// Returns a `TextWrapPosStackToken` that may be popped by calling `.pop()`
    #[doc(alias = "PushTextWrapPos")]
    pub fn push_text_wrap_pos(&self) -> TextWrapPosStackToken {
        self.push_text_wrap_pos_with_pos(0.0)
    }

    /// Changes the text wrapping position by pushing a change to the text wrapping position stack.
    ///
    /// Returns a `TextWrapPosStackToken` that may be popped by calling `.pop()`
    ///
    /// - `> 0.0`: wrap at `wrap_pos_x` position in window local space
    /// - `= 0.0`: wrap to end of window (or column)
    /// - `< 0.0`: no wrapping
    #[doc(alias = "PushTextWrapPos")]
    pub fn push_text_wrap_pos_with_pos(&self, wrap_pos_x: f32) -> TextWrapPosStackToken {
        unsafe { sys::igPushTextWrapPos(wrap_pos_x) };
        TextWrapPosStackToken { _ctx: self.ctx }
    }

    /// Changes an item flag by pushing a change to the item flag stack.
    ///
    /// Returns a `ItemFlagsStackToken` that may be popped by calling `.pop()`
    #[doc(alias = "PushItemFlag")]
    pub fn push_item_flag(&self, item_flag: ItemFlag) -> ItemFlagsStackToken {
        use self::ItemFlag::*;
        match item_flag {
            AllowKeyboardFocus(v) => unsafe { sys::igPushAllowKeyboardFocus(v) },
            ButtonRepeat(v) => unsafe { sys::igPushButtonRepeat(v) },
        }
        ItemFlagsStackToken {
            discriminant: mem::discriminant(&item_flag),
            _ctx: self.ctx,
        }
    }
}

/// A temporary change in item flags
#[derive(Copy, Clone, Debug, PartialEq)]
pub enum ItemFlag {
    AllowKeyboardFocus(bool),
    ButtonRepeat(bool),
}

pub struct ItemWidthStackToken {
    _ctx: *const Context,
}

impl ItemWidthStackToken {
    /// Pops a change from the item width stack
    #[doc(alias = "PopItemWidth")]
    pub fn pop(mut self, _: &Ui<'_>) {
        self._ctx = ptr::null();
        unsafe { sys::igPopItemWidth() };
    }
}

/// Tracks a change pushed to the text wrap position stack
pub struct TextWrapPosStackToken {
    _ctx: *const Context,
}

impl TextWrapPosStackToken {
    /// Pops a change from the text wrap position stack
    #[doc(alias = "PopTextWrapPos")]
    pub fn pop(mut self, _: &Ui<'_>) {
        self._ctx = ptr::null();
        unsafe { sys::igPopTextWrapPos() };
    }
}

/// Tracks a change pushed to the item flags stack
pub struct ItemFlagsStackToken {
    discriminant: mem::Discriminant<ItemFlag>,
    _ctx: *const Context,
}

impl ItemFlagsStackToken {
    /// Pops a change from the item flags stack

    #[doc(alias = "PopAllowKeyboardFocus", alias = "PopButtonRepeat")]
    pub fn pop(mut self, _: &Ui<'_>) {
        self._ctx = ptr::null();
        const ALLOW_KEYBOARD_FOCUS: ItemFlag = ItemFlag::AllowKeyboardFocus(true);
        const BUTTON_REPEAT: ItemFlag = ItemFlag::ButtonRepeat(true);

        if self.discriminant == mem::discriminant(&ALLOW_KEYBOARD_FOCUS) {
            unsafe { sys::igPopAllowKeyboardFocus() };
        } else if self.discriminant == mem::discriminant(&BUTTON_REPEAT) {
            unsafe { sys::igPopButtonRepeat() };
        } else {
            unreachable!();
        }
    }
}

create_token!(
    /// Tracks an ID pushed to the ID stack that can be popped by calling `.pop()`
    /// or by dropping.
    pub struct IdStackToken<'ui>;

    /// Pops a change from the ID stack
    drop { sys::igPopID() }
);

impl IdStackToken<'_> {
    /// Pops a change from the ID stack
    pub fn pop(self) {
        self.end()
    }
}

/// # ID stack
impl<'ui> Ui<'ui> {
    /// Pushes an identifier to the ID stack.
    ///
    /// Returns an `IdStackToken` that can be popped by calling `.end()`
    /// or by dropping manually.
    #[doc(alias = "PushId")]
    pub fn push_id<'a, I: Into<Id<'a>>>(&self, id: I) -> IdStackToken<'ui> {
        let id = id.into();

        unsafe {
            match id {
                Id::Int(i) => sys::igPushID_Int(i),
                Id::Str(s) => {
                    let start = s.as_ptr() as *const c_char;
                    let end = start.add(s.len());
                    sys::igPushID_StrStr(start, end)
                }
                Id::Ptr(p) => sys::igPushID_Ptr(p as *const c_void),
            }
        }
        IdStackToken::new(self)
    }
}
