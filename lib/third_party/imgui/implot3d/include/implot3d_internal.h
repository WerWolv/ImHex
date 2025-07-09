//--------------------------------------------------
// ImPlot3D v0.3 WIP
// implot3d_internal.h
// Date: 2024-11-17
// Author: Breno Cunha Queiroz (brenocq.com)
//
// Acknowledgments:
//  ImPlot3D is heavily inspired by ImPlot
//  (https://github.com/epezent/implot) by Evan Pezent,
//  and follows a similar code style and structure to
//  maintain consistency with ImPlot's API.
//--------------------------------------------------

// Table of Contents:
// [SECTION] Constants
// [SECTION] Generic Helpers
// [SECTION] Forward Declarations
// [SECTION] Callbacks
// [SECTION] Structs
// [SECTION] Context Pointer
// [SECTION] Context Utils
// [SECTION] Style Utils
// [SECTION] Item Utils
// [SECTION] Plot Utils
// [SECTION] Setup Utils
// [SECTION] Formatter
// [SECTION] Locator

#pragma once

#ifndef IMPLOT3D_VERSION
#include "implot3d.h"
#endif

#ifndef IMGUI_DISABLE
#include "imgui_internal.h"

//-----------------------------------------------------------------------------
// [SECTION] Constants
//-----------------------------------------------------------------------------

// Default label format for axis labels
#define IMPLOT3D_LABEL_FORMAT "%g"
// Max character size for tick labels
#define IMPLOT3D_LABEL_MAX_SIZE 32

//-----------------------------------------------------------------------------
// [SECTION] Generic Helpers
//-----------------------------------------------------------------------------

namespace ImPlot3D {

// Computes the common (base-10) logarithm
static inline float ImLog10(float x) { return log10f(x); }
// Returns true if flag is set
template <typename TSet, typename TFlag> static inline bool ImHasFlag(TSet set, TFlag flag) { return (set & flag) == flag; }
// Flips a flag in a flagset
template <typename TSet, typename TFlag> static inline void ImFlipFlag(TSet& set, TFlag flag) { ImHasFlag(set, flag) ? set &= ~flag : set |= flag; }
template <typename T> static inline T ImRemap01(T x, T x0, T x1) { return (x1 - x0) ? ((x - x0) / (x1 - x0)) : 0; }
// Returns true if val is NAN
static inline bool ImNan(float val) { return isnan(val); }
// Returns true if val is NAN or INFINITY
static inline bool ImNanOrInf(float val) { return !(val >= -FLT_MAX && val <= FLT_MAX) || ImNan(val); }
// Turns NANs to 0s
static inline double ImConstrainNan(float val) { return ImNan(val) ? 0 : val; }
// Turns infinity to floating point maximums
static inline double ImConstrainInf(double val) { return val >= FLT_MAX ? FLT_MAX : val <= -FLT_MAX ? -FLT_MAX : val; }
// True if two numbers are approximately equal using units in the last place.
static inline bool ImAlmostEqual(double v1, double v2, int ulp = 2) {
    return ImAbs(v1 - v2) < FLT_EPSILON * ImAbs(v1 + v2) * ulp || ImAbs(v1 - v2) < FLT_MIN;
}
// Set alpha channel of 32-bit color from float in range [0.0 1.0]
static inline ImU32 ImAlphaU32(ImU32 col, float alpha) { return col & ~((ImU32)((1.0f - alpha) * 255) << IM_COL32_A_SHIFT); }
// Mix color a and b by factor s in [0 256]
static inline ImU32 ImMixU32(ImU32 a, ImU32 b, ImU32 s) {
#ifdef IMPLOT3D_MIX64
    const ImU32 af = 256 - s;
    const ImU32 bf = s;
    const ImU64 al = (a & 0x00ff00ff) | (((ImU64)(a & 0xff00ff00)) << 24);
    const ImU64 bl = (b & 0x00ff00ff) | (((ImU64)(b & 0xff00ff00)) << 24);
    const ImU64 mix = (al * af + bl * bf);
    return ((mix >> 32) & 0xff00ff00) | ((mix & 0xff00ff00) >> 8);
#else
    const ImU32 af = 256 - s;
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

// Fills a buffer with n samples linear interpolated from vmin to vmax
template <typename T> void FillRange(ImVector<T>& buffer, int n, T vmin, T vmax) {
    buffer.resize(n);
    T step = (vmax - vmin) / (n - 1);
    for (int i = 0; i < n; ++i) {
        buffer[i] = vmin + i * step;
    }
}

} // namespace ImPlot3D

//-----------------------------------------------------------------------------
// [SECTION] Forward Declarations
//-----------------------------------------------------------------------------

struct ImPlot3DTicker;

//------------------------------------------------------------------------------
// [SECTION] Callbacks
//------------------------------------------------------------------------------

typedef void (*ImPlot3DLocator)(ImPlot3DTicker& ticker, const ImPlot3DRange& range, float pixels, ImPlot3DFormatter formatter, void* formatter_data);

//-----------------------------------------------------------------------------
// [SECTION] Structs
//-----------------------------------------------------------------------------

struct ImDrawList3D {
    // [Internal] Define which texture should be used when rendering triangles.
    struct ImTextureBufferItem {
        ImTextureRef TexRef;
        unsigned int VtxIdx;
    };

