// MIT License

// Copyright (c) 2020 Evan Pezent

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// ImPlot v0.8 WIP

#pragma once
#include "imgui.h"

//-----------------------------------------------------------------------------
// Macros and Defines
//-----------------------------------------------------------------------------

// Define attributes of all API symbols declarations (e.g. for DLL under Windows)
// Using ImPlot via a shared library is not recommended, because we don't guarantee
// backward nor forward ABI compatibility and also function call overhead. If you
// do use ImPlot as a DLL, be sure to call SetImGuiContext (details below).
#ifndef IMPLOT_API
#define IMPLOT_API
#endif

// ImPlot version string
#define IMPLOT_VERSION "0.8 WIP"
// Indicates variable should deduced automatically.
#define IMPLOT_AUTO -1
// Special color used to indicate that a color should be deduced automatically.
#define IMPLOT_AUTO_COL ImVec4(0,0,0,-1)

//-----------------------------------------------------------------------------
// Forward Declarations and Basic Types
//-----------------------------------------------------------------------------

// Forward declarations
struct ImPlotContext;          // ImPlot context (opaque struct, see implot_internal.h)

// Enums/Flags
typedef int ImPlotFlags;       // -> enum ImPlotFlags_
typedef int ImPlotAxisFlags;   // -> enum ImPlotAxisFlags_
typedef int ImPlotCol;         // -> enum ImPlotCol_
typedef int ImPlotStyleVar;    // -> enum ImPlotStyleVar_
typedef int ImPlotMarker;      // -> enum ImPlotMarker_
typedef int ImPlotColormap;    // -> enum ImPlotColormap_
typedef int ImPlotLocation;    // -> enum ImPlotLocation_
typedef int ImPlotOrientation; // -> enum ImPlotOrientation_
typedef int ImPlotYAxis;       // -> enum ImPlotYAxis_;

// Options for plots.
enum ImPlotFlags_ {
    ImPlotFlags_None          = 0,       // default
    ImPlotFlags_NoLegend      = 1 << 0,  // the top-left legend will not be displayed
    ImPlotFlags_NoMenus       = 1 << 1,  // the user will not be able to open context menus with double-right click
    ImPlotFlags_NoBoxSelect   = 1 << 2,  // the user will not be able to box-select with right-mouse
    ImPlotFlags_NoMousePos    = 1 << 3,  // the mouse position, in plot coordinates, will not be displayed inside of the plot
    ImPlotFlags_NoHighlight   = 1 << 4,  // plot items will not be highlighted when their legend entry is hovered
    ImPlotFlags_NoChild       = 1 << 5,  // a child window region will not be used to capture mouse scroll (can boost performance for single ImGui window applications)
    ImPlotFlags_YAxis2        = 1 << 6,  // enable a 2nd y-axis on the right side
    ImPlotFlags_YAxis3        = 1 << 7,  // enable a 3rd y-axis on the right side
    ImPlotFlags_Query         = 1 << 8,  // the user will be able to draw query rects with middle-mouse
    ImPlotFlags_Crosshairs    = 1 << 9,  // the default mouse cursor will be replaced with a crosshair when hovered
    ImPlotFlags_AntiAliased   = 1 << 10, // plot lines will be software anti-aliased (not recommended for density plots, prefer MSAA)
    ImPlotFlags_CanvasOnly    = ImPlotFlags_NoLegend | ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect | ImPlotFlags_NoMousePos
};

// Options for plot axes (X and Y).
enum ImPlotAxisFlags_ {
    ImPlotAxisFlags_None          = 0,      // default
    ImPlotAxisFlags_NoGridLines   = 1 << 0, // no grid lines will be displayed
    ImPlotAxisFlags_NoTickMarks   = 1 << 1, // no tick marks will be displayed
    ImPlotAxisFlags_NoTickLabels  = 1 << 2, // no text labels will be displayed
    ImPlotAxisFlags_LogScale      = 1 << 3, // a logartithmic (base 10) axis scale will be used (mutually exclusive with ImPlotAxisFlags_Time)
    ImPlotAxisFlags_Time          = 1 << 4, // axis will display date/time formatted labels (mutually exclusive with ImPlotAxisFlags_LogScale)
    ImPlotAxisFlags_Invert        = 1 << 5, // the axis will be inverted
    ImPlotAxisFlags_LockMin       = 1 << 6, // the axis minimum value will be locked when panning/zooming
    ImPlotAxisFlags_LockMax       = 1 << 7, // the axis maximum value will be locked when panning/zooming
    ImPlotAxisFlags_Lock          = ImPlotAxisFlags_LockMin | ImPlotAxisFlags_LockMax,
    ImPlotAxisFlags_NoDecorations = ImPlotAxisFlags_NoGridLines | ImPlotAxisFlags_NoTickMarks | ImPlotAxisFlags_NoTickLabels
};

