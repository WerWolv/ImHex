use std::ops::{Index, IndexMut};

use crate::internal::RawCast;
use crate::sys;
use crate::Direction;

/// User interface style/colors
#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct Style {
    /// Global alpha applies to everything
    pub alpha: f32,
    /// Additional alpha multiplier applied to disabled elements. Multiplies over current value of [`Style::alpha`].
    pub disabled_alpha: f32,
    /// Padding within a window
    pub window_padding: [f32; 2],
    /// Rounding radius of window corners.
    ///
    /// Set to 0.0 to have rectangular windows.
    /// Large values tend to lead to a variety of artifacts and are not recommended.
    pub window_rounding: f32,
    /// Thickness of border around windows.
    ///
    /// Generally set to 0.0 or 1.0 (other values are not well tested and cost more CPU/GPU).
    pub window_border_size: f32,
    /// Minimum window size
    pub window_min_size: [f32; 2],
    /// Alignment for title bar text.
    ///
    /// Defaults to [0.5, 0.5] for left-aligned, vertically centered.
    pub window_title_align: [f32; 2],
    /// Side of the collapsing/docking button in the title bar (left/right).
    ///
    /// Defaults to Direction::Left.
    pub window_menu_button_position: Direction,
    /// Rounding radius of child window corners.
    ///
    /// Set to 0.0 to have rectangular child windows.
    pub child_rounding: f32,
    /// Thickness of border around child windows.
    ///
    /// Generally set to 0.0 or 1.0 (other values are not well tested and cost more CPU/GPU).
    pub child_border_size: f32,
    /// Rounding radius of popup window corners.
    ///
    /// Note that tooltip windows use `window_rounding` instead.
    pub popup_rounding: f32,
    /// Thickness of border around popup/tooltip windows.
    ///
    /// Generally set to 0.0 or 1.0 (other values are not well tested and cost more CPU/GPU).
    pub popup_border_size: f32,
    /// Padding within a framed rectangle (used by most widgets)
    pub frame_padding: [f32; 2],
    /// Rounding radius of frame corners (used by most widgets).
    ///
    /// Set to 0.0 to have rectangular frames.
    pub frame_rounding: f32,
    /// Thickness of border around frames.
    ///
    /// Generally set to 0.0 or 1.0 (other values are not well tested and cost more CPU/GPU).
    pub frame_border_size: f32,
    /// Horizontal and vertical spacing between widgets/lines
    pub item_spacing: [f32; 2],
    /// Horizontal and vertical spacing between elements of a composed widget (e.g. a slider and
    /// its label)
    pub item_inner_spacing: [f32; 2],
    /// Padding within a table cell.
    pub cell_padding: [f32; 2],
    /// Expand reactive bounding box for touch-based system where touch position is not accurate
    /// enough.
    ///
    /// Unfortunately we don't sort widgets so priority on overlap will always be given to the
    /// first widget, so don't grow this too much.
    pub touch_extra_padding: [f32; 2],
    /// Horizontal indentation when e.g. entering a tree node.
    ///
    /// Generally equal to (font size + horizontal frame padding * 2).
    pub indent_spacing: f32,
    /// Minimum horizontal spacing between two columns
    pub columns_min_spacing: f32,
    /// Width of the vertical scrollbar, height of the horizontal scrollbar
    pub scrollbar_size: f32,
    /// Rounding radius of scrollbar grab corners
    pub scrollbar_rounding: f32,
    /// Minimum width/height of a grab box for slider/scrollbar
    pub grab_min_size: f32,
    /// Rounding radius of grab corners.
    ///
    /// Set to 0.0 to have rectangular slider grabs.
    pub grab_rounding: f32,
    /// The size in pixels of the dead-zone around zero on logarithmic sliders that cross zero
    pub log_slider_deadzone: f32,
    /// Rounding radius of upper corners of tabs.
    ///
    /// Set to 0.0 to have rectangular tabs.
    pub tab_rounding: f32,
    /// Thickness of border around tabs
    pub tab_border_size: f32,
    /// Minimum width for close button to appear on an unselected tab when hovered.
    ///
    /// `= 0.0`: always show when hovering
    /// `= f32::MAX`: never show close button unless selected
    pub tab_min_width_for_close_button: f32,
    /// Side of the color button position color editor widgets (left/right).
    pub color_button_position: Direction,
    /// Alignment of button text when button is larger than text.
    ///
    /// Defaults to [0.5, 0.5] (centered).
    pub button_text_align: [f32; 2],
    /// Alignment of selectable text when selectable is larger than text.
    ///
    /// Defaults to [0.5, 0.5] (top-left aligned).
    pub selectable_text_align: [f32; 2],
    /// Window positions are clamped to be visible within the display area or monitors by at least
    /// this amount.
    ///
    /// Only applies to regular windows.
    pub display_window_padding: [f32; 2],
    /// If you cannot see the edges of your screen (e.g. on a TV), increase the safe area padding.
    ///
    /// Also applies to popups/tooltips in addition to regular windows.
    pub display_safe_area_padding: [f32; 2],
    /// Scale software-rendered mouse cursor.
    ///
    /// May be removed later.
    pub mouse_cursor_scale: f32,
    /// Enable anti-aliased lines/borders.
    ///
    /// Disable if you are really tight on CPU/GPU. Latched at the beginning of the frame.
    pub anti_aliased_lines: bool,
    /// Enable anti-aliased lines/borders using textures where possible.
    ///
    /// Require back-end to render with bilinear filtering. Latched at the beginning of the frame.
    pub anti_aliased_lines_use_tex: bool,
    /// Enable anti-aliased edges around filled shapes (rounded rectangles, circles, etc.).
    ///
    /// Disable if you are really tight on CPU/GPU. Latched at the beginning of the frame.
    pub anti_aliased_fill: bool,
    /// Tessellation tolerance when using path_bezier_curve_to without a specific number of
    /// segments.
    ///
    /// Decrease for highly tessellated curves (higher quality, more polygons), increase to reduce
    /// quality.
    pub curve_tessellation_tol: f32,
    /// Maximum error (in pixels) allowed when drawing circles or rounded corner rectangles with no
    /// explicit segment count specified.
    ///
    /// Decrease for higher quality but more geometry.
    pub circle_tesselation_max_error: f32,
    /// Style colors.
    pub colors: [[f32; 4]; StyleColor::COUNT],
}

