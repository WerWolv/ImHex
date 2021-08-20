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

// Support for pre-1.84 versions. ImPool's GetSize() -> GetBufSize()
#if (IMGUI_VERSION_NUM < 18303)
#define GetBufSize GetSize
#endif

//-----------------------------------------------------------------------------
// [SECTION] Constants
//-----------------------------------------------------------------------------

// Constants can be changed unless stated otherwise. We may move some of these
// to ImPlotStyleVar_ over time.

// The maximum number of supported y-axes (DO NOT CHANGE THIS)
#define IMPLOT_Y_AXES    3
// Zoom rate for scroll (e.g. 0.1f = 10% plot range every scroll click)
#define IMPLOT_ZOOM_RATE 0.1f
// Mimimum allowable timestamp value 01/01/1970 @ 12:00am (UTC) (DO NOT DECREASE THIS)
#define IMPLOT_MIN_TIME  0
// Maximum allowable timestamp value 01/01/3000 @ 12:00am (UTC) (DO NOT INCREASE THIS)
#define IMPLOT_MAX_TIME  32503680000
// Default label format for axis labels
#define IMPLOT_LABEL_FMT "%g"
// Plot values less than or equal to 0 will be replaced with this on log scale axes
#define IMPLOT_LOG_ZERO DBL_MIN

//-----------------------------------------------------------------------------
// [SECTION] Macros
//-----------------------------------------------------------------------------

// Split ImU32 color into RGB components [0 255]
#define IM_COL32_SPLIT_RGB(col,r,g,b) \
    ImU32 r = ((col >> IM_COL32_R_SHIFT) & 0xFF); \
    ImU32 g = ((col >> IM_COL32_G_SHIFT) & 0xFF); \
    ImU32 b = ((col >> IM_COL32_B_SHIFT) & 0xFF);

//-----------------------------------------------------------------------------
// [SECTION] Forward Declarations
//-----------------------------------------------------------------------------

struct ImPlotTick;
struct ImPlotAxis;
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
// [SECTION] Generic Helpers
//-----------------------------------------------------------------------------

// Computes the common (base-10) logarithm
static inline float  ImLog10(float x)  { return log10f(x); }
static inline double ImLog10(double x) { return log10(x);  }
// Returns true if a flag is set
template <typename TSet, typename TFlag>
static inline bool ImHasFlag(TSet set, TFlag flag) { return (set & flag) == flag; }
// Flips a flag in a flagset
template <typename TSet, typename TFlag>
static inline void ImFlipFlag(TSet& set, TFlag flag) { ImHasFlag(set, flag) ? set &= ~flag : set |= flag; }
// Linearly remaps x from [x0 x1] to [y0 y1].
template <typename T>
static inline T ImRemap(T x, T x0, T x1, T y0, T y1) { return y0 + (x - x0) * (y1 - y0) / (x1 - x0); }
// Linear rempas x from [x0 x1] to [0 1]
template <typename T>
static inline T ImRemap01(T x, T x0, T x1) { return (x - x0) / (x1 - x0); }
// Returns always positive modulo (assumes r != 0)
static inline int ImPosMod(int l, int r) { return (l % r + r) % r; }
// Returns true if val is NAN or INFINITY
static inline bool ImNanOrInf(double val) { return val == HUGE_VAL || val == -HUGE_VAL || isnan(val); }
// Turns NANs to 0s
static inline double ImConstrainNan(double val) { return isnan(val) ? 0 : val; }
// Turns infinity to floating point maximums
static inline double ImConstrainInf(double val) { return val == HUGE_VAL ?  DBL_MAX : val == -HUGE_VAL ? - DBL_MAX : val; }
// Turns numbers less than or equal to 0 to 0.001 (sort of arbitrary, is there a better way?)
static inline double ImConstrainLog(double val) { return val <= 0 ? 0.001f : val; }
// Turns numbers less than 0 to zero
static inline double ImConstrainTime(double val) { return val < IMPLOT_MIN_TIME ? IMPLOT_MIN_TIME : (val > IMPLOT_MAX_TIME ? IMPLOT_MAX_TIME : val); }
// True if two numbers are approximately equal using units in the last place.
static inline bool ImAlmostEqual(double v1, double v2, int ulp = 2) { return ImAbs(v1-v2) < DBL_EPSILON * ImAbs(v1+v2) * ulp || ImAbs(v1-v2) < DBL_MIN; }
// Finds min value in an unsorted array
template <typename T>
static inline T ImMinArray(const T* values, int count) { T m = values[0]; for (int i = 1; i < count; ++i) { if (values[i] < m) { m = values[i]; } } return m; }
// Finds the max value in an unsorted array
template <typename T>
static inline T ImMaxArray(const T* values, int count) { T m = values[0]; for (int i = 1; i < count; ++i) { if (values[i] > m) { m = values[i]; } } return m; }
// Finds the min and max value in an unsorted array
template <typename T>
static inline void ImMinMaxArray(const T* values, int count, T* min_out, T* max_out) {
    T Min = values[0]; T Max = values[0];
    for (int i = 1; i < count; ++i) {
        if (values[i] < Min) { Min = values[i]; }
        if (values[i] > Max) { Max = values[i]; }
    }
    *min_out = Min; *max_out = Max;
}
// Finds the sim of an array
template <typename T>
static inline T ImSum(const T* values, int count) {
    T sum  = 0;
    for (int i = 0; i < count; ++i)
        sum += values[i];
    return sum;
}
// Finds the mean of an array
template <typename T>
static inline double ImMean(const T* values, int count) {
    double den = 1.0 / count;
    double mu  = 0;
    for (int i = 0; i < count; ++i)
        mu += values[i] * den;
    return mu;
}
// Finds the sample standard deviation of an array
template <typename T>
static inline double ImStdDev(const T* values, int count) {
    double den = 1.0 / (count - 1.0);
    double mu  = ImMean(values, count);
    double x   = 0;
    for (int i = 0; i < count; ++i)
        x += (values[i] - mu) * (values[i] - mu) * den;
    return sqrt(x);
}
// Mix color a and b by factor s in [0 256]
static inline ImU32 ImMixU32(ImU32 a, ImU32 b, ImU32 s) {
#ifdef IMPLOT_MIX64
    const ImU32 af = 256-s;
    const ImU32 bf = s;
    const ImU64 al = (a & 0x00ff00ff) | (((ImU64)(a & 0xff00ff00)) << 24);
    const ImU64 bl = (b & 0x00ff00ff) | (((ImU64)(b & 0xff00ff00)) << 24);
    const ImU64 mix = (al * af + bl * bf);
    return ((mix >> 32) & 0xff00ff00) | ((mix & 0xff00ff00) >> 8);
#else
    const ImU32 af = 256-s;
    const ImU32 bf = s;
    const ImU32 al = (a & 0x00ff00ff);
    const ImU32 ah = (a & 0xff00ff00) >> 8;
    const ImU32 bl = (b & 0x00ff00ff);
    const ImU32 bh = (b & 0xff00ff00) >> 8;
    const ImU32 ml = (al * af + bl * bf);
    const ImU32 mh = (ah * af + bh * bf);
    return (mh & 0xff00ff00) | ((ml & 0xff00ff00) >> 8);
#endif
}