// Plot styling colors.
enum ImPlotCol_ {
    // item styling colors
    ImPlotCol_Line,          // plot line/outline color (defaults to next unused color in current colormap)
    ImPlotCol_Fill,          // plot fill color for bars (defaults to the current line color)
    ImPlotCol_MarkerOutline, // marker outline color (defaults to the current line color)
    ImPlotCol_MarkerFill,    // marker fill color (defaults to the current line color)
    ImPlotCol_ErrorBar,      // error bar color (defaults to ImGuiCol_Text)
    // plot styling colors
    ImPlotCol_FrameBg,       // plot frame background color (defaults to ImGuiCol_FrameBg)
    ImPlotCol_PlotBg,        // plot area background color (defaults to ImGuiCol_WindowBg)
    ImPlotCol_PlotBorder,    // plot area border color (defaults to ImGuiCol_Border)
    ImPlotCol_LegendBg,      // legend background color (defaults to ImGuiCol_PopupBg)
    ImPlotCol_LegendBorder,  // legend border color (defaults to ImPlotCol_PlotBorder)
    ImPlotCol_LegendText,    // legend text color (defaults to ImPlotCol_InlayText)
    ImPlotCol_TitleText,     // plot title text color (defaults to ImGuiCol_Text)
    ImPlotCol_InlayText,     // color of text appearing inside of plots (defaults to ImGuiCol_Text)
    ImPlotCol_XAxis,         // x-axis label and tick lables color (defaults to ImGuiCol_Text)
    ImPlotCol_XAxisGrid,     // x-axis grid color (defaults to 25% ImPlotCol_XAxis)
    ImPlotCol_YAxis,         // y-axis label and tick labels color (defaults to ImGuiCol_Text)
    ImPlotCol_YAxisGrid,     // y-axis grid color (defaults to 25% ImPlotCol_YAxis)
    ImPlotCol_YAxis2,        // 2nd y-axis label and tick labels color (defaults to ImGuiCol_Text)
    ImPlotCol_YAxisGrid2,    // 2nd y-axis grid/label color (defaults to 25% ImPlotCol_YAxis2)
    ImPlotCol_YAxis3,        // 3rd y-axis label and tick labels color (defaults to ImGuiCol_Text)
    ImPlotCol_YAxisGrid3,    // 3rd y-axis grid/label color (defaults to 25% ImPlotCol_YAxis3)
    ImPlotCol_Selection,     // box-selection color (defaults to yellow)
    ImPlotCol_Query,         // box-query color (defaults to green)
    ImPlotCol_Crosshairs,    // crosshairs color (defaults to ImPlotCol_PlotBorder)
    ImPlotCol_COUNT
};

// Plot styling variables.
enum ImPlotStyleVar_ {
    // item styling variables
    ImPlotStyleVar_LineWeight,         // float,  plot item line weight in pixels
    ImPlotStyleVar_Marker,             // int,    marker specification
    ImPlotStyleVar_MarkerSize,         // float,  marker size in pixels (roughly the marker's "radius")
    ImPlotStyleVar_MarkerWeight,       // float,  plot outline weight of markers in pixels
    ImPlotStyleVar_FillAlpha,          // float,  alpha modifier applied to all plot item fills
    ImPlotStyleVar_ErrorBarSize,       // float,  error bar whisker width in pixels
    ImPlotStyleVar_ErrorBarWeight,     // float,  error bar whisker weight in pixels
    ImPlotStyleVar_DigitalBitHeight,   // float,  digital channels bit height (at 1) in pixels
    ImPlotStyleVar_DigitalBitGap,      // float,  digital channels bit padding gap in pixels
    // plot styling variables
    ImPlotStyleVar_PlotBorderSize,     // float,  thickness of border around plot area
    ImPlotStyleVar_MinorAlpha,         // float,  alpha multiplier applied to minor axis grid lines
    ImPlotStyleVar_MajorTickLen,       // ImVec2, major tick lengths for X and Y axes
    ImPlotStyleVar_MinorTickLen,       // ImVec2, minor tick lengths for X and Y axes
    ImPlotStyleVar_MajorTickSize,      // ImVec2, line thickness of major ticks
    ImPlotStyleVar_MinorTickSize,      // ImVec2, line thickness of minor ticks
    ImPlotStyleVar_MajorGridSize,      // ImVec2, line thickness of major grid lines
    ImPlotStyleVar_MinorGridSize,      // ImVec2, line thickness of minor grid lines
    ImPlotStyleVar_PlotPadding,        // ImVec2, padding between widget frame and plot area, labels, or outside legends (i.e. main padding)
    ImPlotStyleVar_LabelPadding,       // ImVec2, padding between axes labels, tick labels, and plot edge
    ImPlotStyleVar_LegendPadding,      // ImVec2, legend padding from plot edges
    ImPlotStyleVar_LegendInnerPadding, // ImVec2, legend inner padding from legend edges
    ImPlotStyleVar_LegendSpacing,      // ImVec2, spacing between legend entries
    ImPlotStyleVar_MousePosPadding,    // ImVec2, padding between plot edge and interior info text
    ImPlotStyleVar_AnnotationPadding,  // ImVec2, text padding around annotation labels
    ImPlotStyleVar_PlotDefaultSize,    // ImVec2, default size used when ImVec2(0,0) is passed to BeginPlot
    ImPlotStyleVar_PlotMinSize,        // ImVec2, minimum size plot frame can be when shrunk
    ImPlotStyleVar_COUNT
};

// Marker specifications.
enum ImPlotMarker_ {
    ImPlotMarker_None = -1, // no marker
    ImPlotMarker_Circle,    // a circle marker
    ImPlotMarker_Square,    // a square maker
    ImPlotMarker_Diamond,   // a diamond marker
    ImPlotMarker_Up,        // an upward-pointing triangle marker
    ImPlotMarker_Down,      // an downward-pointing triangle marker
    ImPlotMarker_Left,      // an leftward-pointing triangle marker
    ImPlotMarker_Right,     // an rightward-pointing triangle marker
    ImPlotMarker_Cross,     // a cross marker (not fillable)
    ImPlotMarker_Plus,      // a plus marker (not fillable)
    ImPlotMarker_Asterisk,  // a asterisk marker (not fillable)
    ImPlotMarker_COUNT
};

