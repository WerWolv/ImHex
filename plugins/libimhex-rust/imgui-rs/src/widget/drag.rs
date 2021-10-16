use std::os::raw::c_void;
use std::ptr;

use crate::internal::DataTypeKind;
use crate::sys;
use crate::widget::slider::SliderFlags;
use crate::Ui;

/// Builder for a drag slider widget.
#[derive(Copy, Clone, Debug)]
#[must_use]
pub struct Drag<T, L, F = &'static str> {
    label: L,
    speed: f32,
    min: Option<T>,
    max: Option<T>,
    display_format: Option<F>,
    flags: SliderFlags,
}

impl<L: AsRef<str>, T: DataTypeKind> Drag<T, L> {
    /// Constructs a new drag slider builder.
    #[doc(alias = "DragScalar", alias = "DragScalarN")]
    pub fn new(label: L) -> Self {
        Drag {
            label,
            speed: 1.0,
            min: None,
            max: None,
            display_format: None,
            flags: SliderFlags::empty(),
        }
    }
}

impl<L: AsRef<str>, T: DataTypeKind, F: AsRef<str>> Drag<T, L, F> {
    /// Sets the range (inclusive)
    pub fn range(mut self, min: T, max: T) -> Self {
        self.min = Some(min);
        self.max = Some(max);
        self
    }
    /// Sets the value increment for a movement of one pixel.
    ///
    /// Example: speed=0.2 means mouse needs to move 5 pixels to increase the slider value by 1
    pub fn speed(mut self, speed: f32) -> Self {
        self.speed = speed;
        self
    }
    /// Sets the display format using *a C-style printf string*
    pub fn display_format<F2: AsRef<str>>(self, display_format: F2) -> Drag<T, L, F2> {
        Drag {
            label: self.label,
            speed: self.speed,
            min: self.min,
            max: self.max,
            display_format: Some(display_format),
            flags: self.flags,
        }
    }
    /// Replaces all current settings with the given flags
    pub fn flags(mut self, flags: SliderFlags) -> Self {
        self.flags = flags;
        self
    }
    /// Builds a drag slider that is bound to the given value.
    ///
    /// Returns true if the slider value was changed.
    pub fn build(self, ui: &Ui<'_>, value: &mut T) -> bool {
        unsafe {
            let (one, two) = ui.scratch_txt_with_opt(self.label, self.display_format);

            sys::igDragScalar(
                one,
                T::KIND as i32,
                value as *mut T as *mut c_void,
                self.speed,
                self.min
                    .as_ref()
                    .map(|min| min as *const T)
                    .unwrap_or(ptr::null()) as *const c_void,
                self.max
                    .as_ref()
                    .map(|max| max as *const T)
                    .unwrap_or(ptr::null()) as *const c_void,
                two,
                self.flags.bits() as i32,
            )
        }
    }
    /// Builds a horizontal array of multiple drag sliders attached to the given slice.
    ///
    /// Returns true if any slider value was changed.
    pub fn build_array(self, ui: &Ui<'_>, values: &mut [T]) -> bool {
        unsafe {
            let (one, two) = ui.scratch_txt_with_opt(self.label, self.display_format);

            sys::igDragScalarN(
                one,
                T::KIND as i32,
                values.as_mut_ptr() as *mut c_void,
                values.len() as i32,
                self.speed,
                self.min
                    .as_ref()
                    .map(|min| min as *const T)
                    .unwrap_or(ptr::null()) as *const c_void,
                self.max
                    .as_ref()
                    .map(|max| max as *const T)
                    .unwrap_or(ptr::null()) as *const c_void,
                two,
                self.flags.bits() as i32,
            )
        }
    }
}

/// Builder for a drag slider widget.
#[derive(Copy, Clone, Debug)]
#[must_use]
pub struct DragRange<T, L, F = &'static str, M = &'static str> {
    label: L,
    speed: f32,
    min: Option<T>,
    max: Option<T>,
    display_format: Option<F>,
    max_display_format: Option<M>,
    flags: SliderFlags,
}

