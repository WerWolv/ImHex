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

// You may use this file to debug, understand or extend ImPlot features but we
// don't provide any guarantee of forward compatibility!

//-----------------------------------------------------------------------------
// [SECTION] Header Mess
//-----------------------------------------------------------------------------

#pragma once

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include <time.h>
#include "imgui_internal.h"

#ifndef IMPLOT_VERSION
#error Must include implot.h before implot_internal.h
#endif

//-----------------------------------------------------------------------------
// [SECTION] Forward Declarations
//-----------------------------------------------------------------------------

struct ImPlotTick;
struct ImPlotAxis;
struct ImPlotAxisState;
struct ImPlotAxisColor;
struct ImPlotItem;
struct ImPlotLegendData;
struct ImPlotPlot;
struct ImPlotNextPlotData;

//-----------------------------------------------------------------------------
// [SECTION] Context Pointer
//-----------------------------------------------------------------------------

extern IMPLOT_API ImPlotContext* GImPlot; // Current implicit context pointer

//-----------------------------------------------------------------------------
// [SECTION] Macros and Constants
//-----------------------------------------------------------------------------

// Constants can be changed unless stated otherwise. We may move some of these
// to ImPlotStyleVar_ over time.

// The maximum number of supported y-axes (DO NOT CHANGE THIS)
#define IMPLOT_Y_AXES    3
// The number of times to subdivided grid divisions (best if a multiple of 1, 2, and 5)
#define IMPLOT_SUB_DIV   10
// Zoom rate for scroll (e.g. 0.1f = 10% plot range every scroll click)
#define IMPLOT_ZOOM_RATE 0.1f
// Mimimum allowable timestamp value 01/01/1970 @ 12:00am (UTC) (DO NOT DECREASE THIS)
#define IMPLOT_MIN_TIME  0
// Maximum allowable timestamp value 01/01/3000 @ 12:00am (UTC) (DO NOT INCREASE THIS)
#define IMPLOT_MAX_TIME  32503680000

//-----------------------------------------------------------------------------
// [SECTION] Generic Helpers
//-----------------------------------------------------------------------------

// Computes the common (base-10) logarithm
static inline float  ImLog10(float x)  { return log10f(x); }
static inline double ImLog10(double x) { return log10(x);  }
// Returns true if a flag is set
template <typename TSet, typename TFlag>
inline bool ImHasFlag(TSet set, TFlag flag) { return (set & flag) == flag; }
// Flips a flag in a flagset
template <typename TSet, typename TFlag>
inline void ImFlipFlag(TSet& set, TFlag flag) { ImHasFlag(set, flag) ? set &= ~flag : set |= flag; }
// Linearly remaps x from [x0 x1] to [y0 y1].
template <typename T>
inline T ImRemap(T x, T x0, T x1, T y0, T y1) { return y0 + (x - x0) * (y1 - y0) / (x1 - x0); }
// Returns always positive modulo (assumes r != 0)
inline int ImPosMod(int l, int r) { return (l % r + r) % r; }
// Returns true if val is NAN or INFINITY
inline bool ImNanOrInf(double val) { return val == HUGE_VAL || val == -HUGE_VAL || isnan(val); }
// Turns NANs to 0s
inline double ImConstrainNan(double val) { return isnan(val) ? 0 : val; }
// Turns infinity to floating point maximums
inline double ImConstrainInf(double val) { return val == HUGE_VAL ?  DBL_MAX : val == -HUGE_VAL ? - DBL_MAX : val; }
// Turns numbers less than or equal to 0 to 0.001 (sort of arbitrary, is there a better way?)
inline double ImConstrainLog(double val) { return val <= 0 ? 0.001f : val; }
// Turns numbers less than 0 to zero
inline double ImConstrainTime(double val) { return val < IMPLOT_MIN_TIME ? IMPLOT_MIN_TIME : (val > IMPLOT_MAX_TIME ? IMPLOT_MAX_TIME : val); }

// Offset calculator helper
template <int Count>
struct ImOffsetCalculator {
    ImOffsetCalculator(const int* sizes) {
        Offsets[0] = 0;
        for (int i = 1; i < Count; ++i)
            Offsets[i] = Offsets[i-1] + sizes[i-1];
    }
    int Offsets[Count];
};

// Character buffer writer helper
struct ImBufferWriter
{
    char*  Buffer;
    int Size;
    int Pos;

    ImBufferWriter(char* buffer, int size) {
        Buffer = buffer;
        Size = size;
        Pos = 0;
    }

    void Write(const char* fmt, ...) {
        va_list argp;
        va_start(argp, fmt);
        const int written = ::vsnprintf(&Buffer[Pos], Size - Pos - 1, fmt, argp);
        if (written > 0)
          Pos += ImMin(written, Size-Pos-1);
        va_end(argp);
    }
};

// Fixed size point array
template <int N>
struct ImPlotPointArray {
    inline ImPlotPoint&       operator[](int i)       { return Data[i]; }
    inline const ImPlotPoint& operator[](int i) const { return Data[i]; }
    inline int Size()                                 { return N; }
    ImPlotPoint Data[N];
};

//-----------------------------------------------------------------------------
// [SECTION] ImPlot Enums
//-----------------------------------------------------------------------------

typedef int ImPlotScale;       // -> enum ImPlotScale_
typedef int ImPlotTimeUnit;    // -> enum ImPlotTimeUnit_
typedef int ImPlotDateFmt;     // -> enum ImPlotDateFmt_
typedef int ImPlotTimeFmt;     // -> enum ImPlotTimeFmt_

