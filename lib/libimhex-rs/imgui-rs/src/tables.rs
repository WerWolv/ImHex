use std::ffi::CStr;
use std::marker::PhantomData;

use bitflags::bitflags;

use crate::sys;
use crate::{Id, ImColor32, Ui};

bitflags! {
    /// Flags passed to `begin_table` methods.
    ///
    /// Important! Sizing policies have complex and subtle side effects, more so than you would expect.
    /// Read comments/demos carefully + experiment with live demos to get acquainted with them.
    /// - The DEFAULT sizing policies are:
    ///    - Default to [SizingFixedFit]    if [ScrollX] is on, or if host window has (WindowFlags::AlwaysAutoResize)[crate::WindowFlags::AlwaysAutoResize].
    ///    - Default to [SizingStretchSame] if [ScrollX] is off.
    /// - When [ScrollX] is off:
    ///    - Table defaults to [SizingStretchSame] -> all Columns defaults to [TableColumnFlags::WidthStretch] with same weight.
    ///    - Columns sizing policy allowed: [Stretch] (default), [Fixed]/Auto.
    ///    - [Fixed] Columns will generally obtain their requested width (unless the table cannot fit them all).
    ///    - [Stretch] Columns will share the remaining width.
    ///    - Mixed [Fixed]/[Stretch] columns is possible but has various side-effects on resizing behaviors.
    ///      The typical use of mixing sizing policies is: any number of LEADING [Fixed] columns, followed by one or two TRAILING [Stretch] columns.
    ///      (this is because the visible order of columns have subtle but necessary effects on how they react to manual resizing).
    /// - When [ScrollX] is on:
    ///    - Table defaults to [SizingFixedFit] -> all Columns defaults to [TableColumnFlags::WidthFixed]
    ///    - Columns sizing policy allowed: [Fixed]/Auto mostly.
    ///    - [Fixed] Columns can be enlarged as needed. Table will show an horizontal scrollbar if needed.
    ///    - When using auto-resizing (non-resizable) fixed columns, querying the content width to use item right-alignment e.g. SetNextItemWidth(-FLT_MIN) doesn't make sense, would create a feedback loop.
    ///    - Using [Stretch] columns OFTEN DOES NOT MAKE SENSE if [ScrollX] is on, UNLESS you have specified a value for `inner_width` in BeginTable().
    ///      If you specify a value for `inner_width` then effectively the scrolling space is known and [Stretch] or mixed [Fixed]/[Stretch] columns become meaningful again.
    /// - Read on documentation at the top of imgui_tables.cpp for more details.
    #[repr(transparent)]
    pub struct TableFlags: u32 {
        // Features

        /// Enable resizing columns.
        const RESIZABLE = sys::ImGuiTableFlags_Resizable;
        /// Enable reordering columns in header row, though you must set up a header row
        /// with `begin_table_header` or `table_setup_column`.
        const REORDERABLE =sys::ImGuiTableFlags_Reorderable;
        /// Enable hiding/disabling columns in context menu.
        const HIDEABLE = sys::ImGuiTableFlags_Hideable;
        /// Enable sorting. See `table_get_sort_specs` to object sort specs. Also see [SortMulti]
        /// and [SortTristate].
        const SORTABLE = sys::ImGuiTableFlags_Sortable;
        /// Disable persisting columns order, width, and sort settings in the .ini file.
        const NO_SAVED_SETTINGS = sys::ImGuiTableFlags_NoSavedSettings;
        /// Right-click on columns body/contents will display table context menu.
        /// By default you can only right click in a headers row.
        const CONTEXT_MENU_IN_BODY = sys::ImGuiTableFlags_ContextMenuInBody;

        // Decorations

        /// Set each RowBg color with [table_row_bg] or [table_row_bg_alt] (equivalent of calling
        /// `table_set_bg_color` with `ROW_BG0` on each row manually)
        const ROW_BG = sys::ImGuiTableFlags_RowBg;
        /// Draw horizontal borders between rows.
        const BORDERS_INNER_H = sys::ImGuiTableFlags_BordersInnerH;
        /// Draw horizontal borders at the top and bottom.
        const BORDERS_OUTER_H = sys::ImGuiTableFlags_BordersOuterH;
        /// Draw vertical borders between columns.
        const BORDERS_INNER_V = sys::ImGuiTableFlags_BordersInnerV;
        /// Draw vertical borders on the left and right sides.
        const BORDERS_OUTER_V = sys::ImGuiTableFlags_BordersOuterV;
        /// Draw all horizontal borders (this is just [BORDERS_INNER_H] | [BORDERS_OUTER_H]).
        const BORDERS_H = sys::ImGuiTableFlags_BordersH;
        /// Draw all vertical borders (this is just [BORDERS_INNER_V] | [BORDERS_OUTER_V]).
        const BORDERS_V = sys::ImGuiTableFlags_BordersV;
        /// Draw all inner borders (this is just [BORDERS_INNER_H] | [BORDERS_INNER_V]).
        const BORDERS_INNER = sys::ImGuiTableFlags_BordersInner;
        /// Draw all outer borders (this is just [BORDERS_OUTER_H] | [BORDERS_OUTER_V]).
        const BORDERS_OUTER = sys::ImGuiTableFlags_BordersOuter;
        /// Draw all borders (this is just [BORDERS_INNER] | [BORDERS_OUTER]).
        const BORDERS = sys::ImGuiTableFlags_Borders;
        /// **ALPHA** Disable vertical borders in columns Body (borders will always appears in Headers).
        /// May move to Style
        const NO_BORDERS_IN_BODY = sys::ImGuiTableFlags_NoBordersInBody;
        /// **ALPHA** Disable vertical borders in columns Body until hovered for resize (borders will always appears in Headers).
        /// May move to style
        const NO_BORDERS_IN_BODY_UNTIL_RESIZE = sys::ImGuiTableFlags_NoBordersInBodyUntilResize;

        // Sizing Policy (read above for defaults)

        /// Columns default to [WidthFixed] or [WidthAuto] (if resizable or not resizable),
        /// matching contents width.
        const SIZING_FIXED_FIT = sys::ImGuiTableFlags_SizingFixedFit;
        /// Columns default to [WidthFixed] or [WidthAuto] (if resizable or not resizable),
        /// matching the maximum contents width of all columns.
        /// Implicitly enable [NoKeepColumnsVisible].
        const SIZING_FIXED_SAME = sys::ImGuiTableFlags_SizingFixedSame;
        /// Columns default to [WidthStretch] with default weights proportional to each columns
        /// contents widths.
        const SIZING_STRETCH_PROP = sys::ImGuiTableFlags_SizingStretchProp;
        /// Columns default to [WidthStretch] with default weights all equal, unless overridden by
        /// a column's `TableHeader`.
        const SIZING_STRETCH_SAME = sys::ImGuiTableFlags_SizingStretchSame;

        // Sizing Extra Options

        /// Make outer width auto-fit to columns, overriding outer_size.x value. Only available when
        /// [ScrollX]/[ScrollY] are disabled and [Stretch] columns are not used.
        const NO_HOST_EXTEND_X = sys::ImGuiTableFlags_NoHostExtendX;
        /// Make outer height stop exactly at outer_size.y (prevent auto-extending table past the limit).
        /// Only available when [ScrollX]/[ScrollY] are disabled.
        /// Data below the limit will be clipped and not visible.
        const NO_HOST_EXTEND_Y = sys::ImGuiTableFlags_NoHostExtendY;
        /// Disable keeping column always minimally visible when [ScrollX] is off and table
        /// gets too small. Not recommended if columns are resizable.
        const NO_KEEP_COLUMNS_VISIBLE = sys::ImGuiTableFlags_NoKeepColumnsVisible;
        /// Disable distributing remainder width to stretched columns (width allocation on a 100-wide
        /// table with 3 columns: Without this flag: 33,33,34. With this flag: 33,33,33).
        /// With larger number of columns, resizing will appear to be less smooth.
        const PRECISE_WIDTHS = sys::ImGuiTableFlags_PreciseWidths;

        // Clipping

        /// Disable clipping rectangle for every individual columns (reduce draw command count, items will
        /// be able to overflow into other columns). Generally incompatible with [table_setup_scroll_freeze].
        const NO_CLIP = sys::ImGuiTableFlags_NoClip;

        // Padding

        /// Default if [BordersOuterV] is on. Enable outer-most padding. Generally desirable if you have headers.
        const PAD_OUTER_X = sys::ImGuiTableFlags_PadOuterX;
        /// Default if [BordersOuterV] is off. Disable outer-most padding.
        const NO_PAD_OUTER_X = sys::ImGuiTableFlags_NoPadOuterX;
        /// Disable inner padding between columns (double inner padding if [BordersOuterV] is on, single
        /// inner padding if BordersOuterV is off).
        const NO_PAD_INNER_X = sys::ImGuiTableFlags_NoPadInnerX;

        // Scrolling

        /// Enable horizontal scrolling. Require 'outer_size' parameter of [begin_table] to specify the
        /// container size. Changes default sizing policy. Because this create a child window,
        /// [ScrollY] is currently generally recommended when using [ScrollX].
        const SCROLL_X = sys::ImGuiTableFlags_ScrollX;
        /// Enable vertical scrolling. Require 'outer_size' parameter of [begin_table] to specify the
        /// container size.
        const SCROLL_Y = sys::ImGuiTableFlags_ScrollY;

        // Sorting

        /// Hold shift when clicking headers to sort on multiple column. [table_get_sort_specs] may return specs where `[spec_count] > 1`.
        const SORT_MULTI = sys::ImGuiTableFlags_SortMulti;
        /// Allow no sorting, disable default sorting. `table_get_sort_specs` may return specs where `[specs_count] == 0`.
        const SORT_TRISTATE = sys::ImGuiTableFlags_SortTristate;
    }
}

