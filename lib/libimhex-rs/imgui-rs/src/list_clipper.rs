use std::marker::PhantomData;
use std::thread;

use crate::sys;
use crate::Ui;

pub struct ListClipper {
    items_count: i32,
    items_height: f32,
}

impl ListClipper {
    pub const fn new(items_count: i32) -> Self {
        ListClipper {
            items_count,
            items_height: -1.0,
        }
    }

    pub const fn items_height(mut self, items_height: f32) -> Self {
        self.items_height = items_height;
        self
    }

    pub fn begin<'ui>(self, ui: &Ui<'ui>) -> ListClipperToken<'ui> {
        let list_clipper = unsafe {
            let list_clipper = sys::ImGuiListClipper_ImGuiListClipper();
            sys::ImGuiListClipper_Begin(list_clipper, self.items_count, self.items_height);
            list_clipper
        };
        ListClipperToken::new(ui, list_clipper)
    }
}

pub struct ListClipperToken<'ui> {
    list_clipper: *mut sys::ImGuiListClipper,
    _phantom: PhantomData<&'ui Ui<'ui>>,
}

impl<'ui> ListClipperToken<'ui> {
    fn new(_: &Ui<'ui>, list_clipper: *mut sys::ImGuiListClipper) -> Self {
        Self {
            list_clipper,
            _phantom: PhantomData,
        }
    }

    pub fn step(&mut self) -> bool {
        unsafe { sys::ImGuiListClipper_Step(self.list_clipper) }
    }

    pub fn end(&mut self) {
        unsafe {
            sys::ImGuiListClipper_End(self.list_clipper);
        }
    }

    pub fn display_start(&self) -> i32 {
        unsafe { (*self.list_clipper).DisplayStart }
    }

    pub fn display_end(&self) -> i32 {
        unsafe { (*self.list_clipper).DisplayEnd }
    }
}

impl<'ui> Drop for ListClipperToken<'ui> {
    fn drop(&mut self) {
        if !self.step() {
            unsafe {
                sys::ImGuiListClipper_destroy(self.list_clipper);
            };
        } else if !thread::panicking() {
            panic!(
                "Forgot to call End(), or to Step() until false? \
            This is the only token in the repository which users must call `.end()` or `.step()` \
            with. See https://github.com/imgui-rs/imgui-rs/issues/438"
            );
        }
    }
}
