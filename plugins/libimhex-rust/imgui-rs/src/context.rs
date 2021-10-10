use parking_lot::ReentrantMutex;
use std::cell::{RefCell, UnsafeCell};
use std::ffi::{CStr, CString};
use std::mem::ManuallyDrop;
use std::ops::Drop;
use std::path::PathBuf;
use std::ptr;
use std::rc::Rc;

use crate::clipboard::{ClipboardBackend, ClipboardContext};
use crate::fonts::atlas::{FontAtlas, FontAtlasRefMut, FontId, SharedFontAtlas};
use crate::io::Io;
use crate::style::Style;
use crate::sys;
use crate::Ui;

/// An imgui-rs context.
///
/// A context needs to be created to access most library functions. Due to current Dear ImGui
/// design choices, at most one active Context can exist at any time. This limitation will likely
/// be removed in a future Dear ImGui version.
///
/// If you need more than one context, you can use suspended contexts. As long as only one context
/// is active at a time, it's possible to have multiple independent contexts.
///
/// # Examples
///
/// Creating a new active context:
/// ```
/// let ctx = imgui::Context::create();
/// // ctx is dropped naturally when it goes out of scope, which deactivates and destroys the
/// // context
/// ```
///
/// Never try to create an active context when another one is active:
///
/// ```should_panic
/// let ctx1 = imgui::Context::create();
///
/// let ctx2 = imgui::Context::create(); // PANIC
/// ```
///
/// Suspending an active context allows you to create another active context:
///
/// ```
/// let ctx1 = imgui::Context::create();
/// let suspended1 = ctx1.suspend();
/// let ctx2 = imgui::Context::create(); // this is now OK
/// ```

#[derive(Debug)]
pub struct Context {
    raw: *mut sys::ImGuiContext,
    shared_font_atlas: Option<Rc<RefCell<SharedFontAtlas>>>,
    ini_filename: Option<CString>,
    log_filename: Option<CString>,
    platform_name: Option<CString>,
    renderer_name: Option<CString>,
    // we need to box this because we hand imgui a pointer to it,
    // and we don't want to deal with finding `clipboard_ctx`.
    // we also put it in an unsafecell since we're going to give
    // imgui a mutable pointer to it.
    clipboard_ctx: Box<UnsafeCell<ClipboardContext>>,
}

// This mutex needs to be used to guard all public functions that can affect the underlying
// Dear ImGui active context
static CTX_MUTEX: ReentrantMutex<()> = parking_lot::const_reentrant_mutex(());

fn clear_current_context() {
    unsafe {
        sys::igSetCurrentContext(ptr::null_mut());
    }
}
fn no_current_context() -> bool {
    let ctx = unsafe { sys::igGetCurrentContext() };
    ctx.is_null()
}