bitflags! {
    /// Flags for [table_next_row_with_flags].
    #[repr(transparent)]
    pub struct TableRowFlags: u32 {
        /// Identify header row (set default background color + width of its contents
        /// accounted different for auto column width)
        const HEADERS = sys::ImGuiTableRowFlags_Headers;
    }
}

bitflags! {
    /// Flags for [TableColumnSetup] and [table_setup_column_with].
    #[repr(transparent)]
    #[derive(Default)]
    pub struct TableColumnFlags: u32 {
        // Input configuration flags

        /// Default as a hidden/disabled column.
        const DEFAULT_HIDE = sys::ImGuiTableColumnFlags_DefaultHide;
        /// Default as a sorting column.
        const DEFAULT_SORT = sys::ImGuiTableColumnFlags_DefaultSort;
        /// Column will stretch. Preferable with horizontal scrolling disabled (default
        /// if table sizing policy is [ImGuiTableFlags::SizingStretchSame] or
        /// [ImGuiTableFlags::SizingStretchProp]).
        const WIDTH_STRETCH = sys::ImGuiTableColumnFlags_WidthStretch;
        /// Column will not stretch. Preferable with horizontal scrolling enabled (default
        /// if table sizing policy is [ImGuiTableFlags::SizingFixedFit] and table is resizable).
        const WIDTH_FIXED = sys::ImGuiTableColumnFlags_WidthFixed;
        /// Disable manual resizing.
        const NO_RESIZE = sys::ImGuiTableColumnFlags_NoResize;
        /// Disable manual reordering this column, this will also prevent other columns from
        /// crossing over this column.
        const NO_REORDER = sys::ImGuiTableColumnFlags_NoReorder;
        /// Disable ability to hide/disable this column.
        const NO_HIDE = sys::ImGuiTableColumnFlags_NoHide;
        /// Disable clipping for this column (all [NO_CLIP] columns will render in a same
        /// draw command).
        const NO_CLIP = sys::ImGuiTableColumnFlags_NoClip;
        /// Disable ability to sort on this field (even if [ImGuiTableFlags::Sortable] is
        /// set on the table).
        const NO_SORT = sys::ImGuiTableColumnFlags_NoSort;
        /// Disable ability to sort in the ascending direction.
        const NO_SORT_ASCENDING = sys::ImGuiTableColumnFlags_NoSortAscending;
        /// Disable ability to sort in the descending direction.
        const NO_SORT_DESCENDING = sys::ImGuiTableColumnFlags_NoSortDescending;
        /// Disable header text width contribution to automatic column width.
        const NO_HEADER_WIDTH = sys::ImGuiTableColumnFlags_NoHeaderWidth;
        /// Make the initial sort direction Ascending when first sorting on this column (default).
        const PREFER_SORT_ASCENDING = sys::ImGuiTableColumnFlags_PreferSortAscending;
        /// Make the initial sort direction Descending when first sorting on this column.
        const PREFER_SORT_DESCENDING = sys::ImGuiTableColumnFlags_PreferSortDescending;
        /// Use current Indent value when entering cell (default for column 0).
        const INDENT_ENABLE = sys::ImGuiTableColumnFlags_IndentEnable;
        /// Ignore current Indent value when entering cell (default for columns > 0).
        /// Indentation changes _within_ the cell will still be honored.
        const INDENT_DISABLE = sys::ImGuiTableColumnFlags_IndentDisable;

        // Output status flags, read-only via [table_get_column_flags]

        /// Status: is enabled == not hidden by user/api (referred to as "Hide" in
        /// [DefaultHide] and [NoHide]) flags.
        const IS_ENABLED = sys::ImGuiTableColumnFlags_IsEnabled;
        /// Status: is visible == is enabled AND not clipped by scrolling.
        const IS_VISIBLE = sys::ImGuiTableColumnFlags_IsVisible;
        /// Status: is currently part of the sort specs
        const IS_SORTED = sys::ImGuiTableColumnFlags_IsSorted;
        /// Status: is hovered by mouse
        const IS_HOVERED = sys::ImGuiTableColumnFlags_IsHovered;
    }
}

