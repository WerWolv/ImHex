//--------------------------------------------------
// ImPlot3D v0.3 WIP
// implot3d.h
// Date: 2024-11-16
// Author: Breno Cunha Queiroz (brenocq.com)
//
// Acknowledgments:
//  ImPlot3D is heavily inspired by ImPlot
//  (https://github.com/epezent/implot) by Evan Pezent,
//  and follows a similar code style and structure to
//  maintain consistency with ImPlot's API.
//--------------------------------------------------

// Table of Contents:
// [SECTION] Macros and Defines
// [SECTION] Forward declarations and basic types
// [SECTION] Flags & Enumerations
// [SECTION] Callbacks
// [SECTION] Context
// [SECTION] Begin/End Plot
// [SECTION] Setup
// [SECTION] Plot Items
// [SECTION] Plot Utils
// [SECTION] Miscellaneous
// [SECTION] Styles
// [SECTION] Demo
// [SECTION] Debugging
// [SECTION] ImPlot3DPoint
// [SECTION] ImPlot3DRay
// [SECTION] ImPlot3DPlane
// [SECTION] ImPlot3DBox
// [SECTION] ImPlot3DQuat
// [SECTION] ImPlot3DStyle
// [SECTION] Meshes

#pragma once
#include "imgui.h"
#ifndef IMGUI_DISABLE

//-----------------------------------------------------------------------------
// [SECTION] Macros and Defines
//-----------------------------------------------------------------------------

#ifndef IMPLOT3D_API
#define IMPLOT3D_API
#endif

#define IMPLOT3D_VERSION "0.3 WIP"            // ImPlot3D version
#define IMPLOT3D_VERSION_NUM 300              // Integer encoded version
#define IMPLOT3D_AUTO -1                      // Deduce variable automatically
#define IMPLOT3D_AUTO_COL ImVec4(0, 0, 0, -1) // Deduce color automatically
#define IMPLOT3D_TMP template <typename T> IMPLOT3D_API

//-----------------------------------------------------------------------------
// [SECTION] Forward declarations and basic types
//-----------------------------------------------------------------------------

// Forward declarations
struct ImPlot3DContext;
struct ImPlot3DStyle;
struct ImPlot3DPoint;
struct ImPlot3DRay;
struct ImPlot3DPlane;
struct ImPlot3DBox;
struct ImPlot3DRange;
struct ImPlot3DQuat;

// Enums
typedef int ImPlot3DCond;     // -> ImPlot3DCond_              // Enum: Condition for flags
typedef int ImPlot3DCol;      // -> ImPlot3DCol_               // Enum: Styling colors
typedef int ImPlot3DStyleVar; // -> ImPlot3DStyleVar_          // Enum: Style variables
typedef int ImPlot3DMarker;   // -> ImPlot3DMarker_            // Enum: Marker styles
typedef int ImPlot3DLocation; // -> ImPlot3DLocation_          // Enum: Locations
typedef int ImAxis3D;         // -> ImAxis3D_                  // Enum: Axis indices
typedef int ImPlane3D;        // -> ImPlane3D_                  // Enum: Plane indices
typedef int ImPlot3DColormap; // -> ImPlot3DColormap_          // Enum: Colormaps

// Flags
typedef int ImPlot3DFlags;         // -> ImPlot3DFlags_         // Flags: for BeginPlot()
typedef int ImPlot3DItemFlags;     // -> ImPlot3DItemFlags_     // Flags: Item flags
typedef int ImPlot3DScatterFlags;  // -> ImPlot3DScatterFlags_  // Flags: Scatter plot flags
typedef int ImPlot3DLineFlags;     // -> ImPlot3DLineFlags_     // Flags: Line plot flags
typedef int ImPlot3DTriangleFlags; // -> ImPlot3DTriangleFlags_ // Flags: Triangle plot flags
typedef int ImPlot3DQuadFlags;     // -> ImPlot3DQuadFlags_     // Flags: QuadFplot flags
typedef int ImPlot3DSurfaceFlags;  // -> ImPlot3DSurfaceFlags_  // Flags: Surface plot flags
typedef int ImPlot3DMeshFlags;     // -> ImPlot3DMeshFlags_     // Flags: Mesh plot flags
typedef int ImPlot3DImageFlags;    // -> ImPlot3DImageFlags_    // Flags: Image plot flags
typedef int ImPlot3DLegendFlags;   // -> ImPlot3DLegendFlags_   // Flags: Legend flags
typedef int ImPlot3DAxisFlags;     // -> ImPlot3DAxisFlags_     // Flags: Axis flags

// Fallback for ImGui versions before v1.92: define ImTextureRef as ImTextureID
// You can `#define IMPLOT3D_NO_IMTEXTUREREF` to avoid this fallback
#if !defined(IMGUI_HAS_TEXTURES) && !defined(IMPLOT3D_NO_IMTEXTUREREF)
typedef ImTextureID ImTextureRef;
#endif

//-----------------------------------------------------------------------------
// [SECTION] Flags & Enumerations
//-----------------------------------------------------------------------------

// Flags for ImPlot3D::BeginPlot()
enum ImPlot3DFlags_ {
    ImPlot3DFlags_None = 0,             // Default
    ImPlot3DFlags_NoTitle = 1 << 0,     // Hide plot title
    ImPlot3DFlags_NoLegend = 1 << 1,    // Hide plot legend
    ImPlot3DFlags_NoMouseText = 1 << 2, // Hide mouse position in plot coordinates
    ImPlot3DFlags_NoClip = 1 << 3,      // Disable 3D box clipping
    ImPlot3DFlags_NoMenus = 1 << 4,     // The user will not be able to open context menus
    ImPlot3DFlags_CanvasOnly = ImPlot3DFlags_NoTitle | ImPlot3DFlags_NoLegend | ImPlot3DFlags_NoMouseText,
};

// Represents a condition for SetupAxisLimits etc. (same as ImGuiCond, but we only support a subset of those enums)
enum ImPlot3DCond_ {
    ImPlot3DCond_None = ImGuiCond_None,     // No condition (always set the variable), same as _Always
    ImPlot3DCond_Always = ImGuiCond_Always, // No condition (always set the variable)
    ImPlot3DCond_Once = ImGuiCond_Once,     // Set the variable once per runtime session (only the first call will succeed)
};