unsafe impl RawCast<sys::ImGuiStyle> for Style {}

impl Style {
    /// Scales all sizes in the style
    #[doc(alias = "ScaleAllSizes")]
    pub fn scale_all_sizes(&mut self, scale_factor: f32) {
        unsafe {
            sys::ImGuiStyle_ScaleAllSizes(self.raw_mut(), scale_factor);
        }
    }
    /// Replaces current colors with classic Dear ImGui style
    #[doc(alias = "StyleColors", alias = "StyleColorsClassic")]
    pub fn use_classic_colors(&mut self) -> &mut Self {
        unsafe {
            sys::igStyleColorsClassic(self.raw_mut());
        }
        self
    }
    /// Replaces current colors with a new, recommended style
    #[doc(alias = "StyleColors", alias = "StyleColorsDark")]
    pub fn use_dark_colors(&mut self) -> &mut Self {
        unsafe {
            sys::igStyleColorsDark(self.raw_mut());
        }
        self
    }
    /// Replaces current colors with a light style. Best used with borders and a custom, thicker
    /// font
    #[doc(alias = "StyleColors", alias = "StyleColorsLight")]
    pub fn use_light_colors(&mut self) -> &mut Self {
        unsafe {
            sys::igStyleColorsLight(self.raw_mut());
        }
        self
    }
}