// Built-in colormaps
enum ImPlotColormap_ {
    ImPlotColormap_Default  = 0,  // ImPlot default colormap         (n=10)
    ImPlotColormap_Deep     = 1,  // a.k.a. seaborn deep             (n=10)
    ImPlotColormap_Dark     = 2,  // a.k.a. matplotlib "Set1"        (n=9)
    ImPlotColormap_Pastel   = 3,  // a.k.a. matplotlib "Pastel1"     (n=9)
    ImPlotColormap_Paired   = 4,  // a.k.a. matplotlib "Paired"      (n=12)
    ImPlotColormap_Viridis  = 5,  // a.k.a. matplotlib "viridis"     (n=11)
    ImPlotColormap_Plasma   = 6,  // a.k.a. matplotlib "plasma"      (n=11)
    ImPlotColormap_Hot      = 7,  // a.k.a. matplotlib/MATLAB "hot"  (n=11)
    ImPlotColormap_Cool     = 8,  // a.k.a. matplotlib/MATLAB "cool" (n=11)
    ImPlotColormap_Pink     = 9,  // a.k.a. matplotlib/MATLAB "pink" (n=11)
    ImPlotColormap_Jet      = 10, // a.k.a. MATLAB "jet"             (n=11)
    ImPlotColormap_COUNT
};

// Used to position items on a plot (e.g. legends, labels, etc.)
enum ImPlotLocation_ {
    ImPlotLocation_Center    = 0,                                          // center-center
    ImPlotLocation_North     = 1 << 0,                                     // top-center
    ImPlotLocation_South     = 1 << 1,                                     // bottom-center
    ImPlotLocation_West      = 1 << 2,                                     // center-left
    ImPlotLocation_East      = 1 << 3,                                     // center-right
    ImPlotLocation_NorthWest = ImPlotLocation_North | ImPlotLocation_West, // top-left
    ImPlotLocation_NorthEast = ImPlotLocation_North | ImPlotLocation_East, // top-right
    ImPlotLocation_SouthWest = ImPlotLocation_South | ImPlotLocation_West, // bottom-left
    ImPlotLocation_SouthEast = ImPlotLocation_South | ImPlotLocation_East  // bottom-right
};

// Used to orient items on a plot (e.g. legends, labels, etc.)
enum ImPlotOrientation_ {
    ImPlotOrientation_Horizontal, // left/right
    ImPlotOrientation_Vertical    // up/down
};

// Enums for different y-axes.
enum ImPlotYAxis_ {
    ImPlotYAxis_1 = 0, // left (default)
    ImPlotYAxis_2 = 1, // first on right side
    ImPlotYAxis_3 = 2  // second on right side
};

// Double precision version of ImVec2 used by ImPlot. Extensible by end users.
struct ImPlotPoint {
    double x, y;
    ImPlotPoint()                         { x = y = 0.0; }
    ImPlotPoint(double _x, double _y)     { x = _x; y = _y; }
    ImPlotPoint(const ImVec2& p)          { x = p.x; y = p.y; }
    double  operator[] (size_t idx) const { return (&x)[idx]; }
    double& operator[] (size_t idx)       { return (&x)[idx]; }
#ifdef IMPLOT_POINT_CLASS_EXTRA
    IMPLOT_POINT_CLASS_EXTRA     // Define additional constructors and implicit cast operators in imconfig.h
                                 // to convert back and forth between your math types and ImPlotPoint.
#endif
};

// A range defined by a min/max value. Used for plot axes ranges.
struct ImPlotRange {
    double Min, Max;
    ImPlotRange()                         { Min = 0; Max = 0; }
    ImPlotRange(double _min, double _max) { Min = _min; Max = _max; }
    bool Contains(double value) const     { return value >= Min && value <= Max; };
    double Size() const                   { return Max - Min; };
};

// Combination of two ranges for X and Y axes.
struct ImPlotLimits {
    ImPlotRange X, Y;
    bool Contains(const ImPlotPoint& p) const { return Contains(p.x, p.y); }
    bool Contains(double x, double y) const   { return X.Contains(x) && Y.Contains(y); }
};

// Plot style structure
struct ImPlotStyle {
    // item styling variables
    float   LineWeight;              // = 1,      item line weight in pixels
    int     Marker;                  // = ImPlotMarker_None, marker specification
    float   MarkerSize;              // = 4,      marker size in pixels (roughly the marker's "radius")
    float   MarkerWeight;            // = 1,      outline weight of markers in pixels
    float   FillAlpha;               // = 1,      alpha modifier applied to plot fills
    float   ErrorBarSize;            // = 5,      error bar whisker width in pixels
    float   ErrorBarWeight;          // = 1.5,    error bar whisker weight in pixels
    float   DigitalBitHeight;        // = 8,      digital channels bit height (at y = 1.0f) in pixels
    float   DigitalBitGap;           // = 4,      digital channels bit padding gap in pixels
    // plot styling variables
    float   PlotBorderSize;          // = 1,      line thickness of border around plot area
    float   MinorAlpha;              // = 0.25    alpha multiplier applied to minor axis grid lines
    ImVec2  MajorTickLen;            // = 10,10   major tick lengths for X and Y axes
    ImVec2  MinorTickLen;            // = 5,5     minor tick lengths for X and Y axes
    ImVec2  MajorTickSize;           // = 1,1     line thickness of major ticks
    ImVec2  MinorTickSize;           // = 1,1     line thickness of minor ticks
    ImVec2  MajorGridSize;           // = 1,1     line thickness of major grid lines
    ImVec2  MinorGridSize;           // = 1,1     line thickness of minor grid lines
    ImVec2  PlotPadding;             // = 10,10   padding between widget frame and plot area, labels, or outside legends (i.e. main padding)
    ImVec2  LabelPadding;            // = 5,5     padding between axes labels, tick labels, and plot edge
    ImVec2  LegendPadding;           // = 10,10   legend padding from plot edges
    ImVec2  LegendInnerPadding;      // = 5,5     legend inner padding from legend edges
    ImVec2  LegendSpacing;           // = 0,0     spacing between legend entries
    ImVec2  MousePosPadding;         // = 10,10   padding between plot edge and interior mouse location text
    ImVec2  AnnotationPadding;       // = 2,2     text padding around annotation labels
    ImVec2  PlotDefaultSize;         // = 400,300 default size used when ImVec2(0,0) is passed to BeginPlot
    ImVec2  PlotMinSize;             // = 300,225 minimum size plot frame can be when shrunk
    // colors
    ImVec4  Colors[ImPlotCol_COUNT]; //           array of plot specific colors
    // settings/flags
    bool    AntiAliasedLines;        // = false,  enable global anti-aliasing on plot lines (overrides ImPlotFlags_AntiAliased)
    bool    UseLocalTime;            // = false,  axis labels will be formatted for your timezone when ImPlotAxisFlag_Time is enabled
    bool    UseISO8601;              // = false,  dates will be formatted according to ISO 8601 where applicable (e.g. YYYY-MM-DD, YYYY-MM, --MM-DD, etc.)
    bool    Use24HourClock;          // = false,  times will be formatted using a 24 hour clock
    IMPLOT_API ImPlotStyle();
};