// XY axes scaling combinations
enum ImPlotScale_ {
    ImPlotScale_LinLin, // linear x, linear y
    ImPlotScale_LogLin, // log x,    linear y
    ImPlotScale_LinLog, // linear x, log y
    ImPlotScale_LogLog  // log x,    log y
};

enum ImPlotTimeUnit_ {
    ImPlotTimeUnit_Us,  // microsecond
    ImPlotTimeUnit_Ms,  // millisecond
    ImPlotTimeUnit_S,   // second
    ImPlotTimeUnit_Min, // minute
    ImPlotTimeUnit_Hr,  // hour
    ImPlotTimeUnit_Day, // day
    ImPlotTimeUnit_Mo,  // month
    ImPlotTimeUnit_Yr,  // year
    ImPlotTimeUnit_COUNT
};

enum ImPlotDateFmt_ {              // default        [ ISO 8601     ]
    ImPlotDateFmt_None = 0,
    ImPlotDateFmt_DayMo,           // 10/3           [ --10-03      ]
    ImPlotDateFmt_DayMoYr,         // 10/3/91        [ 1991-10-03   ]
    ImPlotDateFmt_MoYr,            // Oct 1991       [ 1991-10      ]
    ImPlotDateFmt_Mo,              // Oct            [ --10         ]
    ImPlotDateFmt_Yr               // 1991           [ 1991         ]
};

enum ImPlotTimeFmt_ {              // default        [ 24 Hour Clock ]
    ImPlotTimeFmt_None = 0,
    ImPlotTimeFmt_Us,              // .428 552       [ .428 552     ]
    ImPlotTimeFmt_SUs,             // :29.428 552    [ :29.428 552  ]
    ImPlotTimeFmt_SMs,             // :29.428        [ :29.428      ]
    ImPlotTimeFmt_S,               // :29            [ :29          ]
    ImPlotTimeFmt_HrMinSMs,        // 7:21:29.428pm  [ 19:21:29.428 ]
    ImPlotTimeFmt_HrMinS,          // 7:21:29pm      [ 19:21:29     ]
    ImPlotTimeFmt_HrMin,           // 7:21pm         [ 19:21        ]
    ImPlotTimeFmt_Hr               // 7pm            [ 19:00        ]
};

//-----------------------------------------------------------------------------
// [SECTION] ImPlot Structs
//-----------------------------------------------------------------------------

// Combined data/time format spec
struct ImPlotDateTimeFmt {
    ImPlotDateTimeFmt(ImPlotDateFmt date_fmt, ImPlotTimeFmt time_fmt, bool use_24_hr_clk = false, bool use_iso_8601 = false) {
        Date           = date_fmt;
        Time           = time_fmt;
        UseISO8601     = use_iso_8601;
        Use24HourClock = use_24_hr_clk;
    }
    ImPlotDateFmt Date;
    ImPlotTimeFmt Time;
    bool UseISO8601;
    bool Use24HourClock;
};

// Two part timestamp struct.
struct ImPlotTime {
    time_t S;  // second part
    int    Us; // microsecond part
    ImPlotTime() { S = 0; Us = 0; }
    ImPlotTime(time_t s, int us = 0) { S  = s + us / 1000000; Us = us % 1000000; }
    void RollOver() { S  = S + Us / 1000000;  Us = Us % 1000000; }
    double ToDouble() const { return (double)S + (double)Us / 1000000.0; }
    static ImPlotTime FromDouble(double t) { return ImPlotTime((time_t)t, (int)(t * 1000000 - floor(t) * 1000000)); }
};

static inline ImPlotTime operator+(const ImPlotTime& lhs, const ImPlotTime& rhs)
{ return ImPlotTime(lhs.S + rhs.S, lhs.Us + rhs.Us); }
static inline ImPlotTime operator-(const ImPlotTime& lhs, const ImPlotTime& rhs)
{ return ImPlotTime(lhs.S - rhs.S, lhs.Us - rhs.Us); }
static inline bool operator==(const ImPlotTime& lhs, const ImPlotTime& rhs)
{ return lhs.S == rhs.S && lhs.Us == rhs.Us; }
static inline bool operator<(const ImPlotTime& lhs, const ImPlotTime& rhs)
{ return lhs.S == rhs.S ? lhs.Us < rhs.Us : lhs.S < rhs.S; }
static inline bool operator>(const ImPlotTime& lhs, const ImPlotTime& rhs)
{ return rhs < lhs; }
static inline bool operator<=(const ImPlotTime& lhs, const ImPlotTime& rhs)
{ return lhs < rhs || lhs == rhs; }
static inline bool operator>=(const ImPlotTime& lhs, const ImPlotTime& rhs)
{ return lhs > rhs || lhs == rhs; }

// Storage for colormap modifiers
struct ImPlotColormapMod {
    ImPlotColormapMod(const ImVec4* colormap, int colormap_size) {
        Colormap     = colormap;
        ColormapSize = colormap_size;
    }
    const ImVec4* Colormap;
    int ColormapSize;
};

// ImPlotPoint with positive/negative error values
struct ImPlotPointError
{
    double X, Y, Neg, Pos;
    ImPlotPointError(double x, double y, double neg, double pos) {
        X = x; Y = y; Neg = neg; Pos = pos;
    }
};