impl Index<StyleColor> for Style {
    type Output = [f32; 4];
    #[inline]
    fn index(&self, index: StyleColor) -> &[f32; 4] {
        &self.colors[index as usize]
    }
}

impl IndexMut<StyleColor> for Style {
    #[inline]
    fn index_mut(&mut self, index: StyleColor) -> &mut [f32; 4] {
        &mut self.colors[index as usize]
    }
}

/// A color identifier for styling
#[repr(u32)]
#[derive(Copy, Clone, Debug, Hash, Eq, PartialEq)]
pub enum StyleColor {
    Text = sys::ImGuiCol_Text,
    TextDisabled = sys::ImGuiCol_TextDisabled,
    /// Background of normal windows
    WindowBg = sys::ImGuiCol_WindowBg,
    /// Background of child windows
    ChildBg = sys::ImGuiCol_ChildBg,
    /// Background of popups, menus, tooltips windows
    PopupBg = sys::ImGuiCol_PopupBg,
    Border = sys::ImGuiCol_Border,
    BorderShadow = sys::ImGuiCol_BorderShadow,
    /// Background of checkbox, radio button, plot, slider, text input
    FrameBg = sys::ImGuiCol_FrameBg,
    FrameBgHovered = sys::ImGuiCol_FrameBgHovered,
    FrameBgActive = sys::ImGuiCol_FrameBgActive,
    TitleBg = sys::ImGuiCol_TitleBg,
    TitleBgActive = sys::ImGuiCol_TitleBgActive,
    TitleBgCollapsed = sys::ImGuiCol_TitleBgCollapsed,
    MenuBarBg = sys::ImGuiCol_MenuBarBg,
    ScrollbarBg = sys::ImGuiCol_ScrollbarBg,
    ScrollbarGrab = sys::ImGuiCol_ScrollbarGrab,
    ScrollbarGrabHovered = sys::ImGuiCol_ScrollbarGrabHovered,
    ScrollbarGrabActive = sys::ImGuiCol_ScrollbarGrabActive,
    CheckMark = sys::ImGuiCol_CheckMark,
    SliderGrab = sys::ImGuiCol_SliderGrab,
    SliderGrabActive = sys::ImGuiCol_SliderGrabActive,
    Button = sys::ImGuiCol_Button,
    ButtonHovered = sys::ImGuiCol_ButtonHovered,
    ButtonActive = sys::ImGuiCol_ButtonActive,
    Header = sys::ImGuiCol_Header,
    HeaderHovered = sys::ImGuiCol_HeaderHovered,
    HeaderActive = sys::ImGuiCol_HeaderActive,
    Separator = sys::ImGuiCol_Separator,
    SeparatorHovered = sys::ImGuiCol_SeparatorHovered,
    SeparatorActive = sys::ImGuiCol_SeparatorActive,
    ResizeGrip = sys::ImGuiCol_ResizeGrip,
    ResizeGripHovered = sys::ImGuiCol_ResizeGripHovered,
    ResizeGripActive = sys::ImGuiCol_ResizeGripActive,
    Tab = sys::ImGuiCol_Tab,
    TabHovered = sys::ImGuiCol_TabHovered,
    TabActive = sys::ImGuiCol_TabActive,
    TabUnfocused = sys::ImGuiCol_TabUnfocused,
    TabUnfocusedActive = sys::ImGuiCol_TabUnfocusedActive,
    PlotLines = sys::ImGuiCol_PlotLines,
    PlotLinesHovered = sys::ImGuiCol_PlotLinesHovered,
    PlotHistogram = sys::ImGuiCol_PlotHistogram,
    PlotHistogramHovered = sys::ImGuiCol_PlotHistogramHovered,
    TableHeaderBg = sys::ImGuiCol_TableHeaderBg,
    TableBorderStrong = sys::ImGuiCol_TableBorderStrong,
    TableBorderLight = sys::ImGuiCol_TableBorderLight,
    TableRowBg = sys::ImGuiCol_TableRowBg,
    TableRowBgAlt = sys::ImGuiCol_TableRowBgAlt,
    TextSelectedBg = sys::ImGuiCol_TextSelectedBg,
    DragDropTarget = sys::ImGuiCol_DragDropTarget,
    /// Gamepad/keyboard: current highlighted item
    NavHighlight = sys::ImGuiCol_NavHighlight,
    /// Highlight window when using CTRL+TAB
    NavWindowingHighlight = sys::ImGuiCol_NavWindowingHighlight,
    /// Darken/colorize entire screen behind the CTRL+TAB window list, when active
    NavWindowingDimBg = sys::ImGuiCol_NavWindowingDimBg,
    /// Darken/colorize entire screen behind a modal window, when one is active
    ModalWindowDimBg = sys::ImGuiCol_ModalWindowDimBg,
}