bitflags! {
    /// Enum for [table_set_bg_color].
    /// Background colors are rendering in 3 layers:
    ///  - Layer 0: draw with RowBg0 color if set, otherwise draw with ColumnBg0 if set.
    ///  - Layer 1: draw with RowBg1 color if set, otherwise draw with ColumnBg1 if set.
    ///  - Layer 2: draw with CellBg color if set.
    /// The purpose of the two row/columns layers is to let you decide if a background color
    /// changes should override or blend with the existing color.
    /// When using [TableFlags::RowBg] on the table, each row has the RowBg0 color automatically
    /// set for odd/even rows.
    /// If you set the color of RowBg0 target, your color will override the existing RowBg0 color.
    /// If you set the color of RowBg1 or ColumnBg1 target, your color will blend over the RowBg0 color.
    #[repr(transparent)]
    pub struct TableBgTarget: u32 {
        /// Set row background color 0 (generally used for background, automatically set when
        /// [TableFlags::RowBg] is used)
        const ROW_BG0 = sys::ImGuiTableBgTarget_RowBg0;
        /// Set row background color 1 (generally used for selection marking)
        const ROW_BG1 = sys::ImGuiTableBgTarget_RowBg1;
        /// Set cell background color (top-most color)
        const CELL_BG = sys::ImGuiTableBgTarget_CellBg;
    }
}