impl Context {
    /// Creates a new active imgui-rs context.
    ///
    /// # Panics
    ///
    /// Panics if an active context already exists
    #[doc(alias = "CreateContext")]
    pub fn create() -> Self {
        Self::create_internal(None)
    }
    /// Creates a new active imgui-rs context with a shared font atlas.
    ///
    /// # Panics
    ///
    /// Panics if an active context already exists
    #[doc(alias = "CreateContext")]
    pub fn create_with_shared_font_atlas(shared_font_atlas: Rc<RefCell<SharedFontAtlas>>) -> Self {
        Self::create_internal(Some(shared_font_atlas))
    }
    /// Suspends this context so another context can be the active context.
    #[doc(alias = "CreateContext")]
    pub fn suspend(self) -> SuspendedContext {
        let _guard = CTX_MUTEX.lock();
        assert!(
            self.is_current_context(),
            "context to be suspended is not the active context"
        );
        clear_current_context();
        SuspendedContext(self)
    }
    /// Returns the path to the ini file, or None if not set
    pub fn ini_filename(&self) -> Option<PathBuf> {
        let io = self.io();
        if io.ini_filename.is_null() {
            None
        } else {
            let s = unsafe { CStr::from_ptr(io.ini_filename) };
            Some(PathBuf::from(s.to_str().ok()?))
        }
    }
    /// Sets the path to the ini file (default is "imgui.ini")
    ///
    /// Pass None to disable automatic .Ini saving.
    pub fn set_ini_filename<T: Into<Option<PathBuf>>>(&mut self, ini_filename: T) {
        let ini_filename: Option<PathBuf> = ini_filename.into();
        let ini_filename = ini_filename.and_then(|v| CString::new(v.to_str()?).ok());

        self.io_mut().ini_filename = ini_filename
            .as_ref()
            .map(|x| x.as_ptr())
            .unwrap_or(ptr::null());
        self.ini_filename = ini_filename;
    }
    /// Returns the path to the log file, or None if not set
    // TODO: why do we return an `Option<PathBuf>` instead of an `Option<&Path>`?
    pub fn log_filename(&self) -> Option<PathBuf> {
        let io = self.io();
        if io.log_filename.is_null() {
            None
        } else {
            let cstr = unsafe { CStr::from_ptr(io.log_filename) };
            Some(PathBuf::from(cstr.to_str().ok()?))
        }
    }
    /// Sets the log filename (default is "imgui_log.txt").
    pub fn set_log_filename<T: Into<Option<PathBuf>>>(&mut self, log_filename: T) {
        let log_filename = log_filename
            .into()
            .and_then(|v| CString::new(v.to_str()?).ok());

        self.io_mut().log_filename = log_filename
            .as_ref()
            .map(|x| x.as_ptr())
            .unwrap_or(ptr::null());
        self.log_filename = log_filename;
    }
    /// Returns the backend platform name, or None if not set
    pub fn platform_name(&self) -> Option<&str> {
        let io = self.io();
        if io.backend_platform_name.is_null() {
            None
        } else {
            let cstr = unsafe { CStr::from_ptr(io.backend_platform_name) };
            cstr.to_str().ok()
        }
    }
    /// Sets the backend platform name
    pub fn set_platform_name<T: Into<Option<String>>>(&mut self, platform_name: T) {
        let platform_name: Option<CString> =
            platform_name.into().and_then(|v| CString::new(v).ok());
        self.io_mut().backend_platform_name = platform_name
            .as_ref()
            .map(|x| x.as_ptr())
            .unwrap_or(ptr::null());
        self.platform_name = platform_name;
    }
    /// Returns the backend renderer name, or None if not set
    pub fn renderer_name(&self) -> Option<&str> {
        let io = self.io();
        if io.backend_renderer_name.is_null() {
            None
        } else {
            let cstr = unsafe { CStr::from_ptr(io.backend_renderer_name) };
            cstr.to_str().ok()
        }
    }
    /// Sets the backend renderer name
    pub fn set_renderer_name<T: Into<Option<String>>>(&mut self, renderer_name: T) {
        let renderer_name: Option<CString> =
            renderer_name.into().and_then(|v| CString::new(v).ok());

        self.io_mut().backend_renderer_name = renderer_name
            .as_ref()
            .map(|x| x.as_ptr())
            .unwrap_or(ptr::null());

        self.renderer_name = renderer_name;
    }
    /// Loads settings from a string slice containing settings in .Ini file format
    #[doc(alias = "LoadIniSettingsFromMemory")]
    pub fn load_ini_settings(&mut self, data: &str) {
        unsafe { sys::igLoadIniSettingsFromMemory(data.as_ptr() as *const _, data.len()) }
    }
    /// Saves settings to a mutable string buffer in .Ini file format
    #[doc(alias = "SaveInitSettingsToMemory")]
    pub fn save_ini_settings(&mut self, buf: &mut String) {
        let data = unsafe { CStr::from_ptr(sys::igSaveIniSettingsToMemory(ptr::null_mut())) };
        buf.push_str(&data.to_string_lossy());
    }
    /// Sets the clipboard backend used for clipboard operations
    pub fn set_clipboard_backend<T: ClipboardBackend>(&mut self, backend: T) {
        let clipboard_ctx: Box<UnsafeCell<_>> = Box::new(ClipboardContext::new(backend).into());
        let io = self.io_mut();
        io.set_clipboard_text_fn = Some(crate::clipboard::set_clipboard_text);
        io.get_clipboard_text_fn = Some(crate::clipboard::get_clipboard_text);

        io.clipboard_user_data = clipboard_ctx.get() as *mut _;
        self.clipboard_ctx = clipboard_ctx;
    }
    fn create_internal(shared_font_atlas: Option<Rc<RefCell<SharedFontAtlas>>>) -> Self {
        let _guard = CTX_MUTEX.lock();
        assert!(
            no_current_context(),
            "A new active context cannot be created, because another one already exists"
        );

        let shared_font_atlas_ptr = match &shared_font_atlas {
            Some(shared_font_atlas) => {
                let borrowed_font_atlas = shared_font_atlas.borrow();
                borrowed_font_atlas.0
            }
            None => ptr::null_mut(),
        };
        // Dear ImGui implicitly sets the current context during igCreateContext if the current
        // context doesn't exist
        let raw = unsafe { sys::igCreateContext(shared_font_atlas_ptr) };

        Context {
            raw,
            shared_font_atlas,
            ini_filename: None,
            log_filename: None,
            platform_name: None,
            renderer_name: None,
            clipboard_ctx: Box::new(ClipboardContext::dummy().into()),
        }
    }
    fn is_current_context(&self) -> bool {
        let ctx = unsafe { sys::igGetCurrentContext() };
        self.raw == ctx
    }

