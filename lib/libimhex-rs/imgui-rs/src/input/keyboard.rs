use crate::sys;
use crate::Ui;

/// A key identifier
#[repr(u32)]
#[derive(Copy, Clone, Debug, Hash, Eq, PartialEq)]
pub enum Key {
    Tab = sys::ImGuiKey_Tab,
    LeftArrow = sys::ImGuiKey_LeftArrow,
    RightArrow = sys::ImGuiKey_RightArrow,
    UpArrow = sys::ImGuiKey_UpArrow,
    DownArrow = sys::ImGuiKey_DownArrow,
    PageUp = sys::ImGuiKey_PageUp,
    PageDown = sys::ImGuiKey_PageDown,
    Home = sys::ImGuiKey_Home,
    End = sys::ImGuiKey_End,
    Insert = sys::ImGuiKey_Insert,
    Delete = sys::ImGuiKey_Delete,
    Backspace = sys::ImGuiKey_Backspace,
    Space = sys::ImGuiKey_Space,
    Enter = sys::ImGuiKey_Enter,
    Escape = sys::ImGuiKey_Escape,
    KeyPadEnter = sys::ImGuiKey_KeyPadEnter,
    A = sys::ImGuiKey_A,
    C = sys::ImGuiKey_C,
    V = sys::ImGuiKey_V,
    X = sys::ImGuiKey_X,
    Y = sys::ImGuiKey_Y,
    Z = sys::ImGuiKey_Z,
}

impl Key {
    /// All possible `Key` variants
    pub const VARIANTS: [Key; Key::COUNT] = [
        Key::Tab,
        Key::LeftArrow,
        Key::RightArrow,
        Key::UpArrow,
        Key::DownArrow,
        Key::PageUp,
        Key::PageDown,
        Key::Home,
        Key::End,
        Key::Insert,
        Key::Delete,
        Key::Backspace,
        Key::Space,
        Key::Enter,
        Key::Escape,
        Key::KeyPadEnter,
        Key::A,
        Key::C,
        Key::V,
        Key::X,
        Key::Y,
        Key::Z,
    ];
    /// Total count of `Key` variants
    pub const COUNT: usize = sys::ImGuiKey_COUNT as usize;
}

#[test]
fn test_key_variants() {
    for (idx, &value) in Key::VARIANTS.iter().enumerate() {
        assert_eq!(idx, value as usize);
    }
}

/// Target widget selection for keyboard focus
#[derive(Copy, Clone, Debug, Hash, Eq, PartialEq)]
pub enum FocusedWidget {
    /// Previous widget
    Previous,
    /// Next widget
    Next,
    /// Widget using a relative positive offset (0 is the next widget).
    ///
    /// Use this to access sub components of a multiple component widget.
    Offset(u32),
}

impl FocusedWidget {
    #[inline]
    fn as_offset(self) -> i32 {
        match self {
            FocusedWidget::Previous => -1,
            FocusedWidget::Next => 0,
            FocusedWidget::Offset(offset) => offset as i32,
        }
    }
}

/// # Input: Keyboard
impl<'ui> Ui<'ui> {
    /// Returns the key index of the given key identifier.
    ///
    /// Equivalent to indexing the Io struct `key_map` field: `ui.io().key_map[key]`
    #[inline]
    #[doc(alias = "GetKeyIndex")]
    fn key_index(&self, key: Key) -> i32 {
        unsafe { sys::igGetKeyIndex(key as i32) }
    }
    /// Returns true if the key is being held.
    ///
    /// Equivalent to indexing the Io struct `keys_down` field: `ui.io().keys_down[key_index]`
    #[inline]
    #[doc(alias = "IsKeyDown")]
    pub fn is_key_down(&self, key: Key) -> bool {
        let key_index = self.key_index(key);
        self.is_key_index_down(key_index)
    }

    /// Same as [`is_key_down`](Self::is_key_down) but takes a key index. The meaning of
    /// index is defined by your backend implementation.
    #[inline]
    #[doc(alias = "IsKeyDown")]
    pub fn is_key_index_down(&self, key_index: i32) -> bool {
        unsafe { sys::igIsKeyDown(key_index) }
    }