#[derive(Copy, Clone, Debug, Eq, PartialEq, PartialOrd, Ord)]
pub enum TableSortDirection {
    Ascending,
    Descending,
}

impl<'ui> Ui<'ui> {
    /// Begins a table with no flags and with standard sizing constraints.
    ///
    /// This does no work on styling the headers (the top row) -- see either
    /// [begin_table_header](Self::begin_table_header) or the more complex
    /// [table_setup_column](Self::table_setup_column).
    ///
    /// Nb: we take `column` as a usize, but it will be converted with `as i32` to an i32.
    /// If this makes a difference to you, you are probably trying to make too many columns.
    #[must_use = "if return is dropped immediately, table is ended immediately."]
    pub fn begin_table(
        &self,
        str_id: impl AsRef<str>,
        column_count: usize,
    ) -> Option<TableToken<'ui>> {
        self.begin_table_with_flags(str_id, column_count, TableFlags::empty())
    }

    /// Begins a table with flags and standard sizing constraints.
    ///
    /// This does no work on styling the headers (the top row) -- see either
    /// [begin_table_header](Self::begin_table_header) or the more complex
    /// [table_setup_column](Self::table_setup_column).
    ///
    /// Nb: we take `column` as a usize, but it will be converted with `as i32` to an i32.
    /// If this makes a difference to you, you are probably trying to make too many columns.
    #[must_use = "if return is dropped immediately, table is ended immediately."]
    pub fn begin_table_with_flags(
        &self,
        str_id: impl AsRef<str>,
        column_count: usize,
        flags: TableFlags,
    ) -> Option<TableToken<'ui>> {
        self.begin_table_with_sizing(str_id, column_count, flags, [0.0, 0.0], 0.0)
    }

    /// Begins a table with all flags and sizing constraints. This is the base method,
    /// and gives users the most flexibility.
    ///
    /// This does no work on styling the headers (the top row) -- see either
    /// [begin_table_header](Self::begin_table_header) or the more complex
    /// [table_setup_column](Self::table_setup_column).
    ///
    /// Nb: we take `column` as a usize, but it will be converted with `as i32` to an i32.
    /// If this makes a difference to you, you are probably trying to make too many columns.
    #[must_use = "if return is dropped immediately, table is ended immediately."]
    pub fn begin_table_with_sizing(
        &self,
        str_id: impl AsRef<str>,
        column: usize,
        flags: TableFlags,
        outer_size: [f32; 2],
        inner_width: f32,
    ) -> Option<TableToken<'ui>> {
        let should_render = unsafe {
            sys::igBeginTable(
                self.scratch_txt(str_id),
                column as i32,
                flags.bits() as i32,
                outer_size.into(),
                inner_width,
            )
        };

        // todo: once msrv is 1.54, convert this to .then(||)
        if should_render {
            Some(TableToken::new(self))
        } else {
            None
        }
    }

    /// Begins a table with no flags and with standard sizing constraints.
    ///
    /// Takes an array of table header information, the length of which determines
    /// how many columns will be created.
    #[cfg(feature = "min-const-generics")]
    #[must_use = "if return is dropped immediately, table is ended immediately."]
    pub fn begin_table_header<'a, Name: AsRef<str>, const N: usize>(
        &self,
        str_id: impl AsRef<str>,
        column_data: [TableColumnSetup<'a, Name>; N],
    ) -> Option<TableToken<'ui>> {
        self.begin_table_header_with_flags(str_id, column_data, TableFlags::empty())
    }

    /// Begins a table with flags and standard sizing constraints.
    ///
    /// Takes an array of table header information, the length of which determines
    /// how many columns will be created.
    #[cfg(feature = "min-const-generics")]
    #[must_use = "if return is dropped immediately, table is ended immediately."]
    pub fn begin_table_header_with_flags<'a, Name: AsRef<str>, const N: usize>(
        &self,
        str_id: impl AsRef<str>,
        column_data: [TableColumnSetup<'a, Name>; N],
        flags: TableFlags,
    ) -> Option<TableToken<'ui>> {
        self.begin_table_header_with_sizing(str_id, column_data, flags, [0.0, 0.0], 0.0)
    }

    /// Begins a table with all flags and sizing constraints. This is the base method,
    /// and gives users the most flexibility.
    /// Takes an array of table header information, the length of which determines
    /// how many columns will be created.
    #[cfg(feature = "min-const-generics")]
    #[must_use = "if return is dropped immediately, table is ended immediately."]
    pub fn begin_table_header_with_sizing<'a, Name: AsRef<str>, const N: usize>(
        &self,
        str_id: impl AsRef<str>,
        column_data: [TableColumnSetup<'a, Name>; N],
        flags: TableFlags,
        outer_size: [f32; 2],
        inner_width: f32,
    ) -> Option<TableToken<'ui>> {
        self.begin_table_with_sizing(str_id, N, flags, outer_size, inner_width)
            .map(|data| {
                for value in column_data {
                    self.table_setup_column_with(value);
                }
                self.table_headers_row();

                data
            })
    }

    /// Moves a table to the next row (ie, down) with no flags,
    /// and with the next row having a standard computed height.
    ///
    /// If your table was made with [begin_table], this **must** be called
    /// before rendering any cells (along with [table_next_column]).
    /// If your table was made with [begin_table_header], this does not need to be called,
    /// though [table_next_column] still should be.
    ///
    /// [begin_table]: Self::begin_table
    /// [begin_table_header]: Self::begin_table_header
    /// [table_next_column]: Self::table_next_column
    #[inline]
    pub fn table_next_row(&self) {
        self.table_next_row_with_flags(TableRowFlags::empty());
    }

    /// Moves a  table to the next row (ie, down), with the given flags,
    /// and with the next row having a standard computed height.
    ///
    /// Setting a flag here will make the next row a "header" now, which may
    /// require setup of column data.
    ///
    /// See [table_next_row](Self::table_next_row) for information on how moving rows work. To set the row
    /// with a given height, see [table_next_row_with_height](Self::table_next_row_with_height).
    #[inline]
    pub fn table_next_row_with_flags(&self, flags: TableRowFlags) {
        self.table_next_row_with_height(flags, 0.0);
    }

    /// Moves a  table to the next row (ie, down), with the given flags,
    /// and with the given minimum height.
    ///
    /// See [table_next_row](Self::table_next_row) for information on how moving rows work.
    #[inline]
    pub fn table_next_row_with_height(&self, flags: TableRowFlags, min_row_height: f32) {
        unsafe {
            sys::igTableNextRow(flags.bits() as i32, min_row_height);
        }
    }

    /// Moves onto the next column. If at `column_count`, this will move to the next row.
    /// In this way, you can use this function as an iterator over each cell in the table.
    ///
    /// # Example
    /// ```no_run
    /// # let mut ctx = imgui::Context::create();
    /// # { let ui = ctx.frame();
    /// if let Some(_t) = ui.begin_table("Basic-Table", 2) {
    ///     // we have to call next_row because we didn't make headers..
    ///     ui.table_next_row();
    ///
    ///     // you always have to call this to start...
    ///     // take advantage of this in loops!
    ///     ui.table_next_column();
    ///     ui.text("x: 0, y: 0");
    ///
    ///     ui.table_next_column();
    ///     ui.text("x: 1, y: 0");
    ///     
    ///     // notice that we go down a row here too.
    ///     ui.table_next_column();
    ///     ui.text("x: 0, y: 1");
    ///
    ///     ui.table_next_column();
    ///     ui.text("x: 1, y: 1");
    /// }
    /// # };
    /// ```
    ///
    /// This functions returns true if the given column is **visible.** It is not
    /// marked as must use, as you can still render commands into the not-visible column,
    /// though you can choose to not as an optimization.
    pub fn table_next_column(&self) -> bool {
        unsafe { sys::igTableNextColumn() }
    }

    /// Moves onto the given column.
    ///
    /// # Example
    /// ```no_run
    /// # let mut ctx = imgui::Context::create();
    /// # { let ui = ctx.frame();
    /// if let Some(_t) = ui.begin_table("Basic-Table", 2) {
    ///     // we have to call next_row because we didn't make headers..
    ///     ui.table_next_row();
    ///
    ///     for i in 0..2 {
    ///         ui.table_set_column_index(i);
    ///         ui.text(format!("x: {}", i));
    ///     }
    ///     
    ///     // oops I just remembered, i need to add something on idx 0!
    ///     ui.table_set_column_index(0);
    ///     // if i uncomment this line, we'll write on top of our previous "x: 0"
    ///     // line:
    ///     // ui.text("hello from the future on top of the past");
    ///     // so we do a .newline();
    ///     ui.new_line();
    ///     ui.text("hello from the future");
    ///
    ///     // imgui will understand this and row spacing will be adjusted automatically.
    /// }
    /// # };
    /// ```
    ///
    /// This functions returns true if the given column is **visible.** It is not
    /// marked as must use, as you can still render commands into the not-visible column,
    /// though you can choose to not as an optimization.
    ///
    /// # Panics
    /// If `column_index >= ui.table_column_count`, this function will panic. In `debug` releases,
    /// we will panic on the Rust side, for a nicer error message, though in release, we will
    /// panic in C++, which will result in an ugly stack overflow.
    pub fn table_set_column_index(&self, column_index: usize) -> bool {
        #[cfg(debug_assertions)]
        {
            let size = self.table_column_count() as usize;
            if column_index >= size {
                panic!(
                    "column_index >= self.table_get_column_count().\
                Requested {}, but only have {} columns.",
                    column_index, size
                );
            }
        }

        unsafe { sys::igTableSetColumnIndex(column_index as i32) }
    }

    /// Specify label per column, with no flags and default sizing. You can avoid calling
    /// this method entirely by using [begin_table_header](Self::begin_table_header).
    ///
    /// # Example
    /// ```no_run
    /// # let mut ctx = imgui::Context::create();
    /// # { let ui = ctx.frame();
    /// if let Some(_t) = ui.begin_table("My Table", 2) {
    ///     ui.table_setup_column("One");
    ///     ui.table_setup_column("Two");
    ///     ui.table_setup_column("Three");
    ///     ui.table_headers_row();
    ///
    ///     // call next_column/set_column_index and proceed like normal.
    ///     // the above code is the equivalent of just using `begin_table_header`
    ///     // but does allow for some columns to have headers and others to not
    /// }
    /// # };
    /// ```
    ///
    /// Along with [table_headers_row](Self::table_headers_row), this method is used to create a header
    /// row and automatically submit a table header for each column.
    /// Headers are required to perform: reordering, sorting, and opening the context menu (though,
    /// the context menu can also be made available in columns body using [TableFlags::CONTEXT_MENU_IN_BODY].
    pub fn table_setup_column(&self, str_id: impl AsRef<str>) {
        self.table_setup_column_with(TableColumnSetup::new(str_id))
    }

    /// Specify label per column, with data given in [TableColumnSetup]. You can avoid calling
    /// this method entirely by using [begin_table_header](Self::begin_table_header).
    ///
    /// See [table_setup_column](Self::table_setup_column) for an example of how to setup columns
    /// yourself.
    ///
    /// Along with [table_headers_row](Self::table_headers_row), this method is used to create a header
    /// row and automatically submit a table header for each column.
    /// Headers are required to perform: reordering, sorting, and opening the context menu (though,
    /// the context menu can also be made available in columns body using [TableFlags::CONTEXT_MENU_IN_BODY].
    pub fn table_setup_column_with<N: AsRef<str>>(&self, data: TableColumnSetup<'_, N>) {
        unsafe {
            sys::igTableSetupColumn(
                self.scratch_txt(data.name),
                data.flags.bits() as i32,
                data.init_width_or_weight,
                data.user_id.as_imgui_id(),
            )
        }
    }

    /// Locks columns/rows so they stay visible when scrolled. Generally, you will be calling this
    /// so that the header column is always visible (though go wild if you want). You can avoid
    /// calling this entirely by passing `true` to [begin_table_header](Self::begin_table_header).
    ///
    /// # Example
    /// ```no_run
    /// # let mut ctx = imgui::Context::create();
    /// # { let ui = ctx.frame();
    /// const COLUMN_COUNT: usize = 3;
    /// if let Some(_t) = ui.begin_table("scroll-freeze-example", COLUMN_COUNT) {
    ///     // locks the header row. Notice how we need to call it BEFORE `table_headers_row`.
    ///     ui.table_setup_scroll_freeze(1, COLUMN_COUNT);
    ///     ui.table_setup_column("One");
    ///     ui.table_setup_column("Two");
    ///     ui.table_setup_column("Three");
    ///     ui.table_headers_row();
    /// }
    /// # };
    /// ```
    ///
    /// Nb: we take `locked_columns` and `locked_rows` as a `usize`, but it will be converted
    /// with `as i32` to an i32. If this makes a difference to you, you are probably
    /// trying to make too many columns.
    pub fn table_setup_scroll_freeze(&self, locked_columns: usize, locked_rows: usize) {
        unsafe {
            sys::igTableSetupScrollFreeze(locked_columns as i32, locked_rows as i32);
        }
    }

    /// Along with [table_setup_column](Self::table_setup_column), this method is used
    /// to create a header row and automatically submit a table header for each column.
    ///
    /// For an example of using this method, see [table_setup_column](Self::table_setup_column).
    ///
    /// Headers are required to perform: reordering, sorting, and opening the context menu (though,
    /// the context menu can also be made available in columns body using [TableFlags::CONTEXT_MENU_IN_BODY].
    ///
    /// You may manually submit headers using [table_next_column] + [table_header] calls, but this is
    ///  only useful in some advanced use cases (e.g. adding custom widgets in header row).
    /// See [table_header](Self::table_header) for more information.
    ///
    /// [table_next_column]: Self::table_next_column
    /// [table_header]: Self::table_header
    pub fn table_headers_row(&self) {
        unsafe {
            sys::igTableHeadersRow();
        }
    }

    /// Use this function to manually declare a column cell to be a header.
    ///
    /// You generally should avoid using this outside of specific cases,
    /// such as custom widgets. Instead, use [table_headers_row](Self::table_headers_row)
    /// and [table_setup_column](Self::table_setup_column).
    pub fn table_header(&self, label: impl AsRef<str>) {
        unsafe {
            sys::igTableHeader(self.scratch_txt(label));
        }
    }

    /// Gets the numbers of columns in the current table.
    pub fn table_column_count(&self) -> usize {
        unsafe { sys::igTableGetColumnCount() as usize }
    }

    /// Gets the current column index in the current table.
    pub fn table_column_index(&self) -> usize {
        unsafe { sys::igTableGetColumnIndex() as usize }
    }

    /// Gets the current row index in the current table.
    pub fn table_row_index(&self) -> usize {
        unsafe { sys::igTableGetRowIndex() as usize }
    }

    /// Gets the name of the current column. If there is no currently bound name
    /// for this column, we will return an empty string.
    ///
    /// Use [table_column_name_with_column](Self::table_column_name_with_column)
    /// for arbitrary indices.
    pub fn table_column_name(&mut self) -> &str {
        unsafe {
            // imgui uses utf8...though that is a continuous process there.
            CStr::from_ptr(sys::igTableGetColumnName(-1))
                .to_str()
                .unwrap()
        }
    }

    /// Gets the name of a given column. If there is no currently bound name
    /// for this column, we will return an empty string.
    ///
    /// Use [table_column_name](Self::table_column_name) for the current column.
    pub fn table_column_name_with_column(&mut self, column: usize) -> &str {
        unsafe {
            // imgui uses utf8...though that is a continuous process there.
            CStr::from_ptr(sys::igTableGetColumnName(column as i32))
                .to_str()
                .unwrap()
        }
    }

    /// Gets the flags on the current column in the current table.
    pub fn table_column_flags(&self) -> TableColumnFlags {
        unsafe {
            TableColumnFlags::from_bits(sys::igTableGetColumnFlags(-1) as u32)
                .expect("bad column flags")
        }
    }

    /// Gets the flags on the given column in the current table. To get the current column's
    /// flags without having to call [table_column_index](Self::table_column_index), use
    /// [table_column_flags](Self::table_column_flags).
    pub fn table_column_flags_with_column(&self, column_n: usize) -> TableColumnFlags {
        unsafe {
            TableColumnFlags::from_bits(sys::igTableGetColumnFlags(column_n as i32) as u32)
                .expect("bad column flags")
        }
    }

    /// Sets the given background color for this column. See [TableBgTarget]
    /// for more information on how colors work for tables.
    ///
    /// Use [table_set_bg_color_with_column](Self::table_set_bg_color_with_column) to set
    /// for arbitrary indices.
    pub fn table_set_bg_color(&self, target: TableBgTarget, color: impl Into<ImColor32>) {
        unsafe {
            sys::igTableSetBgColor(target.bits() as i32, color.into().into(), -1);
        }
    }

    /// Sets the given background color for any column. See [TableBgTarget]
    /// for more information on how colors work for tables.
    ///
    /// Use [table_set_bg_color](Self::table_set_bg_color) for the current column.
    pub fn table_set_bg_color_with_column(
        &self,
        target: TableBgTarget,
        color: impl Into<ImColor32>,
        column_index: usize,
    ) {
        unsafe {
            sys::igTableSetBgColor(
                target.bits() as i32,
                color.into().into(),
                column_index as i32,
            );
        }
    }

    /// Change user accessible enabled/disabled state of the current column.
    ///
    /// Set to false to hide the column. Users can use the context menu to change
    /// this themselves by right-clicking in headers, or right-clicking in columns body
    /// if [TableFlags::CONTEXT_MENU_IN_BODY].
    ///
    /// Use [table_set_enabled_with_column](Self::table_set_enabled_with_column) to set
    /// for arbitrary indices.
    pub fn table_set_enabled(&self, enabled: bool) {
        unsafe { sys::igTableSetColumnEnabled(-1, enabled) }
    }

    /// Change user accessible enabled/disabled state of the current column.
    ///
    /// Set to false to hide the column. Users can use the context menu to change
    /// this themselves by right-clicking in headers, or right-clicking in columns body
    /// if [TableFlags::CONTEXT_MENU_IN_BODY].
    pub fn table_set_enabled_with_column(&self, enabled: bool, column_idx: usize) {
        unsafe { sys::igTableSetColumnEnabled(column_idx as i32, enabled) }
    }

    /// Gets the sorting data for a table. This will be `None` when not sorting.
    ///
    /// See the examples folder for how to use the sorting API.
    pub fn table_sort_specs_mut(&self) -> Option<TableSortSpecsMut<'_>> {
        unsafe {
            let value = sys::igTableGetSortSpecs();
            if value.is_null() {
                None
            } else {
                Some(TableSortSpecsMut(value, PhantomData))
            }
        }
    }
}