// Input mapping structure, default values listed in the comments.
struct ImPlotInputMap {
    ImGuiMouseButton PanButton;             // LMB      enables panning when held
    ImGuiKeyModFlags PanMod;                // none     optional modifier that must be held for panning
    ImGuiMouseButton FitButton;             // LMB      fits visible data when double clicked
    ImGuiMouseButton ContextMenuButton;     // RMB      opens plot context menu (if enabled) when double clicked
    ImGuiMouseButton BoxSelectButton;       // RMB      begins box selection when pressed and confirms selection when released
    ImGuiKeyModFlags BoxSelectMod;          // none     optional modifier that must be held for box selection
    ImGuiMouseButton BoxSelectCancelButton; // LMB      cancels active box selection when pressed
    ImGuiMouseButton QueryButton;           // MMB      begins query selection when pressed and end query selection when released
    ImGuiKeyModFlags QueryMod;              // none     optional modifier that must be held for query selection
    ImGuiKeyModFlags QueryToggleMod;        // Ctrl     when held, active box selections turn into queries
    ImGuiKeyModFlags HorizontalMod;         // Alt      expands active box selection/query horizontally to plot edge when held
    ImGuiKeyModFlags VerticalMod;           // Shift    expands active box selection/query vertically to plot edge when held
    IMPLOT_API ImPlotInputMap();
};

//-----------------------------------------------------------------------------
// ImPlot End-User API
//-----------------------------------------------------------------------------