impl StyleColor {
    /// All possible `StyleColor` variants
    pub const VARIANTS: [StyleColor; StyleColor::COUNT] = [
        StyleColor::Text,
        StyleColor::TextDisabled,
        StyleColor::WindowBg,
        StyleColor::ChildBg,
        StyleColor::PopupBg,
        StyleColor::Border,
        StyleColor::BorderShadow,
        StyleColor::FrameBg,
        StyleColor::FrameBgHovered,
        StyleColor::FrameBgActive,
        StyleColor::TitleBg,
        StyleColor::TitleBgActive,
        StyleColor::TitleBgCollapsed,
        StyleColor::MenuBarBg,
        StyleColor::ScrollbarBg,
        StyleColor::ScrollbarGrab,
        StyleColor::ScrollbarGrabHovered,
        StyleColor::ScrollbarGrabActive,
        StyleColor::CheckMark,
        StyleColor::SliderGrab,
        StyleColor::SliderGrabActive,
        StyleColor::Button,
        StyleColor::ButtonHovered,
        StyleColor::ButtonActive,
        StyleColor::Header,
        StyleColor::HeaderHovered,
        StyleColor::HeaderActive,
        StyleColor::Separator,
        StyleColor::SeparatorHovered,
        StyleColor::SeparatorActive,
        StyleColor::ResizeGrip,
        StyleColor::ResizeGripHovered,
        StyleColor::ResizeGripActive,
        StyleColor::Tab,
        StyleColor::TabHovered,
        StyleColor::TabActive,
        StyleColor::TabUnfocused,
        StyleColor::TabUnfocusedActive,
        StyleColor::PlotLines,
        StyleColor::PlotLinesHovered,
        StyleColor::PlotHistogram,
        StyleColor::PlotHistogramHovered,
        StyleColor::TableHeaderBg,
        StyleColor::TableBorderStrong,
        StyleColor::TableBorderLight,
        StyleColor::TableRowBg,
        StyleColor::TableRowBgAlt,
        StyleColor::TextSelectedBg,
        StyleColor::DragDropTarget,
        StyleColor::NavHighlight,
        StyleColor::NavWindowingHighlight,
        StyleColor::NavWindowingDimBg,
        StyleColor::ModalWindowDimBg,
    ];
    /// Total count of `StyleColor` variants
    pub const COUNT: usize = sys::ImGuiCol_COUNT as usize;
}