/// A struct containing all the data needed to setup a table column header
/// via [begin_table_header](Ui::begin_table_header) or [table_setup_column](Ui::table_setup_column).
#[derive(Debug, Default)]
pub struct TableColumnSetup<'a, Name> {
    /// The name of column to be displayed to users.
    pub name: Name,
    /// The flags this column will have.
    pub flags: TableColumnFlags,
    /// The width or weight of the given column.
    pub init_width_or_weight: f32,
    /// A user_id, primarily used in sorting operations.
    pub user_id: Id<'a>,
}

impl<'a, Name: AsRef<str>> TableColumnSetup<'a, Name> {
    pub fn new(name: Name) -> Self {
        Self {
            name,
            flags: TableColumnFlags::empty(),
            init_width_or_weight: 0.0,
            user_id: Id::Int(0),
        }
    }
}

/// A wrapper around table sort specs.
///
/// To use this simply, use [conditional_sort] and provide a closure --
/// if you should sort your data, then the closure will be ran and imgui
/// will be informed that your data is sorted.
///
/// For manual control (such as if sorting can fail), use [should_sort] to
/// check if you should sort your data, sort your data using [specs] for information
/// on how to sort it, and then [set_sorted] to indicate that the data is sorted.
///
/// [conditional_sort]: Self::conditional_sort
/// [should_sort]: Self::should_sort
/// [specs]: Self::specs
/// [set_sorted]: Self::set_sorted
pub struct TableSortSpecsMut<'ui>(*mut sys::ImGuiTableSortSpecs, PhantomData<Ui<'ui>>);