enum ImPlot3DCol_ {
    // Item colors
    ImPlot3DCol_Line = 0,      // Line color
    ImPlot3DCol_Fill,          // Fill color
    ImPlot3DCol_MarkerOutline, // Marker outline color
    ImPlot3DCol_MarkerFill,    // Marker fill color
    // Plot colors
    ImPlot3DCol_TitleText,  // Title color
    ImPlot3DCol_InlayText,  // Color for texts appearing inside of plots
    ImPlot3DCol_FrameBg,    // Frame background color
    ImPlot3DCol_PlotBg,     // Plot area background color
    ImPlot3DCol_PlotBorder, // Plot area border color
    // Legend colors
    ImPlot3DCol_LegendBg,     // Legend background color
    ImPlot3DCol_LegendBorder, // Legend border color
    ImPlot3DCol_LegendText,   // Legend text color
    // Axis colors
    ImPlot3DCol_AxisText, // Axis label and tick lables color
    ImPlot3DCol_AxisGrid, // Axis grid color
    ImPlot3DCol_AxisTick, // Axis tick color (defaults to AxisGrid)
    ImPlot3DCol_COUNT,
};

// Plot styling variables
enum ImPlot3DStyleVar_ {
    // Item style
    ImPlot3DStyleVar_LineWeight,   // float, plot item line weight in pixels
    ImPlot3DStyleVar_Marker,       // int,   marker specification
    ImPlot3DStyleVar_MarkerSize,   // float, marker size in pixels (roughly the marker's "radius")
    ImPlot3DStyleVar_MarkerWeight, // float, plot outline weight of markers in pixels
    ImPlot3DStyleVar_FillAlpha,    // float, alpha modifier applied to all plot item fills
    // Plot style
    ImPlot3DStyleVar_PlotDefaultSize, // ImVec2, default size used when ImVec2(0,0) is passed to BeginPlot
    ImPlot3DStyleVar_PlotMinSize,     // ImVec2, minimum size plot frame can be when shrunk
    ImPlot3DStyleVar_PlotPadding,     // ImVec2, padding between widget frame and plot area, labels, or outside legends (i.e. main padding)
    ImPlot3DStyleVar_LabelPadding,    // ImVec2, padding between axes labels, tick labels, and plot edge
    // Legend style
    ImPlot3DStyleVar_LegendPadding,      // ImVec2, legend padding from plot edges
    ImPlot3DStyleVar_LegendInnerPadding, // ImVec2, legend inner padding from legend edges
    ImPlot3DStyleVar_LegendSpacing,      // ImVec2, spacing between legend entries
    ImPlot3DStyleVar_COUNT
};

enum ImPlot3DMarker_ {
    ImPlot3DMarker_None = -1, // No marker
    ImPlot3DMarker_Circle,    // Circle marker (default)
    ImPlot3DMarker_Square,    // Square maker
    ImPlot3DMarker_Diamond,   // Diamond marker
    ImPlot3DMarker_Up,        // Upward-pointing triangle marker
    ImPlot3DMarker_Down,      // Downward-pointing triangle marker
    ImPlot3DMarker_Left,      // Leftward-pointing triangle marker
    ImPlot3DMarker_Right,     // Rightward-pointing triangle marker
    ImPlot3DMarker_Cross,     // Cross marker (not fillable)
    ImPlot3DMarker_Plus,      // Plus marker (not fillable)
    ImPlot3DMarker_Asterisk,  // Asterisk marker (not fillable)
    ImPlot3DMarker_COUNT
};

// Flags for items
enum ImPlot3DItemFlags_ {
    ImPlot3DItemFlags_None = 0,          // Default
    ImPlot3DItemFlags_NoLegend = 1 << 0, // The item won't have a legend entry displayed
    ImPlot3DItemFlags_NoFit = 1 << 1,    // The item won't be considered for plot fits
};

// Flags for PlotScatter
enum ImPlot3DScatterFlags_ {
    ImPlot3DScatterFlags_None = 0, // Default
    ImPlot3DScatterFlags_NoLegend = ImPlot3DItemFlags_NoLegend,
    ImPlot3DScatterFlags_NoFit = ImPlot3DItemFlags_NoFit,
};

// Flags for PlotLine
enum ImPlot3DLineFlags_ {
    ImPlot3DLineFlags_None = 0, // Default
    ImPlot3DLineFlags_NoLegend = ImPlot3DItemFlags_NoLegend,
    ImPlot3DLineFlags_NoFit = ImPlot3DItemFlags_NoFit,
    ImPlot3DLineFlags_Segments = 1 << 10, // A line segment will be rendered from every two consecutive points
    ImPlot3DLineFlags_Loop = 1 << 11,     // The last and first point will be connected to form a closed loop
    ImPlot3DLineFlags_SkipNaN = 1 << 12,  // NaNs values will be skipped instead of rendered as missing data
};

// Flags for PlotTriangle
enum ImPlot3DTriangleFlags_ {
    ImPlot3DTriangleFlags_None = 0, // Default
    ImPlot3DTriangleFlags_NoLegend = ImPlot3DItemFlags_NoLegend,
    ImPlot3DTriangleFlags_NoFit = ImPlot3DItemFlags_NoFit,
    ImPlot3DTriangleFlags_NoLines = 1 << 10,   // No lines will be rendered
    ImPlot3DTriangleFlags_NoFill = 1 << 11,    // No fill will be rendered
    ImPlot3DTriangleFlags_NoMarkers = 1 << 12, // No markers will be rendered
};

// Flags for PlotQuad
enum ImPlot3DQuadFlags_ {
    ImPlot3DQuadFlags_None = 0, // Default
    ImPlot3DQuadFlags_NoLegend = ImPlot3DItemFlags_NoLegend,
    ImPlot3DQuadFlags_NoFit = ImPlot3DItemFlags_NoFit,
    ImPlot3DQuadFlags_NoLines = 1 << 10,   // No lines will be rendered
    ImPlot3DQuadFlags_NoFill = 1 << 11,    // No fill will be rendered
    ImPlot3DQuadFlags_NoMarkers = 1 << 12, // No markers will be rendered
};