    /// Get a reference to the current context
    pub fn current() -> Option<ContextRef> {
        let _guard = CTX_MUTEX.lock();

        let raw = unsafe { sys::igGetCurrentContext() };

        if raw.is_null() {
            None
        } else {
            Some(ContextRef(ManuallyDrop::new(Context {
                raw,
                shared_font_atlas: None, // XXX: this might be needed tbh
                ini_filename: None,
                log_filename: None,
                platform_name: None,
                renderer_name: None,
                clipboard_ctx: Box::new(ClipboardContext::dummy().into()),
            })))
        }
    }
}

/// A reference to a [`Context`] object
#[derive(Debug)]
pub struct ContextRef(ManuallyDrop<Context>);

impl core::ops::Deref for ContextRef {
    type Target = Context;

    fn deref(&self) -> &Context {
        &*self.0
    }
}

impl core::ops::DerefMut for ContextRef {
    fn deref_mut(&mut self) -> &mut Context {
        &mut *self.0
    }
}

impl Drop for Context {
    #[doc(alias = "DestroyContext")]
    fn drop(&mut self) {
        let _guard = CTX_MUTEX.lock();
        // If this context is the active context, Dear ImGui automatically deactivates it during
        // destruction
        unsafe {
            sys::igDestroyContext(self.raw);
        }
    }
}

/// A suspended imgui-rs context.
///
/// A suspended context retains its state, but is not usable without activating it first.
///
/// # Examples
///
/// Suspended contexts are not directly very useful, but you can activate them:
///
/// ```
/// let suspended = imgui::SuspendedContext::create();
/// match suspended.activate() {
///   Ok(ctx) => {
///     // ctx is now the active context
///   },
///   Err(suspended) => {
///     // activation failed, so you get the suspended context back
///   }
/// }
/// ```
#[derive(Debug)]
pub struct SuspendedContext(Context);

