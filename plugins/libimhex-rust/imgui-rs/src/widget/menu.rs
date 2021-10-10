// use crate::string::ImStr;
use crate::sys;
use crate::Ui;

/// # Widgets: Menus
impl<'ui> Ui<'ui> {
    /// Creates and starts appending to a full-screen menu bar.
    ///
    /// Returns `Some(MainMenuBarToken)` if the menu bar is visible. After content has been
    /// rendered, the token must be ended by calling `.end()`.
    ///
    /// Returns `None` if the menu bar is not visible and no content should be rendered.
    #[must_use]
    #[doc(alias = "BeginMainMenuBar")]
    pub fn begin_main_menu_bar(&self) -> Option<MainMenuBarToken<'ui>> {
        if unsafe { sys::igBeginMainMenuBar() } {
            Some(MainMenuBarToken::new(self))
        } else {
            None
        }
    }
    /// Creates a full-screen main menu bar and runs a closure to construct the contents.
    ///
    /// Note: the closure is not called if the menu bar is not visible.
    #[doc(alias = "BeginMenuBar")]
    pub fn main_menu_bar<F: FnOnce()>(&self, f: F) {
        if let Some(_menu_bar) = self.begin_main_menu_bar() {
            f();
        }
    }
    /// Creates and starts appending to the menu bar of the current window.
    ///
    /// Returns `Some(MenuBarToken)` if the menu bar is visible. After content has been
    /// rendered, the token must be ended by calling `.end()`.
    ///
    /// Returns `None` if the menu bar is not visible and no content should be rendered.
    #[must_use]
    #[doc(alias = "BeginMenuBar")]
    pub fn begin_menu_bar(&self) -> Option<MenuBarToken<'_>> {
        if unsafe { sys::igBeginMenuBar() } {
            Some(MenuBarToken::new(self))
        } else {
            None
        }
    }
    /// Creates a menu bar in the current window and runs a closure to construct the contents.
    ///
    /// Note: the closure is not called if the menu bar is not visible.
    #[doc(alias = "BeginMenuBar")]
    pub fn menu_bar<F: FnOnce()>(&self, f: F) {
        if let Some(_menu_bar) = self.begin_menu_bar() {
            f();
        }
    }

    /// Creates and starts appending to a sub-menu entry.
    ///
    /// Returns `Some(MenuToken)` if the menu is visible. After content has been
    /// rendered, the token must be ended by calling `.end()`.
    ///
    /// Returns `None` if the menu is not visible and no content should be rendered.
    ///
    /// This is the equivalent of [begin_menu_with_enabled](Self::begin_menu_with_enabled)
    /// with `enabled` set to `true`.
    #[must_use]
    #[doc(alias = "BeginMenu")]
    pub fn begin_menu(&self, label: impl AsRef<str>) -> Option<MenuToken<'_>> {
        self.begin_menu_with_enabled(label, true)
    }

    /// Creates and starts appending to a sub-menu entry.
    ///
    /// Returns `Some(MenuToken)` if the menu is visible. After content has been
    /// rendered, the token must be ended by calling `.end()`.
    ///
    /// Returns `None` if the menu is not visible and no content should be rendered.
    #[must_use]
    #[doc(alias = "BeginMenu")]
    pub fn begin_menu_with_enabled(
        &self,
        label: impl AsRef<str>,
        enabled: bool,
    ) -> Option<MenuToken<'_>> {
        if unsafe { sys::igBeginMenu(self.scratch_txt(label), enabled) } {
            Some(MenuToken::new(self))
        } else {
            None
        }
    }
    /// Creates a menu and runs a closure to construct the contents.
    ///
    /// Note: the closure is not called if the menu is not visible.
    ///
    /// This is the equivalent of [menu_with_enabled](Self::menu_with_enabled)
    /// with `enabled` set to `true`.
    #[doc(alias = "BeginMenu")]
    pub fn menu<F: FnOnce()>(&self, label: impl AsRef<str>, f: F) {
        self.menu_with_enabled(label, true, f);
    }

    /// Creates a menu and runs a closure to construct the contents.
    ///
    /// Note: the closure is not called if the menu is not visible.
    #[doc(alias = "BeginMenu")]
    pub fn menu_with_enabled<F: FnOnce()>(&self, label: impl AsRef<str>, enabled: bool, f: F) {
        if let Some(_menu) = self.begin_menu_with_enabled(label, enabled) {
            f();
        }
    }
}

/// Builder for a menu item.
#[derive(Copy, Clone, Debug)]
#[must_use]
pub struct MenuItem<Label, Shortcut = &'static str> {
    label: Label,
    shortcut: Option<Shortcut>,
    selected: bool,
    enabled: bool,
}

impl<Label: AsRef<str>> MenuItem<Label> {
    /// Construct a new menu item builder.
    pub fn new(label: Label) -> Self {
        MenuItem {
            label,
            shortcut: None,
            selected: false,
            enabled: true,
        }
    }
}

impl<Label: AsRef<str>, Shortcut: AsRef<str>> MenuItem<Label, Shortcut> {
    /// Sets the menu item shortcut.
    ///
    /// Shortcuts are displayed for convenience only and are not automatically handled.
    #[inline]
    pub fn shortcut<Shortcut2: AsRef<str>>(
        self,
        shortcut: Shortcut2,
    ) -> MenuItem<Label, Shortcut2> {
        MenuItem {
            label: self.label,
            shortcut: Some(shortcut),
            selected: self.selected,
            enabled: self.enabled,
        }
    }
    /// Sets the selected state of the menu item.
    ///
    /// Default: false
    #[inline]
    pub fn selected(mut self, selected: bool) -> Self {
        self.selected = selected;
        self
    }
    /// Enables/disables the menu item.
    ///
    /// Default: enabled
    #[inline]
    pub fn enabled(mut self, enabled: bool) -> Self {
        self.enabled = enabled;
        self
    }
    /// Builds the menu item.
    ///
    /// Returns true if the menu item is activated.
    #[doc(alias = "MenuItemBool")]
    pub fn build(self, ui: &Ui<'_>) -> bool {
        unsafe {
            let (label, shortcut) = ui.scratch_txt_with_opt(self.label, self.shortcut);
            sys::igMenuItem_Bool(label, shortcut, self.selected, self.enabled)
        }
    }

    #[doc(alias = "MenuItemBool")]
    /// Builds the menu item using a mutable reference to selected state.
    pub fn build_with_ref(self, ui: &Ui<'_>, selected: &mut bool) -> bool {
        if self.selected(*selected).build(ui) {
            *selected = !*selected;
            true
        } else {
            false
        }
    }
}

create_token!(
    /// Tracks a main menu bar that can be ended by calling `.end()`
    /// or by dropping
    pub struct MainMenuBarToken<'ui>;

    /// Ends a main menu bar
    drop { sys::igEndMainMenuBar() }
);

create_token!(
    /// Tracks a menu bar that can be ended by calling `.end()`
    /// or by dropping
    pub struct MenuBarToken<'ui>;

    /// Ends a menu bar
    drop { sys::igEndMenuBar() }
);

create_token!(
    /// Tracks a menu that can be ended by calling `.end()`
    /// or by dropping
    pub struct MenuToken<'ui>;

    /// Ends a menu
    drop { sys::igEndMenu() }
);
