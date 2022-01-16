use bitflags::bitflags;
use std::os::raw::c_void;

use crate::internal::DataTypeKind;
use crate::sys;
use crate::Ui;

bitflags!(
    /// Flags for sliders
    #[repr(transparent)]
    pub struct SliderFlags: u32 {
        /// Clamp value to min/max bounds when input manually with CTRL+Click.
        ///
        /// By default CTRL+click allows going out of bounds.
        const ALWAYS_CLAMP = sys::ImGuiSliderFlags_AlwaysClamp;
        /// Make the widget logarithmic instead of linear
        const LOGARITHMIC = sys::ImGuiSliderFlags_Logarithmic;
        /// Disable rounding underlying value to match precision of the display format string
        const NO_ROUND_TO_FORMAT = sys::ImGuiSliderFlags_NoRoundToFormat;
        /// Disable CTRL+Click or Enter key allowing to input text directly into the widget
        const NO_INPUT = sys::ImGuiSliderFlags_NoInput;
    }
);

/// Builder for a slider widget.
#[derive(Copy, Clone, Debug)]
#[must_use]
pub struct Slider<Label, Data, Format = &'static str> {
    label: Label,
    min: Data,
    max: Data,
    display_format: Option<Format>,
    flags: SliderFlags,
}

impl<T: AsRef<str>, K: DataTypeKind> Slider<T, K> {
    /// Constructs a new slider builder with the given range.
    #[doc(alias = "SliderScalar", alias = "SliderScalarN")]
    pub fn new(label: T, min: K, max: K) -> Self {
        Slider {
            label,
            min,
            max,
            display_format: None,
            flags: SliderFlags::empty(),
        }
    }
}

impl<Label, Data, Format> Slider<Label, Data, Format>
where
    Label: AsRef<str>,
    Data: DataTypeKind,
    Format: AsRef<str>,
{
    /// Sets the range inclusively, such that both values given
    /// are valid values which the slider can be dragged to.
    ///
    /// ```rust
    /// # use imgui::im_str;
    /// imgui::Slider::new(im_str!("Example"), i8::MIN, i8::MAX)
    ///     .range(4, 8)
    ///     // Remember to call .build(&ui)
    ///     ;
    /// ```
    ///
    /// It is safe, though up to C++ Dear ImGui, on how to handle when
    /// `min > max`.
    #[inline]
    pub fn range(mut self, min: Data, max: Data) -> Self {
        self.min = min;
        self.max = max;
        self
    }
    /// Sets the display format using *a C-style printf string*
    #[inline]
    pub fn display_format<Format2: AsRef<str>>(
        self,
        display_format: Format2,
    ) -> Slider<Label, Data, Format2> {
        Slider {
            label: self.label,
            min: self.min,
            max: self.max,
            display_format: Some(display_format),
            flags: self.flags,
        }
    }
    /// Replaces all current settings with the given flags
    #[inline]
    pub fn flags(mut self, flags: SliderFlags) -> Self {
        self.flags = flags;
        self
    }
    /// Builds a slider that is bound to the given value.
    ///
    /// Returns true if the slider value was changed.
    pub fn build(self, ui: &Ui<'_>, value: &mut Data) -> bool {
        unsafe {
            let (label, display_format) = ui.scratch_txt_with_opt(self.label, self.display_format);

            sys::igSliderScalar(
                label,
                Data::KIND as i32,
                value as *mut Data as *mut c_void,
                &self.min as *const Data as *const c_void,
                &self.max as *const Data as *const c_void,
                display_format,
                self.flags.bits() as i32,
            )
        }
    }
    /// Builds a horizontal array of multiple sliders attached to the given slice.
    ///
    /// Returns true if any slider value was changed.
    pub fn build_array(self, ui: &Ui<'_>, values: &mut [Data]) -> bool {
        unsafe {
            let (label, display_format) = ui.scratch_txt_with_opt(self.label, self.display_format);

            sys::igSliderScalarN(
                label,
                Data::KIND as i32,
                values.as_mut_ptr() as *mut c_void,
                values.len() as i32,
                &self.min as *const Data as *const c_void,
                &self.max as *const Data as *const c_void,
                display_format,
                self.flags.bits() as i32,
            )
        }
    }
}

/// Builder for a vertical slider widget.
#[derive(Clone, Debug)]
#[must_use]
pub struct VerticalSlider<Label, Data, Format = &'static str> {
    label: Label,
    size: [f32; 2],
    min: Data,
    max: Data,
    display_format: Option<Format>,
    flags: SliderFlags,
}

impl<Label, Data> VerticalSlider<Label, Data>
where
    Label: AsRef<str>,
    Data: DataTypeKind,
{
    /// Constructs a new vertical slider builder with the given size and range.
    ///
    /// ```rust
    /// # use imgui::im_str;
    /// imgui::VerticalSlider::new(im_str!("Example"), [20.0, 20.0], i8::MIN, i8::MAX)
    ///     .range(4, 8)
    ///     // Remember to call .build(&ui)
    ///     ;
    /// ```
    ///
    /// It is safe, though up to C++ Dear ImGui, on how to handle when
    /// `min > max`.
    #[doc(alias = "VSliderScalar")]
    pub fn new(label: Label, size: [f32; 2], min: Data, max: Data) -> Self {
        VerticalSlider {
            label,
            size,
            min,
            max,
            display_format: None,
            flags: SliderFlags::empty(),
        }
    }
}