// Interior plot label/annotation
struct ImPlotAnnotation {
    ImVec2 Pos;
    ImVec2 Offset;
    ImU32  ColorBg;
    ImU32  ColorFg;
    int    TextOffset;
    bool   Clamp;
};

// Collection of plot labels
struct ImPlotAnnotationCollection {

    ImVector<ImPlotAnnotation> Annotations;
    ImGuiTextBuffer            TextBuffer;
    int                        Size;

    ImPlotAnnotationCollection() { Reset(); }

    void AppendV(const ImVec2& pos, const ImVec2& off, ImU32 bg, ImU32 fg, bool clamp, const char* fmt,  va_list args) IM_FMTLIST(7) {
        ImPlotAnnotation an;
        an.Pos = pos; an.Offset = off;
        an.ColorBg = bg; an.ColorFg = fg;
        an.TextOffset = TextBuffer.size();
        an.Clamp = clamp;
        Annotations.push_back(an);
        TextBuffer.appendfv(fmt, args);
        const char nul[] = "";
        TextBuffer.append(nul,nul+1);
        Size++;
    }

    void Append(const ImVec2& pos, const ImVec2& off, ImU32 bg, ImU32 fg, bool clamp, const char* fmt,  ...) IM_FMTARGS(7) {
        va_list args;
        va_start(args, fmt);
        AppendV(pos, off, bg, fg, clamp, fmt, args);
        va_end(args);
    }

    const char* GetText(int idx) {
        return TextBuffer.Buf.Data + Annotations[idx].TextOffset;
    }

    void Reset() {
        Annotations.shrink(0);
        TextBuffer.Buf.shrink(0);
        Size = 0;
    }
};

// Tick mark info
struct ImPlotTick
{
    double PlotPos;
    float  PixelPos;
    ImVec2 LabelSize;
    int    TextOffset;
    bool   Major;
    bool   ShowLabel;
    int    Level;

    ImPlotTick(double value, bool major, bool show_label) {
        PlotPos      = value;
        Major        = major;
        ShowLabel    = show_label;
        TextOffset   = -1;
        Level        = 0;
    }
};

// Collection of ticks
struct ImPlotTickCollection {
    ImVector<ImPlotTick> Ticks;
    ImGuiTextBuffer      TextBuffer;
    float                TotalWidth;
    float                TotalHeight;
    float                MaxWidth;
    float                MaxHeight;
    int                  Size;

    ImPlotTickCollection() { Reset(); }

    void Append(const ImPlotTick& tick) {
        if (tick.ShowLabel) {
            TotalWidth  += tick.ShowLabel ? tick.LabelSize.x : 0;
            TotalHeight += tick.ShowLabel ? tick.LabelSize.y : 0;
            MaxWidth    =  tick.LabelSize.x > MaxWidth  ? tick.LabelSize.x : MaxWidth;
            MaxHeight   =  tick.LabelSize.y > MaxHeight ? tick.LabelSize.y : MaxHeight;
        }
        Ticks.push_back(tick);
        Size++;
    }

    void Append(double value, bool major, bool show_label, void (*labeler)(ImPlotTick& tick, ImGuiTextBuffer& buf)) {
        ImPlotTick tick(value, major, show_label);
        if (labeler)
            labeler(tick, TextBuffer);
        Append(tick);
    }

    const char* GetText(int idx) {
        return TextBuffer.Buf.Data + Ticks[idx].TextOffset;
    }

    void Reset() {
        Ticks.shrink(0);
        TextBuffer.Buf.shrink(0);
        TotalWidth = TotalHeight = MaxWidth = MaxHeight = 0;
        Size = 0;
    }
};

// Axis state information that must persist after EndPlot
struct ImPlotAxis
{
    ImPlotAxisFlags Flags;
    ImPlotAxisFlags PreviousFlags;
    ImPlotRange     Range;
    ImPlotOrientation Direction;
    bool            Dragging;
    bool            HoveredExt;
    bool            HoveredTot;
    double*         LinkedMin;
    double*         LinkedMax;
    ImPlotTime      PickerTimeMin, PickerTimeMax;
    int             PickerLevel;

    ImPlotAxis() {
        Flags      = PreviousFlags = ImPlotAxisFlags_None;
        Range.Min  = 0;
        Range.Max  = 1;
        Dragging   = false;
        HoveredExt = false;
        HoveredTot = false;
        LinkedMin  = LinkedMax = NULL;
        PickerLevel = 0;
    }

    bool SetMin(double _min) {
        _min = ImConstrainNan(ImConstrainInf(_min));
        if (ImHasFlag(Flags, ImPlotAxisFlags_LogScale))
            _min = ImConstrainLog(_min);
        if (ImHasFlag(Flags, ImPlotAxisFlags_Time))
            _min = ImConstrainTime(_min);
        if (_min >= Range.Max)
            return false;
        Range.Min = _min;
        PickerTimeMin = ImPlotTime::FromDouble(Range.Min);
        return true;
    };

    bool SetMax(double _max) {
        _max = ImConstrainNan(ImConstrainInf(_max));
        if (ImHasFlag(Flags, ImPlotAxisFlags_LogScale))
            _max = ImConstrainLog(_max);
        if (ImHasFlag(Flags, ImPlotAxisFlags_Time))
            _max = ImConstrainTime(_max);
        if (_max <= Range.Min)
            return false;
        Range.Max = _max;
        PickerTimeMax = ImPlotTime::FromDouble(Range.Max);
        return true;
    };

