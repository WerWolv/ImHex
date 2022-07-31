use bitflags::bitflags;

use crate::sys;
use crate::Ui;

bitflags!(
    /// Flags for selectables
    #[repr(transparent)]
    pub struct SelectableFlags: u32 {
        /// Clicking this don't close parent popup window
        const DONT_CLOSE_POPUPS = sys::ImGuiSelectableFlags_DontClosePopups;
        /// Selectable frame can span all columns (text will still fit in current column)
        const SPAN_ALL_COLUMNS = sys::ImGuiSelectableFlags_SpanAllColumns;
        /// Generate press events on double clicks too
        const ALLOW_DOUBLE_CLICK = sys::ImGuiSelectableFlags_AllowDoubleClick;
        /// Cannot be selected, display greyed out text
        const DISABLED = sys::ImGuiSelectableFlags_Disabled;
        /// (WIP) Hit testing to allow subsequent widgets to overlap this one
        const ALLOW_ITEM_OVERLAP = sys::ImGuiSelectableFlags_AllowItemOverlap;
    }
);

/// Builder for a selectable widget.
#[derive(Copy, Clone, Debug)]
#[must_use]
pub struct Selectable<T> {
    label: T,
    selected: bool,
    flags: SelectableFlags,
    size: [f32; 2],
}

impl<T: AsRef<str>> Selectable<T> {
    /// Constructs a new selectable builder.
    #[inline]
    #[doc(alias = "Selectable")]
    pub fn new(label: T) -> Self {
        Selectable {
            label,
            selected: false,
            flags: SelectableFlags::empty(),
            size: [0.0, 0.0],
        }
    }
    /// Replaces all current settings with the given flags
    #[inline]
    pub fn flags(mut self, flags: SelectableFlags) -> Self {
        self.flags = flags;
        self
    }
    /// Sets the selected state of the selectable
    #[inline]
    pub fn selected(mut self, selected: bool) -> Self {
        self.selected = selected;
        self
    }
    /// Enables/disables closing parent popup window on click.
    ///
    /// Default: enabled
    #[inline]
    pub fn close_popups(mut self, value: bool) -> Self {
        self.flags.set(SelectableFlags::DONT_CLOSE_POPUPS, !value);
        self
    }
    /// Enables/disables full column span (text will still fit in the current column).
    ///
    /// Default: disabled
    #[inline]
    pub fn span_all_columns(mut self, value: bool) -> Self {
        self.flags.set(SelectableFlags::SPAN_ALL_COLUMNS, value);
        self
    }
    /// Enables/disables click event generation on double clicks.
    ///
    /// Default: disabled
    #[inline]
    pub fn allow_double_click(mut self, value: bool) -> Self {
        self.flags.set(SelectableFlags::ALLOW_DOUBLE_CLICK, value);
        self
    }
    /// Enables/disables the selectable.
    ///
    /// When disabled, it cannot be selected and the text uses the disabled text color.
    ///
    /// Default: disabled
    #[inline]
    pub fn disabled(mut self, value: bool) -> Self {
        self.flags.set(SelectableFlags::DISABLED, value);
        self
    }
    /// Sets the size of the selectable.
    ///
    /// For the X axis:
    ///
    /// - `> 0.0`: use given width
    /// - `= 0.0`: use remaining width
    ///
    /// For the Y axis:
    ///
    /// - `> 0.0`: use given height
    /// - `= 0.0`: use label height
    #[inline]
    pub fn size(mut self, size: [f32; 2]) -> Self {
        self.size = size;
        self
    }
    /// Builds the selectable.
    ///
    /// Returns true if the selectable was clicked.
    pub fn build(self, ui: &Ui<'_>) -> bool {
        unsafe {
            sys::igSelectable_Bool(
                ui.scratch_txt(self.label),
                self.selected,
                self.flags.bits() as i32,
                self.size.into(),
            )
        }
    }

    /// Builds the selectable using a mutable reference to selected state.
    pub fn build_with_ref(self, ui: &Ui<'_>, selected: &mut bool) -> bool {
        if self.selected(*selected).build(ui) {
            *selected = !*selected;
            true
        } else {
            false
        }
    }
}