impl<T: DataTypeKind, L: AsRef<str>> DragRange<T, L> {
    /// Constructs a new drag slider builder.
    #[doc(alias = "DragIntRange2", alias = "DragFloatRange2")]
    pub fn new(label: L) -> DragRange<T, L> {
        DragRange {
            label,
            speed: 1.0,
            min: None,
            max: None,
            display_format: None,
            max_display_format: None,
            flags: SliderFlags::empty(),
        }
    }
}

impl<T, L, F, M> DragRange<T, L, F, M>
where
    T: DataTypeKind,
    L: AsRef<str>,
    F: AsRef<str>,
    M: AsRef<str>,
{
    pub fn range(mut self, min: T, max: T) -> Self {
        self.min = Some(min);
        self.max = Some(max);
        self
    }
    /// Sets the value increment for a movement of one pixel.
    ///
    /// Example: speed=0.2 means mouse needs to move 5 pixels to increase the slider value by 1
    pub fn speed(mut self, speed: f32) -> Self {
        self.speed = speed;
        self
    }
    /// Sets the display format using *a C-style printf string*
    pub fn display_format<F2: AsRef<str>>(self, display_format: F2) -> DragRange<T, L, F2, M> {
        DragRange {
            label: self.label,
            speed: self.speed,
            min: self.min,
            max: self.max,
            display_format: Some(display_format),
            max_display_format: self.max_display_format,
            flags: self.flags,
        }
    }
    /// Sets the display format for the max value using *a C-style printf string*
    pub fn max_display_format<M2: AsRef<str>>(
        self,
        max_display_format: M2,
    ) -> DragRange<T, L, F, M2> {
        DragRange {
            label: self.label,
            speed: self.speed,
            min: self.min,
            max: self.max,
            display_format: self.display_format,
            max_display_format: Some(max_display_format),
            flags: self.flags,
        }
    }
    /// Replaces all current settings with the given flags
    pub fn flags(mut self, flags: SliderFlags) -> Self {
        self.flags = flags;
        self
    }
}

impl<L, F, M> DragRange<f32, L, F, M>
where
    L: AsRef<str>,
    F: AsRef<str>,
    M: AsRef<str>,
{
    /// Builds a drag range slider that is bound to the given min/max values.
    ///
    /// Returns true if the slider value was changed.
    #[doc(alias = "DragFloatRange2")]
    pub fn build(self, ui: &Ui<'_>, min: &mut f32, max: &mut f32) -> bool {
        let label;
        let mut display_format = std::ptr::null();
        let mut max_display_format = std::ptr::null();

        // we do this ourselves the long way...
        unsafe {
            let buffer = &mut *ui.buffer.get();
            buffer.refresh_buffer();

            label = buffer.push(self.label);
            if let Some(v) = self.display_format {
                display_format = buffer.push(v);
            }
            if let Some(v) = self.max_display_format {
                max_display_format = buffer.push(v);
            }

            sys::igDragFloatRange2(
                label,
                min as *mut f32,
                max as *mut f32,
                self.speed,
                self.min.unwrap_or(0.0),
                self.max.unwrap_or(0.0),
                display_format,
                max_display_format,
                self.flags.bits() as i32,
            )
        }
    }
}

impl<L, F, M> DragRange<i32, L, F, M>
where
    L: AsRef<str>,
    F: AsRef<str>,
    M: AsRef<str>,
{
    /// Builds a drag range slider that is bound to the given min/max values.
    ///
    /// Returns true if the slider value was changed.
    #[doc(alias = "DragIntRange2")]
    pub fn build(self, ui: &Ui<'_>, min: &mut i32, max: &mut i32) -> bool {
        unsafe {
            let label;
            let mut display_format = std::ptr::null();
            let mut max_display_format = std::ptr::null();

            // we do this ourselves the long way...
            let buffer = &mut *ui.buffer.get();
            buffer.refresh_buffer();

            label = buffer.push(self.label);
            if let Some(v) = self.display_format {
                display_format = buffer.push(v);
            }
            if let Some(v) = self.max_display_format {
                max_display_format = buffer.push(v);
            }

            sys::igDragIntRange2(
                label,
                min as *mut i32,
                max as *mut i32,
                self.speed,
                self.min.unwrap_or(0),
                self.max.unwrap_or(0),
                display_format,
                max_display_format,
                self.flags.bits() as i32,
            )
        }
    }
}