// Flags for PlotSurface
enum ImPlot3DSurfaceFlags_ {
    ImPlot3DSurfaceFlags_None = 0, // Default
    ImPlot3DSurfaceFlags_NoLegend = ImPlot3DItemFlags_NoLegend,
    ImPlot3DSurfaceFlags_NoFit = ImPlot3DItemFlags_NoFit,
    ImPlot3DSurfaceFlags_NoLines = 1 << 10,   // No lines will be rendered
    ImPlot3DSurfaceFlags_NoFill = 1 << 11,    // No fill will be rendered
    ImPlot3DSurfaceFlags_NoMarkers = 1 << 12, // No markers will be rendered
};

// Flags for PlotMesh
enum ImPlot3DMeshFlags_ {
    ImPlot3DMeshFlags_None = 0, // Default
    ImPlot3DMeshFlags_NoLegend = ImPlot3DItemFlags_NoLegend,
    ImPlot3DMeshFlags_NoFit = ImPlot3DItemFlags_NoFit,
    ImPlot3DMeshFlags_NoLines = 1 << 10,   // No lines will be rendered
    ImPlot3DMeshFlags_NoFill = 1 << 11,    // No fill will be rendered
    ImPlot3DMeshFlags_NoMarkers = 1 << 12, // No markers will be rendered
};

// Flags for PlotImage
enum ImPlot3DImageFlags_ {
    ImPlot3DImageFlags_None = 0, // Default
    ImPlot3DImageFlags_NoLegend = ImPlot3DItemFlags_NoLegend,
    ImPlot3DImageFlags_NoFit = ImPlot3DItemFlags_NoFit,
};

// Flags for legends
enum ImPlot3DLegendFlags_ {
    ImPlot3DLegendFlags_None = 0,                 // Default
    ImPlot3DLegendFlags_NoButtons = 1 << 0,       // Legend icons will not function as hide/show buttons
    ImPlot3DLegendFlags_NoHighlightItem = 1 << 1, // Plot items will not be highlighted when their legend entry is hovered
    ImPlot3DLegendFlags_Horizontal = 1 << 2,      // Legend entries will be displayed horizontally
};

// Used to position legend on a plot
enum ImPlot3DLocation_ {
    ImPlot3DLocation_Center = 0,                                                 // Center-center
    ImPlot3DLocation_North = 1 << 0,                                             // Top-center
    ImPlot3DLocation_South = 1 << 1,                                             // Bottom-center
    ImPlot3DLocation_West = 1 << 2,                                              // Center-left
    ImPlot3DLocation_East = 1 << 3,                                              // Center-right
    ImPlot3DLocation_NorthWest = ImPlot3DLocation_North | ImPlot3DLocation_West, // Top-left
    ImPlot3DLocation_NorthEast = ImPlot3DLocation_North | ImPlot3DLocation_East, // Top-right
    ImPlot3DLocation_SouthWest = ImPlot3DLocation_South | ImPlot3DLocation_West, // Bottom-left
    ImPlot3DLocation_SouthEast = ImPlot3DLocation_South | ImPlot3DLocation_East  // Bottom-right
};

// Flags for axis
enum ImPlot3DAxisFlags_ {
    ImPlot3DAxisFlags_None = 0,              // Default
    ImPlot3DAxisFlags_NoLabel = 1 << 0,      // No axis label will be displayed
    ImPlot3DAxisFlags_NoGridLines = 1 << 1,  // No grid lines will be displayed
    ImPlot3DAxisFlags_NoTickMarks = 1 << 2,  // No tick marks will be displayed
    ImPlot3DAxisFlags_NoTickLabels = 1 << 3, // No tick labels will be displayed
    ImPlot3DAxisFlags_LockMin = 1 << 4,      // The axis minimum value will be locked when panning/zooming
    ImPlot3DAxisFlags_LockMax = 1 << 5,      // The axis maximum value will be locked when panning/zooming
    ImPlot3DAxisFlags_AutoFit = 1 << 6,      // Axis will be auto-fitting to data extents
    ImPlot3DAxisFlags_Invert = 1 << 7,       // The axis will be inverted
    ImPlot3DAxisFlags_PanStretch = 1 << 8,   // Panning in a locked or constrained state will cause the axis to stretch if possible
    ImPlot3DAxisFlags_Lock = ImPlot3DAxisFlags_LockMin | ImPlot3DAxisFlags_LockMax,
    ImPlot3DAxisFlags_NoDecorations = ImPlot3DAxisFlags_NoLabel | ImPlot3DAxisFlags_NoGridLines | ImPlot3DAxisFlags_NoTickLabels,
};

// Axis indices
enum ImAxis3D_ {
    ImAxis3D_X = 0,
    ImAxis3D_Y,
    ImAxis3D_Z,
    ImAxis3D_COUNT,
};

// Plane indices
enum ImPlane3D_ {
    ImPlane3D_YZ = 0,
    ImPlane3D_XZ,
    ImPlane3D_XY,
    ImPlane3D_COUNT,
};

// Colormaps
enum ImPlot3DColormap_ {
    ImPlot3DColormap_Deep = 0,      // Same as seaborn "deep"
    ImPlot3DColormap_Dark = 1,      // Same as matplotlib "Set1"
    ImPlot3DColormap_Pastel = 2,    // Same as matplotlib "Pastel1"
    ImPlot3DColormap_Paired = 3,    // Same as matplotlib "Paired"
    ImPlot3DColormap_Viridis = 4,   // Same as matplotlib "viridis"
    ImPlot3DColormap_Plasma = 5,    // Same as matplotlib "plasma"
    ImPlot3DColormap_Hot = 6,       // Same as matplotlib/MATLAB "hot"
    ImPlot3DColormap_Cool = 7,      // Same as matplotlib/MATLAB "cool"
    ImPlot3DColormap_Pink = 8,      // Same as matplotlib/MATLAB "pink"
    ImPlot3DColormap_Jet = 9,       // Same as matplotlib/MATLAB "jet"
    ImPlot3DColormap_Twilight = 10, // Same as matplotlib "twilight"
    ImPlot3DColormap_RdBu = 11,     // Same as matplotlib "RdBu"
    ImPlot3DColormap_BrBG = 12,     // Same as matplotlib "BrGB"
    ImPlot3DColormap_PiYG = 13,     // Same as matplotlib "PiYG"
    ImPlot3DColormap_Spectral = 14, // Same as matplotlib "Spectral"
    ImPlot3DColormap_Greys = 15,    // White/black
};