impl<Label, Data, Format> VerticalSlider<Label, Data, Format>
where
    Label: AsRef<str>,
    Data: DataTypeKind,
    Format: AsRef<str>,
{
    /// Sets the range for the vertical slider.
    ///
    /// ```rust
    /// # use imgui::im_str;
    /// imgui::VerticalSlider::new(im_str!("Example"), [20.0, 20.0], i8::MIN, i8::MAX)
    ///     .range(4, 8)
    ///     // Remember to call .build(&ui)
    ///     ;
    /// ```
    ///
    /// It is safe, though up to C++ Dear ImGui, on how to handle when
    /// `min > max`.
    #[inline]
    pub fn range(mut self, min: Data, max: Data) -> Self {
        self.min = min;
        self.max = max;
        self
    }
    /// Sets the display format using *a C-style printf string*
    #[inline]
    pub fn display_format<Format2: AsRef<str>>(
        self,
        display_format: Format2,
    ) -> VerticalSlider<Label, Data, Format2> {
        VerticalSlider {
            label: self.label,
            size: self.size,
            min: self.min,
            max: self.max,
            display_format: Some(display_format),
            flags: self.flags,
        }
    }
    /// Replaces all current settings with the given flags
    #[inline]
    pub fn flags(mut self, flags: SliderFlags) -> Self {
        self.flags = flags;
        self
    }
    /// Builds a vertical slider that is bound to the given value.
    ///
    /// Returns true if the slider value was changed.
    pub fn build(self, ui: &Ui<'_>, value: &mut Data) -> bool {
        unsafe {
            let (label, display_format) = ui.scratch_txt_with_opt(self.label, self.display_format);

            sys::igVSliderScalar(
                label,
                self.size.into(),
                Data::KIND as i32,
                value as *mut Data as *mut c_void,
                &self.min as *const Data as *const c_void,
                &self.max as *const Data as *const c_void,
                display_format,
                self.flags.bits() as i32,
            )
        }
    }
}

/// Builder for an angle slider widget.
#[derive(Copy, Clone, Debug)]
#[must_use]
pub struct AngleSlider<Label, Format = &'static str> {
    label: Label,
    min_degrees: f32,
    max_degrees: f32,
    display_format: Format,
    flags: SliderFlags,
}

impl<Label> AngleSlider<Label>
where
    Label: AsRef<str>,
{
    /// Constructs a new angle slider builder, where its minimum defaults to -360.0 and
    /// maximum defaults to 360.0
    #[doc(alias = "SliderAngle")]
    pub fn new(label: Label) -> Self {
        AngleSlider {
            label,
            min_degrees: -360.0,
            max_degrees: 360.0,
            display_format: "%.0f deg",
            flags: SliderFlags::empty(),
        }
    }
}

impl<Label, Format> AngleSlider<Label, Format>
where
    Label: AsRef<str>,
    Format: AsRef<str>,
{
    /// Sets the range in degrees (inclusive)
    /// ```rust
    /// # use imgui::im_str;
    /// imgui::AngleSlider::new(im_str!("Example"))
    ///     .range_degrees(-20.0, 20.0)
    ///     // Remember to call .build(&ui)
    ///     ;
    /// ```
    ///
    /// It is safe, though up to C++ Dear ImGui, on how to handle when
    /// `min > max`.
    #[inline]
    pub fn range_degrees(mut self, min_degrees: f32, max_degrees: f32) -> Self {
        self.min_degrees = min_degrees;
        self.max_degrees = max_degrees;
        self
    }
    /// Sets the minimum value (in degrees)
    #[inline]
    pub fn min_degrees(mut self, min_degrees: f32) -> Self {
        self.min_degrees = min_degrees;
        self
    }
    /// Sets the maximum value (in degrees)
    #[inline]
    pub fn max_degrees(mut self, max_degrees: f32) -> Self {
        self.max_degrees = max_degrees;
        self
    }
    /// Sets the display format using *a C-style printf string*
    #[inline]
    pub fn display_format<Format2: AsRef<str>>(
        self,
        display_format: Format2,
    ) -> AngleSlider<Label, Format2> {
        AngleSlider {
            label: self.label,
            min_degrees: self.min_degrees,
            max_degrees: self.max_degrees,
            display_format,
            flags: self.flags,
        }
    }
    /// Replaces all current settings with the given flags
    #[inline]
    pub fn flags(mut self, flags: SliderFlags) -> Self {
        self.flags = flags;
        self
    }
    /// Builds an angle slider that is bound to the given value (in radians).
    ///
    /// Returns true if the slider value was changed.
    pub fn build(self, ui: &Ui<'_>, value_rad: &mut f32) -> bool {
        unsafe {
            let (label, display_format) = ui.scratch_txt_two(self.label, self.display_format);

            sys::igSliderAngle(
                label,
                value_rad as *mut _,
                self.min_degrees,
                self.max_degrees,
                display_format,
                self.flags.bits() as i32,
            )
        }
    }
}