    void SetRange(double _min, double _max) {
        Range.Min = _min;
        Range.Max = _max;
        Constrain();
        PickerTimeMin = ImPlotTime::FromDouble(Range.Min);
        PickerTimeMax = ImPlotTime::FromDouble(Range.Max);
    }

    void SetRange(const ImPlotRange& range) {
        SetRange(range.Min, range.Max);
    }

    void Constrain() {
        Range.Min = ImConstrainNan(ImConstrainInf(Range.Min));
        Range.Max = ImConstrainNan(ImConstrainInf(Range.Max));
        if (ImHasFlag(Flags, ImPlotAxisFlags_LogScale)) {
            Range.Min = ImConstrainLog(Range.Min);
            Range.Max = ImConstrainLog(Range.Max);
        }
        if (ImHasFlag(Flags, ImPlotAxisFlags_Time)) {
            Range.Min = ImConstrainTime(Range.Min);
            Range.Max = ImConstrainTime(Range.Max);
        }
        if (Range.Max <= Range.Min)
            Range.Max = Range.Min + DBL_EPSILON;
    }
};

// Axis state information only needed between BeginPlot/EndPlot
struct ImPlotAxisState
{
    ImPlotAxis* Axis;
    ImGuiCond   RangeCond;
    bool        HasRange;
    bool        Present;
    bool        HasLabels;
    bool        Invert;
    bool        LockMin;
    bool        LockMax;
    bool        Lock;
    bool        IsTime;

    ImPlotAxisState(ImPlotAxis* axis, bool has_range, ImGuiCond range_cond, bool present) {
        Axis         = axis;
        HasRange     = has_range;
        RangeCond    = range_cond;
        Present      = present;
        HasLabels    = !ImHasFlag(Axis->Flags, ImPlotAxisFlags_NoTickLabels);
        Invert       = ImHasFlag(Axis->Flags, ImPlotAxisFlags_Invert);
        LockMin      = ImHasFlag(Axis->Flags, ImPlotAxisFlags_LockMin) || (HasRange && RangeCond == ImGuiCond_Always);
        LockMax      = ImHasFlag(Axis->Flags, ImPlotAxisFlags_LockMax) || (HasRange && RangeCond == ImGuiCond_Always);
        Lock         = !Present || ((LockMin && LockMax) || (HasRange && RangeCond == ImGuiCond_Always));
        IsTime       = ImHasFlag(Axis->Flags, ImPlotAxisFlags_Time);
    }

    ImPlotAxisState() { }
};

struct ImPlotAxisColor
{
    ImU32 Major, Minor, MajTxt, MinTxt;
    ImPlotAxisColor() { Major = Minor = MajTxt = MinTxt = 0; }
};

// State information for Plot items
struct ImPlotItem
{
    ImGuiID      ID;
    ImVec4       Color;
    int          NameOffset;
    bool         Show;
    bool         LegendHovered;
    bool         SeenThisFrame;

    ImPlotItem() {
        ID            = 0;
        Color         = ImPlot::NextColormapColor();
        NameOffset    = -1;
        Show          = true;
        SeenThisFrame = false;
        LegendHovered = false;
    }

    ~ImPlotItem() { ID = 0; }
};

// Holds Legend state labels and item references
struct ImPlotLegendData
{
    ImVector<int>   Indices;
    ImGuiTextBuffer Labels;
    void Reset() { Indices.shrink(0); Labels.Buf.shrink(0); }
};

// Holds Plot state information that must persist after EndPlot
struct ImPlotPlot
{
    ImGuiID            ID;
    ImPlotFlags        Flags;
    ImPlotFlags        PreviousFlags;
    ImPlotAxis         XAxis;
    ImPlotAxis         YAxis[IMPLOT_Y_AXES];
    ImPlotLegendData   LegendData;
    ImPool<ImPlotItem> Items;
    ImVec2             SelectStart;
    ImVec2             QueryStart;
    ImRect             QueryRect;
    bool               Selecting;
    bool               Querying;
    bool               Queried;
    bool               DraggingQuery;
    bool               LegendHovered;
    bool               LegendOutside;
    bool               LegendFlipSide;
    int                ColormapIdx;
    int                CurrentYAxis;
    ImPlotLocation     MousePosLocation;
    ImPlotLocation     LegendLocation;
    ImPlotOrientation  LegendOrientation;

    ImPlotPlot() {
        Flags           = PreviousFlags = ImPlotFlags_None;
        XAxis.Direction = ImPlotOrientation_Horizontal;
        for (int i = 0; i < IMPLOT_Y_AXES; ++i)
            YAxis[i].Direction = ImPlotOrientation_Vertical;
        SelectStart       = QueryStart = ImVec2(0,0);
        Selecting         = Querying = Queried = DraggingQuery = LegendHovered = LegendOutside = LegendFlipSide = false;
        ColormapIdx       = CurrentYAxis = 0;
        LegendLocation    = ImPlotLocation_North | ImPlotLocation_West;
        LegendOrientation = ImPlotOrientation_Vertical;
        MousePosLocation  = ImPlotLocation_South | ImPlotLocation_East;
    }

    int         GetLegendCount() const   { return LegendData.Indices.size(); }
    ImPlotItem* GetLegendItem(int i);
    const char* GetLegendLabel(int i);
};

