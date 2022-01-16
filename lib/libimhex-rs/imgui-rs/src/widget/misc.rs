use bitflags::bitflags;
use std::ops::{BitAnd, BitAndAssign, BitOrAssign, Not};

use crate::sys;
use crate::{Direction, Ui};

bitflags!(
    /// Flags for invisible buttons
    #[repr(transparent)]
    pub struct ButtonFlags: u32 {
        /// React on left mouse button
        const MOUSE_BUTTON_LEFT = sys::ImGuiButtonFlags_MouseButtonLeft;
        /// React on right mouse button
        const MOUSE_BUTTON_RIGHT = sys::ImGuiButtonFlags_MouseButtonRight;
        /// React on middle mouse button
        const MOUSE_BUTTON_MIDDLE = sys::ImGuiButtonFlags_MouseButtonMiddle;
    }
);

/// # Widgets: Miscellaneous
impl<'ui> Ui<'ui> {
    /// Renders a clickable button.
    ///
    /// Returns true if this button was clicked.
    ///
    /// This is the equivalent of [button_with_size](Self::button_with_size)
    /// with `size` set to `[0.0, 0.0]`, which will size the button to the
    /// label's width in the current style.
    /// the current style.
    #[doc(alias = "Button")]
    pub fn button(&self, label: impl AsRef<str>) -> bool {
        self.button_with_size(label, [0.0, 0.0])
    }

    /// Renders a clickable button.
    ///
    /// Returns true if this button was clicked.
    ///
    /// Setting `size` as `[0.0, 0.0]` will size the button to the label's width in
    /// the current style.
    #[doc(alias = "Button")]
    pub fn button_with_size(&self, label: impl AsRef<str>, size: [f32; 2]) -> bool {
        unsafe { sys::igButton(self.scratch_txt(label), size.into()) }
    }
    /// Renders a small clickable button that is easy to embed in text.
    ///
    /// Returns true if this button was clicked.
    #[doc(alias = "SmallButton")]
    pub fn small_button(&self, label: impl AsRef<str>) -> bool {
        unsafe { sys::igSmallButton(self.scratch_txt(label)) }
    }
    /// Renders a widget with button behaviour without the visual look.
    ///
    /// Returns true if this button was clicked.
    #[doc(alias = "InvisibleButton")]
    pub fn invisible_button(&self, id: impl AsRef<str>, size: [f32; 2]) -> bool {
        unsafe { sys::igInvisibleButton(self.scratch_txt(id), size.into(), 0) }
    }
    /// Renders a widget with button behaviour without the visual look.
    ///
    /// Returns true if this button was clicked.
    #[doc(alias = "InvisibleButton")]
    pub fn invisible_button_flags(
        &self,
        id: impl AsRef<str>,
        size: [f32; 2],
        flags: ButtonFlags,
    ) -> bool {
        unsafe { sys::igInvisibleButton(self.scratch_txt(id), size.into(), flags.bits() as i32) }
    }
    /// Renders a square button with an arrow shape.
    ///
    /// Returns true if this button was clicked.
    #[doc(alias = "ArrowButton")]
    pub fn arrow_button(&self, id: impl AsRef<str>, direction: Direction) -> bool {
        unsafe { sys::igArrowButton(self.scratch_txt(id), direction as i32) }
    }
    /// Renders a simple checkbox.
    ///
    /// Returns true if this checkbox was clicked.
    #[doc(alias = "Checkbox")]
    pub fn checkbox(&self, label: impl AsRef<str>, value: &mut bool) -> bool {
        unsafe { sys::igCheckbox(self.scratch_txt(label), value as *mut bool) }
    }
    /// Renders a checkbox suitable for toggling bit flags using a mask.
    ///
    /// Returns true if this checkbox was clicked.
    pub fn checkbox_flags<T>(&self, label: impl AsRef<str>, flags: &mut T, mask: T) -> bool
    where
        T: Copy + PartialEq + BitOrAssign + BitAndAssign + BitAnd<Output = T> + Not<Output = T>,
    {
        let mut value = *flags & mask == mask;
        let pressed = self.checkbox(label, &mut value);
        if pressed {
            if value {
                *flags |= mask;
            } else {
                *flags &= !mask;
            }
        }
        pressed
    }
    /// Renders a simple radio button.
    ///
    /// Returns true if this radio button was clicked.
    #[doc(alias = "RadioButtonBool")]
    pub fn radio_button_bool(&self, label: impl AsRef<str>, active: bool) -> bool {
        unsafe { sys::igRadioButton_Bool(self.scratch_txt(label), active) }
    }
    /// Renders a radio button suitable for choosing an arbitrary value.
    ///
    /// Returns true if this radio button was clicked.
    #[doc(alias = "RadioButtonBool")]
    pub fn radio_button<T>(&self, label: impl AsRef<str>, value: &mut T, button_value: T) -> bool
    where
        T: Copy + PartialEq,
    {
        let pressed = self.radio_button_bool(label, *value == button_value);
        if pressed {
            *value = button_value;
        }
        pressed
    }
    /// Renders a small circle and keeps the cursor on the same line
    #[doc(alias = "Bullet")]
    pub fn bullet(&self) {
        unsafe { sys::igBullet() };
    }
}