//-----------------------------------------------------------------------------
// [SECTION] Callbacks
//-----------------------------------------------------------------------------

// Callback signature for axis tick label formatter
typedef int (*ImPlot3DFormatter)(float value, char* buff, int size, void* user_data);

namespace ImPlot3D {

//-----------------------------------------------------------------------------
// [SECTION] Context
//-----------------------------------------------------------------------------
IMPLOT3D_API ImPlot3DContext* CreateContext();
IMPLOT3D_API void DestroyContext(ImPlot3DContext* ctx = nullptr);
IMPLOT3D_API ImPlot3DContext* GetCurrentContext();
IMPLOT3D_API void SetCurrentContext(ImPlot3DContext* ctx);

//-----------------------------------------------------------------------------
// [SECTION] Begin/End Plot
//-----------------------------------------------------------------------------

// Starts a 3D plotting context. If this function returns true, EndPlot() MUST
// be called! You are encouraged to use the following convention:
//
// if (ImPlot3D::BeginPlot(...)) {
//     ImPlot3D::PlotLine(...);
//     ...
//     ImPlot3D::EndPlot();
// }
//
// Important notes:
// - #title_id must be unique to the current ImGui ID scope. If you need to avoid ID
//   collisions or don't want to display a title in the plot, use double hashes
//   (e.g. "MyPlot##HiddenIdText" or "##NoTitle").
// - #size is the **frame** size of the plot widget, not the plot area.
IMPLOT3D_API bool BeginPlot(const char* title_id, const ImVec2& size = ImVec2(-1, 0), ImPlot3DFlags flags = 0);
IMPLOT3D_API void EndPlot(); // Only call if BeginPlot() returns true!

//-----------------------------------------------------------------------------
// [SECTION] Setup
//-----------------------------------------------------------------------------

// The following API allows you to setup and customize various aspects of the
// current plot. The functions should be called immediately after BeginPlot()
// and before any other API calls. Typical usage is as follows:

// if (ImPlot3D::BeginPlot(...)) {                     1) Begin a new plot
//     ImPlot3D::SetupAxis(ImAxis3D_X, "My X-Axis");    2) Make Setup calls
//     ImPlot3D::SetupAxis(ImAxis3D_Y, "My Y-Axis");
//     ImPlot3D::SetupLegend(ImPlotLocation_North);
//     ...
//     ImPlot3D::SetupFinish();                        3) [Optional] Explicitly finish setup
//     ImPlot3D::PlotLine(...);                        4) Plot items
//     ...
//     ImPlot3D::EndPlot();                            5) End the plot
// }
//
// Important notes:
//
// - Always call Setup code at the top of your BeginPlot conditional statement.
// - Setup is locked once you start plotting or explicitly call SetupFinish.
//   Do NOT call Setup code after you begin plotting or after you make
//   any non-Setup API calls (e.g. utils like PlotToPixels also lock Setup).
// - Calling SetupFinish is OPTIONAL, but probably good practice. If you do not
//   call it yourself, then the first subsequent plotting or utility function will
//   call it for you.

// Enables an axis or sets the label and/or flags for an existing axis. Leave #label = nullptr for no label
IMPLOT3D_API void SetupAxis(ImAxis3D axis, const char* label = nullptr, ImPlot3DAxisFlags flags = 0);

IMPLOT3D_API void SetupAxisLimits(ImAxis3D axis, double v_min, double v_max, ImPlot3DCond cond = ImPlot3DCond_Once);

IMPLOT3D_API void SetupAxisFormat(ImAxis3D axis, ImPlot3DFormatter formatter, void* data = nullptr);

// Sets an axis' ticks and optionally the labels. To keep the default ticks, set #keep_default=true
IMPLOT3D_API void SetupAxisTicks(ImAxis3D axis, const double* values, int n_ticks, const char* const labels[] = nullptr, bool keep_default = false);

// Sets an axis' ticks and optionally the labels for the next plot. To keep the default ticks, set #keep_default=true
IMPLOT3D_API void SetupAxisTicks(ImAxis3D axis, double v_min, double v_max, int n_ticks, const char* const labels[] = nullptr,
                                 bool keep_default = false);

// Sets an axis' limits constraints
IMPLOT3D_API void SetupAxisLimitsConstraints(ImAxis3D axis, double v_min, double v_max);

// Sets an axis' zoom constraints
IMPLOT3D_API void SetupAxisZoomConstraints(ImAxis3D axis, double z_min, double z_max);

// Sets the label and/or flags for primary X/Y/Z axes (shorthand for three calls to SetupAxis)
IMPLOT3D_API void SetupAxes(const char* x_label, const char* y_label, const char* z_label, ImPlot3DAxisFlags x_flags = 0,
                            ImPlot3DAxisFlags y_flags = 0, ImPlot3DAxisFlags z_flags = 0);

// Sets the X/Y/Z axes range limits. If ImPlot3DCond_Always is used, the axes limits will be locked (shorthand for two calls to SetupAxisLimits)
IMPLOT3D_API void SetupAxesLimits(double x_min, double x_max, double y_min, double y_max, double z_min, double z_max,
                                  ImPlot3DCond cond = ImPlot3DCond_Once);

// Sets the plot box rotation given the elevation and azimuth angles in degrees. If ImPlot3DCond_Always is used, the rotation will be locked
IMPLOT3D_API void SetupBoxRotation(float elevation, float azimuth, bool animate = false, ImPlot3DCond cond = ImPlot3DCond_Once);

// Sets the plot box rotation given a quaternion. If ImPlot3DCond_Always is used, the rotation will be locked
IMPLOT3D_API void SetupBoxRotation(ImPlot3DQuat rotation, bool animate = false, ImPlot3DCond cond = ImPlot3DCond_Once);

// Sets the plot box initial rotation given the elevation and azimuth angles in degrees. The initial rotation is the rotation the plot goes back to
// when a left mouse button double click happens
IMPLOT3D_API void SetupBoxInitialRotation(float elevation, float azimuth);

// Sets the plot box initial rotation given a quaternion. The initial rotation is the rotation the plot goes back to when a left mouse button double
// click happens
IMPLOT3D_API void SetupBoxInitialRotation(ImPlot3DQuat rotation);

// Sets the plot box X/Y/Z scale. A scale of 1.0 is the default. Values greater than 1.0 enlarge the plot, while values between 0.0 and 1.0 shrink it
IMPLOT3D_API void SetupBoxScale(float x, float y, float z);

IMPLOT3D_API void SetupLegend(ImPlot3DLocation location, ImPlot3DLegendFlags flags = 0);

//-----------------------------------------------------------------------------
// [SECTION] Plot Items
//-----------------------------------------------------------------------------

IMPLOT3D_TMP void PlotScatter(const char* label_id, const T* xs, const T* ys, const T* zs, int count, ImPlot3DScatterFlags flags = 0, int offset = 0,
                              int stride = sizeof(T));

IMPLOT3D_TMP void PlotLine(const char* label_id, const T* xs, const T* ys, const T* zs, int count, ImPlot3DLineFlags flags = 0, int offset = 0,
                           int stride = sizeof(T));

IMPLOT3D_TMP void PlotTriangle(const char* label_id, const T* xs, const T* ys, const T* zs, int count, ImPlot3DTriangleFlags flags = 0,
                               int offset = 0, int stride = sizeof(T));

IMPLOT3D_TMP void PlotQuad(const char* label_id, const T* xs, const T* ys, const T* zs, int count, ImPlot3DQuadFlags flags = 0, int offset = 0,
                           int stride = sizeof(T));

// Plot the surface defined by a grid of vertices. The grid is defined by the x and y arrays, and the z array contains the height of each vertex. A
// total of x_count * y_count vertices are expected for each array. Leave #scale_min and #scale_max both at 0 for automatic color scaling, or set them
// to a predefined range
IMPLOT3D_TMP void PlotSurface(const char* label_id, const T* xs, const T* ys, const T* zs, int x_count, int y_count, double scale_min = 0.0,
                              double scale_max = 0.0, ImPlot3DSurfaceFlags flags = 0, int offset = 0, int stride = sizeof(T));

IMPLOT3D_API void PlotMesh(const char* label_id, const ImPlot3DPoint* vtx, const unsigned int* idx, int vtx_count, int idx_count,
                           ImPlot3DMeshFlags flags = 0);

// Plots a rectangular image in 3D defined by its center and two direction vectors (axes).
// #center is the center of the rectangle in plot coordinates.
// #axis_u and #axis_v define the local axes and half-extents of the rectangle in 3D space.
// The rectangle is formed by moving from the center along ±axis_u and ±axis_v.
// #uv0 and #uv1 define the texture mapping.
// #tint_col can be used to tint the image.
IMPLOT3D_API void PlotImage(const char* label_id, ImTextureRef tex_ref, const ImPlot3DPoint& center, const ImPlot3DPoint& axis_u,
                            const ImPlot3DPoint& axis_v, const ImVec2& uv0 = ImVec2(0, 0), const ImVec2& uv1 = ImVec2(1, 1),
                            const ImVec4& tint_col = ImVec4(1, 1, 1, 1), ImPlot3DImageFlags flags = 0);

// Plots an image using four arbitrary 3D points that define a quad in space.
// Each corner (p0 to p3) corresponds to a corner in the image, and #uv0 to #uv3 are the texture coordinates for each.
// This overload allows full control over orientation, shape, and distortion.
// Note: The quad is internally split into two triangles, so non-rectangular quads may produce rendering artifacts
// since distortion is interpolated per triangle rather than over the full quad.
IMPLOT3D_API void PlotImage(const char* label_id, ImTextureRef tex_ref, const ImPlot3DPoint& p0, const ImPlot3DPoint& p1, const ImPlot3DPoint& p2,
                            const ImPlot3DPoint& p3, const ImVec2& uv0 = ImVec2(0, 0), const ImVec2& uv1 = ImVec2(1, 0),
                            const ImVec2& uv2 = ImVec2(1, 1), const ImVec2& uv3 = ImVec2(0, 1), const ImVec4& tint_col = ImVec4(1, 1, 1, 1),
                            ImPlot3DImageFlags flags = 0);

// Plots a centered text label at point x,y,z. It is possible to set the text angle in radians and offset in pixels
IMPLOT3D_API void PlotText(const char* text, float x, float y, float z, float angle = 0.0f, const ImVec2& pix_offset = ImVec2(0, 0));

//-----------------------------------------------------------------------------
// [SECTION] Plot Utils
//-----------------------------------------------------------------------------

// Convert a position in the current plot's coordinate system to pixels
IMPLOT3D_API ImVec2 PlotToPixels(const ImPlot3DPoint& point);
IMPLOT3D_API ImVec2 PlotToPixels(double x, double y, double z);
// Convert a pixel coordinate to a ray in the current plot's coordinate system
IMPLOT3D_API ImPlot3DRay PixelsToPlotRay(const ImVec2& pix);
IMPLOT3D_API ImPlot3DRay PixelsToPlotRay(double x, double y);
// Convert a pixel coordinate to a point in an axis plane in the current plot's coordinate system
IMPLOT3D_API ImPlot3DPoint PixelsToPlotPlane(const ImVec2& pix, ImPlane3D plane, bool mask = true);
IMPLOT3D_API ImPlot3DPoint PixelsToPlotPlane(double x, double y, ImPlane3D plane, bool mask = true);

IMPLOT3D_API ImVec2 GetPlotPos();  // Get the current plot position (top-left) in pixels
IMPLOT3D_API ImVec2 GetPlotSize(); // Get the current plot size in pixels

//-----------------------------------------------------------------------------
// [SECTION] Miscellaneous
//-----------------------------------------------------------------------------

IMPLOT3D_API ImDrawList* GetPlotDrawList();

//-----------------------------------------------------------------------------
// [SECTION] Styles
//-----------------------------------------------------------------------------

// Get current style
IMPLOT3D_API ImPlot3DStyle& GetStyle();
IMPLOT3D_API void SetStyle(const ImPlot3DStyle& style);

// Set color styles
IMPLOT3D_API void StyleColorsAuto(ImPlot3DStyle* dst = nullptr);    // Set colors with ImGui style
IMPLOT3D_API void StyleColorsDark(ImPlot3DStyle* dst = nullptr);    // Set colors with dark style
IMPLOT3D_API void StyleColorsLight(ImPlot3DStyle* dst = nullptr);   // Set colors with light style
IMPLOT3D_API void StyleColorsClassic(ImPlot3DStyle* dst = nullptr); // Set colors with classic style

// Temporarily modify a style color. Don't forget to call PopStyleColor!
IMPLOT3D_API void PushStyleColor(ImPlot3DCol idx, ImU32 col);
IMPLOT3D_API void PushStyleColor(ImPlot3DCol idx, const ImVec4& col);
// Undo temporary style color modification(s). Undo multiple pushes at once by increasing count
IMPLOT3D_API void PopStyleColor(int count = 1);

// Temporarily modify a style variable of float type. Don't forget to call PopStyleVar!
IMPLOT3D_API void PushStyleVar(ImPlot3DStyleVar idx, float val);
// Temporarily modify a style variable of int type. Don't forget to call PopStyleVar!
IMPLOT3D_API void PushStyleVar(ImPlot3DStyleVar idx, int val);
// Temporarily modify a style variable of ImVec2 type. Don't forget to call PopStyleVar!
IMPLOT3D_API void PushStyleVar(ImPlot3DStyleVar idx, const ImVec2& val);
// Undo temporary style variable modification(s). Undo multiple pushes at once by increasing count
IMPLOT3D_API void PopStyleVar(int count = 1);

// Set the line color and weight for the next item only
IMPLOT3D_API void SetNextLineStyle(const ImVec4& col = IMPLOT3D_AUTO_COL, float weight = IMPLOT3D_AUTO);
// Set the fill color for the next item only
IMPLOT3D_API void SetNextFillStyle(const ImVec4& col = IMPLOT3D_AUTO_COL, float alpha_mod = IMPLOT3D_AUTO);
// Set the marker style for the next item only
IMPLOT3D_API void SetNextMarkerStyle(ImPlot3DMarker marker = IMPLOT3D_AUTO, float size = IMPLOT3D_AUTO, const ImVec4& fill = IMPLOT3D_AUTO_COL,
                                     float weight = IMPLOT3D_AUTO, const ImVec4& outline = IMPLOT3D_AUTO_COL);

// Get color
IMPLOT3D_API ImVec4 GetStyleColorVec4(ImPlot3DCol idx);
IMPLOT3D_API ImU32 GetStyleColorU32(ImPlot3DCol idx);

//-----------------------------------------------------------------------------
// [SECTION] Colormaps
//-----------------------------------------------------------------------------

// Item styling is based on colormaps when the relevant ImPlot3DCol_XXX is set to
// IMPLOT3D_AUTO_COL (default). Several built-in colormaps are available. You can
// add and then push/pop your own colormaps as well. To permanently set a colormap,
// modify the Colormap index member of your ImPlot3DStyle.

// Colormap data will be ignored and a custom color will be used if you have done one of the following:
//     1) Modified an item style color in your ImPlot3DStyle to anything other than IMPLOT3D_AUTO_COL.
//     3) Set the next item style with a SetNextXXXStyle function.

// Add a new colormap. The color data will be copied. The colormap can be used by pushing either the returned index or the
// string name with PushColormap. The colormap name must be unique and the size must be greater than 1. You will receive
// an assert otherwise! By default colormaps are considered to be qualitative (i.e. discrete). If you want to create a
// continuous colormap, set #qual=false. This will treat the colors you provide as keys, and ImPlot3D will build a linearly
// interpolated lookup table. The memory footprint of this table will be exactly ((size-1)*255+1)*4 bytes.

IMPLOT3D_API ImPlot3DColormap AddColormap(const char* name, const ImVec4* cols, int size, bool qual = true);
IMPLOT3D_API ImPlot3DColormap AddColormap(const char* name, const ImU32* cols, int size, bool qual = true);

// Returns the number of available colormaps (i.e. the built-in + user-added count)
IMPLOT3D_API int GetColormapCount();
// Returns a null terminated string name for a colormap given an index. Returns nullptr if index is invalid
IMPLOT3D_API const char* GetColormapName(ImPlot3DColormap cmap);
// Returns an index number for a colormap given a valid string name. Returns -1 if name is invalid
IMPLOT3D_API ImPlot3DColormap GetColormapIndex(const char* name);

// Temporarily switch to one of the built-in (i.e. ImPlot3DColormap_XXX) or user-added colormaps (i.e. a return value of AddColormap). Don't forget to
// call PopColormap!
IMPLOT3D_API void PushColormap(ImPlot3DColormap cmap);
// Push a colormap by string name. Use built-in names such as "Default", "Deep", "Jet", etc. or a string you provided to AddColormap. Don't forget to
// call PopColormap!
IMPLOT3D_API void PushColormap(const char* name);
// Undo temporary colormap modification(s). Undo multiple pushes at once by increasing count
IMPLOT3D_API void PopColormap(int count = 1);

// Returns the next color from the current colormap and advances the colormap for the current plot
// Can also be used with no return value to skip colors if desired. You need to call it between Begin/EndPlot!
IMPLOT3D_API ImVec4 NextColormapColor();

// Returns the size of a colormap
IMPLOT3D_API int GetColormapSize(ImPlot3DColormap cmap = IMPLOT3D_AUTO);
// Returns a color from a colormap given an index >= 0 (modulo will be performed)
IMPLOT3D_API ImVec4 GetColormapColor(int idx, ImPlot3DColormap cmap = IMPLOT3D_AUTO);
// Sample a color from the current colormap given t between 0 and 1
IMPLOT3D_API ImVec4 SampleColormap(float t, ImPlot3DColormap cmap = IMPLOT3D_AUTO);

//-----------------------------------------------------------------------------
// [SECTION] Demo
//-----------------------------------------------------------------------------
// Add implot3d_demo.cpp to your sources to use methods in this section

// Shows the ImPlot3D demo window
IMPLOT3D_API void ShowDemoWindow(bool* p_open = nullptr);
// Shows all ImPlot3D demos, without enclosing window
IMPLOT3D_API void ShowAllDemos();

// Shows ImPlot3D style editor block (not a window)
IMPLOT3D_API void ShowStyleEditor(ImPlot3DStyle* ref = nullptr);

// Shows ImPlot3D metrics/debug information window.
IMPLOT3D_API void ShowMetricsWindow(bool* p_popen = nullptr);

} // namespace ImPlot3D