    ImVector<ImDrawIdx> IdxBuffer;  // Index buffer
    ImVector<ImDrawVert> VtxBuffer; // Vertex buffer
    ImVector<float> ZBuffer;        // Z buffer. Depth value for each triangle
    unsigned int _VtxCurrentIdx;    // [Internal] current vertex index
    ImDrawVert* _VtxWritePtr; // [Internal] point within VtxBuffer.Data after each add command (to avoid using the ImVector<> operators too much)
    ImDrawIdx* _IdxWritePtr;  // [Internal] point within IdxBuffer.Data after each add command (to avoid using the ImVector<> operators too much)
    float* _ZWritePtr;        // [Internal] point within ZBuffer.Data after each add command (to avoid using the ImVector<> operators too much)
    ImDrawListFlags _Flags;   // [Internal] draw list flags
    ImVector<ImTextureBufferItem> _TextureBuffer; // [Internal] buffer for SetTexture/ResetTexture
    ImDrawListSharedData* _SharedData;            // [Internal] shared draw list data

    ImDrawList3D() {
        _Flags = ImDrawListFlags_None;
        _SharedData = nullptr;
        ResetBuffers();
    }

    void PrimReserve(int idx_count, int vtx_count);
    void PrimUnreserve(int idx_count, int vtx_count);

    void SetTexture(ImTextureRef tex_ref);
    void ResetTexture();

    void SortedMoveToImGuiDrawList();

    void ResetBuffers() {
        IdxBuffer.clear();
        VtxBuffer.clear();
        ZBuffer.clear();
        _VtxCurrentIdx = 0;
        _VtxWritePtr = VtxBuffer.Data;
        _IdxWritePtr = IdxBuffer.Data;
        _ZWritePtr = ZBuffer.Data;
        _TextureBuffer.clear();
        ResetTexture();
    }

    constexpr static unsigned int MaxIdx() { return sizeof(ImDrawIdx) == 2 ? 65535 : 4294967295; }
};

struct ImPlot3DNextItemData {
    ImVec4 Colors[4]; // ImPlot3DCol_Line, ImPlot3DCol_Fill, ImPlot3DCol_MarkerOutline, ImPlot3DCol_MarkerFill,
    float LineWeight;
    ImPlot3DMarker Marker;
    float MarkerSize;
    float MarkerWeight;
    float FillAlpha;
    bool RenderLine;
    bool RenderFill;
    bool RenderMarkerLine;
    bool RenderMarkerFill;
    bool IsAutoFill;
    bool IsAutoLine;
    bool Hidden;

    ImPlot3DNextItemData() { Reset(); }

