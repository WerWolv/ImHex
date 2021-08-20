// MIT License

// Copyright (c) 2021 Evan Pezent

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

// ImPlot v0.11 WIP

#pragma once
#include "imgui.h"

//-----------------------------------------------------------------------------
// Macros and Defines
//-----------------------------------------------------------------------------

// Define attributes of all API symbols declarations (e.g. for DLL under Windows)
// Using ImPlot via a shared library is not recommended, because we don't guarantee
// backward nor forward ABI compatibility and also function call overhead. If you
// do use ImPlot as a DLL, be sure to call SetImGuiContext (see Miscellanous section).
#ifndef IMPLOT_API
#define IMPLOT_API
#endif

// ImPlot version string
#define IMPLOT_VERSION "0.11 WIP"
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
typedef int ImPlotFlags;        // -> enum ImPlotFlags_
typedef int ImPlotAxisFlags;    // -> enum ImPlotAxisFlags_
typedef int ImPlotSubplotFlags; // -> enum ImPlotSubplotFlags_
typedef int ImPlotCol;          // -> enum ImPlotCol_
typedef int ImPlotStyleVar;     // -> enum ImPlotStyleVar_
typedef int ImPlotMarker;       // -> enum ImPlotMarker_
typedef int ImPlotColormap;     // -> enum ImPlotColormap_
typedef int ImPlotLocation;     // -> enum ImPlotLocation_
typedef int ImPlotOrientation;  // -> enum ImPlotOrientation_
typedef int ImPlotYAxis;        // -> enum ImPlotYAxis_;
typedef int ImPlotBin;          // -> enum ImPlotBin_

// Options for plots (see BeginPlot).
enum ImPlotFlags_ {
    ImPlotFlags_None          = 0,       // default
    ImPlotFlags_NoTitle       = 1 << 0,  // the plot title will not be displayed (titles are also hidden if preceeded by double hashes, e.g. "##MyPlot")
    ImPlotFlags_NoLegend      = 1 << 1,  // the legend will not be displayed
    ImPlotFlags_NoMenus       = 1 << 2,  // the user will not be able to open context menus with right-click
    ImPlotFlags_NoBoxSelect   = 1 << 3,  // the user will not be able to box-select with right-click drag
    ImPlotFlags_NoMousePos    = 1 << 4,  // the mouse position, in plot coordinates, will not be displayed inside of the plot
    ImPlotFlags_NoHighlight   = 1 << 5,  // plot items will not be highlighted when their legend entry is hovered
    ImPlotFlags_NoChild       = 1 << 6,  // a child window region will not be used to capture mouse scroll (can boost performance for single ImGui window applications)
    ImPlotFlags_Equal         = 1 << 7,  // primary x and y axes will be constrained to have the same units/pixel (does not apply to auxiliary y-axes)
    ImPlotFlags_YAxis2        = 1 << 8,  // enable a 2nd y-axis on the right side
    ImPlotFlags_YAxis3        = 1 << 9,  // enable a 3rd y-axis on the right side
    ImPlotFlags_Query         = 1 << 10, // the user will be able to draw query rects with middle-mouse or CTRL + right-click drag
    ImPlotFlags_Crosshairs    = 1 << 11, // the default mouse cursor will be replaced with a crosshair when hovered
    ImPlotFlags_AntiAliased   = 1 << 12, // plot lines will be software anti-aliased (not recommended for high density plots, prefer MSAA)
    ImPlotFlags_CanvasOnly    = ImPlotFlags_NoTitle | ImPlotFlags_NoLegend | ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect | ImPlotFlags_NoMousePos
};

// Options for plot axes (see BeginPlot).
enum ImPlotAxisFlags_ {
    ImPlotAxisFlags_None          = 0,       // default
    ImPlotAxisFlags_NoLabel       = 1 << 0,  // the axis label will not be displayed (axis labels also hidden if the supplied string name is NULL)
    ImPlotAxisFlags_NoGridLines   = 1 << 1,  // no grid lines will be displayed
    ImPlotAxisFlags_NoTickMarks   = 1 << 2,  // no tick marks will be displayed
    ImPlotAxisFlags_NoTickLabels  = 1 << 3,  // no text labels will be displayed
    ImPlotAxisFlags_Foreground    = 1 << 4,  // grid lines will be displayed in the foreground (i.e. on top of data) in stead of the background
    ImPlotAxisFlags_LogScale      = 1 << 5,  // a logartithmic (base 10) axis scale will be used (mutually exclusive with ImPlotAxisFlags_Time)
    ImPlotAxisFlags_Time          = 1 << 6,  // axis will display date/time formatted labels (mutually exclusive with ImPlotAxisFlags_LogScale)
    ImPlotAxisFlags_Invert        = 1 << 7,  // the axis will be inverted
    ImPlotAxisFlags_NoInitialFit  = 1 << 8,  // axis will not be initially fit to data extents on the first rendered frame (also the case if SetNextPlotLimits explicitly called)
    ImPlotAxisFlags_AutoFit       = 1 << 9,  // axis will be auto-fitting to data extents
    ImPlotAxisFlags_RangeFit      = 1 << 10, // axis will only fit points if the point is in the visible range of the **orthoganol** axis
    ImPlotAxisFlags_LockMin       = 1 << 11, // the axis minimum value will be locked when panning/zooming
    ImPlotAxisFlags_LockMax       = 1 << 12, // the axis maximum value will be locked when panning/zooming
    ImPlotAxisFlags_Lock          = ImPlotAxisFlags_LockMin | ImPlotAxisFlags_LockMax,
    ImPlotAxisFlags_NoDecorations = ImPlotAxisFlags_NoLabel | ImPlotAxisFlags_NoGridLines | ImPlotAxisFlags_NoTickMarks | ImPlotAxisFlags_NoTickLabels
};