//-----------------------------------------------------------------------------
// [SECTION] ImPlot3DPoint
//-----------------------------------------------------------------------------

// ImPlot3DPoint: 3D vector to store points in 3D
struct ImPlot3DPoint {
    float x, y, z;
    constexpr ImPlot3DPoint() : x(0.0f), y(0.0f), z(0.0f) {}
    constexpr ImPlot3DPoint(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}

    // Accessors
    float& operator[](size_t idx) {
        IM_ASSERT(idx == 0 || idx == 1 || idx == 2);
        return ((float*)(void*)(char*)this)[idx];
    }
    float operator[](size_t idx) const {
        IM_ASSERT(idx == 0 || idx == 1 || idx == 2);
        return ((const float*)(const void*)(const char*)this)[idx];
    }

    // Binary operators
    IMPLOT3D_API ImPlot3DPoint operator*(float rhs) const;
    IMPLOT3D_API ImPlot3DPoint operator/(float rhs) const;
    IMPLOT3D_API ImPlot3DPoint operator+(const ImPlot3DPoint& rhs) const;
    IMPLOT3D_API ImPlot3DPoint operator-(const ImPlot3DPoint& rhs) const;
    IMPLOT3D_API ImPlot3DPoint operator*(const ImPlot3DPoint& rhs) const;
    IMPLOT3D_API ImPlot3DPoint operator/(const ImPlot3DPoint& rhs) const;