impl TableSortSpecsMut<'_> {
    /// Gets the specs for a given sort. In most scenarios, this will be a slice of 1 entry.
    pub fn specs(&self) -> Specs<'_> {
        let value =
            unsafe { std::slice::from_raw_parts((*self.0).Specs, (*self.0).SpecsCount as usize) };

        Specs(value)
    }

    /// Returns true if the data should be sorted.
    pub fn should_sort(&self) -> bool {
        unsafe { (*self.0).SpecsDirty }
    }

    /// Sets the internal flag that the data has been sorted.
    pub fn set_sorted(&mut self) {
        unsafe {
            (*self.0).SpecsDirty = false;
        }
    }

    /// Provide a closure, which will receive the Specs for a sort.
    ///
    /// If you should sort the data, the closure will run, and ImGui will be
    /// told that the data has been sorted.
    ///
    /// If you need manual control over sorting, consider using [should_sort], [specs],
    /// and [set_sorted] yourself.
    ///
    /// [should_sort]: Self::should_sort
    /// [specs]: Self::specs
    /// [set_sorted]: Self::set_sorted
    pub fn conditional_sort(mut self, mut f: impl FnMut(Specs<'_>)) {
        let is_dirty = self.should_sort();

        if is_dirty {
            f(self.specs());
        }

        self.set_sorted();
    }
}