    void Reset() {
        for (int i = 0; i < 4; i++)
            Colors[i] = IMPLOT3D_AUTO_COL;
        LineWeight = IMPLOT3D_AUTO;
        Marker = IMPLOT3D_AUTO;
        MarkerSize = IMPLOT3D_AUTO;
        MarkerWeight = IMPLOT3D_AUTO;
        FillAlpha = IMPLOT3D_AUTO;
        RenderLine = false;
        RenderFill = false;
        RenderMarkerLine = true;
        RenderMarkerFill = true;
        IsAutoFill = true;
        IsAutoLine = true;
        Hidden = false;
    }
};

// Colormap data storage
struct ImPlot3DColormapData {
    ImVector<ImU32> Keys;
    ImVector<int> KeyCounts;
    ImVector<int> KeyOffsets;
    ImVector<ImU32> Tables;
    ImVector<int> TableSizes;
    ImVector<int> TableOffsets;
    ImGuiTextBuffer Text;
    ImVector<int> TextOffsets;
    ImVector<bool> Quals;
    ImGuiStorage Map;
    int Count;

    ImPlot3DColormapData() { Count = 0; }

    int Append(const char* name, const ImU32* keys, int count, bool qual) {
        if (GetIndex(name) != -1)
            return -1;
        KeyOffsets.push_back(Keys.size());
        KeyCounts.push_back(count);
        Keys.reserve(Keys.size() + count);
        for (int i = 0; i < count; ++i)
            Keys.push_back(keys[i]);
        TextOffsets.push_back(Text.size());
        Text.append(name, name + strlen(name) + 1);
        Quals.push_back(qual);
        ImGuiID id = ImHashStr(name);
        int idx = Count++;
        Map.SetInt(id, idx);
        _AppendTable(idx);
        return idx;
    }