    // Unary operator
    IMPLOT3D_API ImPlot3DPoint operator-() const;

    // Compound assignment operators
    IMPLOT3D_API ImPlot3DPoint& operator*=(float rhs);
    IMPLOT3D_API ImPlot3DPoint& operator/=(float rhs);
    IMPLOT3D_API ImPlot3DPoint& operator+=(const ImPlot3DPoint& rhs);
    IMPLOT3D_API ImPlot3DPoint& operator-=(const ImPlot3DPoint& rhs);
    IMPLOT3D_API ImPlot3DPoint& operator*=(const ImPlot3DPoint& rhs);
    IMPLOT3D_API ImPlot3DPoint& operator/=(const ImPlot3DPoint& rhs);

    // Comparison operators
    IMPLOT3D_API bool operator==(const ImPlot3DPoint& rhs) const;
    IMPLOT3D_API bool operator!=(const ImPlot3DPoint& rhs) const;

    // Dot product
    IMPLOT3D_API float Dot(const ImPlot3DPoint& rhs) const;

    // Cross product
    IMPLOT3D_API ImPlot3DPoint Cross(const ImPlot3DPoint& rhs) const;

    // Get vector length
    IMPLOT3D_API float Length() const;

    // Get vector squared length
    IMPLOT3D_API float LengthSquared() const;