/// A wrapper around a slice of [TableColumnSortSpecs].
///
/// This slice may be 0 if [TableFlags::SORT_TRISTATE] is true, may be > 1 is [TableFlags::SORT_MULTI] is true,
/// but is generally == 1.
///
/// Consume this struct as an iterator.
pub struct Specs<'a>(&'a [sys::ImGuiTableColumnSortSpecs]);

impl<'a> Specs<'a> {
    pub fn iter(self) -> impl Iterator<Item = TableColumnSortSpecs<'a>> {
        self.0.iter().map(|v| TableColumnSortSpecs(v))
    }
}

pub struct TableColumnSortSpecs<'a>(&'a sys::ImGuiTableColumnSortSpecs);
impl<'a> TableColumnSortSpecs<'a> {
    /// User id of the column (if specified by a TableSetupColumn() call)
    pub fn column_user_id(&self) -> sys::ImGuiID {
        self.0.ColumnUserID
    }

    /// Index of the column
    pub fn column_idx(&self) -> usize {
        self.0.ColumnIndex as usize
    }

    /// Index within parent [Specs] slice where this was found -- always stored in order starting
    /// from 0, tables sorted on a single criteria will always have a 0 here.
    ///
    /// Generally, you don't need to access this, as it's the same as calling `specs.iter().enumerate()`.
    pub fn sort_order(&self) -> usize {
        self.0.SortOrder as usize
    }

    /// Gets the sort direction for the given column. This will nearly always be `Some` if you
    /// can access it.
    pub fn sort_direction(&self) -> Option<TableSortDirection> {
        match self.0.SortDirection() {
            0 => None,
            1 => Some(TableSortDirection::Ascending),
            2 => Some(TableSortDirection::Descending),
            _ => unimplemented!(),
        }
    }
}

create_token!(
    /// Tracks a table which can be rendered onto, ending with `.end()`
    /// or by dropping.
    pub struct TableToken<'ui>;

    /// Ends the table.
    drop { sys::igEndTable() }
);