impl SuspendedContext {
    /// Creates a new suspended imgui-rs context.
    #[doc(alias = "CreateContext")]
    pub fn create() -> Self {
        Self::create_internal(None)
    }
    /// Creates a new suspended imgui-rs context with a shared font atlas.
    pub fn create_with_shared_font_atlas(shared_font_atlas: Rc<RefCell<SharedFontAtlas>>) -> Self {
        Self::create_internal(Some(shared_font_atlas))
    }
    /// Attempts to activate this suspended context.
    ///
    /// If there is no active context, this suspended context is activated and `Ok` is returned,
    /// containing the activated context.
    /// If there is already an active context, nothing happens and `Err` is returned, containing
    /// the original suspended context.
    #[doc(alias = "SetCurrentContext")]
    pub fn activate(self) -> Result<Context, SuspendedContext> {
        let _guard = CTX_MUTEX.lock();
        if no_current_context() {
            unsafe {
                sys::igSetCurrentContext(self.0.raw);
            }
            Ok(self.0)
        } else {
            Err(self)
        }
    }
    fn create_internal(shared_font_atlas: Option<Rc<RefCell<SharedFontAtlas>>>) -> Self {
        let _guard = CTX_MUTEX.lock();
        let raw = unsafe { sys::igCreateContext(ptr::null_mut()) };
        let ctx = Context {
            raw,
            shared_font_atlas,
            ini_filename: None,
            log_filename: None,
            platform_name: None,
            renderer_name: None,
            clipboard_ctx: Box::new(ClipboardContext::dummy().into()),
        };
        if ctx.is_current_context() {
            // Oops, the context was activated -> deactivate
            clear_current_context();
        }
        SuspendedContext(ctx)
    }
}

#[test]
fn test_one_context() {
    let _guard = crate::test::TEST_MUTEX.lock();
    let _ctx = Context::create();
    assert!(!no_current_context());
}

#[test]
fn test_drop_clears_current_context() {
    let _guard = crate::test::TEST_MUTEX.lock();
    {
        let _ctx1 = Context::create();
        assert!(!no_current_context());
    }
    assert!(no_current_context());
    {
        let _ctx2 = Context::create();
        assert!(!no_current_context());
    }
    assert!(no_current_context());
}

#[test]
fn test_new_suspended() {
    let _guard = crate::test::TEST_MUTEX.lock();
    let ctx = Context::create();
    let _suspended = SuspendedContext::create();
    assert!(ctx.is_current_context());
    ::std::mem::drop(_suspended);
    assert!(ctx.is_current_context());
}

#[test]
fn test_suspend() {
    let _guard = crate::test::TEST_MUTEX.lock();
    let ctx = Context::create();
    assert!(!no_current_context());
    let _suspended = ctx.suspend();
    assert!(no_current_context());
    let _ctx2 = Context::create();
}

#[test]
fn test_drop_suspended() {
    let _guard = crate::test::TEST_MUTEX.lock();
    let suspended = Context::create().suspend();
    assert!(no_current_context());
    let ctx2 = Context::create();
    ::std::mem::drop(suspended);
    assert!(ctx2.is_current_context());
}

#[test]
fn test_suspend_activate() {
    let _guard = crate::test::TEST_MUTEX.lock();
    let suspended = Context::create().suspend();
    assert!(no_current_context());
    let ctx = suspended.activate().unwrap();
    assert!(ctx.is_current_context());
}

#[test]
fn test_suspend_failure() {
    let _guard = crate::test::TEST_MUTEX.lock();
    let suspended = Context::create().suspend();
    let _ctx = Context::create();
    assert!(suspended.activate().is_err());
}

#[test]
fn test_shared_font_atlas() {
    let _guard = crate::test::TEST_MUTEX.lock();
    let atlas = Rc::new(RefCell::new(SharedFontAtlas::create()));
    let suspended1 = SuspendedContext::create_with_shared_font_atlas(atlas.clone());
    let mut ctx2 = Context::create_with_shared_font_atlas(atlas);
    {
        let _borrow = ctx2.fonts();
    }
    let _suspended2 = ctx2.suspend();
    let mut ctx = suspended1.activate().unwrap();
    let _borrow = ctx.fonts();
}

#[test]
#[should_panic]
fn test_shared_font_atlas_borrow_panic() {
    let _guard = crate::test::TEST_MUTEX.lock();
    let atlas = Rc::new(RefCell::new(SharedFontAtlas::create()));
    let _suspended = SuspendedContext::create_with_shared_font_atlas(atlas.clone());
    let mut ctx = Context::create_with_shared_font_atlas(atlas.clone());
    let _borrow1 = atlas.borrow();
    let _borrow2 = ctx.fonts();
}