    void _AppendTable(ImPlot3DColormap cmap) {
        int key_count = GetKeyCount(cmap);
        const ImU32* keys = GetKeys(cmap);
        int off = Tables.size();
        TableOffsets.push_back(off);
        if (IsQual(cmap)) {
            Tables.reserve(key_count);
            for (int i = 0; i < key_count; ++i)
                Tables.push_back(keys[i]);
            TableSizes.push_back(key_count);
        } else {
            int max_size = 255 * (key_count - 1) + 1;
            Tables.reserve(off + max_size);
            // ImU32 last = keys[0];
            // Tables.push_back(last);
            // int n = 1;
            for (int i = 0; i < key_count - 1; ++i) {
                for (int s = 0; s < 255; ++s) {
                    ImU32 a = keys[i];
                    ImU32 b = keys[i + 1];
                    ImU32 c = ImPlot3D::ImMixU32(a, b, s);
                    // if (c != last) {
                    Tables.push_back(c);
                    // last = c;
                    // n++;
                    // }
                }
            }
            ImU32 c = keys[key_count - 1];
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

    inline bool IsQual(ImPlot3DColormap cmap) const { return Quals[cmap]; }
    inline const char* GetName(ImPlot3DColormap cmap) const { return cmap < Count ? Text.Buf.Data + TextOffsets[cmap] : nullptr; }
    inline ImPlot3DColormap GetIndex(const char* name) const {
        ImGuiID key = ImHashStr(name);
        return Map.GetInt(key, -1);
    }

    inline const ImU32* GetKeys(ImPlot3DColormap cmap) const { return &Keys[KeyOffsets[cmap]]; }
    inline int GetKeyCount(ImPlot3DColormap cmap) const { return KeyCounts[cmap]; }
    inline ImU32 GetKeyColor(ImPlot3DColormap cmap, int idx) const { return Keys[KeyOffsets[cmap] + idx]; }
    inline void SetKeyColor(ImPlot3DColormap cmap, int idx, ImU32 value) {
        Keys[KeyOffsets[cmap] + idx] = value;
        RebuildTables();
    }

    inline const ImU32* GetTable(ImPlot3DColormap cmap) const { return &Tables[TableOffsets[cmap]]; }
    inline int GetTableSize(ImPlot3DColormap cmap) const { return TableSizes[cmap]; }
    inline ImU32 GetTableColor(ImPlot3DColormap cmap, int idx) const { return Tables[TableOffsets[cmap] + idx]; }

    inline ImU32 LerpTable(ImPlot3DColormap cmap, float t) const {
        int off = TableOffsets[cmap];
        int siz = TableSizes[cmap];
        int idx = Quals[cmap] ? ImClamp((int)(siz * t), 0, siz - 1) : (int)((siz - 1) * t + 0.5f);
        return Tables[off + idx];
    }
};

// State information for plot items
struct ImPlot3DItem {
    ImGuiID ID;
    ImU32 Color;
    int NameOffset;
    bool Show;
    bool LegendHovered;
    bool SeenThisFrame;

    ImPlot3DItem() {
        ID = 0;
        Color = IM_COL32_WHITE;
        NameOffset = -1;
        Show = true;
        LegendHovered = false;
        SeenThisFrame = false;
    }
    ~ImPlot3DItem() { ID = 0; }
};

// Holds legend state
struct ImPlot3DLegend {
    ImPlot3DLegendFlags Flags;
    ImPlot3DLegendFlags PreviousFlags;
    ImPlot3DLocation Location;
    ImPlot3DLocation PreviousLocation;
    ImVector<int> Indices;
    ImGuiTextBuffer Labels;
    ImRect Rect;
    bool Hovered;
    bool Held;

    ImPlot3DLegend() {
        PreviousFlags = Flags = ImPlot3DLegendFlags_None;
        Hovered = Held = false;
        PreviousLocation = Location = ImPlot3DLocation_NorthWest;
    }

    void Reset() {
        Indices.shrink(0);
        Labels.Buf.shrink(0);
    }
};

// Holds items
struct ImPlot3DItemGroup {
    ImPool<ImPlot3DItem> ItemPool;
    ImPlot3DLegend Legend;
    int ColormapIdx;

    ImPlot3DItemGroup() { ColormapIdx = 0; }

    int GetItemCount() const { return ItemPool.GetBufSize(); }
    ImGuiID GetItemID(const char* label_id) { return ImGui::GetID(label_id); }
    ImPlot3DItem* GetItem(ImGuiID id) { return ItemPool.GetByKey(id); }
    ImPlot3DItem* GetItem(const char* label_id) { return GetItem(GetItemID(label_id)); }
    ImPlot3DItem* GetOrAddItem(ImGuiID id) { return ItemPool.GetOrAddByKey(id); }
    ImPlot3DItem* GetItemByIndex(int i) { return ItemPool.GetByIndex(i); }
    int GetItemIndex(ImPlot3DItem* item) { return ItemPool.GetIndex(item); }
    int GetLegendCount() const { return Legend.Indices.size(); }
    ImPlot3DItem* GetLegendItem(int i) { return ItemPool.GetByIndex(Legend.Indices[i]); }
    const char* GetLegendLabel(int i) { return Legend.Labels.Buf.Data + GetLegendItem(i)->NameOffset; }
    void Reset() {
        ItemPool.Clear();
        Legend.Reset();
        ColormapIdx = 0;
    }
};

// Tick mark info
struct ImPlot3DTick {
    float PlotPos;
    bool Major;
    bool ShowLabel;
    ImVec2 LabelSize;
    int TextOffset;
    int Idx;

    ImPlot3DTick(double value, bool major, bool show_label) {
        PlotPos = (float)value;
        Major = major;
        ShowLabel = show_label;
        TextOffset = -1;
    }
};

// Collection of ticks
struct ImPlot3DTicker {
    ImVector<ImPlot3DTick> Ticks;
    ImGuiTextBuffer TextBuffer;

    ImPlot3DTicker() { Reset(); }

    ImPlot3DTick& AddTick(double value, bool major, bool show_label, const char* label) {
        ImPlot3DTick tick(value, major, show_label);
        if (show_label && label != nullptr) {
            tick.TextOffset = TextBuffer.size();
            TextBuffer.append(label, label + strlen(label) + 1);
            tick.LabelSize = ImGui::CalcTextSize(TextBuffer.Buf.Data + tick.TextOffset);
        }
        return AddTick(tick);
    }

    ImPlot3DTick& AddTick(double value, bool major, bool show_label, ImPlot3DFormatter formatter, void* data) {
        ImPlot3DTick tick(value, major, show_label);
        if (show_label && formatter != nullptr) {
            char buff[IMPLOT3D_LABEL_MAX_SIZE];
            tick.TextOffset = TextBuffer.size();
            formatter(tick.PlotPos, buff, sizeof(buff), data);
            TextBuffer.append(buff, buff + strlen(buff) + 1);
            tick.LabelSize = ImGui::CalcTextSize(TextBuffer.Buf.Data + tick.TextOffset);
        }
        return AddTick(tick);
    }

    inline ImPlot3DTick& AddTick(ImPlot3DTick tick) {
        tick.Idx = Ticks.size();
        Ticks.push_back(tick);
        return Ticks.back();
    }

    const char* GetText(int idx) const { return TextBuffer.Buf.Data + Ticks[idx].TextOffset; }

    const char* GetText(const ImPlot3DTick& tick) const { return GetText(tick.Idx); }

    void Reset() {
        Ticks.shrink(0);
        TextBuffer.Buf.shrink(0);
    }

    int TickCount() const { return Ticks.Size; }
};

// Holds axis information
struct ImPlot3DAxis {
    ImPlot3DAxisFlags Flags;
    ImPlot3DAxisFlags PreviousFlags;
    ImPlot3DRange Range;
    ImPlot3DCond RangeCond;
    ImGuiTextBuffer Label;
    // Ticks
    ImPlot3DTicker Ticker;
    ImPlot3DFormatter Formatter;
    void* FormatterData;
    ImPlot3DLocator Locator;
    bool ShowDefaultTicks;
    // Fit data
    bool FitThisFrame;
    ImPlot3DRange FitExtents;
    // Constraints
    ImPlot3DRange ConstraintRange;
    ImPlot3DRange ConstraintZoom;
    // User input
    bool Hovered;
    bool Held;

    // Constructor
    ImPlot3DAxis() {
        PreviousFlags = Flags = ImPlot3DAxisFlags_None;
        // Range
        Range.Min = 0.0f;
        Range.Max = 1.0f;
        RangeCond = ImPlot3DCond_None;
        // Ticks
        Formatter = nullptr;
        FormatterData = nullptr;
        Locator = nullptr;
        ShowDefaultTicks = true;
        // Fit data
        FitThisFrame = true;
        FitExtents = ImPlot3DRange(HUGE_VAL, -HUGE_VAL);
        // Constraints
        ConstraintRange = ImPlot3DRange(-INFINITY, INFINITY);
        ConstraintZoom = ImPlot3DRange(FLT_MIN, INFINITY);
        // User input
        Hovered = false;
        Held = false;
    }

    inline void Reset() {
        RangeCond = ImPlot3DCond_None;
        // Ticks
        Ticker.Reset();
        Formatter = nullptr;
        FormatterData = nullptr;
        Locator = nullptr;
        ShowDefaultTicks = true;
        // Fit data
        FitExtents = ImPlot3DRange(HUGE_VAL, -HUGE_VAL);
        // Constraints
        ConstraintRange = ImPlot3DRange(-INFINITY, INFINITY);
        ConstraintZoom = ImPlot3DRange(FLT_MIN, INFINITY);
    }

    inline void SetRange(double v1, double v2) {
        Range.Min = (float)ImMin(v1, v2);
        Range.Max = (float)ImMax(v1, v2);
        Constrain();
    }

    inline bool SetMin(double _min, bool force = false) {
        if (!force && IsLockedMin())
            return false;
        _min = ImPlot3D::ImConstrainNan((float)ImPlot3D::ImConstrainInf(_min));

        // Constraints
        if (_min < ConstraintRange.Min)
            _min = ConstraintRange.Min;
        double zoom = Range.Max - _min;
        if (zoom < ConstraintZoom.Min)
            _min = Range.Max - ConstraintZoom.Min;
        if (zoom > ConstraintZoom.Max)
            _min = Range.Max - ConstraintZoom.Max;

        // Ensure min is less than max
        if (_min >= Range.Max)
            return false;

        Range.Min = (float)_min;
        return true;
    }

    inline bool SetMax(double _max, bool force = false) {
        if (!force && IsLockedMax())
            return false;
        _max = ImPlot3D::ImConstrainNan((float)ImPlot3D::ImConstrainInf(_max));

        // Constraints
        if (_max > ConstraintRange.Max)
            _max = ConstraintRange.Max;
        double zoom = _max - Range.Min;
        if (zoom < ConstraintZoom.Min)
            _max = Range.Min + ConstraintZoom.Min;
        if (zoom > ConstraintZoom.Max)
            _max = Range.Min + ConstraintZoom.Max;

        // Ensure max is greater than min
        if (_max <= Range.Min)
            return false;
        Range.Max = (float)_max;
        return true;
    }

    inline void Constrain() {
        Range.Min = (float)ImPlot3D::ImConstrainNan((float)ImPlot3D::ImConstrainInf((double)Range.Min));
        Range.Max = (float)ImPlot3D::ImConstrainNan((float)ImPlot3D::ImConstrainInf((double)Range.Max));
        if (Range.Min < ConstraintRange.Min)
            Range.Min = ConstraintRange.Min;
        if (Range.Max > ConstraintRange.Max)
            Range.Max = ConstraintRange.Max;
        float zoom = Range.Size();
        if (zoom < ConstraintZoom.Min) {
            float delta = (ConstraintZoom.Min - zoom) * 0.5f;
            Range.Min -= delta;
            Range.Max += delta;
        }
        if (zoom > ConstraintZoom.Max) {
            float delta = (zoom - ConstraintZoom.Max) * 0.5f;
            Range.Min += delta;
            Range.Max -= delta;
        }
        if (Range.Max <= Range.Min)
            Range.Max = Range.Min + FLT_EPSILON;
    }

    inline bool IsRangeLocked() const { return RangeCond == ImPlot3DCond_Always; }
    inline bool IsLockedMin() const { return IsRangeLocked() || ImPlot3D::ImHasFlag(Flags, ImPlot3DAxisFlags_LockMin); }
    inline bool IsLockedMax() const { return IsRangeLocked() || ImPlot3D::ImHasFlag(Flags, ImPlot3DAxisFlags_LockMax); }
    inline bool IsLocked() const { return IsLockedMin() && IsLockedMax(); }
    inline bool IsInputLockedMin() const { return IsLockedMin() || IsAutoFitting(); }
    inline bool IsInputLockedMax() const { return IsLockedMax() || IsAutoFitting(); }
    inline bool IsInputLocked() const { return IsLocked() || IsAutoFitting(); }

    inline bool IsPanLocked(bool increasing) {
        if (ImPlot3D::ImHasFlag(Flags, ImPlot3DAxisFlags_PanStretch)) {
            return IsInputLocked();
        } else {
            if (IsLockedMin() || IsLockedMax() || IsAutoFitting())
                return false;
            if (increasing)
                return Range.Max == ConstraintRange.Max;
            else
                return Range.Min == ConstraintRange.Min;
        }
    }

    inline void SetLabel(const char* label) {
        Label.Buf.shrink(0);
        if (label && ImGui::FindRenderedTextEnd(label, nullptr) != label)
            Label.append(label, label + strlen(label) + 1);
    }

    inline const char* GetLabel() const { return Label.Buf.Data; }

    bool HasLabel() const;
    bool HasGridLines() const;
    bool HasTickLabels() const;
    bool HasTickMarks() const;
    bool IsAutoFitting() const;
    void ExtendFit(float value);
    void ApplyFit();
};

// Holds plot state information that must persist after EndPlot
struct ImPlot3DPlot {
    ImGuiID ID;
    ImPlot3DFlags Flags;
    ImPlot3DFlags PreviousFlags;
    ImGuiTextBuffer Title;
    bool JustCreated;
    bool Initialized;
    // Bounding rectangles
    ImRect FrameRect;  // Outermost bounding rectangle that encapsulates whole the plot/title/padding/etc
    ImRect CanvasRect; // Frame rectangle reduced by padding
    ImRect PlotRect;   // Bounding rectangle for the actual plot area
    // Rotation & axes & box
    ImPlot3DQuat InitialRotation; // Initial rotation quaternion
    ImPlot3DQuat Rotation;        // Current rotation quaternion
    ImPlot3DCond RotationCond;
    ImPlot3DAxis Axes[3];   // X, Y, Z axes
    ImPlot3DPoint BoxScale; // Scale factor for plot box X, Y, Z axes
    // Animation
    float AnimationTime;               // Remaining animation time
    ImPlot3DQuat RotationAnimationEnd; // End rotation for animation
    // User input
    bool SetupLocked;
    bool Hovered;
    bool Held;
    int HeldEdgeIdx;  // Index of the edge being held
    int HeldPlaneIdx; // Index of the plane being held
    // Fit data
    bool FitThisFrame;
    // Items
    ImPlot3DItemGroup Items;
    // 3D draw list
    ImDrawList3D DrawList;
    // Misc
    bool ContextClick; // True if context button was clicked (to distinguish from double click)
    bool OpenContextThisFrame;

    ImPlot3DPlot() {
        PreviousFlags = Flags = ImPlot3DFlags_None;
        JustCreated = true;
        Initialized = false;
        InitialRotation = ImPlot3DQuat(-0.513269f, -0.212596f, -0.318184f, 0.76819f);
        Rotation = ImPlot3DQuat(0.0f, 0.0f, 0.0f, 1.0f);
        RotationCond = ImPlot3DCond_None;
        for (int i = 0; i < 3; i++)
            Axes[i] = ImPlot3DAxis();
        BoxScale = ImPlot3DPoint(1.0f, 1.0f, 1.0f);
        AnimationTime = 0.0f;
        RotationAnimationEnd = Rotation;
        SetupLocked = false;
        Hovered = Held = false;
        HeldEdgeIdx = -1;
        HeldPlaneIdx = -1;
        FitThisFrame = true;
        ContextClick = false;
        OpenContextThisFrame = false;
    }

    inline void SetTitle(const char* title) {
        Title.Buf.shrink(0);
        if (title && ImGui::FindRenderedTextEnd(title, nullptr) != title)
            Title.append(title, title + strlen(title) + 1);
    }
    inline bool HasTitle() const { return !Title.empty() && !ImPlot3D::ImHasFlag(Flags, ImPlot3DFlags_NoTitle); }
    inline const char* GetTitle() const { return Title.Buf.Data; }
    inline bool IsRotationLocked() const { return RotationCond == ImPlot3DCond_Always; }

    void ExtendFit(const ImPlot3DPoint& point);
    ImPlot3DPoint RangeMin() const;
    ImPlot3DPoint RangeMax() const;
    ImPlot3DPoint RangeCenter() const;
    void SetRange(const ImPlot3DPoint& min, const ImPlot3DPoint& max);
    float GetBoxZoom() const;
};

struct ImPlot3DContext {
    ImPool<ImPlot3DPlot> Plots;
    ImPlot3DPlot* CurrentPlot;
    ImPlot3DItemGroup* CurrentItems;
    ImPlot3DItem* CurrentItem;
    ImPlot3DNextItemData NextItemData;
    ImPlot3DStyle Style;
    ImVector<ImGuiColorMod> ColorModifiers;
    ImVector<ImGuiStyleMod> StyleModifiers;
    ImVector<ImPlot3DColormap> ColormapModifiers;
    ImPlot3DColormapData ColormapData;
};

//-----------------------------------------------------------------------------
// [SECTION] Context Pointer
//-----------------------------------------------------------------------------

namespace ImPlot3D {

#ifndef GImPlot3D
extern IMPLOT3D_API ImPlot3DContext* GImPlot3D; // Current context pointer
#endif

//-----------------------------------------------------------------------------
// [SECTION] Context Utils
//-----------------------------------------------------------------------------

IMPLOT3D_API void InitializeContext(ImPlot3DContext* ctx); // Initialize ImPlot3DContext
IMPLOT3D_API void ResetContext(ImPlot3DContext* ctx);      // Reset ImPlot3DContext

//-----------------------------------------------------------------------------
// [SECTION] Style Utils
//-----------------------------------------------------------------------------

IMPLOT3D_API bool IsColorAuto(const ImVec4& col);
IMPLOT3D_API bool IsColorAuto(ImPlot3DCol idx);
IMPLOT3D_API ImVec4 GetAutoColor(ImPlot3DCol idx);
IMPLOT3D_API const char* GetStyleColorName(ImPlot3DCol idx);

// Returns white or black text given background color
static inline ImU32 CalcTextColor(const ImVec4& bg) {
    return (bg.x * 0.299f + bg.y * 0.587f + bg.z * 0.114f) > 0.5f ? IM_COL32_BLACK : IM_COL32_WHITE;
}
static inline ImU32 CalcTextColor(ImU32 bg) { return CalcTextColor(ImGui::ColorConvertU32ToFloat4(bg)); }

// Get styling data for next item (call between BeginItem/EndItem)
IMPLOT3D_API const ImPlot3DNextItemData& GetItemData();

// Returns a color from the Color map given an index >= 0 (modulo will be performed)
IMPLOT3D_API ImU32 GetColormapColorU32(int idx, ImPlot3DColormap cmap);

// Returns the next unused colormap color and advances the colormap. Can be used to skip colors if desired
IMPLOT3D_API ImU32 NextColormapColorU32();

// Render a colormap bar
IMPLOT3D_API void RenderColorBar(const ImU32* colors, int size, ImDrawList& DrawList, const ImRect& bounds, bool vert, bool reversed,
                                 bool continuous);

//-----------------------------------------------------------------------------
// [SECTION] Item Utils
//-----------------------------------------------------------------------------

IMPLOT3D_API bool BeginItem(const char* label_id, ImPlot3DItemFlags flags = 0, ImPlot3DCol recolor_from = IMPLOT3D_AUTO);
IMPLOT3D_API void EndItem();

// Register or get an existing item from the current plot
IMPLOT3D_API ImPlot3DItem* RegisterOrGetItem(const char* label_id, ImPlot3DItemFlags flags, bool* just_created = nullptr);

// Gets the current item from ImPlot3DContext
IMPLOT3D_API ImPlot3DItem* GetCurrentItem();

// Busts the cache for every item for every plot in the current context
IMPLOT3D_API void BustItemCache();

// TODO move to another place
IMPLOT3D_API void AddTextRotated(ImDrawList* draw_list, ImVec2 pos, float angle, ImU32 col, const char* text_begin, const char* text_end = nullptr);

//-----------------------------------------------------------------------------
// [SECTION] Plot Utils
//-----------------------------------------------------------------------------

// Gets the current plot from ImPlot3DContext
IMPLOT3D_API ImPlot3DPlot* GetCurrentPlot();

// Busts the cache for every plot in the current context
IMPLOT3D_API void BustPlotCache();

IMPLOT3D_API ImVec2 GetFramePos();  // Get the current frame position (top-left) in pixels
IMPLOT3D_API ImVec2 GetFrameSize(); // Get the current frame size in pixels

// Convert a position in the current plot's coordinate system to the current plot's normalized device coordinate system (NDC)
// When the cube aspect ratio is [1,1,1], the NDC varies from [-0.5, 0.5] in each axis
IMPLOT3D_API ImPlot3DPoint PlotToNDC(const ImPlot3DPoint& point);
IMPLOT3D_API ImPlot3DPoint NDCToPlot(const ImPlot3DPoint& point);
// Convert a position in the current plot's NDC to pixels
IMPLOT3D_API ImVec2 NDCToPixels(const ImPlot3DPoint& point);
// Convert a pixel coordinate to a ray in the NDC
IMPLOT3D_API ImPlot3DRay PixelsToNDCRay(const ImVec2& pix);
// Convert a ray in the NDC to a ray in the current plot's coordinate system
IMPLOT3D_API ImPlot3DRay NDCRayToPlotRay(const ImPlot3DRay& ray);

//-----------------------------------------------------------------------------
// [SECTION] Setup Utils
//-----------------------------------------------------------------------------

IMPLOT3D_API void SetupLock();

//-----------------------------------------------------------------------------
// [SECTION] Formatter
//-----------------------------------------------------------------------------

int Formatter_Default(float value, char* buff, int size, void* data);

//------------------------------------------------------------------------------
// [SECTION] Locator
//------------------------------------------------------------------------------

void Locator_Default(ImPlot3DTicker& ticker, const ImPlot3DRange& range, float pixels, ImPlot3DFormatter formatter, void* formatter_data);

} // namespace ImPlot3D

#endif // #ifndef IMGUI_DISABLE