// Lerp across an array of 32-bit collors given t in [0.0 1.0]
static inline ImU32 ImLerpU32(const ImU32* colors, int size, float t) {
    int i1 = (int)((size - 1 ) * t);
    int i2 = i1 + 1;
    if (i2 == size || size == 1)
        return colors[i1];
    float den = 1.0f / (size - 1);
    float t1 = i1 * den;
    float t2 = i2 * den;
    float tr = ImRemap01(t, t1, t2);
    return ImMixU32(colors[i1], colors[i2], (ImU32)(tr*256));
}

// Set alpha channel of 32-bit color from float in range [0.0 1.0]
static inline ImU32 ImAlphaU32(ImU32 col, float alpha) {
    return col & ~((ImU32)((1.0f-alpha)*255)<<IM_COL32_A_SHIFT);
}

// Character buffer writer helper (FIXME: Can't we replace this with ImGuiTextBuffer?)
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
        va_list args;
        va_start(args, fmt);
        WriteV(fmt, args);
        va_end(args);
    }

    void WriteV(const char* fmt, va_list args) {
        const int written = ::vsnprintf(&Buffer[Pos], Size - Pos - 1, fmt, args);
        if (written > 0)
          Pos += ImMin(written, Size-Pos-1);
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

// Input mapping structure, default values listed in the comments.
struct ImPlotInputMap {
    ImGuiMouseButton PanButton;             // LMB      enables panning when held
    ImGuiKeyModFlags PanMod;                // none     optional modifier that must be held for panning
    ImGuiMouseButton FitButton;             // LMB      fits visible data when double clicked
    ImGuiMouseButton ContextMenuButton;     // RMB      opens plot context menu (if enabled) when clicked
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
// [SECTION] ImPlot Structs
//-----------------------------------------------------------------------------

// Combined date/time format spec
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

// Colormap data storage
struct ImPlotColormapData {
    ImVector<ImU32> Keys;
    ImVector<int>   KeyCounts;
    ImVector<int>   KeyOffsets;
    ImVector<ImU32> Tables;
    ImVector<int>   TableSizes;
    ImVector<int>   TableOffsets;
    ImGuiTextBuffer Text;
    ImVector<int>   TextOffsets;
    ImVector<bool>  Quals;
    ImGuiStorage    Map;
    int             Count;

    ImPlotColormapData() { Count = 0; }

    int Append(const char* name, const ImU32* keys, int count, bool qual) {
        if (GetIndex(name) != -1)
            return -1;
        KeyOffsets.push_back(Keys.size());
        KeyCounts.push_back(count);
        Keys.reserve(Keys.size()+count);
        for (int i = 0; i < count; ++i)
            Keys.push_back(keys[i]);
        TextOffsets.push_back(Text.size());
        Text.append(name, name + strlen(name) + 1);
        Quals.push_back(qual);
        ImGuiID id = ImHashStr(name);
        int idx = Count++;
        Map.SetInt(id,idx);
        _AppendTable(idx);
        return idx;
    }

    void _AppendTable(ImPlotColormap cmap) {
        int key_count     = GetKeyCount(cmap);
        const ImU32* keys = GetKeys(cmap);
        int off = Tables.size();
        TableOffsets.push_back(off);
        if (IsQual(cmap)) {
            Tables.reserve(key_count);
            for (int i = 0; i < key_count; ++i)
                Tables.push_back(keys[i]);
            TableSizes.push_back(key_count);
        }
        else {
            int max_size = 255 * (key_count-1) + 1;
            Tables.reserve(off + max_size);
            // ImU32 last = keys[0];
            // Tables.push_back(last);
            // int n = 1;
            for (int i = 0; i < key_count-1; ++i) {
                for (int s = 0; s < 255; ++s) {
                    ImU32 a = keys[i];
                    ImU32 b = keys[i+1];
                    ImU32 c = ImMixU32(a,b,s);
                    // if (c != last) {
                        Tables.push_back(c);
                        // last = c;
                        // n++;
                    // }
                }
            }
            ImU32 c = keys[key_count-1];
            // if (c != last) {
                Tables.push_back(c);
                // n++;
            // }
            // TableSizes.push_back(n);
            TableSizes.push_back(max_size);
        }
    }

    void RebuildTables() {
        Tables.resize(0);
        TableSizes.resize(0);
        TableOffsets.resize(0);
        for (int i = 0; i < Count; ++i)
            _AppendTable(i);
    }

    inline bool           IsQual(ImPlotColormap cmap) const                      { return Quals[cmap];                                             }
    inline const char*    GetName(ImPlotColormap cmap) const                     { return cmap < Count ? Text.Buf.Data + TextOffsets[cmap] : NULL; }
    inline ImPlotColormap GetIndex(const char* name) const                       { ImGuiID key = ImHashStr(name); return Map.GetInt(key,-1);       }

    inline const ImU32*   GetKeys(ImPlotColormap cmap) const                     { return &Keys[KeyOffsets[cmap]];                                 }
    inline int            GetKeyCount(ImPlotColormap cmap) const                 { return KeyCounts[cmap];                                         }
    inline ImU32          GetKeyColor(ImPlotColormap cmap, int idx) const        { return Keys[KeyOffsets[cmap]+idx];                              }
    inline void           SetKeyColor(ImPlotColormap cmap, int idx, ImU32 value) { Keys[KeyOffsets[cmap]+idx] = value; RebuildTables();            }

    inline const ImU32*   GetTable(ImPlotColormap cmap) const                    { return &Tables[TableOffsets[cmap]];                             }
    inline int            GetTableSize(ImPlotColormap cmap) const                { return TableSizes[cmap];                                        }
    inline ImU32          GetTableColor(ImPlotColormap cmap, int idx) const      { return Tables[TableOffsets[cmap]+idx];                          }

    inline ImU32 LerpTable(ImPlotColormap cmap, float t) const {
        int off = TableOffsets[cmap];
        int siz = TableSizes[cmap];
        int idx = Quals[cmap] ? ImClamp((int)(siz*t),0,siz-1) : (int)((siz - 1) * t + 0.5f);
        return Tables[off + idx];
    }

};

// ImPlotPoint with positive/negative error values
struct ImPlotPointError {
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
    float                TotalWidthMax;
    float                TotalWidth;
    float                TotalHeight;
    float                MaxWidth;
    float                MaxHeight;
    int                  Size;

    ImPlotTickCollection() { Reset(); }

    const ImPlotTick& Append(const ImPlotTick& tick) {
        if (tick.ShowLabel) {
            TotalWidth    += tick.ShowLabel ? tick.LabelSize.x : 0;
            TotalHeight   += tick.ShowLabel ? tick.LabelSize.y : 0;
            MaxWidth      =  tick.LabelSize.x > MaxWidth  ? tick.LabelSize.x : MaxWidth;
            MaxHeight     =  tick.LabelSize.y > MaxHeight ? tick.LabelSize.y : MaxHeight;
        }
        Ticks.push_back(tick);
        Size++;
        return Ticks.back();
    }

    const ImPlotTick& Append(double value, bool major, bool show_label, const char* fmt) {
        ImPlotTick tick(value, major, show_label);
        if (show_label && fmt != NULL) {
            char temp[32];
            tick.TextOffset = TextBuffer.size();
            snprintf(temp, 32, fmt, tick.PlotPos);
            TextBuffer.append(temp, temp + strlen(temp) + 1);
            tick.LabelSize = ImGui::CalcTextSize(TextBuffer.Buf.Data + tick.TextOffset);
        }
        return Append(tick);
    }

    const char* GetText(int idx) const {
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
    ImPlotAxisFlags   Flags;
    ImPlotAxisFlags   PreviousFlags;
    ImPlotRange       Range;
    float             Pixels;
    ImPlotOrientation Orientation;
    bool              Dragging;
    bool              ExtHovered;
    bool              AllHovered;
    bool              Present;
    bool              HasRange;
    double*           LinkedMin;
    double*           LinkedMax;
    ImPlotTime        PickerTimeMin, PickerTimeMax;
    int               PickerLevel;
    ImU32             ColorMaj, ColorMin, ColorTxt;
    ImGuiCond         RangeCond;
    ImRect            HoverRect;

    ImPlotAxis() {
        Flags       = PreviousFlags = ImPlotAxisFlags_None;
        Range.Min   = 0;
        Range.Max   = 1;
        Dragging    = false;
        ExtHovered  = false;
        AllHovered  = false;
        LinkedMin   = LinkedMax = NULL;
        PickerLevel = 0;
        ColorMaj    = ColorMin = ColorTxt = 0;
    }

    bool SetMin(double _min, bool force=false) {
        if (!force && IsLockedMin())
            return false;
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

    bool SetMax(double _max, bool force=false) {
        if (!force && IsLockedMax())
            return false;
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

    void SetAspect(double unit_per_pix) {
        double new_size = unit_per_pix * Pixels;
        double delta    = (new_size - Range.Size()) * 0.5f;
        if (IsLocked())
            return;
        else if (IsLockedMin() && !IsLockedMax())
            SetRange(Range.Min, Range.Max  + 2*delta);
        else if (!IsLockedMin() && IsLockedMax())
            SetRange(Range.Min - 2*delta, Range.Max);
        else
            SetRange(Range.Min - delta, Range.Max + delta);
    }

    double GetAspect() const { return Range.Size() / Pixels; }

    void Constrain() {
        Range.Min = ImConstrainNan(ImConstrainInf(Range.Min));
        Range.Max = ImConstrainNan(ImConstrainInf(Range.Max));
        if (IsLog()) {
            Range.Min = ImConstrainLog(Range.Min);
            Range.Max = ImConstrainLog(Range.Max);
        }
        if (IsTime()) {
            Range.Min = ImConstrainTime(Range.Min);
            Range.Max = ImConstrainTime(Range.Max);
        }
        if (Range.Max <= Range.Min)
            Range.Max = Range.Min + DBL_EPSILON;
    }

    inline bool IsLabeled()         const { return !ImHasFlag(Flags, ImPlotAxisFlags_NoTickLabels);                          }
    inline bool IsInverted()        const { return ImHasFlag(Flags, ImPlotAxisFlags_Invert);                                 }

    inline bool IsAutoFitting()     const { return ImHasFlag(Flags, ImPlotAxisFlags_AutoFit);                                }
    inline bool IsRangeLocked()     const { return HasRange && RangeCond == ImGuiCond_Always;                                }

    inline bool IsLockedMin()       const { return !Present || IsRangeLocked() || ImHasFlag(Flags, ImPlotAxisFlags_LockMin); }
    inline bool IsLockedMax()       const { return !Present || IsRangeLocked() || ImHasFlag(Flags, ImPlotAxisFlags_LockMax); }
    inline bool IsLocked()          const { return IsLockedMin() && IsLockedMax();                                           }

    inline bool IsInputLockedMin()  const { return IsLockedMin() || IsAutoFitting();                                         }
    inline bool IsInputLockedMax()  const { return IsLockedMax() || IsAutoFitting();                                         }
    inline bool IsInputLocked()     const { return IsLocked()    || IsAutoFitting();                                         }

    inline bool IsTime()            const { return ImHasFlag(Flags, ImPlotAxisFlags_Time);                                   }
    inline bool IsLog()             const { return ImHasFlag(Flags, ImPlotAxisFlags_LogScale);                               }
};

// Align plots group data
struct ImPlotAlignmentData {
    ImPlotOrientation Orientation;
    float PadA;
    float PadB;
    float PadAMax;
    float PadBMax;
    ImPlotAlignmentData() {
        Orientation = ImPlotOrientation_Vertical;
        PadA = PadB = PadAMax = PadBMax = 0;
    }
    void Begin() { PadAMax = PadBMax = 0; }
    void Update(float& pad_a, float& pad_b) {
        if (PadAMax < pad_a) PadAMax = pad_a;
        if (pad_a < PadA)    pad_a   = PadA;
        if (PadBMax < pad_b) PadBMax = pad_b;
        if (pad_b < PadB)    pad_b   = PadB;
    }
    void End()   { PadA = PadAMax; PadB = PadBMax;      }
    void Reset() { PadA = PadB = PadAMax = PadBMax = 0; }
};

// State information for Plot items
struct ImPlotItem
{
    ImGuiID      ID;
    ImU32        Color;
    int          NameOffset;
    bool         Show;
    bool         LegendHovered;
    bool         SeenThisFrame;

    ImPlotItem() {
        ID            = 0;
        NameOffset    = -1;
        Show          = true;
        SeenThisFrame = false;
        LegendHovered = false;
    }

    ~ImPlotItem() { ID = 0; }
};

// Holds Legend state
struct ImPlotLegendData
{
    ImVector<int>     Indices;
    ImGuiTextBuffer   Labels;
    bool              Hovered;
    bool              Outside;
    bool              CanGoInside;
    bool              FlipSideNextFrame;
    ImPlotLocation    Location;
    ImPlotOrientation Orientation;
    ImRect            Rect;

    ImPlotLegendData() {
        CanGoInside = true;
        Hovered      = Outside = FlipSideNextFrame = false;
        Location     = ImPlotLocation_North | ImPlotLocation_West;
        Orientation  = ImPlotOrientation_Vertical;
    }

    void Reset() { Indices.shrink(0); Labels.Buf.shrink(0); }
};

// Holds Items and Legend data
struct ImPlotItemGroup
{
    ImGuiID            ID;
    ImPlotLegendData   Legend;
    ImPool<ImPlotItem> ItemPool;
    int                ColormapIdx;

    ImPlotItemGroup() { ColormapIdx = 0; }

    int         GetItemCount() const             { return ItemPool.GetBufSize();                                 }
    ImGuiID     GetItemID(const char*  label_id) { return ImGui::GetID(label_id); /* GetIDWithSeed */            }
    ImPlotItem* GetItem(ImGuiID id)              { return ItemPool.GetByKey(id);                                 }
    ImPlotItem* GetItem(const char* label_id)    { return GetItem(GetItemID(label_id));                          }
    ImPlotItem* GetOrAddItem(ImGuiID id)         { return ItemPool.GetOrAddByKey(id);                            }
    ImPlotItem* GetItemByIndex(int i)            { return ItemPool.GetByIndex(i);                                }
    int         GetItemIndex(ImPlotItem* item)   { return ItemPool.GetIndex(item);                               }
    int         GetLegendCount() const           { return Legend.Indices.size();                                 }
    ImPlotItem* GetLegendItem(int i)             { return ItemPool.GetByIndex(Legend.Indices[i]);                }
    const char* GetLegendLabel(int i)            { return Legend.Labels.Buf.Data + GetLegendItem(i)->NameOffset; }
    void        Reset()                          { ItemPool.Clear(); Legend.Reset(); ColormapIdx = 0;            }
};

// Holds Plot state information that must persist after EndPlot
struct ImPlotPlot
{
    ImGuiID         ID;
    ImPlotFlags     Flags;
    ImPlotFlags     PreviousFlags;
    ImPlotAxis      XAxis;
    ImPlotAxis      YAxis[IMPLOT_Y_AXES];
    ImPlotItemGroup Items;
    ImVec2          SelectStart;
    ImRect          SelectRect;
    ImVec2          QueryStart;
    ImRect          QueryRect;
    bool            Initialized;
    bool            Selecting;
    bool            Selected;
    bool            ContextLocked;
    bool            Querying;
    bool            Queried;
    bool            DraggingQuery;
    bool            FrameHovered;
    bool            FrameHeld;
    bool            PlotHovered;
    int             CurrentYAxis;
    ImPlotLocation  MousePosLocation;
    ImRect          FrameRect;
    ImRect          CanvasRect;
    ImRect          PlotRect;
    ImRect          AxesRect;

    ImPlotPlot() {
        Flags             = PreviousFlags = ImPlotFlags_None;
        XAxis.Orientation = ImPlotOrientation_Horizontal;
        for (int i = 0; i < IMPLOT_Y_AXES; ++i)
            YAxis[i].Orientation = ImPlotOrientation_Vertical;
        SelectStart       = QueryStart = ImVec2(0,0);
        Initialized       = Selecting = Selected = ContextLocked = Querying = Queried = DraggingQuery = false;
        CurrentYAxis       = 0;
        MousePosLocation  = ImPlotLocation_South | ImPlotLocation_East;
    }

    inline bool AnyYInputLocked() const { return YAxis[0].IsInputLocked() || (YAxis[1].Present ? YAxis[1].IsInputLocked() : false) || (YAxis[2].Present ? YAxis[2].IsInputLocked() : false); }
    inline bool AllYInputLocked() const { return YAxis[0].IsInputLocked() && (YAxis[1].Present ? YAxis[1].IsInputLocked() : true ) && (YAxis[2].Present ? YAxis[2].IsInputLocked() : true ); }
    inline bool IsInputLocked() const   { return XAxis.IsInputLocked() && YAxis[0].IsInputLocked() && YAxis[1].IsInputLocked() && YAxis[2].IsInputLocked();                                  }
};

// Holds subplot data that must persist afer EndSubplot
struct ImPlotSubplot {
    ImGuiID                       ID;
    ImPlotSubplotFlags            Flags;
    ImPlotSubplotFlags            PreviousFlags;
    ImPlotItemGroup               Items;
    int                           Rows;
    int                           Cols;
    int                           CurrentIdx;
    ImRect                        FrameRect;
    ImRect                        GridRect;
    ImVec2                        CellSize;
    ImVector<ImPlotAlignmentData> RowAlignmentData;
    ImVector<ImPlotAlignmentData> ColAlignmentData;
    ImVector<float>               RowRatios;
    ImVector<float>               ColRatios;
    ImVector<ImPlotRange>         RowLinkData;
    ImVector<ImPlotRange>         ColLinkData;
    float                         TempSizes[2];
    bool                          FrameHovered;

    ImPlotSubplot() {
        Rows = Cols = CurrentIdx  = 0;
        FrameHovered              = false;
        Items.Legend.Location     = ImPlotLocation_North;
        Items.Legend.Orientation  = ImPlotOrientation_Horizontal;
        Items.Legend.CanGoInside  = false;
    }
};

// Temporary data storage for upcoming plot
struct ImPlotNextPlotData
{
    ImGuiCond   XRangeCond;
    ImGuiCond   YRangeCond[IMPLOT_Y_AXES];
    ImPlotRange XRange;
    ImPlotRange YRange[IMPLOT_Y_AXES];
    bool        HasXRange;
    bool        HasYRange[IMPLOT_Y_AXES];
    bool        ShowDefaultTicksX;
    bool        ShowDefaultTicksY[IMPLOT_Y_AXES];
    char        FmtX[16];
    char        FmtY[IMPLOT_Y_AXES][16];
    bool        HasFmtX;
    bool        HasFmtY[IMPLOT_Y_AXES];
    bool        FitX;
    bool        FitY[IMPLOT_Y_AXES];
    double*     LinkedXmin;
    double*     LinkedXmax;
    double*     LinkedYmin[IMPLOT_Y_AXES];
    double*     LinkedYmax[IMPLOT_Y_AXES];

    ImPlotNextPlotData() { Reset(); }

    void Reset() {
        HasXRange         = false;
        ShowDefaultTicksX = true;
        HasFmtX           = false;
        FitX              = false;
        LinkedXmin = LinkedXmax = NULL;
        for (int i = 0; i < IMPLOT_Y_AXES; ++i) {
            HasYRange[i]         = false;
            ShowDefaultTicksY[i] = true;
            HasFmtY[i]           = false;
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
    ImPlotNextItemData() { Reset(); }
    void Reset() {
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
    ImPool<ImPlotPlot>    Plots;
    ImPool<ImPlotSubplot> Subplots;
    ImPlotPlot*           CurrentPlot;
    ImPlotSubplot*        CurrentSubplot;
    ImPlotItemGroup*      CurrentItems;
    ImPlotItem*           CurrentItem;
    ImPlotItem*           PreviousItem;

    // Tick Marks and Labels
    ImPlotTickCollection CTicks;
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

    // Axis Rendering Flags
    bool RenderX;
    bool RenderY[IMPLOT_Y_AXES];


    // Axis Locking Flags
    bool ChildWindowMade;

    // Style and Colormaps
    ImPlotStyle                 Style;
    ImVector<ImGuiColorMod>     ColorModifiers;
    ImVector<ImGuiStyleMod>     StyleModifiers;
    ImPlotColormapData          ColormapData;
    ImVector<ImPlotColormap>    ColormapModifiers;

    // Time
    tm Tm;

    // Temp data for general use
    ImVector<double>   Temp1, Temp2;

    // Misc
    int                DigitalPlotItemCnt;
    int                DigitalPlotOffset;
    ImPlotNextPlotData NextPlotData;
    ImPlotNextItemData NextItemData;
    ImPlotInputMap     InputMap;
    ImPlotPoint        MousePos[IMPLOT_Y_AXES];

    // Align plots
    ImPool<ImPlotAlignmentData> AlignmentData;
    ImPlotAlignmentData*        CurrentAlignmentH;
    ImPlotAlignmentData*        CurrentAlignmentV;
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
IMPLOT_API void ResetCtxForNextPlot(ImPlotContext* ctx);
// Restes an ImPlot context for the next call to BeginAlignedPlots
IMPLOT_API void ResetCtxForNextAlignedPlots(ImPlotContext* ctx);
// Resets an ImPlot context for the next call to BeginSubplot
IMPLOT_API void ResetCtxForNextSubplot(ImPlotContext* ctx);

//-----------------------------------------------------------------------------
// [SECTION] Input Utils
//-----------------------------------------------------------------------------

// Allows changing how keyboard/mouse interaction works.
IMPLOT_API ImPlotInputMap& GetInputMap();

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
// [SECTION] Subplot Utils
//-----------------------------------------------------------------------------

// Advances to next subplot
IMPLOT_API void SubplotNextCell();

// Shows a subplot's context menu.
IMPLOT_API void ShowSubplotsContextMenu(ImPlotSubplot& subplot);

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
static inline int GetCurrentYAxis() { return GImPlot->CurrentPlot->CurrentYAxis; }
// Updates axis ticks, lins, and label colors
IMPLOT_API void UpdateAxisColors(int axis_flag, ImPlotAxis* axis);

// Updates plot-to-pixel space transformation variables for the current plot.
IMPLOT_API void UpdateTransformCache();
// Gets the XY scale for the current plot and y-axis
static inline ImPlotScale GetCurrentScale() { return GImPlot->Scales[GetCurrentYAxis()]; }

// Returns true if the user has requested data to be fit.
static inline bool FitThisFrame() { return GImPlot->FitThisFrame; }
// Extend the the extents of an axis on current plot so that it encompes v
static inline void FitPointAxis(ImPlotAxis& axis, ImPlotRange& ext, double v) {
    if (!ImNanOrInf(v) && !(ImHasFlag(axis.Flags, ImPlotAxisFlags_LogScale) && v <= 0)) {
        ext.Min = v < ext.Min ? v : ext.Min;
        ext.Max = v > ext.Max ? v : ext.Max;
    }
}
// Extend the the extents of an axis on current plot so that it encompes v
static inline void FitPointMultiAxis(ImPlotAxis& axis, ImPlotAxis& alt, ImPlotRange& ext, double v, double v_alt) {
    if (ImHasFlag(axis.Flags, ImPlotAxisFlags_RangeFit) && !alt.Range.Contains(v_alt))
        return;
    if (!ImNanOrInf(v) && !(ImHasFlag(axis.Flags, ImPlotAxisFlags_LogScale) && v <= 0)) {
        ext.Min = v < ext.Min ? v : ext.Min;
        ext.Max = v > ext.Max ? v : ext.Max;
    }
}
// Extends the current plot's axes so that it encompasses a vertical line at x
static inline void FitPointX(double x) {
    FitPointAxis(GImPlot->CurrentPlot->XAxis, GImPlot->ExtentsX, x);
}
// Extends the current plot's axes so that it encompasses a horizontal line at y
static inline void FitPointY(double y) {
    const ImPlotYAxis y_axis  = GImPlot->CurrentPlot->CurrentYAxis;
    FitPointAxis(GImPlot->CurrentPlot->YAxis[y_axis], GImPlot->ExtentsY[y_axis], y);
}
// Extends the current plot's axes so that it encompasses point p
static inline void FitPoint(const ImPlotPoint& p) {
    const ImPlotYAxis y_axis  = GImPlot->CurrentPlot->CurrentYAxis;
    FitPointMultiAxis(GImPlot->CurrentPlot->XAxis, GImPlot->CurrentPlot->YAxis[y_axis], GImPlot->ExtentsX, p.x, p.y);
    FitPointMultiAxis(GImPlot->CurrentPlot->YAxis[y_axis], GImPlot->CurrentPlot->XAxis, GImPlot->ExtentsY[y_axis], p.y, p.x);
}

// Returns true if two ranges overlap
static inline bool RangesOverlap(const ImPlotRange& r1, const ImPlotRange& r2)
{ return r1.Min <= r2.Max && r2.Min <= r1.Max; }

// Updates pointers for linked axes from axis internal range.
IMPLOT_API void PushLinkedAxis(ImPlotAxis& axis);
// Updates axis internal range from points for linked axes.
IMPLOT_API void PullLinkedAxis(ImPlotAxis& axis);

// Shows an axis's context menu.
IMPLOT_API void ShowAxisContextMenu(ImPlotAxis& axis, ImPlotAxis* equal_axis, bool time_allowed = false);

// Get format spec for axis
static inline const char* GetFormatX()              { return GImPlot->NextPlotData.HasFmtX    ? GImPlot->NextPlotData.FmtX    : IMPLOT_LABEL_FMT; }
static inline const char* GetFormatY(ImPlotYAxis y) { return GImPlot->NextPlotData.HasFmtY[y] ? GImPlot->NextPlotData.FmtY[y] : IMPLOT_LABEL_FMT; }

//-----------------------------------------------------------------------------
// [SECTION] Legend Utils
//-----------------------------------------------------------------------------

// Gets the position of an inner rect that is located inside of an outer rect according to an ImPlotLocation and padding amount.
IMPLOT_API ImVec2 GetLocationPos(const ImRect& outer_rect, const ImVec2& inner_size, ImPlotLocation location, const ImVec2& pad = ImVec2(0,0));
// Calculates the bounding box size of a legend
IMPLOT_API ImVec2 CalcLegendSize(ImPlotItemGroup& items, const ImVec2& pad, const ImVec2& spacing, ImPlotOrientation orientation);
// Renders legend entries into a bounding box
IMPLOT_API bool ShowLegendEntries(ImPlotItemGroup& items, const ImRect& legend_bb, bool interactable, const ImVec2& pad, const ImVec2& spacing, ImPlotOrientation orientation, ImDrawList& DrawList);
// Shows an alternate legend for the plot identified by #title_id, outside of the plot frame (can be called before or after of Begin/EndPlot but must occur in the same ImGui window!).
IMPLOT_API void ShowAltLegend(const char* title_id, ImPlotOrientation orientation = ImPlotOrientation_Vertical, const ImVec2 size = ImVec2(0,0), bool interactable = true);
// Shows an legends's context menu.
IMPLOT_API bool ShowLegendContextMenu(ImPlotLegendData& legend, bool visible);

//-----------------------------------------------------------------------------
// [SECTION] Tick Utils
//-----------------------------------------------------------------------------

// Label a tick with time formatting.
IMPLOT_API void LabelTickTime(ImPlotTick& tick, ImGuiTextBuffer& buffer, const ImPlotTime& t, ImPlotDateTimeFmt fmt);

// Populates a list of ImPlotTicks with normal spaced and formatted ticks
IMPLOT_API void AddTicksDefault(const ImPlotRange& range, float pix, ImPlotOrientation orn, ImPlotTickCollection& ticks, const char* fmt);
// Populates a list of ImPlotTicks with logarithmic space and formatted ticks
IMPLOT_API void AddTicksLogarithmic(const ImPlotRange& range, float pix, ImPlotOrientation orn, ImPlotTickCollection& ticks, const char* fmt);
// Populates a list of ImPlotTicks with time formatted ticks.
IMPLOT_API void AddTicksTime(const ImPlotRange& range, float plot_width, ImPlotTickCollection& ticks);
// Populates a list of ImPlotTicks with custom spaced and labeled ticks
IMPLOT_API void AddTicksCustom(const double* values, const char* const labels[], int n, ImPlotTickCollection& ticks, const char* fmt);

// Create a a string label for a an axis value
IMPLOT_API int LabelAxisValue(const ImPlotAxis& axis, const ImPlotTickCollection& ticks, double value, char* buff, int size);

//-----------------------------------------------------------------------------
// [SECTION] Styling Utils
//-----------------------------------------------------------------------------

// Get styling data for next item (call between Begin/EndItem)
static inline const ImPlotNextItemData& GetItemData() { return GImPlot->NextItemData; }

// Returns true if a color is set to be automatically determined
static inline bool IsColorAuto(const ImVec4& col) { return col.w == -1; }
// Returns true if a style color is set to be automaticaly determined
static inline bool IsColorAuto(ImPlotCol idx) { return IsColorAuto(GImPlot->Style.Colors[idx]); }
// Returns the automatically deduced style color
IMPLOT_API ImVec4 GetAutoColor(ImPlotCol idx);

// Returns the style color whether it is automatic or custom set
static inline ImVec4 GetStyleColorVec4(ImPlotCol idx) { return IsColorAuto(idx) ? GetAutoColor(idx) : GImPlot->Style.Colors[idx]; }
static inline ImU32  GetStyleColorU32(ImPlotCol idx)  { return ImGui::ColorConvertFloat4ToU32(GetStyleColorVec4(idx)); }

// Draws vertical text. The position is the bottom left of the text rect.
IMPLOT_API void AddTextVertical(ImDrawList *DrawList, ImVec2 pos, ImU32 col, const char* text_begin, const char* text_end = NULL);
// Draws multiline horizontal text centered.
IMPLOT_API void AddTextCentered(ImDrawList* DrawList, ImVec2 top_center, ImU32 col, const char* text_begin, const char* text_end = NULL);
// Calculates the size of vertical text
static inline ImVec2 CalcTextSizeVertical(const char *text) {
    ImVec2 sz = ImGui::CalcTextSize(text);
    return ImVec2(sz.y, sz.x);
}
// Returns white or black text given background color
static inline ImU32 CalcTextColor(const ImVec4& bg) { return (bg.x * 0.299 + bg.y * 0.587 + bg.z * 0.114) > 0.5 ? IM_COL32_BLACK : IM_COL32_WHITE; }
static inline ImU32 CalcTextColor(ImU32 bg)         { return CalcTextColor(ImGui::ColorConvertU32ToFloat4(bg)); }
// Lightens or darkens a color for hover
static inline ImU32 CalcHoverColor(ImU32 col)       {  return ImMixU32(col, CalcTextColor(col), 32); }

// Clamps a label position so that it fits a rect defined by Min/Max
static inline ImVec2 ClampLabelPos(ImVec2 pos, const ImVec2& size, const ImVec2& Min, const ImVec2& Max) {
    if (pos.x < Min.x)              pos.x = Min.x;
    if (pos.y < Min.y)              pos.y = Min.y;
    if ((pos.x + size.x) > Max.x)   pos.x = Max.x - size.x;
    if ((pos.y + size.y) > Max.y)   pos.y = Max.y - size.y;
    return pos;
}

// Returns a color from the Color map given an index >= 0 (modulo will be performed).
IMPLOT_API ImU32  GetColormapColorU32(int idx, ImPlotColormap cmap);
// Returns the next unused colormap color and advances the colormap. Can be used to skip colors if desired.
IMPLOT_API ImU32  NextColormapColorU32();
// Linearly interpolates a color from the current colormap given t between 0 and 1.
IMPLOT_API ImU32  SampleColormapU32(float t, ImPlotColormap cmap);

// Render a colormap bar
IMPLOT_API void RenderColorBar(const ImU32* colors, int size, ImDrawList& DrawList, const ImRect& bounds, bool vert, bool reversed, bool continuous);

//-----------------------------------------------------------------------------
// [SECTION] Math and Misc Utils
//-----------------------------------------------------------------------------

// Rounds x to powers of 2,5 and 10 for generating axis labels (from Graphics Gems 1 Chapter 11.2)
IMPLOT_API double NiceNum(double x, bool round);
// Computes order of magnitude of double.
static inline int OrderOfMagnitude(double val) { return val == 0 ? 0 : (int)(floor(log10(fabs(val)))); }
// Returns the precision required for a order of magnitude.
static inline int OrderToPrecision(int order) { return order > 0 ? 0 : 1 - order; }
// Returns a floating point precision to use given a value
static inline int Precision(double val) { return OrderToPrecision(OrderOfMagnitude(val)); }
// Round a value to a given precision
static inline double RoundTo(double val, int prec) { double p = pow(10,(double)prec); return floor(val*p+0.5)/p; }

// Returns the intersection point of two lines A and B (assumes they are not parallel!)
static inline ImVec2 Intersection(const ImVec2& a1, const ImVec2& a2, const ImVec2& b1, const ImVec2& b2) {
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
static inline T OffsetAndStride(const T* data, int idx, int , int , int stride) {
    // idx = ImPosMod(offset + idx, count);
    return *(const T*)(const void*)((const unsigned char*)data + (size_t)idx * stride);
}

// Calculate histogram bin counts and widths
template <typename T>
static inline void CalculateBins(const T* values, int count, ImPlotBin meth, const ImPlotRange& range, int& bins_out, double& width_out) {
    switch (meth) {
        case ImPlotBin_Sqrt:
            bins_out  = (int)ceil(sqrt(count));
            break;
        case ImPlotBin_Sturges:
            bins_out  = (int)ceil(1.0 + log2(count));
            break;
        case ImPlotBin_Rice:
            bins_out  = (int)ceil(2 * cbrt(count));
            break;
        case ImPlotBin_Scott:
            width_out = 3.49 * ImStdDev(values, count) / cbrt(count);
            bins_out  = (int)round(range.Size() / width_out);
            break;
    }
    width_out = range.Size() / bins_out;
}

//-----------------------------------------------------------------------------
// Time Utils
//-----------------------------------------------------------------------------

// Returns true if year is leap year (366 days long)
static inline bool IsLeapYear(int year) {
    return  year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}
// Returns the number of days in a month, accounting for Feb. leap years. #month is zero indexed.
static inline int GetDaysInMonth(int year, int month) {
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

} // namespace ImPlot