    // Normalize to unit length
    IMPLOT3D_API void Normalize();

    // Return vector normalized to unit length
    IMPLOT3D_API ImPlot3DPoint Normalized() const;

    // Friend binary operators to allow commutative behavior
    IMPLOT3D_API friend ImPlot3DPoint operator*(float lhs, const ImPlot3DPoint& rhs);

    // Check if the point is NaN
    IMPLOT3D_API bool IsNaN() const;

#ifdef IMPLOT3D_POINT_CLASS_EXTRA
    IMPLOT3D_POINT_CLASS_EXTRA // Define additional constructors and implicit cast operators in imconfig.h to convert back and forth between your math
                               // types and ImPlot3DPoint
#endif
};

//-----------------------------------------------------------------------------
// [SECTION] ImPlot3DRay
//-----------------------------------------------------------------------------

struct ImPlot3DRay {
    ImPlot3DPoint Origin;
    ImPlot3DPoint Direction;
};

//-----------------------------------------------------------------------------
// [SECTION] ImPlot3DPlane
//-----------------------------------------------------------------------------

struct ImPlot3DPlane {
    ImPlot3DPoint Point;
    ImPlot3DPoint Normal;
};

//-----------------------------------------------------------------------------
// [SECTION] ImPlot3DBox
//-----------------------------------------------------------------------------

struct ImPlot3DBox {
    ImPlot3DPoint Min;
    ImPlot3DPoint Max;

    // Default constructor
    constexpr ImPlot3DBox() : Min(ImPlot3DPoint()), Max(ImPlot3DPoint()) {}

    // Constructor with two points
    constexpr ImPlot3DBox(const ImPlot3DPoint& min, const ImPlot3DPoint& max) : Min(min), Max(max) {}

    // Method to expand the box to include a point
    IMPLOT3D_API void Expand(const ImPlot3DPoint& point);

    // Method to check if a point is inside the box
    IMPLOT3D_API bool Contains(const ImPlot3DPoint& point) const;

    // Method to clip a line segment against the box
    IMPLOT3D_API bool ClipLineSegment(const ImPlot3DPoint& p0, const ImPlot3DPoint& p1, ImPlot3DPoint& p0_clipped, ImPlot3DPoint& p1_clipped) const;
};