#[test]
fn test_ini_load_save() {
    let (_guard, mut ctx) = crate::test::test_ctx();
    let data = "[Window][Debug##Default]
Pos=60,60
Size=400,400
Collapsed=0";
    ctx.load_ini_settings(data);
    let mut buf = String::new();
    ctx.save_ini_settings(&mut buf);
    assert_eq!(data.trim(), buf.trim());
}

#[test]
fn test_default_ini_filename() {
    let _guard = crate::test::TEST_MUTEX.lock();
    let ctx = Context::create();
    assert_eq!(ctx.ini_filename(), Some(PathBuf::from("imgui.ini")));
}

#[test]
fn test_set_ini_filename() {
    let (_guard, mut ctx) = crate::test::test_ctx();
    ctx.set_ini_filename(Some(PathBuf::from("test.ini")));
    assert_eq!(ctx.ini_filename(), Some(PathBuf::from("test.ini")));
}

#[test]
fn test_default_log_filename() {
    let _guard = crate::test::TEST_MUTEX.lock();
    let ctx = Context::create();
    assert_eq!(ctx.log_filename(), Some(PathBuf::from("imgui_log.txt")));
}

#[test]
fn test_set_log_filename() {
    let (_guard, mut ctx) = crate::test::test_ctx();
    ctx.set_log_filename(Some(PathBuf::from("test.log")));
    assert_eq!(ctx.log_filename(), Some(PathBuf::from("test.log")));
}

impl Context {
    /// Returns an immutable reference to the inputs/outputs object
    pub fn io(&self) -> &Io {
        unsafe {
            // safe because Io is a transparent wrapper around sys::ImGuiIO
            &*(sys::igGetIO() as *const Io)
        }
    }
    /// Returns a mutable reference to the inputs/outputs object
    pub fn io_mut(&mut self) -> &mut Io {
        unsafe {
            // safe because Io is a transparent wrapper around sys::ImGuiIO
            &mut *(sys::igGetIO() as *mut Io)
        }
    }
    /// Returns an immutable reference to the user interface style
    #[doc(alias = "GetStyle")]
    pub fn style(&self) -> &Style {
        unsafe {
            // safe because Style is a transparent wrapper around sys::ImGuiStyle
            &*(sys::igGetStyle() as *const Style)
        }
    }
    /// Returns a mutable reference to the user interface style
    #[doc(alias = "GetStyle")]
    pub fn style_mut(&mut self) -> &mut Style {
        unsafe {
            // safe because Style is a transparent wrapper around sys::ImGuiStyle
            &mut *(sys::igGetStyle() as *mut Style)
        }
    }
    /// Returns a mutable reference to the font atlas.
    ///
    /// # Panics
    ///
    /// Panics if the context uses a shared font atlas that is already borrowed
    pub fn fonts(&mut self) -> FontAtlasRefMut<'_> {
        match self.shared_font_atlas {
            Some(ref font_atlas) => FontAtlasRefMut::Shared(font_atlas.borrow_mut()),
            None => unsafe {
                // safe because FontAtlas is a transparent wrapper around sys::ImFontAtlas
                let fonts = &mut *(self.io_mut().fonts as *mut FontAtlas);
                FontAtlasRefMut::Owned(fonts)
            },
        }
    }
    /// Starts a new frame and returns an `Ui` instance for constructing a user interface.
    ///
    /// # Panics
    ///
    /// Panics if the context uses a shared font atlas that is already borrowed
    #[doc(alias = "NewFame")]
    pub fn frame(&mut self) -> Ui<'_> {
        // Clear default font if it no longer exists. This could be an error in the future
        let default_font = self.io().font_default;
        if !default_font.is_null() && self.fonts().get_font(FontId(default_font)).is_none() {
            self.io_mut().font_default = ptr::null_mut();
        }
        // NewFrame/Render/EndFrame mutate the font atlas so we need exclusive access to it
        let font_atlas = self
            .shared_font_atlas
            .as_ref()
            .map(|font_atlas| font_atlas.borrow_mut());
        // TODO: precondition checks
        unsafe {
            sys::igNewFrame();
        }
        Ui {
            ctx: self,
            font_atlas,
            buffer: crate::UiBuffer::new(1024).into(),
        }
    }
}