/// A temporary change in user interface style
#[derive(Copy, Clone, Debug, PartialEq)]
pub enum StyleVar {
    /// Global alpha applies to everything
    Alpha(f32),
    /// Padding within a window
    WindowPadding([f32; 2]),
    /// Rounding radius of window corners
    WindowRounding(f32),
    /// Thickness of border around windows
    WindowBorderSize(f32),
    /// Minimum window size
    WindowMinSize([f32; 2]),
    /// Alignment for title bar text
    WindowTitleAlign([f32; 2]),
    /// Rounding radius of child window corners
    ChildRounding(f32),
    /// Thickness of border around child windows
    ChildBorderSize(f32),
    /// Rounding radius of popup window corners
    PopupRounding(f32),
    /// Thickness of border around popup/tooltip windows
    PopupBorderSize(f32),
    /// Padding within a framed rectangle (used by most widgets)
    FramePadding([f32; 2]),
    /// Rounding radius of frame corners (used by most widgets)
    FrameRounding(f32),
    /// Thickness of border around frames
    FrameBorderSize(f32),
    /// Horizontal and vertical spacing between widgets/lines
    ItemSpacing([f32; 2]),
    /// Horizontal and vertical spacing between elements of a composed widget (e.g. a slider and
    /// its label)
    ItemInnerSpacing([f32; 2]),
    /// Horizontal indentation when e.g. entering a tree node
    IndentSpacing(f32),
    /// Width of the vertical scrollbar, height of the horizontal scrollbar
    ScrollbarSize(f32),
    /// Rounding radius of scrollbar grab corners
    ScrollbarRounding(f32),
    /// Minimum width/height of a grab box for slider/scrollbar
    GrabMinSize(f32),
    /// Rounding radius of grab corners
    GrabRounding(f32),
    /// Rounding radius of upper corners of tabs
    TabRounding(f32),
    /// Alignment of button text when button is larger than text
    ButtonTextAlign([f32; 2]),
    /// Alignment of selectable text when selectable is larger than text
    SelectableTextAlign([f32; 2]),
}

#[test]
fn test_style_scaling() {
    let (_guard, mut ctx) = crate::test::test_ctx();
    let style = ctx.style_mut();
    style.window_padding = [1.0, 2.0];
    style.window_rounding = 3.0;
    style.window_min_size = [4.0, 5.0];
    style.child_rounding = 6.0;
    style.popup_rounding = 7.0;
    style.frame_padding = [8.0, 9.0];
    style.frame_rounding = 10.0;
    style.item_spacing = [11.0, 12.0];
    style.item_inner_spacing = [13.0, 14.0];
    style.touch_extra_padding = [15.0, 16.0];
    style.indent_spacing = 17.0;
    style.columns_min_spacing = 18.0;
    style.scrollbar_size = 19.0;
    style.scrollbar_rounding = 20.0;
    style.grab_min_size = 21.0;
    style.grab_rounding = 22.0;
    style.log_slider_deadzone = 29.0;
    style.tab_rounding = 23.0;
    style.display_window_padding = [24.0, 25.0];
    style.display_safe_area_padding = [26.0, 27.0];
    style.mouse_cursor_scale = 28.0;
    style.scale_all_sizes(2.0);
    assert_eq!(style.window_padding, [2.0, 4.0]);
    assert_eq!(style.window_rounding, 6.0);
    assert_eq!(style.window_min_size, [8.0, 10.0]);
    assert_eq!(style.child_rounding, 12.0);
    assert_eq!(style.popup_rounding, 14.0);
    assert_eq!(style.frame_padding, [16.0, 18.0]);
    assert_eq!(style.frame_rounding, 20.0);
    assert_eq!(style.item_spacing, [22.0, 24.0]);
    assert_eq!(style.item_inner_spacing, [26.0, 28.0]);
    assert_eq!(style.touch_extra_padding, [30.0, 32.0]);
    assert_eq!(style.indent_spacing, 34.0);
    assert_eq!(style.columns_min_spacing, 36.0);
    assert_eq!(style.scrollbar_size, 38.0);
    assert_eq!(style.scrollbar_rounding, 40.0);
    assert_eq!(style.grab_min_size, 42.0);
    assert_eq!(style.grab_rounding, 44.0);
    assert_eq!(style.log_slider_deadzone, 58.0);
    assert_eq!(style.tab_rounding, 46.0);
    assert_eq!(style.display_window_padding, [48.0, 50.0]);
    assert_eq!(style.display_safe_area_padding, [52.0, 54.0]);
    assert_eq!(style.mouse_cursor_scale, 56.0);
}