    /// Returns true if the key was pressed (went from !down to down).
    ///
    /// Affected by key repeat settings (`io.key_repeat_delay`, `io.key_repeat_rate`)
    #[inline]
    #[doc(alias = "IsKeyPressed")]
    pub fn is_key_pressed(&self, key: Key) -> bool {
        let key_index = self.key_index(key);
        self.is_key_index_pressed(key_index)
    }

    /// Same as [`is_key_pressed`](Self::is_key_pressed) but takes a key index.
    ///
    /// The meaning of index is defined by your backend
    /// implementation.
    #[inline]
    #[doc(alias = "IsKeyPressed")]
    pub fn is_key_index_pressed(&self, key_index: i32) -> bool {
        unsafe { sys::igIsKeyPressed(key_index, true) }
    }

    /// Returns true if the key was pressed (went from !down to down).
    ///
    /// Is **not** affected by key repeat settings (`io.key_repeat_delay`, `io.key_repeat_rate`)
    #[inline]
    #[doc(alias = "IsKeyPressed")]
    pub fn is_key_pressed_no_repeat(&self, key: Key) -> bool {
        let key_index = self.key_index(key);
        self.is_key_index_pressed_no_repeat(key_index)
    }

    /// Same as [`is_key_pressed_no_repeat`](Self::is_key_pressed_no_repeat)
    /// but takes a key index.
    ///
    /// The meaning of index is defined by your backend
    /// implementation.
    #[inline]
    #[doc(alias = "IsKeyPressed")]
    pub fn is_key_index_pressed_no_repeat(&self, key_index: i32) -> bool {
        unsafe { sys::igIsKeyPressed(key_index, false) }
    }

    /// Returns true if the key was released (went from down to !down)
    #[inline]
    #[doc(alias = "IsKeyReleased")]
    pub fn is_key_released(&self, key: Key) -> bool {
        let key_index = self.key_index(key);
        self.is_key_index_released(key_index)
    }

    /// Same as [`is_key_released`](Self::is_key_released) but takes a key index.
    ///
    /// The meaning of index is defined by your backend
    /// implementation.
    #[inline]
    #[doc(alias = "IsKeyReleased")]
    pub fn is_key_index_released(&self, key_index: i32) -> bool {
        unsafe { sys::igIsKeyReleased(key_index) }
    }

    /// Returns a count of key presses using the given repeat rate/delay settings.
    ///
    /// Usually returns 0 or 1, but might be >1 if `rate` is small enough that `io.delta_time` >
    /// `rate`.
    #[inline]
    #[doc(alias = "GetKeyPressedAmount")]
    pub fn key_pressed_amount(&self, key: Key, repeat_delay: f32, rate: f32) -> u32 {
        let key_index = self.key_index(key);
        self.key_index_pressed_amount(key_index, repeat_delay, rate)
    }

    #[inline]
    #[doc(alias = "GetKeyPressedAmount")]
    pub fn key_index_pressed_amount(&self, key_index: i32, repeat_delay: f32, rate: f32) -> u32 {
        unsafe { sys::igGetKeyPressedAmount(key_index, repeat_delay, rate) as u32 }
    }

    /// Focuses keyboard on the next widget.
    ///
    /// This is the equivalent to [set_keyboard_focus_here_with_offset](Self::set_keyboard_focus_here_with_offset)
    /// with `target_widget` set to `FocusedWidget::Next`.
    #[inline]
    #[doc(alias = "SetKeyboardFocusHere")]
    pub fn set_keyboard_focus_here(&self) {
        self.set_keyboard_focus_here_with_offset(FocusedWidget::Next);
    }

    /// Focuses keyboard on a widget relative to current position.
    #[inline]
    #[doc(alias = "SetKeyboardFocusHere")]
    pub fn set_keyboard_focus_here_with_offset(&self, target_widget: FocusedWidget) {
        unsafe {
            sys::igSetKeyboardFocusHere(target_widget.as_offset());
        }
    }
}