namespace ImPlot {

//-----------------------------------------------------------------------------
// ImPlot Context
//-----------------------------------------------------------------------------

// Creates a new ImPlot context. Call this after ImGui::CreateContext.
IMPLOT_API ImPlotContext* CreateContext();
// Destroys an ImPlot context. Call this before ImGui::DestroyContext. NULL = destroy current context
IMPLOT_API void DestroyContext(ImPlotContext* ctx = NULL);
// Returns the current ImPlot context. NULL if no context has ben set.
IMPLOT_API ImPlotContext* GetCurrentContext();
// Sets the current ImPlot context.
IMPLOT_API void SetCurrentContext(ImPlotContext* ctx);

//-----------------------------------------------------------------------------
// Begin/End Plot
//-----------------------------------------------------------------------------

// Starts a 2D plotting context. If this function returns true, EndPlot() must
// be called, e.g. "if (BeginPlot(...)) { ... EndPlot(); }". #title_id must
// be unique. If you need to avoid ID collisions or don't want to display a
// title in the plot, use double hashes (e.g. "MyPlot##Hidden" or "##NoTitle").
// If #x_label and/or #y_label are provided, axes labels will be displayed.
IMPLOT_API bool BeginPlot(const char* title_id,
                          const char* x_label      = NULL,
                          const char* y_label      = NULL,
                          const ImVec2& size       = ImVec2(-1,0),
                          ImPlotFlags flags        = ImPlotFlags_None,
                          ImPlotAxisFlags x_flags  = ImPlotAxisFlags_None,
                          ImPlotAxisFlags y_flags  = ImPlotAxisFlags_None,
                          ImPlotAxisFlags y2_flags = ImPlotAxisFlags_NoGridLines,
                          ImPlotAxisFlags y3_flags = ImPlotAxisFlags_NoGridLines);

// Only call EndPlot() if BeginPlot() returns true! Typically called at the end
// of an if statement conditioned on BeginPlot().
IMPLOT_API void EndPlot();

//-----------------------------------------------------------------------------
// Plot Items
//-----------------------------------------------------------------------------

// The template functions below are explicitly instantiated in implot_items.cpp.
// They are not intended to be used generically with custom types. You will get
// a linker error if you try! All functions support the following scalar types:
//
// float, double, ImS8, ImU8, ImS16, ImU16, ImS32, ImU32, ImS64, ImU64
//
//
// If you need to plot custom or non-homogenous data you have a few options:
//
// 1. If your data is a simple struct/class (e.g. Vector2f), you can use striding.
//    This is the most performant option if applicable.
//
//    struct Vector2f { float X, Y; };
//    ...
//    Vector2f data[42];
//    ImPlot::PlotLine("line", &data[0].x, &data[0].y, 42, 0, sizeof(Vector2f)); // or sizeof(float)*2
//
// 2. Write a custom getter function or C++ lambda and pass it and your data to
//    an ImPlot function post-fixed with a G (e.g. PlotScatterG). This has a
//    slight performance cost, but probably not enough to worry about.
//
//    ImPlotPoint MyDataGetter(void* data, int idx) {
//        MyData* my_data = (MyData*)data;
//        ImPlotPoint p;
//        p.x = my_data->GetTime(idx);
//        p.y = my_data->GetValue(idx);
//        return p
//    }
//    ...
//    MyData my_data;
//    ImPlot::PlotScatterG("scatter", MyDataGetter, &my_data, my_data.Size());
//
// NB: All types are converted to double before plotting. You may loose information
// if you try plotting extremely large 64-bit integral types. Proceed with caution!

// Plots a standard 2D line plot.
template <typename T> IMPLOT_API void PlotLine(const char* label_id, const T* values, int count, double xscale=1, double x0=0, int offset=0, int stride=sizeof(T));
template <typename T> IMPLOT_API void PlotLine(const char* label_id, const T* xs, const T* ys, int count, int offset=0, int stride=sizeof(T));
                      IMPLOT_API void PlotLineG(const char* label_id, ImPlotPoint (*getter)(void* data, int idx), void* data, int count, int offset=0);

// Plots a standard 2D scatter plot. Default marker is ImPlotMarker_Circle.
template <typename T> IMPLOT_API  void PlotScatter(const char* label_id, const T* values, int count, double xscale=1, double x0=0, int offset=0, int stride=sizeof(T));
template <typename T> IMPLOT_API  void PlotScatter(const char* label_id, const T* xs, const T* ys, int count, int offset=0, int stride=sizeof(T));
                      IMPLOT_API  void PlotScatterG(const char* label_id, ImPlotPoint (*getter)(void* data, int idx), void* data, int count, int offset=0);

// Plots a a stairstep graph. The y value is continued constantly from every x position, i.e. the interval [x[i], x[i+1]) has the value y[i].
template <typename T> IMPLOT_API void PlotStairs(const char* label_id, const T* values, int count, double xscale=1, double x0=0, int offset=0, int stride=sizeof(T));
template <typename T> IMPLOT_API void PlotStairs(const char* label_id, const T* xs, const T* ys, int count, int offset=0, int stride=sizeof(T));
                      IMPLOT_API void PlotStairsG(const char* label_id, ImPlotPoint (*getter)(void* data, int idx), void* data, int count, int offset=0);

// Plots a shaded (filled) region between two lines, or a line and a horizontal reference.
template <typename T> IMPLOT_API void PlotShaded(const char* label_id, const T* values, int count, double y_ref=0, double xscale=1, double x0=0, int offset=0, int stride=sizeof(T));
template <typename T> IMPLOT_API void PlotShaded(const char* label_id, const T* xs, const T* ys, int count, double y_ref=0, int offset=0, int stride=sizeof(T));
template <typename T> IMPLOT_API void PlotShaded(const char* label_id, const T* xs, const T* ys1, const T* ys2, int count, int offset=0, int stride=sizeof(T));
                      IMPLOT_API void PlotShadedG(const char* label_id, ImPlotPoint (*getter1)(void* data, int idx), void* data1, ImPlotPoint (*getter2)(void* data, int idx), void* data2, int count, int offset=0);

// Plots a vertical bar graph. #width and #shift are in X units.
template <typename T> IMPLOT_API void PlotBars(const char* label_id, const T* values, int count, double width=0.67, double shift=0, int offset=0, int stride=sizeof(T));
template <typename T> IMPLOT_API void PlotBars(const char* label_id, const T* xs, const T* ys, int count, double width, int offset=0, int stride=sizeof(T));
                      IMPLOT_API void PlotBarsG(const char* label_id, ImPlotPoint (*getter)(void* data, int idx), void* data, int count, double width, int offset=0);

// Plots a horizontal bar graph. #height and #shift are in Y units.
template <typename T> IMPLOT_API void PlotBarsH(const char* label_id, const T* values, int count, double height=0.67, double shift=0, int offset=0, int stride=sizeof(T));
template <typename T> IMPLOT_API void PlotBarsH(const char* label_id, const T* xs, const T* ys, int count, double height, int offset=0, int stride=sizeof(T));
                      IMPLOT_API void PlotBarsHG(const char* label_id, ImPlotPoint (*getter)(void* data, int idx), void* data, int count, double height,  int offset=0);

// Plots vertical error bar. The label_id should be the same as the label_id of the associated line or bar plot.
template <typename T> IMPLOT_API void PlotErrorBars(const char* label_id, const T* xs, const T* ys, const T* err, int count, int offset=0, int stride=sizeof(T));
template <typename T> IMPLOT_API void PlotErrorBars(const char* label_id, const T* xs, const T* ys, const T* neg, const T* pos, int count, int offset=0, int stride=sizeof(T));

// Plots horizontal error bars. The label_id should be the same as the label_id of the associated line or bar plot.
template <typename T> IMPLOT_API void PlotErrorBarsH(const char* label_id, const T* xs, const T* ys, const T* err, int count, int offset=0, int stride=sizeof(T));
template <typename T> IMPLOT_API void PlotErrorBarsH(const char* label_id, const T* xs, const T* ys, const T* neg, const T* pos, int count, int offset=0, int stride=sizeof(T));

/// Plots vertical stems.
template <typename T> IMPLOT_API void PlotStems(const char* label_id, const T* values, int count, double y_ref=0, double xscale=1, double x0=0, int offset=0, int stride=sizeof(T));
template <typename T> IMPLOT_API void PlotStems(const char* label_id, const T* xs, const T* ys, int count, double y_ref=0, int offset=0, int stride=sizeof(T));

// Plots a pie chart. If the sum of values > 1 or normalize is true, each value will be normalized. Center and radius are in plot units. #label_fmt can be set to NULL for no labels.
template <typename T> IMPLOT_API void PlotPieChart(const char* const label_ids[], const T* values, int count, double x, double y, double radius, bool normalize=false, const char* label_fmt="%.1f", double angle0=90);

// Plots a 2D heatmap chart. Values are expected to be in row-major order. #label_fmt can be set to NULL for no labels.
template <typename T> IMPLOT_API void PlotHeatmap(const char* label_id, const T* values, int rows, int cols, double scale_min, double scale_max, const char* label_fmt="%.1f", const ImPlotPoint& bounds_min=ImPlotPoint(0,0), const ImPlotPoint& bounds_max=ImPlotPoint(1,1));

// Plots digital data. Digital plots do not respond to y drag or zoom, and are always referenced to the bottom of the plot.
template <typename T> IMPLOT_API void PlotDigital(const char* label_id, const T* xs, const T* ys, int count, int offset=0, int stride=sizeof(T));
                      IMPLOT_API void PlotDigitalG(const char* label_id, ImPlotPoint (*getter)(void* data, int idx), void* data, int count, int offset=0);

// Plots an axis-aligned image. #bounds_min/bounds_max are in plot coordinatse (y-up) and #uv0/uv1 are in texture coordinates (y-down).
IMPLOT_API void PlotImage(const char* label_id, ImTextureID user_texture_id, const ImPlotPoint& bounds_min, const ImPlotPoint& bounds_max, const ImVec2& uv0=ImVec2(0,0), const ImVec2& uv1=ImVec2(1,1), const ImVec4& tint_col=ImVec4(1,1,1,1));

// Plots a centered text label at point x,y with optional pixel offset. Text color can be changed with ImPlot::PushStyleColor(ImPlotCol_InlayText, ...).
IMPLOT_API void PlotText(const char* text, double x, double y, bool vertical=false, const ImVec2& pix_offset=ImVec2(0,0));

// Plots an dummy item (i.e. adds a legend entry colored by ImPlotCol_Line)
IMPLOT_API void PlotDummy(const char* label_id);

//-----------------------------------------------------------------------------
// Plot Utils
//-----------------------------------------------------------------------------

// The following functions MUST be called before BeginPlot!

// Set the axes range limits of the next plot. Call right before BeginPlot(). If ImGuiCond_Always is used, the axes limits will be locked.
IMPLOT_API void SetNextPlotLimits(double xmin, double xmax, double ymin, double ymax, ImGuiCond cond = ImGuiCond_Once);
// Set the X axis range limits of the next plot. Call right before BeginPlot(). If ImGuiCond_Always is used, the X axis limits will be locked.
IMPLOT_API void SetNextPlotLimitsX(double xmin, double xmax, ImGuiCond cond = ImGuiCond_Once);
// Set the Y axis range limits of the next plot. Call right before BeginPlot(). If ImGuiCond_Always is used, the Y axis limits will be locked.
IMPLOT_API void SetNextPlotLimitsY(double ymin, double ymax, ImGuiCond cond = ImGuiCond_Once, ImPlotYAxis y_axis = 0);
// Links the next plot limits to external values. Set to NULL for no linkage. The pointer data must remain valid until the matching call EndPlot.
IMPLOT_API void LinkNextPlotLimits(double* xmin, double* xmax, double* ymin, double* ymax, double* ymin2 = NULL, double* ymax2 = NULL, double* ymin3 = NULL, double* ymax3 = NULL);
// Fits the next plot axes to all plotted data if they are unlocked (equivalent to double-clicks).
IMPLOT_API void FitNextPlotAxes(bool x = true, bool y = true, bool y2 = true, bool y3 = true);

// Set the X axis ticks and optionally the labels for the next plot.
IMPLOT_API void SetNextPlotTicksX(const double* values, int n_ticks, const char* const labels[] = NULL, bool show_default = false);
IMPLOT_API void SetNextPlotTicksX(double x_min, double x_max, int n_ticks, const char* const labels[] = NULL, bool show_default = false);

// Set the Y axis ticks and optionally the labels for the next plot.
IMPLOT_API void SetNextPlotTicksY(const double* values, int n_ticks, const char* const labels[] = NULL, bool show_default = false, ImPlotYAxis y_axis = 0);
IMPLOT_API void SetNextPlotTicksY(double y_min, double y_max, int n_ticks, const char* const labels[] = NULL, bool show_default = false, ImPlotYAxis y_axis = 0);

// The following functions MUST be called between Begin/EndPlot!

// Select which Y axis will be used for subsequent plot elements. The default is ImPlotYAxis_1, or the first (left) Y axis. Enable 2nd and 3rd axes with ImPlotFlags_YAxisX.
IMPLOT_API void SetPlotYAxis(ImPlotYAxis y_axis);
// Hides or shows the next plot item (i.e. as if it were toggled from the legend). Use ImGuiCond_Always if you need to forcefully set this every frame.
IMPLOT_API void HideNextItem(bool hidden = true, ImGuiCond cond = ImGuiCond_Once);

// Convert pixels to a position in the current plot's coordinate system. A negative y_axis uses the current value of SetPlotYAxis (ImPlotYAxis_1 initially).
IMPLOT_API ImPlotPoint PixelsToPlot(const ImVec2& pix, ImPlotYAxis y_axis = IMPLOT_AUTO);
IMPLOT_API ImPlotPoint PixelsToPlot(float x, float y, ImPlotYAxis y_axis = IMPLOT_AUTO);
// Convert a position in the current plot's coordinate system to pixels. A negative y_axis uses the current value of SetPlotYAxis (ImPlotYAxis_1 initially).
IMPLOT_API ImVec2 PlotToPixels(const ImPlotPoint& plt, ImPlotYAxis y_axis = IMPLOT_AUTO);
IMPLOT_API ImVec2 PlotToPixels(double x, double y, ImPlotYAxis y_axis = IMPLOT_AUTO);
// Get the current Plot position (top-left) in pixels.
IMPLOT_API ImVec2 GetPlotPos();
// Get the curent Plot size in pixels.
IMPLOT_API ImVec2 GetPlotSize();
// Returns true if the plot area in the current plot is hovered.
IMPLOT_API bool IsPlotHovered();
// Returns true if the XAxis plot area in the current plot is hovered.
IMPLOT_API bool IsPlotXAxisHovered();
// Returns true if the YAxis[n] plot area in the current plot is hovered.
IMPLOT_API bool IsPlotYAxisHovered(ImPlotYAxis y_axis = 0);
// Returns the mouse position in x,y coordinates of the current plot. A negative y_axis uses the current value of SetPlotYAxis (ImPlotYAxis_1 initially).
IMPLOT_API ImPlotPoint GetPlotMousePos(ImPlotYAxis y_axis = IMPLOT_AUTO);
// Returns the current plot axis range. A negative y_axis uses the current value of SetPlotYAxis (ImPlotYAxis_1 initially).
IMPLOT_API ImPlotLimits GetPlotLimits(ImPlotYAxis y_axis = IMPLOT_AUTO);

// Returns true if the current plot is being queried. Query must be enabled with ImPlotFlags_Query.
IMPLOT_API bool IsPlotQueried();
// Returns the current plot query bounds. Query must be enabled with ImPlotFlags_Query.
IMPLOT_API ImPlotLimits GetPlotQuery(ImPlotYAxis y_axis = IMPLOT_AUTO);

//-----------------------------------------------------------------------------
// Plot Tools
//-----------------------------------------------------------------------------

// The following functions MUST be called between Begin/EndPlot!

// Shows an annotation callout at a chosen point.
IMPLOT_API void Annotate(double x, double y, const ImVec2& pix_offset, const char* fmt, ...)                                       IM_FMTARGS(4);
IMPLOT_API void Annotate(double x, double y, const ImVec2& pix_offset, const ImVec4& color, const char* fmt, ...)                  IM_FMTARGS(5);
IMPLOT_API void AnnotateV(double x, double y, const ImVec2& pix_offset, const char* fmt, va_list args)                             IM_FMTLIST(4);
IMPLOT_API void AnnotateV(double x, double y, const ImVec2& pix_offset, const ImVec4& color, const char* fmt, va_list args)        IM_FMTLIST(5);

// Same as above, but the annotation will always be clamped to stay inside the plot area.
IMPLOT_API void AnnotateClamped(double x, double y, const ImVec2& pix_offset, const char* fmt, ...)                                IM_FMTARGS(4);
IMPLOT_API void AnnotateClamped(double x, double y, const ImVec2& pix_offset, const ImVec4& color, const char* fmt, ...)           IM_FMTARGS(5);
IMPLOT_API void AnnotateClampedV(double x, double y, const ImVec2& pix_offset, const char* fmt, va_list args)                      IM_FMTLIST(4);
IMPLOT_API void AnnotateClampedV(double x, double y, const ImVec2& pix_offset, const ImVec4& color, const char* fmt, va_list args) IM_FMTLIST(5);

// Shows a draggable vertical guide line at an x-value. #col defaults to ImGuiCol_Text.
IMPLOT_API bool DragLineX(const char* id, double* x_value, bool show_label = true, const ImVec4& col = IMPLOT_AUTO_COL, float thickness = 1);
// Shows a draggable horizontal guide line at a y-value. #col defaults to ImGuiCol_Text.
IMPLOT_API bool DragLineY(const char* id, double* y_value, bool show_label = true, const ImVec4& col = IMPLOT_AUTO_COL, float thickness = 1);
// Shows a draggable point at x,y. #col defaults to ImGuiCol_Text.
IMPLOT_API bool DragPoint(const char* id, double* x, double* y, bool show_label = true, const ImVec4& col = IMPLOT_AUTO_COL, float radius = 4);

//-----------------------------------------------------------------------------
// Legend Utils and Tools
//-----------------------------------------------------------------------------

// The following functions MUST be called between Begin/EndPlot!

// Set the location of the current plot's legend.
IMPLOT_API void SetLegendLocation(ImPlotLocation location, ImPlotOrientation orientation = ImPlotOrientation_Vertical, bool outside = false);
// Set the location of the current plot's mouse position text (default = South|East).
IMPLOT_API void SetMousePosLocation(ImPlotLocation location);
// Returns true if a plot item legend entry is hovered.
IMPLOT_API bool IsLegendEntryHovered(const char* label_id);
// Begin a drag and drop source from a legend entry. The only supported flag is SourceNoPreviewTooltip
IMPLOT_API bool BeginLegendDragDropSource(const char* label_id, ImGuiDragDropFlags flags = 0);
// End legend drag and drop source.
IMPLOT_API void EndLegendDragDropSource();
// Begin a popup for a legend entry.
IMPLOT_API bool BeginLegendPopup(const char* label_id, ImGuiMouseButton mouse_button = 1);
// End a popup for a legend entry.
IMPLOT_API void EndLegendPopup();

//-----------------------------------------------------------------------------
// Plot and Item Styling
//-----------------------------------------------------------------------------

// Provides access to plot style structure for permanant modifications to colors, sizes, etc.
IMPLOT_API ImPlotStyle& GetStyle();

// Style colors for current ImGui style (default).
IMPLOT_API void StyleColorsAuto(ImPlotStyle* dst = NULL);
// Style colors for ImGui "Classic".
IMPLOT_API void StyleColorsClassic(ImPlotStyle* dst = NULL);
// Style colors for ImGui "Dark".
IMPLOT_API void StyleColorsDark(ImPlotStyle* dst = NULL);
// Style colors for ImGui "Light".
IMPLOT_API void StyleColorsLight(ImPlotStyle* dst = NULL);

// Use PushStyleX to temporarily modify your ImPlotStyle. The modification
// will last until the matching call to PopStyleX. You MUST call a pop for
// every push, otherwise you will leak memory! This behaves just like ImGui.

// Temporarily modify a plot color. Don't forget to call PopStyleColor!
IMPLOT_API void PushStyleColor(ImPlotCol idx, ImU32 col);
IMPLOT_API void PushStyleColor(ImPlotCol idx, const ImVec4& col);
// Undo temporary color modification. Undo multiple pushes at once by increasing count.
IMPLOT_API void PopStyleColor(int count = 1);

// Temporarily modify a style variable of float type. Don't forget to call PopStyleVar!
IMPLOT_API void PushStyleVar(ImPlotStyleVar idx, float val);
// Temporarily modify a style variable of int type. Don't forget to call PopStyleVar!
IMPLOT_API void PushStyleVar(ImPlotStyleVar idx, int val);
// Temporarily modify a style variable of ImVec2 type. Don't forget to call PopStyleVar!
IMPLOT_API void PushStyleVar(ImPlotStyleVar idx, const ImVec2& val);
// Undo temporary style modification. Undo multiple pushes at once by increasing count.
IMPLOT_API void PopStyleVar(int count = 1);

// The following can be used to modify the style of the next plot item ONLY. They do
// NOT require calls to PopStyleX. Leave style attributes you don't want modified to
// IMPLOT_AUTO or IMPLOT_AUTO_COL. Automatic styles will be deduced from the current
// values in your ImPlotStyle or from Colormap data.

// Set the line color and weight for the next item only.
IMPLOT_API void SetNextLineStyle(const ImVec4& col = IMPLOT_AUTO_COL, float weight = IMPLOT_AUTO);
// Set the fill color for the next item only.
IMPLOT_API void SetNextFillStyle(const ImVec4& col = IMPLOT_AUTO_COL, float alpha_mod = IMPLOT_AUTO);
// Set the marker style for the next item only.
IMPLOT_API void SetNextMarkerStyle(ImPlotMarker marker = IMPLOT_AUTO, float size = IMPLOT_AUTO, const ImVec4& fill = IMPLOT_AUTO_COL, float weight = IMPLOT_AUTO, const ImVec4& outline = IMPLOT_AUTO_COL);
// Set the error bar style for the next item only.
IMPLOT_API void SetNextErrorBarStyle(const ImVec4& col = IMPLOT_AUTO_COL, float size = IMPLOT_AUTO, float weight = IMPLOT_AUTO);

// Gets the last item primary color (i.e. its legend icon color)
IMPLOT_API ImVec4 GetLastItemColor();

// Returns the null terminated string name for an ImPlotCol.
IMPLOT_API const char* GetStyleColorName(ImPlotCol idx);
// Returns the null terminated string name for an ImPlotMarker.
IMPLOT_API const char* GetMarkerName(ImPlotMarker idx);

//-----------------------------------------------------------------------------
// Colormaps
//-----------------------------------------------------------------------------

// Item styling is based on Colormaps when the relevant ImPlotCol is set to
// IMPLOT_AUTO_COL (default). Several built in colormaps are available and can be
// toggled in the demo. You can push/pop or set your own colormaps as well.

// The Colormap data will be ignored and a custom color will be used if you have either:
//     1) Modified an item style color in your ImPlotStyle to anything but IMPLOT_AUTO_COL.
//     2) Pushed an item style color using PushStyleColor().
//     3) Set the next item style with a SetNextXStyle function.

// Temporarily switch to one of the built-in colormaps.
IMPLOT_API void PushColormap(ImPlotColormap colormap);
// Temporarily switch to your custom colormap. The pointer data must persist until the matching call to PopColormap!
IMPLOT_API void PushColormap(const ImVec4* colormap, int size);
// Undo temporary colormap modification.
IMPLOT_API void PopColormap(int count = 1);

// Permanently sets a custom colormap. The colors will be copied to internal memory. Prefer PushColormap instead of calling this each frame.
IMPLOT_API void SetColormap(const ImVec4* colormap, int size);
// Permanently switch to one of the built-in colormaps. If samples is greater than 1, the map will be linearly resampled. Don't call this each frame.
IMPLOT_API void SetColormap(ImPlotColormap colormap, int samples = 0);

// Returns the size of the current colormap.
IMPLOT_API int GetColormapSize();
// Returns a color from the Color map given an index >= 0 (modulo will be performed).
IMPLOT_API ImVec4 GetColormapColor(int index);
// Linearly interpolates a color from the current colormap given t between 0 and 1.
IMPLOT_API ImVec4 LerpColormap(float t);
// Returns the next unused colormap color and advances the colormap. Can be used to skip colors if desired.
IMPLOT_API ImVec4 NextColormapColor();

// Renders a vertical color scale using the current color map. Call this outside of Begin/EndPlot.
IMPLOT_API void ShowColormapScale(double scale_min, double scale_max, float height);

// Returns a null terminated string name for a built-in colormap.
IMPLOT_API const char* GetColormapName(ImPlotColormap colormap);

//-----------------------------------------------------------------------------
// Miscellaneous
//-----------------------------------------------------------------------------

// Allows changing how keyboard/mouse interaction works.
IMPLOT_API ImPlotInputMap& GetInputMap();

// Get the plot draw list for rendering to the current plot area.
IMPLOT_API ImDrawList* GetPlotDrawList();
// Push clip rect for rendering to current plot area.
IMPLOT_API void PushPlotClipRect();
// Pop plot clip rect.
IMPLOT_API void PopPlotClipRect();

// Shows ImPlot style selector dropdown menu.
IMPLOT_API bool ShowStyleSelector(const char* label);
// Shows ImPlot colormap selector dropdown menu.
IMPLOT_API bool ShowColormapSelector(const char* label);
// Shows ImPlot style editor block (not a window).
IMPLOT_API void ShowStyleEditor(ImPlotStyle* ref = NULL);
// Add basic help/info block (not a window): how to manipulate ImPlot as an end-user.
IMPLOT_API void ShowUserGuide();
// Shows ImPlot metrics/debug information.
IMPLOT_API void ShowMetricsWindow(bool* p_popen = NULL);

// Sets the current _ImGui_ context. This is ONLY necessary if you are compiling
// ImPlot as a DLL (not recommended) separate from your ImGui compilation. It
// sets the global variable GImGui, which is not shared across DLL boundaries.
// See GImGui documentation in imgui.cpp for more details.
IMPLOT_API void SetImGuiContext(ImGuiContext* ctx);

//-----------------------------------------------------------------------------
// Demo (add implot_demo.cpp to your sources!)
//-----------------------------------------------------------------------------

// Shows the ImPlot demo. Pass the current ImGui context if ImPlot is a DLL.
IMPLOT_API void ShowDemoWindow(bool* p_open = NULL);

}  // namespace ImPlot