#[test]
fn test_style_color_indexing() {
    let (_guard, mut ctx) = crate::test::test_ctx();
    let style = ctx.style_mut();
    let value = [0.1, 0.2, 0.3, 1.0];
    style[StyleColor::Tab] = value;
    assert_eq!(style[StyleColor::Tab], value);
    assert_eq!(style.colors[StyleColor::Tab as usize], value);
}

#[test]
#[cfg(test)]
fn test_style_memory_layout() {
    use std::mem;
    assert_eq!(mem::size_of::<Style>(), mem::size_of::<sys::ImGuiStyle>());
    assert_eq!(mem::align_of::<Style>(), mem::align_of::<sys::ImGuiStyle>());
    use sys::ImGuiStyle;
    macro_rules! assert_field_offset {
        ($l:ident, $r:ident) => {
            assert_eq!(
                memoffset::offset_of!(Style, $l),
                memoffset::offset_of!(ImGuiStyle, $r)
            );
        };
    }
    assert_field_offset!(alpha, Alpha);
    assert_field_offset!(window_padding, WindowPadding);
    assert_field_offset!(window_rounding, WindowRounding);
    assert_field_offset!(window_border_size, WindowBorderSize);
    assert_field_offset!(window_min_size, WindowMinSize);
    assert_field_offset!(window_title_align, WindowTitleAlign);
    assert_field_offset!(window_menu_button_position, WindowMenuButtonPosition);
    assert_field_offset!(child_rounding, ChildRounding);
    assert_field_offset!(child_border_size, ChildBorderSize);
    assert_field_offset!(popup_rounding, PopupRounding);
    assert_field_offset!(popup_border_size, PopupBorderSize);
    assert_field_offset!(frame_padding, FramePadding);
    assert_field_offset!(frame_rounding, FrameRounding);
    assert_field_offset!(frame_border_size, FrameBorderSize);
    assert_field_offset!(item_spacing, ItemSpacing);
    assert_field_offset!(item_inner_spacing, ItemInnerSpacing);
    assert_field_offset!(touch_extra_padding, TouchExtraPadding);
    assert_field_offset!(indent_spacing, IndentSpacing);
    assert_field_offset!(columns_min_spacing, ColumnsMinSpacing);
    assert_field_offset!(scrollbar_size, ScrollbarSize);
    assert_field_offset!(scrollbar_rounding, ScrollbarRounding);
    assert_field_offset!(grab_min_size, GrabMinSize);
    assert_field_offset!(grab_rounding, GrabRounding);
    assert_field_offset!(log_slider_deadzone, LogSliderDeadzone);
    assert_field_offset!(tab_rounding, TabRounding);
    assert_field_offset!(tab_border_size, TabBorderSize);
    assert_field_offset!(tab_min_width_for_close_button, TabMinWidthForCloseButton);
    assert_field_offset!(color_button_position, ColorButtonPosition);
    assert_field_offset!(button_text_align, ButtonTextAlign);
    assert_field_offset!(selectable_text_align, SelectableTextAlign);
    assert_field_offset!(display_window_padding, DisplayWindowPadding);
    assert_field_offset!(display_safe_area_padding, DisplaySafeAreaPadding);
    assert_field_offset!(mouse_cursor_scale, MouseCursorScale);
    assert_field_offset!(anti_aliased_lines, AntiAliasedLines);
    assert_field_offset!(anti_aliased_lines_use_tex, AntiAliasedLinesUseTex);
    assert_field_offset!(anti_aliased_fill, AntiAliasedFill);
    assert_field_offset!(curve_tessellation_tol, CurveTessellationTol);
    assert_field_offset!(circle_tesselation_max_error, CircleTessellationMaxError);
    assert_field_offset!(colors, Colors);
}

#[test]
fn test_style_color_variants() {
    for (idx, &value) in StyleColor::VARIANTS.iter().enumerate() {
        assert_eq!(idx, value as usize);
    }
}
