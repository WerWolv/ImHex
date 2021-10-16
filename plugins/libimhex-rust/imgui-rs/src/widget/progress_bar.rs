use crate::sys;
// use crate::ImStr;
use crate::Ui;

/// Builder for a progress bar widget.
///
/// # Examples
///
/// ```no_run
/// # use imgui::*;
/// # let mut imgui = Context::create();
/// # let ui = imgui.frame();
/// ProgressBar::new(0.6)
///     .size([100.0, 12.0])
///     .overlay_text("Progress!")
///     .build(&ui);
/// ```
#[derive(Copy, Clone, Debug)]
#[must_use]
pub struct ProgressBar<T = &'static str> {
    fraction: f32,
    size: [f32; 2],
    overlay_text: Option<T>,
}

impl ProgressBar {
    /// Creates a progress bar with a given fraction showing
    /// the progress (0.0 = 0%, 1.0 = 100%).
    ///
    /// The progress bar will be automatically sized to fill the entire width of the window if no
    /// custom size is specified.
    #[inline]
    #[doc(alias = "ProgressBar")]
    pub fn new(fraction: f32) -> Self {
        ProgressBar {
            fraction,
            size: [-1.0, 0.0],
            overlay_text: None,
        }
    }
}

impl<T: AsRef<str>> ProgressBar<T> {
    /// Sets an optional text that will be drawn over the progress bar.
    pub fn overlay_text<T2: AsRef<str>>(self, overlay_text: T2) -> ProgressBar<T2> {
        ProgressBar {
            fraction: self.fraction,
            size: self.size,
            overlay_text: Some(overlay_text),
        }
    }

    /// Sets the size of the progress bar.
    ///
    /// Negative values will automatically align to the end of the axis, zero will let the progress
    /// bar choose a size, and positive values will use the given size.
    #[inline]
    pub fn size(mut self, size: [f32; 2]) -> Self {
        self.size = size;
        self
    }

    /// Builds the progress bar
    pub fn build(self, ui: &Ui<'_>) {
        unsafe {
            sys::igProgressBar(
                self.fraction,
                self.size.into(),
                ui.scratch_txt_opt(self.overlay_text),
            );
        }
    }
}