// Temporary data storage for upcoming plot
struct ImPlotNextPlotData
{
    ImGuiCond   XRangeCond;
    ImGuiCond   YRangeCond[IMPLOT_Y_AXES];
    ImPlotRange X;
    ImPlotRange Y[IMPLOT_Y_AXES];
    bool        HasXRange;
    bool        HasYRange[IMPLOT_Y_AXES];
    bool        ShowDefaultTicksX;
    bool        ShowDefaultTicksY[IMPLOT_Y_AXES];
    bool        FitX;
    bool        FitY[IMPLOT_Y_AXES];
    double*     LinkedXmin;
    double*     LinkedXmax;
    double*     LinkedYmin[IMPLOT_Y_AXES];
    double*     LinkedYmax[IMPLOT_Y_AXES];

    ImPlotNextPlotData() {
        HasXRange         = false;
        ShowDefaultTicksX = true;
        FitX              = false;
        LinkedXmin = LinkedXmax = NULL;
        for (int i = 0; i < IMPLOT_Y_AXES; ++i) {
            HasYRange[i]         = false;
            ShowDefaultTicksY[i] = true;
            FitY[i]              = false;
            LinkedYmin[i] = LinkedYmax[i] = NULL;
        }
    }
};

// Temporary data storage for upcoming item
struct ImPlotNextItemData {
    ImVec4       Colors[5]; // ImPlotCol_Line, ImPlotCol_Fill, ImPlotCol_MarkerOutline, ImPlotCol_MarkerFill, ImPlotCol_ErrorBar
    float        LineWeight;
    ImPlotMarker Marker;
    float        MarkerSize;
    float        MarkerWeight;
    float        FillAlpha;
    float        ErrorBarSize;
    float        ErrorBarWeight;
    float        DigitalBitHeight;
    float        DigitalBitGap;
    bool         RenderLine;
    bool         RenderFill;
    bool         RenderMarkerLine;
    bool         RenderMarkerFill;
    bool         HasHidden;
    bool         Hidden;
    ImGuiCond    HiddenCond;
    ImPlotNextItemData() {
        for (int i = 0; i < 5; ++i)
            Colors[i] = IMPLOT_AUTO_COL;
        LineWeight    = MarkerSize = MarkerWeight = FillAlpha = ErrorBarSize = ErrorBarWeight = DigitalBitHeight = DigitalBitGap = IMPLOT_AUTO;
        Marker        = IMPLOT_AUTO;
        HasHidden     = Hidden = false;
    }
};

// Holds state information that must persist between calls to BeginPlot()/EndPlot()
struct ImPlotContext {
    // Plot States
    ImPool<ImPlotPlot> Plots;
    ImPlotPlot*        CurrentPlot;
    ImPlotItem*        CurrentItem;
    ImPlotItem*        PreviousItem;

    // Bounding Boxes
    ImRect BB_Frame;
    ImRect BB_Canvas;
    ImRect BB_Plot;
    ImRect BB_Axes;
    ImRect BB_X;
    ImRect BB_Y[IMPLOT_Y_AXES];

    // Axis States
    ImPlotAxisColor Col_X;
    ImPlotAxisColor Col_Y[IMPLOT_Y_AXES];
    ImPlotAxisState X;
    ImPlotAxisState Y[IMPLOT_Y_AXES];

    // Tick Marks and Labels
    ImPlotTickCollection XTicks;
    ImPlotTickCollection YTicks[IMPLOT_Y_AXES];
    float                YAxisReference[IMPLOT_Y_AXES];

    // Annotation and User Labels
    ImPlotAnnotationCollection Annotations;

    // Transformations and Data Extents
    ImPlotScale Scales[IMPLOT_Y_AXES];
    ImRect      PixelRange[IMPLOT_Y_AXES];
    double      Mx;
    double      My[IMPLOT_Y_AXES];
    double      LogDenX;
    double      LogDenY[IMPLOT_Y_AXES];
    ImPlotRange ExtentsX;
    ImPlotRange ExtentsY[IMPLOT_Y_AXES];

    // Data Fitting Flags
    bool FitThisFrame;
    bool FitX;
    bool FitY[IMPLOT_Y_AXES];

    // Hover states
    bool Hov_Frame;
    bool Hov_Plot;

    // Axis Rendering Flags
    bool RenderX;
    bool RenderY[IMPLOT_Y_AXES];

    // Axis Locking Flags
    bool LockPlot;
    bool ChildWindowMade;

    // Style and Colormaps
    ImPlotStyle                 Style;
    ImVector<ImGuiColorMod>     ColorModifiers;
    ImVector<ImGuiStyleMod>     StyleModifiers;
    const ImVec4*               Colormap;
    int                         ColormapSize;
    ImVector<ImPlotColormapMod> ColormapModifiers;

    // Time
    tm Tm;

    // Misc
    int                VisibleItemCount;
    int                DigitalPlotItemCnt;
    int                DigitalPlotOffset;
    ImPlotNextPlotData NextPlotData;
    ImPlotNextItemData NextItemData;
    ImPlotInputMap     InputMap;
    ImPlotPoint        MousePos[IMPLOT_Y_AXES];
};

struct ImPlotAxisScale
{
    ImPlotPoint Min, Max;