// Options for subplots (see BeginSubplot).
enum ImPlotSubplotFlags_ {
    ImPlotSubplotFlags_None        = 0,       // default
    ImPlotSubplotFlags_NoTitle     = 1 << 0,  // the subplot title will not be displayed (titles are also hidden if preceeded by double hashes, e.g. "##MySubplot")
    ImPlotSubplotFlags_NoLegend    = 1 << 1,  // the legend will not be displayed (only applicable if ImPlotSubplotFlags_ShareItems is enabled)
    ImPlotSubplotFlags_NoMenus     = 1 << 2,  // the user will not be able to open context menus with right-click
    ImPlotSubplotFlags_NoResize    = 1 << 3,  // resize splitters between subplot cells will be not be provided
    ImPlotSubplotFlags_NoAlign     = 1 << 4,  // subplot edges will not be aligned vertically or horizontally
    ImPlotSubplotFlags_ShareItems  = 1 << 5,  // items across all subplots will be shared and rendered into a single legend entry
    ImPlotSubplotFlags_LinkRows    = 1 << 6,  // link the y-axis limits of all plots in each row (does not apply auxiliary y-axes)
    ImPlotSubplotFlags_LinkCols    = 1 << 7,  // link the x-axis limits of all plots in each column
    ImPlotSubplotFlags_LinkAllX    = 1 << 8,  // link the x-axis limits in every plot in the subplot
    ImPlotSubplotFlags_LinkAllY    = 1 << 9 , // link the y-axis limits in every plot in the subplot (does not apply to auxiliary y-axes)
    ImPlotSubplotFlags_ColMajor    = 1 << 10  // subplots are added in column major order instead of the default row major order
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
    ImPlotStyleVar_FitPadding,         // ImVec2, additional fit padding as a percentage of the fit extents (e.g. ImVec2(0.1f,0.1f) adds 10% to the fit extents of X and Y)
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
    ImPlotColormap_Deep     = 0,   // a.k.a. seaborn deep             (qual=true,  n=10) (default)
    ImPlotColormap_Dark     = 1,   // a.k.a. matplotlib "Set1"        (qual=true,  n=9 )
    ImPlotColormap_Pastel   = 2,   // a.k.a. matplotlib "Pastel1"     (qual=true,  n=9 )
    ImPlotColormap_Paired   = 3,   // a.k.a. matplotlib "Paired"      (qual=true,  n=12)
    ImPlotColormap_Viridis  = 4,   // a.k.a. matplotlib "viridis"     (qual=false, n=11)
    ImPlotColormap_Plasma   = 5,   // a.k.a. matplotlib "plasma"      (qual=false, n=11)
    ImPlotColormap_Hot      = 6,   // a.k.a. matplotlib/MATLAB "hot"  (qual=false, n=11)
    ImPlotColormap_Cool     = 7,   // a.k.a. matplotlib/MATLAB "cool" (qual=false, n=11)
    ImPlotColormap_Pink     = 8,   // a.k.a. matplotlib/MATLAB "pink" (qual=false, n=11)
    ImPlotColormap_Jet      = 9,   // a.k.a. MATLAB "jet"             (qual=false, n=11)
    ImPlotColormap_Twilight = 10,  // a.k.a. matplotlib "twilight"    (qual=false, n=11)
    ImPlotColormap_RdBu     = 11,  // red/blue, Color Brewer          (qual=false, n=11)
    ImPlotColormap_BrBG     = 12,  // brown/blue-green, Color Brewer  (qual=false, n=11)
    ImPlotColormap_PiYG     = 13,  // pink/yellow-green, Color Brewer (qual=false, n=11)
    ImPlotColormap_Spectral = 14,  // color spectrum, Color Brewer    (qual=false, n=11)
    ImPlotColormap_Greys    = 15,  // white/black                     (qual=false, n=2 )
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

// Enums for different automatic histogram binning methods (k = bin count or w = bin width)
enum ImPlotBin_ {
    ImPlotBin_Sqrt    = -1, // k = sqrt(n)
    ImPlotBin_Sturges = -2, // k = 1 + log2(n)
    ImPlotBin_Rice    = -3, // k = 2 * cbrt(n)
    ImPlotBin_Scott   = -4, // w = 3.49 * sigma / cbrt(n)
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
    ImPlotLimits()                                                        { }
    ImPlotLimits(double x_min, double x_max, double y_min, double y_max)  { X.Min = x_min; X.Max = x_max; Y.Min = y_min; Y.Max = y_max; }
    bool Contains(const ImPlotPoint& p) const                             { return Contains(p.x, p.y); }
    bool Contains(double x, double y) const                               { return X.Contains(x) && Y.Contains(y); }
    ImPlotPoint Min() const                                               { return ImPlotPoint(X.Min, Y.Min); }
    ImPlotPoint Max() const                                               { return ImPlotPoint(X.Max, Y.Max); }
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
    ImVec2  LegendSpacing;           // = 5,0     spacing between legend entries
    ImVec2  MousePosPadding;         // = 10,10   padding between plot edge and interior mouse location text
    ImVec2  AnnotationPadding;       // = 2,2     text padding around annotation labels
    ImVec2  FitPadding;              // = 0,0     additional fit padding as a percentage of the fit extents (e.g. ImVec2(0.1f,0.1f) adds 10% to the fit extents of X and Y)
    ImVec2  PlotDefaultSize;         // = 400,300 default size used when ImVec2(0,0) is passed to BeginPlot
    ImVec2  PlotMinSize;             // = 200,150 minimum size plot frame can be when shrunk
    // style colors
    ImVec4  Colors[ImPlotCol_COUNT]; // Array of styling colors. Indexable with ImPlotCol_ enums.
    // colormap
    ImPlotColormap Colormap;         // The current colormap. Set this to either an ImPlotColormap_ enum or an index returned by AddColormap.
    // settings/flags
    bool    AntiAliasedLines;        // = false,  enable global anti-aliasing on plot lines (overrides ImPlotFlags_AntiAliased)
    bool    UseLocalTime;            // = false,  axis labels will be formatted for your timezone when ImPlotAxisFlag_Time is enabled
    bool    UseISO8601;              // = false,  dates will be formatted according to ISO 8601 where applicable (e.g. YYYY-MM-DD, YYYY-MM, --MM-DD, etc.)
    bool    Use24HourClock;          // = false,  times will be formatted using a 24 hour clock
    IMPLOT_API ImPlotStyle();
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
// Destroys an ImPlot context. Call this before ImGui::DestroyContext. NULL = destroy current context.
IMPLOT_API void DestroyContext(ImPlotContext* ctx = NULL);
// Returns the current ImPlot context. NULL if no context has ben set.
IMPLOT_API ImPlotContext* GetCurrentContext();
// Sets the current ImPlot context.
IMPLOT_API void SetCurrentContext(ImPlotContext* ctx);

// Sets the current **ImGui** context. This is ONLY necessary if you are compiling
// ImPlot as a DLL (not recommended) separate from your ImGui compilation. It
// sets the global variable GImGui, which is not shared across DLL boundaries.
// See GImGui documentation in imgui.cpp for more details.
IMPLOT_API void SetImGuiContext(ImGuiContext* ctx);

//-----------------------------------------------------------------------------
// Begin/End Plot
//-----------------------------------------------------------------------------

// Starts a 2D plotting context. If this function returns true, EndPlot() MUST
// be called! You are encouraged to use the following convention:
//
// if (BeginPlot(...)) {
//     ImPlot::PlotLine(...);
//     ...
//     EndPlot();
// }
//
// Important notes:
//
// - #title_id must be unique to the current ImGui ID scope. If you need to avoid ID
//   collisions or don't want to display a title in the plot, use double hashes
//   (e.g. "MyPlot##HiddenIdText" or "##NoTitle").
// - If #x_label and/or #y_label are provided, axes labels will be displayed.
// - #size is the **frame** size of the plot widget, not the plot area. The default
//   size of plots (i.e. when ImVec2(0,0)) can be modified in your ImPlotStyle
//   (default is 400x300 px).
// - Auxiliary y-axes must be enabled with ImPlotFlags_YAxis2/3 to be displayed.
// - See ImPlotFlags and ImPlotAxisFlags for more available options.

IMPLOT_API bool BeginPlot(const char* title_id,
                          const char* x_label      = NULL,
                          const char* y_label      = NULL,
                          const ImVec2& size       = ImVec2(-1,0),
                          ImPlotFlags flags        = ImPlotFlags_None,
                          ImPlotAxisFlags x_flags  = ImPlotAxisFlags_None,
                          ImPlotAxisFlags y_flags  = ImPlotAxisFlags_None,
                          ImPlotAxisFlags y2_flags = ImPlotAxisFlags_NoGridLines,
                          ImPlotAxisFlags y3_flags = ImPlotAxisFlags_NoGridLines,
                          const char* y2_label     = NULL,
                          const char* y3_label     = NULL);

// Only call EndPlot() if BeginPlot() returns true! Typically called at the end
// of an if statement conditioned on BeginPlot(). See example above.
IMPLOT_API void EndPlot();

//-----------------------------------------------------------------------------
// Begin/EndSubplots
//-----------------------------------------------------------------------------

// Starts a subdivided plotting context. If the function returns true,
// EndSubplots() MUST be called! Call BeginPlot/EndPlot AT MOST [rows*cols]
// times in  between the begining and end of the subplot context. Plots are
// added in row major order.
//
// Example:
//
// if (BeginSubplots("My Subplot",2,3,ImVec2(800,400)) {
//     for (int i = 0; i < 6; ++i) {
//         if (BeginPlot(...)) {
//             ImPlot::PlotLine(...);
//             ...
//             EndPlot();
//         }
//     }
//     EndSubplots();
// }
//
// Procudes:
//
// [0][1][2]
// [3][4][5]
//
// Important notes:
//
// - #title_id must be unique to the current ImGui ID scope. If you need to avoid ID
//   collisions or don't want to display a title in the plot, use double hashes
//   (e.g. "MyPlot##HiddenIdText" or "##NoTitle").
// - #rows and #cols must be greater than 0.
// - #size is the size of the entire grid of subplots, not the individual plots
// - #row_ratios and #col_ratios must have AT LEAST #rows and #cols elements,
//   respectively. These are the sizes of the rows and columns expressed in ratios.
//   If the user adjusts the dimensions, the arrays are updated with new ratios.
//
// Important notes regarding BeginPlot from inside of BeginSubplots:
//
// - The #title_id parameter of _BeginPlot_ (see above) does NOT have to be
//   unique when called inside of a subplot context. Subplot IDs are hashed
//   for your convenience so you don't have call PushID or generate unique title
//   strings. Simply pass an empty string to BeginPlot unless you want to title
//   each subplot.
// - The #size parameter of _BeginPlot_ (see above) is ignored when inside of a
//   subplot context. The actual size of the subplot will be based on the
//   #size value you pass to _BeginSubplots_ and #row/#col_ratios if provided.

IMPLOT_API bool BeginSubplots(const char* title_id,
                             int rows,
                             int cols,
                             const ImVec2& size,
                             ImPlotSubplotFlags flags = ImPlotSubplotFlags_None,
                             float* row_ratios        = NULL,
                             float* col_ratios        = NULL);

// Only call EndSubplots() if BeginSubplots() returns true! Typically called at the end
// of an if statement conditioned on BeginSublots(). See example above.
IMPLOT_API void EndSubplots();

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
// 2. Write a custom getter C function or C++ lambda and pass it and optionally your data to
//    an ImPlot function post-fixed with a G (e.g. PlotScatterG). This has a slight performance
//    cost, but probably not enough to worry about unless your data is very large. Examples:
//
//    ImPlotPoint MyDataGetter(void* data, int idx) {
//        MyData* my_data = (MyData*)data;
//        ImPlotPoint p;
//        p.x = my_data->GetTime(idx);
//        p.y = my_data->GetValue(idx);
//        return p
//    }
//    ...
//    auto my_lambda = [](void*, int idx) {
//        double t = idx / 999.0;
//        return ImPlotPoint(t, 0.5+0.5*std::sin(2*PI*10*t));
//    };
//    ...
//    if (ImPlot::BeginPlot("MyPlot")) {
//        MyData my_data;
//        ImPlot::PlotScatterG("scatter", MyDataGetter, &my_data, my_data.Size());
//        ImPlot::PlotLineG("line", my_lambda, nullptr, 1000);
//        ImPlot::EndPlot();
//    }
//
// NB: All types are converted to double before plotting. You may lose information
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

// Plots a shaded (filled) region between two lines, or a line and a horizontal reference. Set y_ref to +/-INFINITY for infinite fill extents.
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

/// Plots infinite vertical or horizontal lines (e.g. for references or asymptotes).
template <typename T> IMPLOT_API void PlotVLines(const char* label_id, const T* xs, int count, int offset=0, int stride=sizeof(T));
template <typename T> IMPLOT_API void PlotHLines(const char* label_id, const T* ys, int count, int offset=0, int stride=sizeof(T));

// Plots a pie chart. If the sum of values > 1 or normalize is true, each value will be normalized. Center and radius are in plot units. #label_fmt can be set to NULL for no labels.
template <typename T> IMPLOT_API void PlotPieChart(const char* const label_ids[], const T* values, int count, double x, double y, double radius, bool normalize=false, const char* label_fmt="%.1f", double angle0=90);

// Plots a 2D heatmap chart. Values are expected to be in row-major order. Leave #scale_min and scale_max both at 0 for automatic color scaling, or set them to a predefined range. #label_fmt can be set to NULL for no labels.
template <typename T> IMPLOT_API void PlotHeatmap(const char* label_id, const T* values, int rows, int cols, double scale_min=0, double scale_max=0, const char* label_fmt="%.1f", const ImPlotPoint& bounds_min=ImPlotPoint(0,0), const ImPlotPoint& bounds_max=ImPlotPoint(1,1));

// Plots a horizontal histogram. #bins can be a positive integer or an ImPlotBin_ method. If #cumulative is true, each bin contains its count plus the counts of all previous bins.
// If #density is true, the PDF is visualized. If both are true, the CDF is visualized. If #range is left unspecified, the min/max of #values will be used as the range.
// If #range is specified, outlier values outside of the range are not binned. However, outliers still count toward normalizing and cumulative counts unless #outliers is false. The largest bin count or density is returned.
template <typename T> IMPLOT_API double PlotHistogram(const char* label_id, const T* values, int count, int bins=ImPlotBin_Sturges, bool cumulative=false, bool density=false, ImPlotRange range=ImPlotRange(), bool outliers=true, double bar_scale=1.0);

// Plots two dimensional, bivariate histogram as a heatmap. #x_bins and #y_bins can be a positive integer or an ImPlotBin. If #density is true, the PDF is visualized.
// If #range is left unspecified, the min/max of #xs an #ys will be used as the ranges. If #range is specified, outlier values outside of range are not binned.
// However, outliers still count toward the normalizing count for density plots unless #outliers is false. The largest bin count or density is returned.
template <typename T> IMPLOT_API double PlotHistogram2D(const char* label_id, const T* xs, const T* ys, int count, int x_bins=ImPlotBin_Sturges, int y_bins=ImPlotBin_Sturges, bool density=false, ImPlotLimits range=ImPlotLimits(), bool outliers=true);

// Plots digital data. Digital plots do not respond to y drag or zoom, and are always referenced to the bottom of the plot.
template <typename T> IMPLOT_API void PlotDigital(const char* label_id, const T* xs, const T* ys, int count, int offset=0, int stride=sizeof(T));
                      IMPLOT_API void PlotDigitalG(const char* label_id, ImPlotPoint (*getter)(void* data, int idx), void* data, int count, int offset=0);

// Plots an axis-aligned image. #bounds_min/bounds_max are in plot coordinates (y-up) and #uv0/uv1 are in texture coordinates (y-down).
IMPLOT_API void PlotImage(const char* label_id, ImTextureID user_texture_id, const ImPlotPoint& bounds_min, const ImPlotPoint& bounds_max, const ImVec2& uv0=ImVec2(0,0), const ImVec2& uv1=ImVec2(1,1), const ImVec4& tint_col=ImVec4(1,1,1,1));

// Plots a centered text label at point x,y with an optional pixel offset. Text color can be changed with ImPlot::PushStyleColor(ImPlotCol_InlayText, ...).
IMPLOT_API void PlotText(const char* text, double x, double y, bool vertical=false, const ImVec2& pix_offset=ImVec2(0,0));

// Plots a dummy item (i.e. adds a legend entry colored by ImPlotCol_Line)
IMPLOT_API void PlotDummy(const char* label_id);

//-----------------------------------------------------------------------------
// Plot Utils
//-----------------------------------------------------------------------------

// The following functions MUST be called BEFORE BeginPlot!

// Set the axes range limits of the next plot. Call right before BeginPlot(). If ImGuiCond_Always is used, the axes limits will be locked.
IMPLOT_API void SetNextPlotLimits(double xmin, double xmax, double ymin, double ymax, ImGuiCond cond = ImGuiCond_Once);
// Set the X axis range limits of the next plot. Call right before BeginPlot(). If ImGuiCond_Always is used, the X axis limits will be locked.
IMPLOT_API void SetNextPlotLimitsX(double xmin, double xmax, ImGuiCond cond = ImGuiCond_Once);
// Set the Y axis range limits of the next plot. Call right before BeginPlot(). If ImGuiCond_Always is used, the Y axis limits will be locked.
IMPLOT_API void SetNextPlotLimitsY(double ymin, double ymax, ImGuiCond cond = ImGuiCond_Once, ImPlotYAxis y_axis = ImPlotYAxis_1);
// Links the next plot limits to external values. Set to NULL for no linkage. The pointer data must remain valid until the matching call to EndPlot.
IMPLOT_API void LinkNextPlotLimits(double* xmin, double* xmax, double* ymin, double* ymax, double* ymin2 = NULL, double* ymax2 = NULL, double* ymin3 = NULL, double* ymax3 = NULL);
// Fits the next plot axes to all plotted data if they are unlocked (equivalent to double-clicks).
IMPLOT_API void FitNextPlotAxes(bool x = true, bool y = true, bool y2 = true, bool y3 = true);

// Set the X axis ticks and optionally the labels for the next plot. To keep the default ticks, set #keep_default=true.
IMPLOT_API void SetNextPlotTicksX(const double* values, int n_ticks, const char* const labels[] = NULL, bool keep_default = false);
IMPLOT_API void SetNextPlotTicksX(double x_min, double x_max, int n_ticks, const char* const labels[] = NULL, bool keep_default = false);
// Set the Y axis ticks and optionally the labels for the next plot. To keep the default ticks, set #keep_default=true.
IMPLOT_API void SetNextPlotTicksY(const double* values, int n_ticks, const char* const labels[] = NULL, bool keep_default = false, ImPlotYAxis y_axis = ImPlotYAxis_1);
IMPLOT_API void SetNextPlotTicksY(double y_min, double y_max, int n_ticks, const char* const labels[] = NULL, bool keep_default = false, ImPlotYAxis y_axis = ImPlotYAxis_1);

// Set the format for numeric X axis labels (default="%g"). Formated values will be doubles (i.e. don't supply %d, %i, etc.). Not applicable if ImPlotAxisFlags_Time enabled.
IMPLOT_API void SetNextPlotFormatX(const char* fmt);
// Set the format for numeric Y axis labels (default="%g"). Formated values will be doubles (i.e. don't supply %d, %i, etc.).
IMPLOT_API void SetNextPlotFormatY(const char* fmt, ImPlotYAxis y_axis=ImPlotYAxis_1);

// The following functions MUST be called BETWEEN Begin/EndPlot!

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

// Returns true if the current plot is being box selected.
IMPLOT_API bool IsPlotSelected();
// Returns the current plot box selection bounds.
IMPLOT_API ImPlotLimits GetPlotSelection(ImPlotYAxis y_axis = IMPLOT_AUTO);

// Returns true if the current plot is being queried or has an active query. Query must be enabled with ImPlotFlags_Query.
IMPLOT_API bool IsPlotQueried();
// Returns the current plot query bounds. Query must be enabled with ImPlotFlags_Query.
IMPLOT_API ImPlotLimits GetPlotQuery(ImPlotYAxis y_axis = IMPLOT_AUTO);
// Set the current plot query bounds. Query must be enabled with ImPlotFlags_Query.
IMPLOT_API void SetPlotQuery(const ImPlotLimits& query, ImPlotYAxis y_axis = IMPLOT_AUTO);

//-----------------------------------------------------------------------------
// Algined Plots
//-----------------------------------------------------------------------------

// Consider using Begin/EndSubplots first. They are more feature rich and
// accomplish the same behaviour by default. The functions below offer lower
// level control of plot alignment.

// Align axis padding over multiple plots in a single row or column. If this function returns true, EndAlignedPlots() must be called. #group_id must be unique.
IMPLOT_API bool BeginAlignedPlots(const char* group_id, ImPlotOrientation orientation = ImPlotOrientation_Vertical);
// Only call EndAlignedPlots() if BeginAlignedPlots() returns true!
IMPLOT_API void EndAlignedPlots();

//-----------------------------------------------------------------------------
// Plot Tools
//-----------------------------------------------------------------------------

// The following functions MUST be called BETWEEN Begin/EndPlot!

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

// The following functions MUST be called BETWEEN Begin/EndPlot!

// Set the location of the current plot's (or subplot's) legend.
IMPLOT_API void SetLegendLocation(ImPlotLocation location, ImPlotOrientation orientation = ImPlotOrientation_Vertical, bool outside = false);
// Set the location of the current plot's mouse position text (default = South|East).
IMPLOT_API void SetMousePosLocation(ImPlotLocation location);
// Returns true if a plot item legend entry is hovered.
IMPLOT_API bool IsLegendEntryHovered(const char* label_id);

// Begin a popup for a legend entry.
IMPLOT_API bool BeginLegendPopup(const char* label_id, ImGuiMouseButton mouse_button = 1);
// End a popup for a legend entry.
IMPLOT_API void EndLegendPopup();

//-----------------------------------------------------------------------------
// Drag and Drop Utils
//-----------------------------------------------------------------------------

// The following functions MUST be called BETWEEN Begin/EndPlot!

// Turns the current plot's plotting area into a drag and drop target. Don't forget to call EndDragDropTarget!
IMPLOT_API bool BeginDragDropTarget();
// Turns the current plot's X-axis into a drag and drop target. Don't forget to call EndDragDropTarget!
IMPLOT_API bool BeginDragDropTargetX();
// Turns the current plot's Y-Axis into a drag and drop target. Don't forget to call EndDragDropTarget!
IMPLOT_API bool BeginDragDropTargetY(ImPlotYAxis axis = ImPlotYAxis_1);
// Turns the current plot's legend into a drag and drop target. Don't forget to call EndDragDropTarget!
IMPLOT_API bool BeginDragDropTargetLegend();
// Ends a drag and drop target (currently just an alias for ImGui::EndDragDropTarget).
IMPLOT_API void EndDragDropTarget();

// NB: By default, plot and axes drag and drop *sources* require holding the Ctrl modifier to initiate the drag.
// You can change the modifier if desired. If ImGuiKeyModFlags_None is provided, the axes will be locked from panning.

// Turns the current plot's plotting area into a drag and drop source. Don't forget to call EndDragDropSource!
IMPLOT_API bool BeginDragDropSource(ImGuiKeyModFlags key_mods = ImGuiKeyModFlags_Ctrl, ImGuiDragDropFlags flags = 0);
// Turns the current plot's X-axis into a drag and drop source. Don't forget to call EndDragDropSource!
IMPLOT_API bool BeginDragDropSourceX(ImGuiKeyModFlags key_mods = ImGuiKeyModFlags_Ctrl, ImGuiDragDropFlags flags = 0);
// Turns the current plot's Y-axis into a drag and drop source. Don't forget to call EndDragDropSource!
IMPLOT_API bool BeginDragDropSourceY(ImPlotYAxis axis = ImPlotYAxis_1, ImGuiKeyModFlags key_mods = ImGuiKeyModFlags_Ctrl, ImGuiDragDropFlags flags = 0);
// Turns an item in the current plot's legend into drag and drop source. Don't forget to call EndDragDropSource!
IMPLOT_API bool BeginDragDropSourceItem(const char* label_id, ImGuiDragDropFlags flags = 0);
// Ends a drag and drop source (currently just an alias for ImGui::EndDragDropSource).
IMPLOT_API void EndDragDropSource();

//-----------------------------------------------------------------------------
// Plot and Item Styling
//-----------------------------------------------------------------------------

// Styling colors in ImPlot works similarly to styling colors in ImGui, but
// with one important difference. Like ImGui, all style colors are stored in an
// indexable array in ImPlotStyle. You can permanently modify these values through
// GetStyle().Colors, or temporarily modify them with Push/Pop functions below.
// However, by default all style colors in ImPlot default to a special color
// IMPLOT_AUTO_COL. The behavior of this color depends upon the style color to
// which it as applied:
//
//     1) For style colors associated with plot items (e.g. ImPlotCol_Line),
//        IMPLOT_AUTO_COL tells ImPlot to color the item with the next unused
//        color in the current colormap. Thus, every item will have a different
//        color up to the number of colors in the colormap, at which point the
//        colormap will roll over. For most use cases, you should not need to
//        set these style colors to anything but IMPLOT_COL_AUTO; you are
//        probably better off changing the current colormap. However, if you
//        need to explicitly color a particular item you may either Push/Pop
//        the style color around the item in question, or use the SetNextXXXStyle
//        API below. If you permanently set one of these style colors to a specific
//        color, or forget to call Pop, then all subsequent items will be styled
//        with the color you set.
//
//     2) For style colors associated with plot styling (e.g. ImPlotCol_PlotBg),
//        IMPLOT_AUTO_COL tells ImPlot to set that color from color data in your
//        **ImGuiStyle**. The ImGuiCol_ that these style colors default to are
//        detailed above, and in general have been mapped to produce plots visually
//        consistent with your current ImGui style. Of course, you are free to
//        manually set these colors to whatever you like, and further can Push/Pop
//        them around individual plots for plot-specific styling (e.g. coloring axes).

// Provides access to plot style structure for permanant modifications to colors, sizes, etc.
IMPLOT_API ImPlotStyle& GetStyle();

// Style plot colors for current ImGui style (default).
IMPLOT_API void StyleColorsAuto(ImPlotStyle* dst = NULL);
// Style plot colors for ImGui "Classic".
IMPLOT_API void StyleColorsClassic(ImPlotStyle* dst = NULL);
// Style plot colors for ImGui "Dark".
IMPLOT_API void StyleColorsDark(ImPlotStyle* dst = NULL);
// Style plot colors for ImGui "Light".
IMPLOT_API void StyleColorsLight(ImPlotStyle* dst = NULL);

// Use PushStyleX to temporarily modify your ImPlotStyle. The modification
// will last until the matching call to PopStyleX. You MUST call a pop for
// every push, otherwise you will leak memory! This behaves just like ImGui.

// Temporarily modify a style color. Don't forget to call PopStyleColor!
IMPLOT_API void PushStyleColor(ImPlotCol idx, ImU32 col);
IMPLOT_API void PushStyleColor(ImPlotCol idx, const ImVec4& col);
// Undo temporary style color modification(s). Undo multiple pushes at once by increasing count.
IMPLOT_API void PopStyleColor(int count = 1);

// Temporarily modify a style variable of float type. Don't forget to call PopStyleVar!
IMPLOT_API void PushStyleVar(ImPlotStyleVar idx, float val);
// Temporarily modify a style variable of int type. Don't forget to call PopStyleVar!
IMPLOT_API void PushStyleVar(ImPlotStyleVar idx, int val);
// Temporarily modify a style variable of ImVec2 type. Don't forget to call PopStyleVar!
IMPLOT_API void PushStyleVar(ImPlotStyleVar idx, const ImVec2& val);
// Undo temporary style variable modification(s). Undo multiple pushes at once by increasing count.
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

// Item styling is based on colormaps when the relevant ImPlotCol_XXX is set to
// IMPLOT_AUTO_COL (default). Several built-in colormaps are available. You can
// add and then push/pop your own colormaps as well. To permanently set a colormap,
// modify the Colormap index member of your ImPlotStyle.

// Colormap data will be ignored and a custom color will be used if you have done one of the following:
//     1) Modified an item style color in your ImPlotStyle to anything other than IMPLOT_AUTO_COL.
//     2) Pushed an item style color using PushStyleColor().
//     3) Set the next item style with a SetNextXXXStyle function.

// Add a new colormap. The color data will be copied. The colormap can be used by pushing either the returned index or the
// string name with PushColormap. The colormap name must be unique and the size must be greater than 1. You will receive
// an assert otherwise! By default colormaps are considered to be qualitative (i.e. discrete). If you want to create a
// continuous colormap, set #qual=false. This will treat the colors you provide as keys, and ImPlot will build a linearly
// interpolated lookup table. The memory footprint of this table will be exactly ((size-1)*255+1)*4 bytes.
IMPLOT_API ImPlotColormap AddColormap(const char* name, const ImVec4* cols, int size, bool qual=true);
IMPLOT_API ImPlotColormap AddColormap(const char* name, const ImU32*  cols, int size, bool qual=true);

// Returns the number of available colormaps (i.e. the built-in + user-added count).
IMPLOT_API int GetColormapCount();
// Returns a null terminated string name for a colormap given an index. Returns NULL if index is invalid.
IMPLOT_API const char* GetColormapName(ImPlotColormap cmap);
// Returns an index number for a colormap given a valid string name. Returns -1 if name is invalid.
IMPLOT_API ImPlotColormap GetColormapIndex(const char* name);

// Temporarily switch to one of the built-in (i.e. ImPlotColormap_XXX) or user-added colormaps (i.e. a return value of AddColormap). Don't forget to call PopColormap!
IMPLOT_API void PushColormap(ImPlotColormap cmap);
// Push a colormap by string name. Use built-in names such as "Default", "Deep", "Jet", etc. or a string you provided to AddColormap. Don't forget to call PopColormap!
IMPLOT_API void PushColormap(const char* name);
// Undo temporary colormap modification(s). Undo multiple pushes at once by increasing count.
IMPLOT_API void PopColormap(int count = 1);

// Returns the next color from the current colormap and advances the colormap for the current plot.
// Can also be used with no return value to skip colors if desired. You need to call this between Begin/EndPlot!
IMPLOT_API ImVec4 NextColormapColor();

// Colormap utils. If cmap = IMPLOT_AUTO (default), the current colormap is assumed.
// Pass an explicit colormap index (built-in or user-added) to specify otherwise.

// Returns the size of a colormap.
IMPLOT_API int GetColormapSize(ImPlotColormap cmap = IMPLOT_AUTO);
// Returns a color from a colormap given an index >= 0 (modulo will be performed).
IMPLOT_API ImVec4 GetColormapColor(int idx, ImPlotColormap cmap = IMPLOT_AUTO);
// Sample a color from the current colormap given t between 0 and 1.
IMPLOT_API ImVec4 SampleColormap(float t, ImPlotColormap cmap = IMPLOT_AUTO);

// Shows a vertical color scale with linear spaced ticks using the specified color map. Use double hashes to hide label (e.g. "##NoLabel").
IMPLOT_API void ColormapScale(const char* label, double scale_min, double scale_max, const ImVec2& size = ImVec2(0,0), ImPlotColormap cmap = IMPLOT_AUTO, const char* fmt = "%g");
// Shows a horizontal slider with a colormap gradient background. Optionally returns the color sampled at t in [0 1].
IMPLOT_API bool ColormapSlider(const char* label, float* t, ImVec4* out = NULL, const char* format = "", ImPlotColormap cmap = IMPLOT_AUTO);
// Shows a button with a colormap gradient brackground.
IMPLOT_API bool ColormapButton(const char* label, const ImVec2& size = ImVec2(0,0), ImPlotColormap cmap = IMPLOT_AUTO);

// When items in a plot sample their color from a colormap, the color is cached and does not change
// unless explicitly overriden. Therefore, if you change the colormap after the item has already been plotted,
// item colors will NOT update. If you need item colors to resample the new colormap, then use this
// function to bust the cached colors. If #plot_title_id is NULL, then every item in EVERY existing plot
// will be cache busted. Otherwise only the plot specified by #plot_title_id will be busted. For the
// latter, this function must be called in the same ImGui ID scope that the plot is in. You should rarely if ever
// need this function, but it is available for applications that require runtime colormap swaps (e.g. Heatmaps demo).
IMPLOT_API void BustColorCache(const char* plot_title_id = NULL);

//-----------------------------------------------------------------------------
// Miscellaneous
//-----------------------------------------------------------------------------

// Render icons similar to those that appear in legends (nifty for data lists).
IMPLOT_API void ItemIcon(const ImVec4& col);
IMPLOT_API void ItemIcon(ImU32 col);
IMPLOT_API void ColormapIcon(ImPlotColormap cmap);

// Get the plot draw list for custom rendering to the current plot area. Call between Begin/EndPlot.
IMPLOT_API ImDrawList* GetPlotDrawList();
// Push clip rect for rendering to current plot area. The rect can be expanded or contracted by #expand pixels. Call between Begin/EndPlot.
IMPLOT_API void PushPlotClipRect(float expand=0);
// Pop plot clip rect. Call between Begin/EndPlot.
IMPLOT_API void PopPlotClipRect();

// Shows ImPlot style selector dropdown menu.
IMPLOT_API bool ShowStyleSelector(const char* label);
// Shows ImPlot colormap selector dropdown menu.
IMPLOT_API bool ShowColormapSelector(const char* label);
// Shows ImPlot style editor block (not a window).
IMPLOT_API void ShowStyleEditor(ImPlotStyle* ref = NULL);
// Add basic help/info block for end users (not a window).
IMPLOT_API void ShowUserGuide();
// Shows ImPlot metrics/debug information window.
IMPLOT_API void ShowMetricsWindow(bool* p_popen = NULL);

//-----------------------------------------------------------------------------
// Demo (add implot_demo.cpp to your sources!)
//-----------------------------------------------------------------------------

// Shows the ImPlot demo window.
IMPLOT_API void ShowDemoWindow(bool* p_open = NULL);

}  // namespace ImPlot
