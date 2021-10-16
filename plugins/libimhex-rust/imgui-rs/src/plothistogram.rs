use std::os::raw::c_float;
use std::{f32, mem};

use super::Ui;

#[must_use]
pub struct PlotHistogram<'ui, 'p, Label, Overlay = &'static str> {
    label: Label,
    values: &'p [f32],
    values_offset: usize,
    overlay_text: Option<Overlay>,
    scale_min: f32,
    scale_max: f32,
    graph_size: [f32; 2],
    ui: &'ui Ui<'ui>,
}

impl<'ui, 'p, Label: AsRef<str>> PlotHistogram<'ui, 'p, Label> {
    pub fn new(ui: &'ui Ui<'ui>, label: Label, values: &'p [f32]) -> Self {
        PlotHistogram {
            label,
            values,
            values_offset: 0usize,
            overlay_text: None,
            scale_min: f32::MAX,
            scale_max: f32::MAX,
            graph_size: [0.0, 0.0],
            ui,
        }
    }
}

impl<'ui, 'p, Label: AsRef<str>, Overlay: AsRef<str>> PlotHistogram<'ui, 'p, Label, Overlay> {
    pub fn values_offset(mut self, values_offset: usize) -> Self {
        self.values_offset = values_offset;
        self
    }

    pub fn overlay_text<NewOverlay: AsRef<str>>(
        self,
        overlay_text: NewOverlay,
    ) -> PlotHistogram<'ui, 'p, Label, NewOverlay> {
        PlotHistogram {
            label: self.label,
            values: self.values,
            values_offset: self.values_offset,
            overlay_text: Some(overlay_text),
            scale_min: self.scale_min,
            scale_max: self.scale_max,
            graph_size: self.graph_size,
            ui: self.ui,
        }
    }

    pub fn scale_min(mut self, scale_min: f32) -> Self {
        self.scale_min = scale_min;
        self
    }

    pub fn scale_max(mut self, scale_max: f32) -> Self {
        self.scale_max = scale_max;
        self
    }

    pub fn graph_size(mut self, graph_size: [f32; 2]) -> Self {
        self.graph_size = graph_size;
        self
    }

    pub fn build(self) {
        unsafe {
            let (label, overlay_text) = self.ui.scratch_txt_with_opt(self.label, self.overlay_text);

            sys::igPlotHistogram_FloatPtr(
                label,
                self.values.as_ptr() as *const c_float,
                self.values.len() as i32,
                self.values_offset as i32,
                overlay_text,
                self.scale_min,
                self.scale_max,
                self.graph_size.into(),
                mem::size_of::<f32>() as i32,
            );
        }
    }
}