    ImPlotAxisScale(int y_axis, float tx, float ty, float zoom_rate) {
        ImPlotContext& gp = *GImPlot;
        Min = ImPlot::PixelsToPlot(gp.BB_Plot.Min - gp.BB_Plot.GetSize() * ImVec2(tx * zoom_rate, ty * zoom_rate), y_axis);
        Max = ImPlot::PixelsToPlot(gp.BB_Plot.Max + gp.BB_Plot.GetSize() * ImVec2((1 - tx) * zoom_rate, (1 - ty) * zoom_rate), y_axis);
    }
};

//-----------------------------------------------------------------------------
// [SECTION] Internal API
// No guarantee of forward compatibility here!
//-----------------------------------------------------------------------------

namespace ImPlot {

//-----------------------------------------------------------------------------
// [SECTION] Context Utils
//-----------------------------------------------------------------------------

// Initializes an ImPlotContext
IMPLOT_API void Initialize(ImPlotContext* ctx);
// Resets an ImPlot context for the next call to BeginPlot
IMPLOT_API void Reset(ImPlotContext* ctx);

//-----------------------------------------------------------------------------
// [SECTION] Plot Utils
//-----------------------------------------------------------------------------

// Gets a plot from the current ImPlotContext
IMPLOT_API ImPlotPlot* GetPlot(const char* title);
// Gets the current plot from the current ImPlotContext
IMPLOT_API ImPlotPlot* GetCurrentPlot();
// Busts the cache for every plot in the current context
IMPLOT_API void BustPlotCache();

// Shows a plot's context menu.
IMPLOT_API void ShowPlotContextMenu(ImPlotPlot& plot);

//-----------------------------------------------------------------------------
// [SECTION] Item Utils
//-----------------------------------------------------------------------------

// Begins a new item. Returns false if the item should not be plotted. Pushes PlotClipRect.
IMPLOT_API bool BeginItem(const char* label_id, ImPlotCol recolor_from = -1);
// Ends an item (call only if BeginItem returns true). Pops PlotClipRect.
IMPLOT_API void EndItem();

// Register or get an existing item from the current plot.
IMPLOT_API ImPlotItem* RegisterOrGetItem(const char* label_id, bool* just_created = NULL);
// Get a plot item from the current plot.
IMPLOT_API ImPlotItem* GetItem(const char* label_id);
// Gets the current item.
IMPLOT_API ImPlotItem* GetCurrentItem();
// Busts the cache for every item for every plot in the current context.
IMPLOT_API void BustItemCache();

//-----------------------------------------------------------------------------
// [SECTION] Axis Utils
//-----------------------------------------------------------------------------

// Gets the current y-axis for the current plot
inline int GetCurrentYAxis() { return GImPlot->CurrentPlot->CurrentYAxis; }
// Updates axis ticks, lins, and label colors
IMPLOT_API void UpdateAxisColors(int axis_flag, ImPlotAxisColor* col);

// Updates plot-to-pixel space transformation variables for the current plot.
IMPLOT_API void UpdateTransformCache();
// Gets the XY scale for the current plot and y-axis
inline ImPlotScale GetCurrentScale() { return GImPlot->Scales[GetCurrentYAxis()]; }

// Returns true if the user has requested data to be fit.
inline bool FitThisFrame() { return GImPlot->FitThisFrame; }
// Extends the current plots axes so that it encompasses point p
IMPLOT_API void FitPoint(const ImPlotPoint& p);

// Returns true if two ranges overlap
inline bool RangesOverlap(const ImPlotRange& r1, const ImPlotRange& r2)
{ return r1.Min <= r2.Max && r2.Min <= r1.Max; }

// Updates pointers for linked axes from axis internal range.
IMPLOT_API void PushLinkedAxis(ImPlotAxis& axis);
// Updates axis internal range from points for linked axes.
IMPLOT_API void PullLinkedAxis(ImPlotAxis& axis);

// Shows an axis's context menu.
IMPLOT_API void ShowAxisContextMenu(ImPlotAxisState& state, bool time_allowed = false);

//-----------------------------------------------------------------------------
// [SECTION] Legend Utils
//-----------------------------------------------------------------------------

// Gets the position of an inner rect that is located inside of an outer rect according to an ImPlotLocation and padding amount.
IMPLOT_API ImVec2 GetLocationPos(const ImRect& outer_rect, const ImVec2& inner_size, ImPlotLocation location, const ImVec2& pad = ImVec2(0,0));
// Calculates the bounding box size of a legend
IMPLOT_API ImVec2 CalcLegendSize(ImPlotPlot& plot, const ImVec2& pad, const ImVec2& spacing, ImPlotOrientation orientation);
// Renders legend entries into a bounding box
IMPLOT_API void ShowLegendEntries(ImPlotPlot& plot, const ImRect& legend_bb, bool interactable, const ImVec2& pad, const ImVec2& spacing, ImPlotOrientation orientation, ImDrawList& DrawList);
// Shows an alternate legend for the plot identified by #title_id, outside of the plot frame (can be called before or after of Begin/EndPlot but must occur in the same ImGui window!).
IMPLOT_API void ShowAltLegend(const char* title_id, ImPlotOrientation orientation = ImPlotOrientation_Vertical, const ImVec2 size = ImVec2(0,0), bool interactable = true);

//-----------------------------------------------------------------------------
// [SECTION] Tick Utils
//-----------------------------------------------------------------------------

// Label a tick with default formatting.
IMPLOT_API void LabelTickDefault(ImPlotTick& tick, ImGuiTextBuffer& buffer);
// Label a tick with scientific formating.
IMPLOT_API void LabelTickScientific(ImPlotTick& tick, ImGuiTextBuffer& buffer);
// Label a tick with time formatting.
IMPLOT_API void LabelTickTime(ImPlotTick& tick, ImGuiTextBuffer& buffer, const ImPlotTime& t, ImPlotDateTimeFmt fmt);

// Populates a list of ImPlotTicks with normal spaced and formatted ticks
IMPLOT_API void AddTicksDefault(const ImPlotRange& range, int nMajor, int nMinor, ImPlotTickCollection& ticks);
// Populates a list of ImPlotTicks with logarithmic space and formatted ticks
IMPLOT_API void AddTicksLogarithmic(const ImPlotRange& range, int nMajor, ImPlotTickCollection& ticks);
// Populates a list of ImPlotTicks with time formatted ticks.
IMPLOT_API void AddTicksTime(const ImPlotRange& range, int nMajor, ImPlotTickCollection& ticks);
// Populates a list of ImPlotTicks with custom spaced and labeled ticks
IMPLOT_API void AddTicksCustom(const double* values, const char* const labels[], int n, ImPlotTickCollection& ticks);

// Create a a string label for a an axis value
IMPLOT_API int LabelAxisValue(const ImPlotAxis& axis, const ImPlotTickCollection& ticks, double value, char* buff, int size);

//-----------------------------------------------------------------------------
// [SECTION] Styling Utils
//-----------------------------------------------------------------------------

// Get styling data for next item (call between Begin/EndItem)
inline const ImPlotNextItemData& GetItemData() { return GImPlot->NextItemData; }

// Returns true if a color is set to be automatically determined
inline bool IsColorAuto(const ImVec4& col) { return col.w == -1; }
// Returns true if a style color is set to be automaticaly determined
inline bool IsColorAuto(ImPlotCol idx) { return IsColorAuto(GImPlot->Style.Colors[idx]); }
// Returns the automatically deduced style color
IMPLOT_API ImVec4 GetAutoColor(ImPlotCol idx);

// Returns the style color whether it is automatic or custom set
inline ImVec4 GetStyleColorVec4(ImPlotCol idx) { return IsColorAuto(idx) ? GetAutoColor(idx) : GImPlot->Style.Colors[idx]; }
inline ImU32  GetStyleColorU32(ImPlotCol idx)  { return ImGui::ColorConvertFloat4ToU32(GetStyleColorVec4(idx)); }

// Get built-in colormap data and size
IMPLOT_API const ImVec4* GetColormap(ImPlotColormap colormap, int* size_out);
// Linearly interpolates a color from the current colormap given t between 0 and 1.
IMPLOT_API ImVec4 LerpColormap(const ImVec4* colormap, int size, float t);
// Resamples a colormap. #size_out must be greater than 1.
IMPLOT_API void ResampleColormap(const ImVec4* colormap_in, int size_in, ImVec4* colormap_out, int size_out);

// Draws vertical text. The position is the bottom left of the text rect.
IMPLOT_API void AddTextVertical(ImDrawList *DrawList, ImVec2 pos, ImU32 col, const char* text_begin, const char* text_end = NULL);
// Calculates the size of vertical text
inline ImVec2 CalcTextSizeVertical(const char *text) { ImVec2 sz = ImGui::CalcTextSize(text); return ImVec2(sz.y, sz.x); }
// Returns white or black text given background color
inline ImU32 CalcTextColor(const ImVec4& bg) { return (bg.x * 0.299 + bg.y * 0.587 + bg.z * 0.114) > 0.5 ? IM_COL32_BLACK : IM_COL32_WHITE; }

// Clamps a label position so that it fits a rect defined by Min/Max
inline ImVec2 ClampLabelPos(ImVec2 pos, const ImVec2& size, const ImVec2& Min, const ImVec2& Max) {
    if (pos.x < Min.x)              pos.x = Min.x;
    if (pos.y < Min.y)              pos.y = Min.y;
    if ((pos.x + size.x) > Max.x)   pos.x = Max.x - size.x;
    if ((pos.y + size.y) > Max.y)   pos.y = Max.y - size.y;
    return pos;
}

//-----------------------------------------------------------------------------
// [SECTION] Math and Misc Utils
//-----------------------------------------------------------------------------

// Rounds x to powers of 2,5 and 10 for generating axis labels (from Graphics Gems 1 Chapter 11.2)
IMPLOT_API double NiceNum(double x, bool round);
// Computes order of magnitude of double.
inline int OrderOfMagnitude(double val) { return val == 0 ? 0 : (int)(floor(log10(fabs(val)))); }
// Returns the precision required for a order of magnitude.
inline int OrderToPrecision(int order) { return order > 0 ? 0 : 1 - order; }
// Returns a floating point precision to use given a value
inline int Precision(double val) { return OrderToPrecision(OrderOfMagnitude(val)); }

// Returns the intersection point of two lines A and B (assumes they are not parallel!)
inline ImVec2 Intersection(const ImVec2& a1, const ImVec2& a2, const ImVec2& b1, const ImVec2& b2) {
    float v1 = (a1.x * a2.y - a1.y * a2.x);  float v2 = (b1.x * b2.y - b1.y * b2.x);
    float v3 = ((a1.x - a2.x) * (b1.y - b2.y) - (a1.y - a2.y) * (b1.x - b2.x));
    return ImVec2((v1 * (b1.x - b2.x) - v2 * (a1.x - a2.x)) / v3, (v1 * (b1.y - b2.y) - v2 * (a1.y - a2.y)) / v3);
}

// Fills a buffer with n samples linear interpolated from vmin to vmax
template <typename T>
void FillRange(ImVector<T>& buffer, int n, T vmin, T vmax) {
    buffer.resize(n);
    T step = (vmax - vmin) / (n - 1);
    for (int i = 0; i < n; ++i) {
        buffer[i] = vmin + i * step;
    }
}

// Offsets and strides a data buffer
template <typename T>
inline T OffsetAndStride(const T* data, int idx, int count, int offset, int stride) {
    idx = ImPosMod(offset + idx, count);
    return *(const T*)(const void*)((const unsigned char*)data + (size_t)idx * stride);
}

//-----------------------------------------------------------------------------
// Time Utils
//-----------------------------------------------------------------------------

// Returns true if year is leap year (366 days long)
inline bool IsLeapYear(int year) {
    return  year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}
// Returns the number of days in a month, accounting for Feb. leap years. #month is zero indexed.
inline int GetDaysInMonth(int year, int month) {
    static const int days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return  days[month] + (int)(month == 1 && IsLeapYear(year));
}

// Make a UNIX timestamp from a tm struct expressed in UTC time (i.e. GMT timezone).
IMPLOT_API ImPlotTime MkGmtTime(struct tm *ptm);
// Make a tm struct expressed in UTC time (i.e. GMT timezone) from a UNIX timestamp.
IMPLOT_API tm* GetGmtTime(const ImPlotTime& t, tm* ptm);

// Make a UNIX timestamp from a tm struct expressed in local time.
IMPLOT_API ImPlotTime MkLocTime(struct tm *ptm);
// Make a tm struct expressed in local time from a UNIX timestamp.
IMPLOT_API tm* GetLocTime(const ImPlotTime& t, tm* ptm);

// NB: The following functions only work if there is a current ImPlotContext because the
// internal tm struct is owned by the context! They are aware of ImPlotStyle.UseLocalTime.

// Make a timestamp from time components.
// year[1970-3000], month[0-11], day[1-31], hour[0-23], min[0-59], sec[0-59], us[0,999999]
IMPLOT_API ImPlotTime MakeTime(int year, int month = 0, int day = 1, int hour = 0, int min = 0, int sec = 0, int us = 0);
// Get year component from timestamp [1970-3000]
IMPLOT_API int GetYear(const ImPlotTime& t);

// Adds or subtracts time from a timestamp. #count > 0 to add, < 0 to subtract.
IMPLOT_API ImPlotTime AddTime(const ImPlotTime& t, ImPlotTimeUnit unit, int count);
// Rounds a timestamp down to nearest unit.
IMPLOT_API ImPlotTime FloorTime(const ImPlotTime& t, ImPlotTimeUnit unit);
// Rounds a timestamp up to the nearest unit.
IMPLOT_API ImPlotTime CeilTime(const ImPlotTime& t, ImPlotTimeUnit unit);
// Rounds a timestamp up or down to the nearest unit.
IMPLOT_API ImPlotTime RoundTime(const ImPlotTime& t, ImPlotTimeUnit unit);
// Combines the date of one timestamp with the time-of-day of another timestamp.
IMPLOT_API ImPlotTime CombineDateTime(const ImPlotTime& date_part, const ImPlotTime& time_part);

// Formats the time part of timestamp t into a buffer according to #fmt
IMPLOT_API int FormatTime(const ImPlotTime& t, char* buffer, int size, ImPlotTimeFmt fmt, bool use_24_hr_clk);
// Formats the date part of timestamp t into a buffer according to #fmt
IMPLOT_API int FormatDate(const ImPlotTime& t, char* buffer, int size, ImPlotDateFmt fmt, bool use_iso_8601);
// Formats the time and/or date parts of a timestamp t into a buffer according to #fmt
IMPLOT_API int FormatDateTime(const ImPlotTime& t, char* buffer, int size, ImPlotDateTimeFmt fmt);

// Shows a date picker widget block (year/month/day).
// #level = 0 for day, 1 for month, 2 for year. Modified by user interaction.
// #t will be set when a day is clicked and the function will return true.
// #t1 and #t2 are optional dates to highlight.
IMPLOT_API bool ShowDatePicker(const char* id, int* level, ImPlotTime* t, const ImPlotTime* t1 = NULL, const ImPlotTime* t2 = NULL);
// Shows a time picker widget block (hour/min/sec).
// #t will be set when a new hour, minute, or sec is selected or am/pm is toggled, and the function will return true.
IMPLOT_API bool ShowTimePicker(const char* id, ImPlotTime* t);

//-----------------------------------------------------------------------------
// [SECTION] Internal / Experimental Plotters
// No guarantee of forward compatibility here!
//-----------------------------------------------------------------------------

// Plots axis-aligned, filled rectangles. Every two consecutive points defines opposite corners of a single rectangle.
IMPLOT_API void PlotRects(const char* label_id, const float* xs, const float* ys, int count, int offset = 0, int stride = sizeof(float));
IMPLOT_API void PlotRects(const char* label_id, const double* xs, const double* ys, int count, int offset = 0, int stride = sizeof(double));
IMPLOT_API void PlotRects(const char* label_id, ImPlotPoint (*getter)(void* data, int idx), void* data, int count, int offset = 0);

} // namespace ImPlot