//-----------------------------------------------------------------------------
// [SECTION] ImPlot3DRange
//-----------------------------------------------------------------------------

struct ImPlot3DRange {
    float Min;
    float Max;

    constexpr ImPlot3DRange() : Min(0.0f), Max(0.0f) {}
    constexpr ImPlot3DRange(float min, float max) : Min(min), Max(max) {}

    IMPLOT3D_API void Expand(float value);
    IMPLOT3D_API bool Contains(float value) const;
    float Size() const { return Max - Min; }
};

//-----------------------------------------------------------------------------
// [SECTION] ImPlot3DQuat
//-----------------------------------------------------------------------------

struct ImPlot3DQuat {
    float x, y, z, w;

    // Constructors
    constexpr ImPlot3DQuat() : x(0.0f), y(0.0f), z(0.0f), w(1.0f) {}
    constexpr ImPlot3DQuat(float _x, float _y, float _z, float _w) : x(_x), y(_y), z(_z), w(_w) {}

    IMPLOT3D_API ImPlot3DQuat(float _angle, const ImPlot3DPoint& _axis);

    // Set quaternion from two vectors
    IMPLOT3D_API static ImPlot3DQuat FromTwoVectors(const ImPlot3DPoint& v0, const ImPlot3DPoint& v1);

    // Set quaternion given elevation and azimuth angles in radians
    IMPLOT3D_API static ImPlot3DQuat FromElAz(float elevation, float azimuth);

    // Get quaternion length
    IMPLOT3D_API float Length() const;

    // Get normalized quaternion
    IMPLOT3D_API ImPlot3DQuat Normalized() const;

    // Conjugate of the quaternion
    IMPLOT3D_API ImPlot3DQuat Conjugate() const;

    // Inverse of the quaternion
    IMPLOT3D_API ImPlot3DQuat Inverse() const;

    // Binary operators
    IMPLOT3D_API ImPlot3DQuat operator*(const ImPlot3DQuat& rhs) const;

    // Normalize the quaternion in place
    IMPLOT3D_API ImPlot3DQuat& Normalize();

    // Rotate a 3D point using the quaternion
    IMPLOT3D_API ImPlot3DPoint operator*(const ImPlot3DPoint& point) const;

    // Comparison operators
    IMPLOT3D_API bool operator==(const ImPlot3DQuat& rhs) const;
    IMPLOT3D_API bool operator!=(const ImPlot3DQuat& rhs) const;

    // Interpolate between two quaternions
    IMPLOT3D_API static ImPlot3DQuat Slerp(const ImPlot3DQuat& q1, const ImPlot3DQuat& q2, float t);

    // Get quaternion dot product
    IMPLOT3D_API float Dot(const ImPlot3DQuat& rhs) const;

#ifdef IMPLOT3D_QUAT_CLASS_EXTRA
    IMPLOT3D_QUAT_CLASS_EXTRA // Define additional constructors and implicit cast operators in imconfig.h to convert back and forth between your math
                              // types and ImPlot3DQuat
#endif
};

//-----------------------------------------------------------------------------
// [SECTION] ImPlot3DStyle
//-----------------------------------------------------------------------------

struct ImPlot3DStyle {
    // Item style
    float LineWeight;   // Line weight in pixels
    int Marker;         // Default marker type (ImPlot3DMarker_None)
    float MarkerSize;   // Marker size in pixels (roughly the marker's "radius")
    float MarkerWeight; // Marker outline weight in pixels
    float FillAlpha;    // Alpha modifier applied to plot fills
    // Plot style
    ImVec2 PlotDefaultSize;
    ImVec2 PlotMinSize;
    ImVec2 PlotPadding;
    ImVec2 LabelPadding;
    // Legend style
    ImVec2 LegendPadding;      // Legend padding from plot edges
    ImVec2 LegendInnerPadding; // Legend inner padding from legend edges
    ImVec2 LegendSpacing;      // Spacing between legend entries
    // Colors
    ImVec4 Colors[ImPlot3DCol_COUNT];
    inline ImVec4 GetColor(ImPlot3DCol idx) const { return Colors[idx]; }
    inline void SetColor(ImPlot3DCol idx, const ImVec4& col) { Colors[idx] = col; }
    // Colormap
    ImPlot3DColormap Colormap; // The current colormap. Set this to either an ImPlot3DColormap_ enum or an index returned by AddColormap
    // Constructor
    IMPLOT3D_API ImPlot3DStyle();
    ImPlot3DStyle(const ImPlot3DStyle& other) = default;
};

//-----------------------------------------------------------------------------
// [SECTION] Meshes
//-----------------------------------------------------------------------------

namespace ImPlot3D {

// Cube
constexpr int CUBE_VTX_COUNT = 8;              // Number of cube vertices
constexpr int CUBE_IDX_COUNT = 36;             // Number of cube indices (12 triangles)
extern ImPlot3DPoint cube_vtx[CUBE_VTX_COUNT]; // Cube vertices
extern unsigned int cube_idx[CUBE_IDX_COUNT];  // Cube indices

// Sphere
constexpr int SPHERE_VTX_COUNT = 162;              // Number of sphere vertices for 128 triangles
constexpr int SPHERE_IDX_COUNT = 960;              // Number of sphere indices (128 triangles)
extern ImPlot3DPoint sphere_vtx[SPHERE_VTX_COUNT]; // Sphere vertices
extern unsigned int sphere_idx[SPHERE_IDX_COUNT];  // Sphere indices

// Duck (Rubber Duck by Poly by Google [CC-BY] via Poly Pizza)
constexpr int DUCK_VTX_COUNT = 254;            // Number of duck vertices
constexpr int DUCK_IDX_COUNT = 1428;           // Number of duck indices
extern ImPlot3DPoint duck_vtx[DUCK_VTX_COUNT]; // Duck vertices
extern unsigned int duck_idx[DUCK_IDX_COUNT];  // Duck indices

} // namespace ImPlot3D

#endif // #ifndef IMGUI_DISABLE
