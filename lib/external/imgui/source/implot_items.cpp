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

// ImPlot v0.13 WIP

#include "implot.h"
#include "implot_internal.h"

#define SQRT_1_2 0.70710678118f
#define SQRT_3_2 0.86602540378f

#ifndef IMPLOT_NO_FORCE_INLINE
    #ifdef _MSC_VER
        #define IMPLOT_INLINE __forceinline
    #elif defined(__GNUC__)
        #define IMPLOT_INLINE inline __attribute__((__always_inline__))
    #elif defined(__CLANG__)
        #if __has_attribute(__always_inline__)
            #define IMPLOT_INLINE inline __attribute__((__always_inline__))
        #else
            #define IMPLOT_INLINE inline
        #endif
    #else
        #define IMPLOT_INLINE inline
    #endif
#else
    #define IMPLOT_INLINE inline
#endif

#if defined __SSE__ || defined __x86_64__ || defined _M_X64
#ifndef IMGUI_ENABLE_SSE
#include <immintrin.h>
#endif
static IMPLOT_INLINE float  ImInvSqrt(float x) { return _mm_cvtss_f32(_mm_rsqrt_ss(_mm_set_ss(x))); }
#else
static IMPLOT_INLINE float  ImInvSqrt(float x) { return 1.0f / sqrtf(x); }
#endif

#define IMPLOT_NORMALIZE2F_OVER_ZERO(VX,VY) do { float d2 = VX*VX + VY*VY; if (d2 > 0.0f) { float inv_len = ImInvSqrt(d2); VX *= inv_len; VY *= inv_len; } } while (0)

// Support for pre-1.82 versions. Users on 1.82+ can use 0 (default) flags to mean "all corners" but in order to support older versions we are more explicit.
#if (IMGUI_VERSION_NUM < 18102) && !defined(ImDrawFlags_RoundCornersAll)
#define ImDrawFlags_RoundCornersAll ImDrawCornerFlags_All
#endif

namespace ImPlot {

//-----------------------------------------------------------------------------
// Utils
//-----------------------------------------------------------------------------

// Calc maximum index size of ImDrawIdx
template <typename T>
struct MaxIdx { static const unsigned int Value; };
template <> const unsigned int MaxIdx<unsigned short>::Value = 65535;
template <> const unsigned int MaxIdx<unsigned int>::Value   = 4294967295;

//-----------------------------------------------------------------------------
// Item Utils
//-----------------------------------------------------------------------------

ImPlotItem* RegisterOrGetItem(const char* label_id, bool* just_created) {
    ImPlotContext& gp = *GImPlot;
    ImPlotItemGroup& Items = *gp.CurrentItems;
    ImGuiID id = Items.GetItemID(label_id);
    if (just_created != NULL)
        *just_created = Items.GetItem(id) == NULL;
    ImPlotItem* item = Items.GetOrAddItem(id);
    if (item->SeenThisFrame)
        return item;
    item->SeenThisFrame = true;
    int idx = Items.GetItemIndex(item);
    item->ID = id;
    if (ImGui::FindRenderedTextEnd(label_id, NULL) != label_id) {
        Items.Legend.Indices.push_back(idx);
        item->NameOffset = Items.Legend.Labels.size();
        Items.Legend.Labels.append(label_id, label_id + strlen(label_id) + 1);
    }
    else {
        item->Show = true;
    }
    return item;
}

ImPlotItem* GetItem(const char* label_id) {
    ImPlotContext& gp = *GImPlot;
    return gp.CurrentItems->GetItem(label_id);
}

bool IsItemHidden(const char* label_id) {
    ImPlotItem* item = GetItem(label_id);
    return item != NULL && !item->Show;
}

ImPlotItem* GetCurrentItem() {
    ImPlotContext& gp = *GImPlot;
    return gp.CurrentItem;
}

void SetNextLineStyle(const ImVec4& col, float weight) {
    ImPlotContext& gp = *GImPlot;
    gp.NextItemData.Colors[ImPlotCol_Line] = col;
    gp.NextItemData.LineWeight             = weight;
}

void SetNextFillStyle(const ImVec4& col, float alpha) {
    ImPlotContext& gp = *GImPlot;
    gp.NextItemData.Colors[ImPlotCol_Fill] = col;
    gp.NextItemData.FillAlpha              = alpha;
}

void SetNextMarkerStyle(ImPlotMarker marker, float size, const ImVec4& fill, float weight, const ImVec4& outline) {
    ImPlotContext& gp = *GImPlot;
    gp.NextItemData.Marker                          = marker;
    gp.NextItemData.Colors[ImPlotCol_MarkerFill]    = fill;
    gp.NextItemData.MarkerSize                      = size;
    gp.NextItemData.Colors[ImPlotCol_MarkerOutline] = outline;
    gp.NextItemData.MarkerWeight                    = weight;
}

void SetNextErrorBarStyle(const ImVec4& col, float size, float weight) {
    ImPlotContext& gp = *GImPlot;
    gp.NextItemData.Colors[ImPlotCol_ErrorBar] = col;
    gp.NextItemData.ErrorBarSize               = size;
    gp.NextItemData.ErrorBarWeight             = weight;
}

ImVec4 GetLastItemColor() {
    ImPlotContext& gp = *GImPlot;
    if (gp.PreviousItem)
        return ImGui::ColorConvertU32ToFloat4(gp.PreviousItem->Color);
    return ImVec4();
}

void BustItemCache() {
    ImPlotContext& gp = *GImPlot;
    for (int p = 0; p < gp.Plots.GetBufSize(); ++p) {
        ImPlotPlot& plot = *gp.Plots.GetByIndex(p);
        plot.Items.Reset();
    }
    for (int p = 0; p < gp.Subplots.GetBufSize(); ++p) {
        ImPlotSubplot& subplot = *gp.Subplots.GetByIndex(p);
        subplot.Items.Reset();
    }
}

void BustColorCache(const char* plot_title_id) {
    ImPlotContext& gp = *GImPlot;
    if (plot_title_id == NULL) {
        BustItemCache();
    }
    else {
        ImGuiID id = ImGui::GetCurrentWindow()->GetID(plot_title_id);
        ImPlotPlot* plot = gp.Plots.GetByKey(id);
        if (plot != NULL)
            plot->Items.Reset();
        else {
            ImPlotSubplot* subplot = gp.Subplots.GetByKey(id);
            if (subplot != NULL)
                subplot->Items.Reset();
        }
    }
}

//-----------------------------------------------------------------------------
// Begin/EndItem
//-----------------------------------------------------------------------------

static const float ITEM_HIGHLIGHT_LINE_SCALE = 2.0f;
static const float ITEM_HIGHLIGHT_MARK_SCALE = 1.25f;


// Begins a new item. Returns false if the item should not be plotted.
bool BeginItem(const char* label_id, ImPlotCol recolor_from) {
    ImPlotContext& gp = *GImPlot;
    IM_ASSERT_USER_ERROR(gp.CurrentPlot != NULL, "PlotX() needs to be called between BeginPlot() and EndPlot()!");
    SetupLock();
    bool just_created;
    ImPlotItem* item = RegisterOrGetItem(label_id, &just_created);
    // set current item
    gp.CurrentItem = item;
    ImPlotNextItemData& s = gp.NextItemData;
    // set/override item color
    if (recolor_from != -1) {
        if (!IsColorAuto(s.Colors[recolor_from]))
            item->Color = ImGui::ColorConvertFloat4ToU32(s.Colors[recolor_from]);
        else if (!IsColorAuto(gp.Style.Colors[recolor_from]))
            item->Color = ImGui::ColorConvertFloat4ToU32(gp.Style.Colors[recolor_from]);
        else if (just_created)
            item->Color = NextColormapColorU32();
    }
    else if (just_created) {
        item->Color = NextColormapColorU32();
    }
    // hide/show item
    if (gp.NextItemData.HasHidden) {
        if (just_created || gp.NextItemData.HiddenCond == ImGuiCond_Always)
            item->Show = !gp.NextItemData.Hidden;
    }
    if (!item->Show) {
        // reset next item data
        gp.NextItemData.Reset();
        gp.PreviousItem = item;
        gp.CurrentItem  = NULL;
        return false;
    }
    else {
        ImVec4 item_color = ImGui::ColorConvertU32ToFloat4(item->Color);
        // stage next item colors
        s.Colors[ImPlotCol_Line]           = IsColorAuto(s.Colors[ImPlotCol_Line])          ? ( IsColorAuto(ImPlotCol_Line)           ? item_color                 : gp.Style.Colors[ImPlotCol_Line]          ) : s.Colors[ImPlotCol_Line];
        s.Colors[ImPlotCol_Fill]           = IsColorAuto(s.Colors[ImPlotCol_Fill])          ? ( IsColorAuto(ImPlotCol_Fill)           ? item_color                 : gp.Style.Colors[ImPlotCol_Fill]          ) : s.Colors[ImPlotCol_Fill];
        s.Colors[ImPlotCol_MarkerOutline]  = IsColorAuto(s.Colors[ImPlotCol_MarkerOutline]) ? ( IsColorAuto(ImPlotCol_MarkerOutline)  ? s.Colors[ImPlotCol_Line]   : gp.Style.Colors[ImPlotCol_MarkerOutline] ) : s.Colors[ImPlotCol_MarkerOutline];
        s.Colors[ImPlotCol_MarkerFill]     = IsColorAuto(s.Colors[ImPlotCol_MarkerFill])    ? ( IsColorAuto(ImPlotCol_MarkerFill)     ? s.Colors[ImPlotCol_Line]   : gp.Style.Colors[ImPlotCol_MarkerFill]    ) : s.Colors[ImPlotCol_MarkerFill];
        s.Colors[ImPlotCol_ErrorBar]       = IsColorAuto(s.Colors[ImPlotCol_ErrorBar])      ? ( GetStyleColorVec4(ImPlotCol_ErrorBar)                                                                         ) : s.Colors[ImPlotCol_ErrorBar];
        // stage next item style vars
        s.LineWeight         = s.LineWeight       < 0 ? gp.Style.LineWeight       : s.LineWeight;
        s.Marker             = s.Marker           < 0 ? gp.Style.Marker           : s.Marker;
        s.MarkerSize         = s.MarkerSize       < 0 ? gp.Style.MarkerSize       : s.MarkerSize;
        s.MarkerWeight       = s.MarkerWeight     < 0 ? gp.Style.MarkerWeight     : s.MarkerWeight;
        s.FillAlpha          = s.FillAlpha        < 0 ? gp.Style.FillAlpha        : s.FillAlpha;
        s.ErrorBarSize       = s.ErrorBarSize     < 0 ? gp.Style.ErrorBarSize     : s.ErrorBarSize;
        s.ErrorBarWeight     = s.ErrorBarWeight   < 0 ? gp.Style.ErrorBarWeight   : s.ErrorBarWeight;
        s.DigitalBitHeight   = s.DigitalBitHeight < 0 ? gp.Style.DigitalBitHeight : s.DigitalBitHeight;
        s.DigitalBitGap      = s.DigitalBitGap    < 0 ? gp.Style.DigitalBitGap    : s.DigitalBitGap;
        // apply alpha modifier(s)
        s.Colors[ImPlotCol_Fill].w       *= s.FillAlpha;
        s.Colors[ImPlotCol_MarkerFill].w *= s.FillAlpha; // TODO: this should be separate, if it at all
        // apply highlight mods
        if (item->LegendHovered) {
            if (!ImHasFlag(gp.CurrentItems->Legend.Flags, ImPlotLegendFlags_NoHighlightItem)) {
                s.LineWeight   *= ITEM_HIGHLIGHT_LINE_SCALE;
                s.MarkerSize   *= ITEM_HIGHLIGHT_MARK_SCALE;
                s.MarkerWeight *= ITEM_HIGHLIGHT_LINE_SCALE;
                // TODO: how to highlight fills?
            }
            if (!ImHasFlag(gp.CurrentItems->Legend.Flags, ImPlotLegendFlags_NoHighlightAxis)) {
                if (gp.CurrentPlot->EnabledAxesX() > 1)
                    gp.CurrentPlot->Axes[gp.CurrentPlot->CurrentX].ColorHiLi = item->Color;
                if (gp.CurrentPlot->EnabledAxesY() > 1)
                    gp.CurrentPlot->Axes[gp.CurrentPlot->CurrentY].ColorHiLi = item->Color;
            }
        }
        // set render flags
        s.RenderLine       = s.Colors[ImPlotCol_Line].w          > 0 && s.LineWeight > 0;
        s.RenderFill       = s.Colors[ImPlotCol_Fill].w          > 0;
        s.RenderMarkerLine = s.Colors[ImPlotCol_MarkerOutline].w > 0 && s.MarkerWeight > 0;
        s.RenderMarkerFill = s.Colors[ImPlotCol_MarkerFill].w    > 0;
        // push rendering clip rect
        PushPlotClipRect();
        return true;
    }
}

// Ends an item (call only if BeginItem returns true)
void EndItem() {
    ImPlotContext& gp = *GImPlot;
    // pop rendering clip rect
    PopPlotClipRect();
    // reset next item data
    gp.NextItemData.Reset();
    // set current item
    gp.PreviousItem = gp.CurrentItem;
    gp.CurrentItem  = NULL;
}

//-----------------------------------------------------------------------------
// INDEXERS
//-----------------------------------------------------------------------------

// Offsets and strides a data buffer
template <typename T>
IMPLOT_INLINE T IndexData(const T* data, int idx, int count, int offset, int stride) {
    const int s = ((offset == 0) << 0) | ((stride == sizeof(T)) << 1);
    switch (s) {
        case 3 : return data[idx];
        case 2 : return data[(offset + idx) % count];
        case 1 : return *(const T*)(const void*)((const unsigned char*)data + (size_t)((idx) ) * stride);
        case 0 : return *(const T*)(const void*)((const unsigned char*)data + (size_t)((offset + idx) % count) * stride);
        default: return T(0);
    }
}

//-----------------------------------------------------------------------------
// GETTERS
//-----------------------------------------------------------------------------

// Getters can be thought of as iterators that convert user data (e.g. raw arrays)
// to ImPlotPoints

template <typename T>
struct GetterIdx {
    GetterIdx(const T* data, int count, int offset = 0, int stride = sizeof(T)) :
        Data(data),
        Count(count),
        Offset(count ? ImPosMod(offset, count) : 0),
        Stride(stride)
    { }
    template <typename I> IMPLOT_INLINE double operator()(I idx) const {
        return (double)IndexData(Data, idx, Count, Offset, Stride);
    }
    const T* Data;
    int Count;
    int Offset;
    int Stride;
};

struct GetterLin {
    GetterLin(double m, double b) : M(m), B(b) { }
    template <typename I> IMPLOT_INLINE double operator()(I idx) const {
        return M * idx + B;
    }
    const double M;
    const double B;
};

struct GetterRef {
    GetterRef(double ref) : Ref(ref) { }
    template <typename I> IMPLOT_INLINE double operator()(I) const { return Ref; }
    const double Ref;
};

template <typename TGetterX, typename TGetterY>
struct GetterXY {
    GetterXY(TGetterX x, TGetterY y, int count) : GetterX(x), GetterY(y), Count(count) { }
    template <typename I> IMPLOT_INLINE ImPlotPoint operator()(I idx) const {
        return ImPlotPoint(GetterX(idx),GetterY(idx));
    }
    const TGetterX GetterX;
    const TGetterY GetterY;
    const int Count;
};

// Interprets an array of Y points as ImPlotPoints where the X value is the index
template <typename T>
struct GetterXs {
    GetterXs(const T* xs, int count, double yscale, double y0, int offset, int stride) :
        Xs(xs),
        Count(count),
        YScale(yscale),
        Y0(y0),
        Offset(count ? ImPosMod(offset, count) : 0),
        Stride(stride)
    { }
    template <typename I> IMPLOT_INLINE ImPlotPoint operator()(I idx) const {
        return ImPlotPoint((double)IndexData(Xs, idx, Count, Offset, Stride), Y0 + YScale * idx);
    }
    const T* const Xs;
    const int Count;
    const double YScale;
    const double Y0;
    const int Offset;
    const int Stride;
};

/// Interprets a user's function pointer as ImPlotPoints
struct GetterFuncPtr {
    GetterFuncPtr(ImPlotPoint (*getter)(void* data, int idx), void* data, int count) :
        Getter(getter),
        Data(data),
        Count(count)
    { }
    template <typename I> IMPLOT_INLINE ImPlotPoint operator()(I idx) const {
        return Getter(Data, idx);
    }
    ImPlotPoint (* const Getter)(void* data, int idx);
    void* const Data;
    const int Count;
};

template <typename TGetter>
struct GetterOverrideX {
    GetterOverrideX(const TGetter& getter, double x) : Getter(getter), X(x), Count(getter.Count) { }
    template <typename I> IMPLOT_INLINE ImPlotPoint operator()(I idx) const {
        ImPlotPoint p = Getter(idx);
        p.x = X;
        return p;
    }
    const TGetter& Getter;
    const double X;
    const int Count;
};

template <typename TGetter>
struct GetterOverrideY {
    GetterOverrideY(const TGetter& getter, double y) : Getter(getter), Y(y), Count(getter.Count) { }
    template <typename I> IMPLOT_INLINE ImPlotPoint operator()(I idx) const {
        ImPlotPoint p = Getter(idx);
        p.y = Y;
        return p;
    }
    const TGetter& Getter;
    const double Y;
    const int Count;
};

template <typename T>
struct GetterError {
    GetterError(const T* xs, const T* ys, const T* neg, const T* pos, int count, int offset, int stride) :
        Xs(xs),
        Ys(ys),
        Neg(neg),
        Pos(pos),
        Count(count),
        Offset(count ? ImPosMod(offset, count) : 0),
        Stride(stride)
    { }
    template <typename I> IMPLOT_INLINE ImPlotPointError operator()(I idx) const {
        return ImPlotPointError((double)IndexData(Xs,  idx, Count, Offset, Stride),
                                (double)IndexData(Ys,  idx, Count, Offset, Stride),
                                (double)IndexData(Neg, idx, Count, Offset, Stride),
                                (double)IndexData(Pos, idx, Count, Offset, Stride));
    }
    const T* const Xs;
    const T* const Ys;
    const T* const Neg;
    const T* const Pos;
    const int Count;
    const int Offset;
    const int Stride;
};

//-----------------------------------------------------------------------------
// TRANSFORMERS
//-----------------------------------------------------------------------------

// Transforms convert points in plot space (i.e. ImPlotPoint) to pixel space (i.e. ImVec2)

struct TransformerLin {
    TransformerLin(double pixMin, double pltMin, double,       double m, double    ) : PixMin(pixMin), PltMin(pltMin), M(m) { }
    template <typename T> IMPLOT_INLINE float operator()(T p) const { return (float)(PixMin + M * (p - PltMin)); }
    double PixMin, PltMin, M;
};

struct TransformerLog {
    TransformerLog(double pixMin, double pltMin, double pltMax, double m, double den) : Den(den), PltMin(pltMin), PltMax(pltMax), PixMin(pixMin), M(m) { }
    template <typename T> IMPLOT_INLINE float operator()(T p) const {
        p = p <= 0.0 ? IMPLOT_LOG_ZERO : p;
        double t = ImLog10(p / PltMin) / Den;
        p = ImLerp(PltMin, PltMax, (float)t);
        return (float)(PixMin + M * (p - PltMin));
    }
    double Den, PltMin, PltMax, PixMin, M;
};

template <typename TransformerX, typename TransformerY>
struct TransformerXY {
    TransformerXY(const ImPlotAxis& x_axis, const ImPlotAxis& y_axis) :
        Tx(x_axis.PixelMin,
           x_axis.Range.Min,
           x_axis.Range.Max,
           x_axis.LinM,
           x_axis.LogD),
        Ty(y_axis.PixelMin,
           y_axis.Range.Min,
           y_axis.Range.Max,
           y_axis.LinM,
           y_axis.LogD)
    { }

    TransformerXY(const ImPlotPlot& plot) :
        TransformerXY(plot.Axes[plot.CurrentX], plot.Axes[plot.CurrentY])
    { }

    TransformerXY() :
        TransformerXY(*GImPlot->CurrentPlot)
    { }

    template <typename P> IMPLOT_INLINE ImVec2 operator()(const P& plt) const {
        ImVec2 out;
        out.x = Tx(plt.x);
        out.y = Ty(plt.y);
        return out;
    }
    TransformerX Tx;
    TransformerY Ty;
};

typedef TransformerXY<TransformerLin,TransformerLin> TransformerLinLin;
typedef TransformerXY<TransformerLin,TransformerLog> TransformerLinLog;
typedef TransformerXY<TransformerLog,TransformerLin> TransformerLogLin;
typedef TransformerXY<TransformerLog,TransformerLog> TransformerLogLog;

//-----------------------------------------------------------------------------
// PRIMITIVE RENDERERS
//-----------------------------------------------------------------------------

IMPLOT_INLINE void PrimLine(const ImVec2& P1, const ImVec2& P2, float half_weight, ImU32 col, ImDrawList& DrawList, ImVec2 uv) {
    float dx = P2.x - P1.x;
    float dy = P2.y - P1.y;
    IMPLOT_NORMALIZE2F_OVER_ZERO(dx, dy);
    dx *= half_weight;
    dy *= half_weight;
    DrawList._VtxWritePtr[0].pos.x = P1.x + dy;
    DrawList._VtxWritePtr[0].pos.y = P1.y - dx;
    DrawList._VtxWritePtr[0].uv    = uv;
    DrawList._VtxWritePtr[0].col   = col;
    DrawList._VtxWritePtr[1].pos.x = P2.x + dy;
    DrawList._VtxWritePtr[1].pos.y = P2.y - dx;
    DrawList._VtxWritePtr[1].uv    = uv;
    DrawList._VtxWritePtr[1].col   = col;
    DrawList._VtxWritePtr[2].pos.x = P2.x - dy;
    DrawList._VtxWritePtr[2].pos.y = P2.y + dx;
    DrawList._VtxWritePtr[2].uv    = uv;
    DrawList._VtxWritePtr[2].col   = col;
    DrawList._VtxWritePtr[3].pos.x = P1.x - dy;
    DrawList._VtxWritePtr[3].pos.y = P1.y + dx;
    DrawList._VtxWritePtr[3].uv    = uv;
    DrawList._VtxWritePtr[3].col   = col;
    DrawList._VtxWritePtr += 4;
    DrawList._IdxWritePtr[0] = (ImDrawIdx)(DrawList._VtxCurrentIdx);
    DrawList._IdxWritePtr[1] = (ImDrawIdx)(DrawList._VtxCurrentIdx + 1);
    DrawList._IdxWritePtr[2] = (ImDrawIdx)(DrawList._VtxCurrentIdx + 2);
    DrawList._IdxWritePtr[3] = (ImDrawIdx)(DrawList._VtxCurrentIdx);
    DrawList._IdxWritePtr[4] = (ImDrawIdx)(DrawList._VtxCurrentIdx + 2);
    DrawList._IdxWritePtr[5] = (ImDrawIdx)(DrawList._VtxCurrentIdx + 3);
    DrawList._IdxWritePtr += 6;
    DrawList._VtxCurrentIdx += 4;
}

IMPLOT_INLINE void PrimRectFilled(const ImVec2& Pmin, const ImVec2& Pmax, ImU32 col, ImDrawList& DrawList, ImVec2 uv) {
    DrawList._VtxWritePtr[0].pos   = Pmin;
    DrawList._VtxWritePtr[0].uv    = uv;
    DrawList._VtxWritePtr[0].col   = col;
    DrawList._VtxWritePtr[1].pos   = Pmax;
    DrawList._VtxWritePtr[1].uv    = uv;
    DrawList._VtxWritePtr[1].col   = col;
    DrawList._VtxWritePtr[2].pos.x = Pmin.x;
    DrawList._VtxWritePtr[2].pos.y = Pmax.y;
    DrawList._VtxWritePtr[2].uv    = uv;
    DrawList._VtxWritePtr[2].col   = col;
    DrawList._VtxWritePtr[3].pos.x = Pmax.x;
    DrawList._VtxWritePtr[3].pos.y = Pmin.y;
    DrawList._VtxWritePtr[3].uv    = uv;
    DrawList._VtxWritePtr[3].col   = col;
    DrawList._VtxWritePtr += 4;
    DrawList._IdxWritePtr[0] = (ImDrawIdx)(DrawList._VtxCurrentIdx);
    DrawList._IdxWritePtr[1] = (ImDrawIdx)(DrawList._VtxCurrentIdx + 1);
    DrawList._IdxWritePtr[2] = (ImDrawIdx)(DrawList._VtxCurrentIdx + 2);
    DrawList._IdxWritePtr[3] = (ImDrawIdx)(DrawList._VtxCurrentIdx);
    DrawList._IdxWritePtr[4] = (ImDrawIdx)(DrawList._VtxCurrentIdx + 1);
    DrawList._IdxWritePtr[5] = (ImDrawIdx)(DrawList._VtxCurrentIdx + 3);
    DrawList._IdxWritePtr += 6;
    DrawList._VtxCurrentIdx += 4;
}

template <typename TGetter, typename TTransformer>
struct LineStripRenderer {
    IMPLOT_INLINE LineStripRenderer(const TGetter& getter, const TTransformer& transformer, ImU32 col, float weight) :
        Getter(getter),
        Transformer(transformer),
        Prims(Getter.Count - 1),
        Col(col),
        HalfWeight(weight/2)
    {
        P1 = Transformer(Getter(0));
    }
    IMPLOT_INLINE bool operator()(ImDrawList& DrawList, const ImRect& cull_rect, const ImVec2& uv, int prim) const {
        ImVec2 P2 = Transformer(Getter(prim + 1));
        if (!cull_rect.Overlaps(ImRect(ImMin(P1, P2), ImMax(P1, P2)))) {
            P1 = P2;
            return false;
        }
        PrimLine(P1,P2,HalfWeight,Col,DrawList,uv);
        P1 = P2;
        return true;
    }
    const TGetter& Getter;
    const TTransformer& Transformer;
    const int Prims;
    const ImU32 Col;
    const float HalfWeight;
    mutable ImVec2 P1;
    static const int IdxConsumed = 6;
    static const int VtxConsumed = 4;
};

template <typename TGetter1, typename TGetter2, typename TTransformer>
struct LineSegmentsRenderer {
    IMPLOT_INLINE LineSegmentsRenderer(const TGetter1& getter1, const TGetter2& getter2, const TTransformer& transformer, ImU32 col, float weight) :
        Getter1(getter1),
        Getter2(getter2),
        Transformer(transformer),
        Prims(ImMin(Getter1.Count, Getter2.Count)),
        Col(col),
        HalfWeight(weight/2)
    {}
    IMPLOT_INLINE bool operator()(ImDrawList& DrawList, const ImRect& cull_rect, const ImVec2& uv, int prim) const {
        ImVec2 P1 = Transformer(Getter1(prim));
        ImVec2 P2 = Transformer(Getter2(prim));
        if (!cull_rect.Overlaps(ImRect(ImMin(P1, P2), ImMax(P1, P2))))
            return false;
        PrimLine(P1,P2,HalfWeight,Col,DrawList,uv);
        return true;
    }
    const TGetter1& Getter1;
    const TGetter2& Getter2;
    const TTransformer& Transformer;
    const int Prims;
    const ImU32 Col;
    const float HalfWeight;
    static const int IdxConsumed = 6;
    static const int VtxConsumed = 4;
};

template <typename TGetter, typename TTransformer>
struct StairsRenderer {
    IMPLOT_INLINE StairsRenderer(const TGetter& getter, const TTransformer& transformer, ImU32 col, float weight) :
        Getter(getter),
        Transformer(transformer),
        Prims(Getter.Count - 1),
        Col(col),
        HalfWeight(weight * 0.5f)
    {
        P1 = Transformer(Getter(0));
    }
    IMPLOT_INLINE bool operator()(ImDrawList& DrawList, const ImRect& cull_rect, const ImVec2& uv, int prim) const {
        ImVec2 P2 = Transformer(Getter(prim + 1));
        if (!cull_rect.Overlaps(ImRect(ImMin(P1, P2), ImMax(P1, P2)))) {
            P1 = P2;
            return false;
        }
        PrimRectFilled(ImVec2(P1.x, P1.y + HalfWeight), ImVec2(P2.x, P1.y - HalfWeight), Col, DrawList, uv);
        PrimRectFilled(ImVec2(P2.x - HalfWeight, P2.y), ImVec2(P2.x + HalfWeight, P1.y), Col, DrawList, uv);
        P1 = P2;
        return true;
    }
    const TGetter& Getter;
    const TTransformer& Transformer;
    const int Prims;
    const ImU32 Col;
    const float HalfWeight;
    mutable ImVec2 P1;
    static const int IdxConsumed = 12;
    static const int VtxConsumed = 8;
};



template <typename TGetter1, typename TGetter2, typename TTransformer>
struct ShadedRenderer {
    IMPLOT_INLINE ShadedRenderer(const TGetter1& getter1, const TGetter2& getter2, const TTransformer& transformer, ImU32 col) :
        Getter1(getter1),
        Getter2(getter2),
        Transformer(transformer),
        Prims(ImMin(Getter1.Count, Getter2.Count) - 1),
        Col(col)
    {
        P11 = Transformer(Getter1(0));
        P12 = Transformer(Getter2(0));
    }

    IMPLOT_INLINE bool operator()(ImDrawList& DrawList, const ImRect& cull_rect, const ImVec2& uv, int prim) const {
        ImVec2 P21 = Transformer(Getter1(prim+1));
        ImVec2 P22 = Transformer(Getter2(prim+1));
        ImRect rect(ImMin(ImMin(ImMin(P11,P12),P21),P22), ImMax(ImMax(ImMax(P11,P12),P21),P22));
        if (!cull_rect.Overlaps(rect)) {
            P11 = P21;
            P12 = P22;
            return false;
        }
        const int intersect = (P11.y > P12.y && P22.y > P21.y) || (P12.y > P11.y && P21.y > P22.y);
        ImVec2 intersection = Intersection(P11,P21,P12,P22);
        DrawList._VtxWritePtr[0].pos = P11;
        DrawList._VtxWritePtr[0].uv  = uv;
        DrawList._VtxWritePtr[0].col = Col;
        DrawList._VtxWritePtr[1].pos = P21;
        DrawList._VtxWritePtr[1].uv  = uv;
        DrawList._VtxWritePtr[1].col = Col;
        DrawList._VtxWritePtr[2].pos = intersection;
        DrawList._VtxWritePtr[2].uv  = uv;
        DrawList._VtxWritePtr[2].col = Col;
        DrawList._VtxWritePtr[3].pos = P12;
        DrawList._VtxWritePtr[3].uv  = uv;
        DrawList._VtxWritePtr[3].col = Col;
        DrawList._VtxWritePtr[4].pos = P22;
        DrawList._VtxWritePtr[4].uv  = uv;
        DrawList._VtxWritePtr[4].col = Col;
        DrawList._VtxWritePtr += 5;
        DrawList._IdxWritePtr[0] = (ImDrawIdx)(DrawList._VtxCurrentIdx);
        DrawList._IdxWritePtr[1] = (ImDrawIdx)(DrawList._VtxCurrentIdx + 1 + intersect);
        DrawList._IdxWritePtr[2] = (ImDrawIdx)(DrawList._VtxCurrentIdx + 3);
        DrawList._IdxWritePtr[3] = (ImDrawIdx)(DrawList._VtxCurrentIdx + 1);
        DrawList._IdxWritePtr[4] = (ImDrawIdx)(DrawList._VtxCurrentIdx + 4);
        DrawList._IdxWritePtr[5] = (ImDrawIdx)(DrawList._VtxCurrentIdx + 3 - intersect);
        DrawList._IdxWritePtr += 6;
        DrawList._VtxCurrentIdx += 5;
        P11 = P21;
        P12 = P22;
        return true;
    }
    const TGetter1& Getter1;
    const TGetter2& Getter2;
    const TTransformer& Transformer;
    const int Prims;
    const ImU32 Col;
    mutable ImVec2 P11;
    mutable ImVec2 P12;
    static const int IdxConsumed = 6;
    static const int VtxConsumed = 5;
};

/// Renders primitive shapes in bulk as efficiently as possible.
template <typename Renderer>
IMPLOT_INLINE void RenderPrimitives(const Renderer& renderer, ImDrawList& DrawList, const ImRect& cull_rect) {
    unsigned int prims        = renderer.Prims;
    unsigned int prims_culled = 0;
    unsigned int idx          = 0;
    const ImVec2 uv = DrawList._Data->TexUvWhitePixel;
    while (prims) {
        // find how many can be reserved up to end of current draw command's limit
        unsigned int cnt = ImMin(prims, (MaxIdx<ImDrawIdx>::Value - DrawList._VtxCurrentIdx) / Renderer::VtxConsumed);
        // make sure at least this many elements can be rendered to avoid situations where at the end of buffer this slow path is not taken all the time
        if (cnt >= ImMin(64u, prims)) {
            if (prims_culled >= cnt)
                prims_culled -= cnt; // reuse previous reservation
            else {
                DrawList.PrimReserve((cnt - prims_culled) * Renderer::IdxConsumed, (cnt - prims_culled) * Renderer::VtxConsumed); // add more elements to previous reservation
                prims_culled = 0;
            }
        }
        else
        {
            if (prims_culled > 0) {
                DrawList.PrimUnreserve(prims_culled * Renderer::IdxConsumed, prims_culled * Renderer::VtxConsumed);
                prims_culled = 0;
            }
            cnt = ImMin(prims, (MaxIdx<ImDrawIdx>::Value - 0/*DrawList._VtxCurrentIdx*/) / Renderer::VtxConsumed);
            DrawList.PrimReserve(cnt * Renderer::IdxConsumed, cnt * Renderer::VtxConsumed); // reserve new draw command
        }
        prims -= cnt;
        for (unsigned int ie = idx + cnt; idx != ie; ++idx) {
            if (!renderer(DrawList, cull_rect, uv, idx))
                prims_culled++;
        }
    }
    if (prims_culled > 0)
        DrawList.PrimUnreserve(prims_culled * Renderer::IdxConsumed, prims_culled * Renderer::VtxConsumed);
}

template <typename Getter, typename Transformer>
IMPLOT_INLINE void RenderLineStrip(const Getter& getter, const Transformer& transformer, ImDrawList& DrawList, float line_weight, ImU32 col) {
    ImPlotContext& gp = *GImPlot;
    if (ImHasFlag(gp.CurrentPlot->Flags, ImPlotFlags_AntiAliased) || gp.Style.AntiAliasedLines) {
        ImVec2 p1 = transformer(getter(0));
        for (int i = 1; i < getter.Count; ++i) {
            ImVec2 p2 = transformer(getter(i));
            if (gp.CurrentPlot->PlotRect.Overlaps(ImRect(ImMin(p1, p2), ImMax(p1, p2))))
                DrawList.AddLine(p1, p2, col, line_weight);
            p1 = p2;
        }
    }
    else {
        RenderPrimitives(LineStripRenderer<Getter,Transformer>(getter, transformer, col, line_weight), DrawList, gp.CurrentPlot->PlotRect);
    }
}

template <typename Getter1, typename Getter2, typename Transformer>
IMPLOT_INLINE void RenderLineSegments(const Getter1& getter1, const Getter2& getter2, const Transformer& transformer, ImDrawList& DrawList, float line_weight, ImU32 col) {
    ImPlotContext& gp = *GImPlot;
    if (ImHasFlag(gp.CurrentPlot->Flags, ImPlotFlags_AntiAliased) || gp.Style.AntiAliasedLines) {
        int I = ImMin(getter1.Count, getter2.Count);
        for (int i = 0; i < I; ++i) {
            ImVec2 p1 = transformer(getter1(i));
            ImVec2 p2 = transformer(getter2(i));
            if (gp.CurrentPlot->PlotRect.Overlaps(ImRect(ImMin(p1, p2), ImMax(p1, p2))))
                DrawList.AddLine(p1, p2, col, line_weight);
        }
    }
    else {
        RenderPrimitives(LineSegmentsRenderer<Getter1,Getter2,Transformer>(getter1, getter2, transformer, col, line_weight), DrawList, gp.CurrentPlot->PlotRect);
    }
}

template <typename Getter, typename Transformer>
IMPLOT_INLINE void RenderStairs(const Getter& getter, const Transformer& transformer, ImDrawList& DrawList, float line_weight, ImU32 col) {
    ImPlotContext& gp = *GImPlot;
    if (ImHasFlag(gp.CurrentPlot->Flags, ImPlotFlags_AntiAliased) || gp.Style.AntiAliasedLines) {
        ImVec2 p1 = transformer(getter(0));
        for (int i = 1; i < getter.Count; ++i) {
            ImVec2 p2 = transformer(getter(i));
            if (gp.CurrentPlot->PlotRect.Overlaps(ImRect(ImMin(p1, p2), ImMax(p1, p2)))) {
                ImVec2 p12(p2.x, p1.y);
                DrawList.AddLine(p1, p12, col, line_weight);
                DrawList.AddLine(p12, p2, col, line_weight);
            }
            p1 = p2;
        }
    }
    else {
        RenderPrimitives(StairsRenderer<Getter,Transformer>(getter, transformer, col, line_weight), DrawList, gp.CurrentPlot->PlotRect);
    }
}

//-----------------------------------------------------------------------------
// MARKER RENDERERS
//-----------------------------------------------------------------------------

IMPLOT_INLINE void TransformMarker(ImVec2* points, int n, const ImVec2& c, float s) {
    for (int i = 0; i < n; ++i) {
        points[i].x = c.x + points[i].x * s;
        points[i].y = c.y + points[i].y * s;
    }
}

IMPLOT_INLINE void RenderMarkerGeneral(ImDrawList& DrawList, ImVec2* points, int n, const ImVec2& c, float s, bool outline, ImU32 col_outline, bool fill, ImU32 col_fill, float weight) {
    TransformMarker(points, n, c, s);
    if (fill)
        DrawList.AddConvexPolyFilled(points, n, col_fill);
    if (outline && !(fill && col_outline == col_fill)) {
        for (int i = 0; i < n; ++i)
            DrawList.AddLine(points[i], points[(i+1)%n], col_outline, weight);
    }
}

IMPLOT_INLINE void RenderMarkerCircle(ImDrawList& DrawList, const ImVec2& c, float s, bool outline, ImU32 col_outline, bool fill, ImU32 col_fill, float weight) {
    ImVec2 marker[10] = {ImVec2(1.0f, 0.0f),
                         ImVec2(0.809017f, 0.58778524f),
                         ImVec2(0.30901697f, 0.95105654f),
                         ImVec2(-0.30901703f, 0.9510565f),
                         ImVec2(-0.80901706f, 0.5877852f),
                         ImVec2(-1.0f, 0.0f),
                         ImVec2(-0.80901694f, -0.58778536f),
                         ImVec2(-0.3090171f, -0.9510565f),
                         ImVec2(0.30901712f, -0.9510565f),
                         ImVec2(0.80901694f, -0.5877853f)};
    RenderMarkerGeneral(DrawList, marker, 10, c, s, outline, col_outline, fill, col_fill, weight);
}

IMPLOT_INLINE void RenderMarkerDiamond(ImDrawList& DrawList, const ImVec2& c, float s, bool outline, ImU32 col_outline, bool fill, ImU32 col_fill, float weight) {
    ImVec2 marker[4] = {ImVec2(1, 0), ImVec2(0, -1), ImVec2(-1, 0), ImVec2(0, 1)};
    RenderMarkerGeneral(DrawList, marker, 4, c, s, outline, col_outline, fill, col_fill, weight);
}

IMPLOT_INLINE void RenderMarkerSquare(ImDrawList& DrawList, const ImVec2& c, float s, bool outline, ImU32 col_outline, bool fill, ImU32 col_fill, float weight) {
    ImVec2 marker[4] = {ImVec2(SQRT_1_2,SQRT_1_2),ImVec2(SQRT_1_2,-SQRT_1_2),ImVec2(-SQRT_1_2,-SQRT_1_2),ImVec2(-SQRT_1_2,SQRT_1_2)};
    RenderMarkerGeneral(DrawList, marker, 4, c, s, outline, col_outline, fill, col_fill, weight);
}

IMPLOT_INLINE void RenderMarkerUp(ImDrawList& DrawList, const ImVec2& c, float s, bool outline, ImU32 col_outline, bool fill, ImU32 col_fill, float weight) {
    ImVec2 marker[3] = {ImVec2(SQRT_3_2,0.5f),ImVec2(0,-1),ImVec2(-SQRT_3_2,0.5f)};
    RenderMarkerGeneral(DrawList, marker, 3, c, s, outline, col_outline, fill, col_fill, weight);
}

IMPLOT_INLINE void RenderMarkerDown(ImDrawList& DrawList, const ImVec2& c, float s, bool outline, ImU32 col_outline, bool fill, ImU32 col_fill, float weight) {
    ImVec2 marker[3] = {ImVec2(SQRT_3_2,-0.5f),ImVec2(0,1),ImVec2(-SQRT_3_2,-0.5f)};
    RenderMarkerGeneral(DrawList, marker, 3, c, s, outline, col_outline, fill, col_fill, weight);
}

IMPLOT_INLINE void RenderMarkerLeft(ImDrawList& DrawList, const ImVec2& c, float s, bool outline, ImU32 col_outline, bool fill, ImU32 col_fill, float weight) {
    ImVec2 marker[3] = {ImVec2(-1,0), ImVec2(0.5, SQRT_3_2), ImVec2(0.5, -SQRT_3_2)};
    RenderMarkerGeneral(DrawList, marker, 3, c, s, outline, col_outline, fill, col_fill, weight);
}

IMPLOT_INLINE void RenderMarkerRight(ImDrawList& DrawList, const ImVec2& c, float s, bool outline, ImU32 col_outline, bool fill, ImU32 col_fill, float weight) {
    ImVec2 marker[3] = {ImVec2(1,0), ImVec2(-0.5, SQRT_3_2), ImVec2(-0.5, -SQRT_3_2)};
    RenderMarkerGeneral(DrawList, marker, 3, c, s, outline, col_outline, fill, col_fill, weight);
}

IMPLOT_INLINE void RenderMarkerAsterisk(ImDrawList& DrawList, const ImVec2& c, float s, bool /*outline*/, ImU32 col_outline, bool /*fill*/, ImU32 /*col_fill*/, float weight) {
    ImVec2 marker[6] = {ImVec2(SQRT_3_2, 0.5f), ImVec2(0, -1), ImVec2(-SQRT_3_2, 0.5f), ImVec2(SQRT_3_2, -0.5f), ImVec2(0, 1),  ImVec2(-SQRT_3_2, -0.5f)};
    TransformMarker(marker, 6, c, s);
    DrawList.AddLine(marker[0], marker[5], col_outline, weight);
    DrawList.AddLine(marker[1], marker[4], col_outline, weight);
    DrawList.AddLine(marker[2], marker[3], col_outline, weight);
}

IMPLOT_INLINE void RenderMarkerPlus(ImDrawList& DrawList, const ImVec2& c, float s, bool /*outline*/, ImU32 col_outline, bool /*fill*/, ImU32 /*col_fill*/, float weight) {
    ImVec2 marker[4] = {ImVec2(1, 0), ImVec2(0, -1), ImVec2(-1, 0), ImVec2(0, 1)};
    TransformMarker(marker, 4, c, s);
    DrawList.AddLine(marker[0], marker[2], col_outline, weight);
    DrawList.AddLine(marker[1], marker[3], col_outline, weight);
}

IMPLOT_INLINE void RenderMarkerCross(ImDrawList& DrawList, const ImVec2& c, float s, bool /*outline*/, ImU32 col_outline, bool /*fill*/, ImU32 /*col_fill*/, float weight) {
    ImVec2 marker[4] = {ImVec2(SQRT_1_2,SQRT_1_2),ImVec2(SQRT_1_2,-SQRT_1_2),ImVec2(-SQRT_1_2,-SQRT_1_2),ImVec2(-SQRT_1_2,SQRT_1_2)};
    TransformMarker(marker, 4, c, s);
    DrawList.AddLine(marker[0], marker[2], col_outline, weight);
    DrawList.AddLine(marker[1], marker[3], col_outline, weight);
}

template <typename Transformer, typename Getter>
IMPLOT_INLINE void RenderMarkers(Getter getter, Transformer transformer, ImDrawList& DrawList, ImPlotMarker marker, float size, bool rend_mk_line, ImU32 col_mk_line, float weight, bool rend_mk_fill, ImU32 col_mk_fill) {
    static void (*marker_table[ImPlotMarker_COUNT])(ImDrawList&, const ImVec2&, float s, bool, ImU32, bool, ImU32, float) = {
        RenderMarkerCircle,
        RenderMarkerSquare,
        RenderMarkerDiamond ,
        RenderMarkerUp ,
        RenderMarkerDown ,
        RenderMarkerLeft,
        RenderMarkerRight,
        RenderMarkerCross,
        RenderMarkerPlus,
        RenderMarkerAsterisk
    };
    ImPlotContext& gp = *GImPlot;
    const ImRect& rect = gp.CurrentPlot->PlotRect;
    for (int i = 0; i < getter.Count; ++i) {
        ImVec2 c = transformer(getter(i));
        if (c.x >= rect.Min.x && c.y >= rect.Min.y && c.x <= rect.Max.x && c.y <= rect.Max.y)
            marker_table[marker](DrawList, c, size, rend_mk_line, col_mk_line, rend_mk_fill, col_mk_fill, weight);
    }
}

//-----------------------------------------------------------------------------
// PLOT LINE
//-----------------------------------------------------------------------------

template <typename Getter>
IMPLOT_INLINE void PlotLineEx(const char* label_id, const Getter& getter) {
    if (BeginItem(label_id, ImPlotCol_Line)) {
        if (FitThisFrame()) {
            for (int i = 0; i < getter.Count; ++i) {
                ImPlotPoint p = getter(i);
                FitPoint(p);
            }
        }
        const ImPlotNextItemData& s = GetItemData();
        ImDrawList& DrawList = *GetPlotDrawList();
        if (getter.Count > 1 && s.RenderLine) {
            const ImU32 col_line    = ImGui::GetColorU32(s.Colors[ImPlotCol_Line]);
            switch (GetCurrentScale()) {
                case ImPlotScale_LinLin: RenderLineStrip(getter, TransformerLinLin(), DrawList, s.LineWeight, col_line); break;
                case ImPlotScale_LogLin: RenderLineStrip(getter, TransformerLogLin(), DrawList, s.LineWeight, col_line); break;
                case ImPlotScale_LinLog: RenderLineStrip(getter, TransformerLinLog(), DrawList, s.LineWeight, col_line); break;
                case ImPlotScale_LogLog: RenderLineStrip(getter, TransformerLogLog(), DrawList, s.LineWeight, col_line); break;
            }
        }
        // render markers
        if (s.Marker != ImPlotMarker_None) {
            // uncomment lines below to render markers over plot rect border
            // PopPlotClipRect();
            // PushPlotClipRect(s.MarkerSize);
            const ImU32 col_line = ImGui::GetColorU32(s.Colors[ImPlotCol_MarkerOutline]);
            const ImU32 col_fill = ImGui::GetColorU32(s.Colors[ImPlotCol_MarkerFill]);
            switch (GetCurrentScale()) {
                case ImPlotScale_LinLin: RenderMarkers(getter, TransformerLinLin(), DrawList, s.Marker, s.MarkerSize, s.RenderMarkerLine, col_line, s.MarkerWeight, s.RenderMarkerFill, col_fill); break;
                case ImPlotScale_LogLin: RenderMarkers(getter, TransformerLogLin(), DrawList, s.Marker, s.MarkerSize, s.RenderMarkerLine, col_line, s.MarkerWeight, s.RenderMarkerFill, col_fill); break;
                case ImPlotScale_LinLog: RenderMarkers(getter, TransformerLinLog(), DrawList, s.Marker, s.MarkerSize, s.RenderMarkerLine, col_line, s.MarkerWeight, s.RenderMarkerFill, col_fill); break;
                case ImPlotScale_LogLog: RenderMarkers(getter, TransformerLogLog(), DrawList, s.Marker, s.MarkerSize, s.RenderMarkerLine, col_line, s.MarkerWeight, s.RenderMarkerFill, col_fill); break;
            }
        }
        EndItem();
    }
}


template <typename T>
void PlotLine(const char* label_id, const T* values, int count, double xscale, double x0, int offset, int stride) {
    GetterXY<GetterLin,GetterIdx<T>> getter(GetterLin(xscale,x0),GetterIdx<T>(values,count,offset,stride),count);
    PlotLineEx(label_id, getter);
}

template IMPLOT_API void PlotLine<ImS8> (const char* label_id, const ImS8* values, int count, double xscale, double x0, int offset, int stride);
template IMPLOT_API void PlotLine<ImU8> (const char* label_id, const ImU8* values, int count, double xscale, double x0, int offset, int stride);
template IMPLOT_API void PlotLine<ImS16>(const char* label_id, const ImS16* values, int count, double xscale, double x0, int offset, int stride);
template IMPLOT_API void PlotLine<ImU16>(const char* label_id, const ImU16* values, int count, double xscale, double x0, int offset, int stride);
template IMPLOT_API void PlotLine<ImS32>(const char* label_id, const ImS32* values, int count, double xscale, double x0, int offset, int stride);
template IMPLOT_API void PlotLine<ImU32>(const char* label_id, const ImU32* values, int count, double xscale, double x0, int offset, int stride);
template IMPLOT_API void PlotLine<ImS64>(const char* label_id, const ImS64* values, int count, double xscale, double x0, int offset, int stride);
template IMPLOT_API void PlotLine<ImU64>(const char* label_id, const ImU64* values, int count, double xscale, double x0, int offset, int stride);
template IMPLOT_API void PlotLine<float>(const char* label_id, const float* values, int count, double xscale, double x0, int offset, int stride);
template IMPLOT_API void PlotLine<double>(const char* label_id, const double* values, int count, double xscale, double x0, int offset, int stride);

template <typename T>
void PlotLine(const char* label_id, const T* xs, const T* ys, int count, int offset, int stride) {
    GetterXY<GetterIdx<T>,GetterIdx<T>> getter(GetterIdx<T>(xs,count,offset,stride),GetterIdx<T>(ys,count,offset,stride),count);
    return PlotLineEx(label_id, getter);
}

template IMPLOT_API void PlotLine<ImS8>(const char* label_id, const ImS8* xs, const ImS8* ys, int count, int offset, int stride);
template IMPLOT_API void PlotLine<ImU8>(const char* label_id, const ImU8* xs, const ImU8* ys, int count, int offset, int stride);
template IMPLOT_API void PlotLine<ImS16>(const char* label_id, const ImS16* xs, const ImS16* ys, int count, int offset, int stride);
template IMPLOT_API void PlotLine<ImU16>(const char* label_id, const ImU16* xs, const ImU16* ys, int count, int offset, int stride);
template IMPLOT_API void PlotLine<ImS32>(const char* label_id, const ImS32* xs, const ImS32* ys, int count, int offset, int stride);
template IMPLOT_API void PlotLine<ImU32>(const char* label_id, const ImU32* xs, const ImU32* ys, int count, int offset, int stride);
template IMPLOT_API void PlotLine<ImS64>(const char* label_id, const ImS64* xs, const ImS64* ys, int count, int offset, int stride);
template IMPLOT_API void PlotLine<ImU64>(const char* label_id, const ImU64* xs, const ImU64* ys, int count, int offset, int stride);
template IMPLOT_API void PlotLine<float>(const char* label_id, const float* xs, const float* ys, int count, int offset, int stride);
template IMPLOT_API void PlotLine<double>(const char* label_id, const double* xs, const double* ys, int count, int offset, int stride);

// custom
void PlotLineG(const char* label_id, ImPlotGetter getter_func, void* data, int count) {
    GetterFuncPtr getter(getter_func,data, count);
    return PlotLineEx(label_id, getter);
}

//-----------------------------------------------------------------------------
// PLOT SCATTER
//-----------------------------------------------------------------------------

template <typename Getter>
IMPLOT_INLINE void PlotScatterEx(const char* label_id, const Getter& getter) {
    if (BeginItem(label_id, ImPlotCol_MarkerOutline)) {
        if (FitThisFrame()) {
            for (int i = 0; i < getter.Count; ++i) {
                ImPlotPoint p = getter(i);
                FitPoint(p);
            }
        }
        const ImPlotNextItemData& s = GetItemData();
        ImDrawList& DrawList = *GetPlotDrawList();
        // render markers
        ImPlotMarker marker = s.Marker == ImPlotMarker_None ? ImPlotMarker_Circle : s.Marker;
        if (marker != ImPlotMarker_None) {
            // uncomment lines below to render markers over plot rect border
            // PopPlotClipRect();
            // PushPlotClipRect(s.MarkerSize);
            const ImU32 col_line = ImGui::GetColorU32(s.Colors[ImPlotCol_MarkerOutline]);
            const ImU32 col_fill = ImGui::GetColorU32(s.Colors[ImPlotCol_MarkerFill]);
            switch (GetCurrentScale()) {
                case ImPlotScale_LinLin: RenderMarkers(getter, TransformerLinLin(), DrawList, marker, s.MarkerSize, s.RenderMarkerLine, col_line, s.MarkerWeight, s.RenderMarkerFill, col_fill); break;
                case ImPlotScale_LogLin: RenderMarkers(getter, TransformerLogLin(), DrawList, marker, s.MarkerSize, s.RenderMarkerLine, col_line, s.MarkerWeight, s.RenderMarkerFill, col_fill); break;
                case ImPlotScale_LinLog: RenderMarkers(getter, TransformerLinLog(), DrawList, marker, s.MarkerSize, s.RenderMarkerLine, col_line, s.MarkerWeight, s.RenderMarkerFill, col_fill); break;
                case ImPlotScale_LogLog: RenderMarkers(getter, TransformerLogLog(), DrawList, marker, s.MarkerSize, s.RenderMarkerLine, col_line, s.MarkerWeight, s.RenderMarkerFill, col_fill); break;
            }
        }
        EndItem();
    }
}

template <typename T>
void PlotScatter(const char* label_id, const T* values, int count, double xscale, double x0, int offset, int stride) {
    GetterXY<GetterLin,GetterIdx<T>> getter(GetterLin(xscale,x0),GetterIdx<T>(values,count,offset,stride),count);
    PlotScatterEx(label_id, getter);
}

template IMPLOT_API void PlotScatter<ImS8>(const char* label_id, const ImS8* values, int count, double xscale, double x0, int offset, int stride);
template IMPLOT_API void PlotScatter<ImU8>(const char* label_id, const ImU8* values, int count, double xscale, double x0, int offset, int stride);
template IMPLOT_API void PlotScatter<ImS16>(const char* label_id, const ImS16* values, int count, double xscale, double x0, int offset, int stride);
template IMPLOT_API void PlotScatter<ImU16>(const char* label_id, const ImU16* values, int count, double xscale, double x0, int offset, int stride);
template IMPLOT_API void PlotScatter<ImS32>(const char* label_id, const ImS32* values, int count, double xscale, double x0, int offset, int stride);
template IMPLOT_API void PlotScatter<ImU32>(const char* label_id, const ImU32* values, int count, double xscale, double x0, int offset, int stride);
template IMPLOT_API void PlotScatter<ImS64>(const char* label_id, const ImS64* values, int count, double xscale, double x0, int offset, int stride);
template IMPLOT_API void PlotScatter<ImU64>(const char* label_id, const ImU64* values, int count, double xscale, double x0, int offset, int stride);
template IMPLOT_API void PlotScatter<float>(const char* label_id, const float* values, int count, double xscale, double x0, int offset, int stride);
template IMPLOT_API void PlotScatter<double>(const char* label_id, const double* values, int count, double xscale, double x0, int offset, int stride);

template <typename T>
void PlotScatter(const char* label_id, const T* xs, const T* ys, int count, int offset, int stride) {
    GetterXY<GetterIdx<T>,GetterIdx<T>> getter(GetterIdx<T>(xs,count,offset,stride),GetterIdx<T>(ys,count,offset,stride),count);
    return PlotScatterEx(label_id, getter);
}

template IMPLOT_API void PlotScatter<ImS8>(const char* label_id, const ImS8* xs, const ImS8* ys, int count, int offset, int stride);
template IMPLOT_API void PlotScatter<ImU8>(const char* label_id, const ImU8* xs, const ImU8* ys, int count, int offset, int stride);
template IMPLOT_API void PlotScatter<ImS16>(const char* label_id, const ImS16* xs, const ImS16* ys, int count, int offset, int stride);
template IMPLOT_API void PlotScatter<ImU16>(const char* label_id, const ImU16* xs, const ImU16* ys, int count, int offset, int stride);
template IMPLOT_API void PlotScatter<ImS32>(const char* label_id, const ImS32* xs, const ImS32* ys, int count, int offset, int stride);
template IMPLOT_API void PlotScatter<ImU32>(const char* label_id, const ImU32* xs, const ImU32* ys, int count, int offset, int stride);
template IMPLOT_API void PlotScatter<ImS64>(const char* label_id, const ImS64* xs, const ImS64* ys, int count, int offset, int stride);
template IMPLOT_API void PlotScatter<ImU64>(const char* label_id, const ImU64* xs, const ImU64* ys, int count, int offset, int stride);
template IMPLOT_API void PlotScatter<float>(const char* label_id, const float* xs, const float* ys, int count, int offset, int stride);
template IMPLOT_API void PlotScatter<double>(const char* label_id, const double* xs, const double* ys, int count, int offset, int stride);

// custom
void PlotScatterG(const char* label_id, ImPlotGetter getter_func, void* data, int count) {
    GetterFuncPtr getter(getter_func,data, count);
    return PlotScatterEx(label_id, getter);
}

//-----------------------------------------------------------------------------
// PLOT STAIRS
//-----------------------------------------------------------------------------

template <typename Getter>
IMPLOT_INLINE void PlotStairsEx(const char* label_id, const Getter& getter) {
    if (BeginItem(label_id, ImPlotCol_Line)) {
        if (FitThisFrame()) {
            for (int i = 0; i < getter.Count; ++i) {
                ImPlotPoint p = getter(i);
                FitPoint(p);
            }
        }
        const ImPlotNextItemData& s = GetItemData();
        ImDrawList& DrawList = *GetPlotDrawList();
        if (getter.Count > 1 && s.RenderLine) {
            const ImU32 col_line    = ImGui::GetColorU32(s.Colors[ImPlotCol_Line]);
            switch (GetCurrentScale()) {
                case ImPlotScale_LinLin: RenderStairs(getter, TransformerLinLin(), DrawList, s.LineWeight, col_line); break;
                case ImPlotScale_LogLin: RenderStairs(getter, TransformerLogLin(), DrawList, s.LineWeight, col_line); break;
                case ImPlotScale_LinLog: RenderStairs(getter, TransformerLinLog(), DrawList, s.LineWeight, col_line); break;
                case ImPlotScale_LogLog: RenderStairs(getter, TransformerLogLog(), DrawList, s.LineWeight, col_line); break;
            }
        }
        // render markers
        if (s.Marker != ImPlotMarker_None) {
            PopPlotClipRect();
            PushPlotClipRect(s.MarkerSize);
            const ImU32 col_line = ImGui::GetColorU32(s.Colors[ImPlotCol_MarkerOutline]);
            const ImU32 col_fill = ImGui::GetColorU32(s.Colors[ImPlotCol_MarkerFill]);
            switch (GetCurrentScale()) {
                case ImPlotScale_LinLin: RenderMarkers(getter, TransformerLinLin(), DrawList, s.Marker, s.MarkerSize, s.RenderMarkerLine, col_line, s.MarkerWeight, s.RenderMarkerFill, col_fill); break;
                case ImPlotScale_LogLin: RenderMarkers(getter, TransformerLogLin(), DrawList, s.Marker, s.MarkerSize, s.RenderMarkerLine, col_line, s.MarkerWeight, s.RenderMarkerFill, col_fill); break;
                case ImPlotScale_LinLog: RenderMarkers(getter, TransformerLinLog(), DrawList, s.Marker, s.MarkerSize, s.RenderMarkerLine, col_line, s.MarkerWeight, s.RenderMarkerFill, col_fill); break;
                case ImPlotScale_LogLog: RenderMarkers(getter, TransformerLogLog(), DrawList, s.Marker, s.MarkerSize, s.RenderMarkerLine, col_line, s.MarkerWeight, s.RenderMarkerFill, col_fill); break;
            }
        }
        EndItem();
    }
}

template <typename T>
void PlotStairs(const char* label_id, const T* values, int count, double xscale, double x0, int offset, int stride) {
    GetterXY<GetterLin,GetterIdx<T>> getter(GetterLin(xscale,x0),GetterIdx<T>(values,count,offset,stride),count);
    PlotStairsEx(label_id, getter);
}

template IMPLOT_API void PlotStairs<ImS8> (const char* label_id, const ImS8* values, int count, double xscale, double x0, int offset, int stride);
template IMPLOT_API void PlotStairs<ImU8> (const char* label_id, const ImU8* values, int count, double xscale, double x0, int offset, int stride);
template IMPLOT_API void PlotStairs<ImS16>(const char* label_id, const ImS16* values, int count, double xscale, double x0, int offset, int stride);
template IMPLOT_API void PlotStairs<ImU16>(const char* label_id, const ImU16* values, int count, double xscale, double x0, int offset, int stride);
template IMPLOT_API void PlotStairs<ImS32>(const char* label_id, const ImS32* values, int count, double xscale, double x0, int offset, int stride);
template IMPLOT_API void PlotStairs<ImU32>(const char* label_id, const ImU32* values, int count, double xscale, double x0, int offset, int stride);
template IMPLOT_API void PlotStairs<ImS64>(const char* label_id, const ImS64* values, int count, double xscale, double x0, int offset, int stride);
template IMPLOT_API void PlotStairs<ImU64>(const char* label_id, const ImU64* values, int count, double xscale, double x0, int offset, int stride);
template IMPLOT_API void PlotStairs<float>(const char* label_id, const float* values, int count, double xscale, double x0, int offset, int stride);
template IMPLOT_API void PlotStairs<double>(const char* label_id, const double* values, int count, double xscale, double x0, int offset, int stride);

template <typename T>
void PlotStairs(const char* label_id, const T* xs, const T* ys, int count, int offset, int stride) {
    GetterXY<GetterIdx<T>,GetterIdx<T>> getter(GetterIdx<T>(xs,count,offset,stride),GetterIdx<T>(ys,count,offset,stride),count);
    return PlotStairsEx(label_id, getter);
}

template IMPLOT_API void PlotStairs<ImS8>(const char* label_id, const ImS8* xs, const ImS8* ys, int count, int offset, int stride);
template IMPLOT_API void PlotStairs<ImU8>(const char* label_id, const ImU8* xs, const ImU8* ys, int count, int offset, int stride);
template IMPLOT_API void PlotStairs<ImS16>(const char* label_id, const ImS16* xs, const ImS16* ys, int count, int offset, int stride);
template IMPLOT_API void PlotStairs<ImU16>(const char* label_id, const ImU16* xs, const ImU16* ys, int count, int offset, int stride);
template IMPLOT_API void PlotStairs<ImS32>(const char* label_id, const ImS32* xs, const ImS32* ys, int count, int offset, int stride);
template IMPLOT_API void PlotStairs<ImU32>(const char* label_id, const ImU32* xs, const ImU32* ys, int count, int offset, int stride);
template IMPLOT_API void PlotStairs<ImS64>(const char* label_id, const ImS64* xs, const ImS64* ys, int count, int offset, int stride);
template IMPLOT_API void PlotStairs<ImU64>(const char* label_id, const ImU64* xs, const ImU64* ys, int count, int offset, int stride);
template IMPLOT_API void PlotStairs<float>(const char* label_id, const float* xs, const float* ys, int count, int offset, int stride);
template IMPLOT_API void PlotStairs<double>(const char* label_id, const double* xs, const double* ys, int count, int offset, int stride);

// custom
void PlotStairsG(const char* label_id, ImPlotGetter getter_func, void* data, int count) {
    GetterFuncPtr getter(getter_func,data, count);
    return PlotStairsEx(label_id, getter);
}

//-----------------------------------------------------------------------------
// PLOT SHADED
//-----------------------------------------------------------------------------

template <typename Getter1, typename Getter2>
IMPLOT_INLINE void PlotShadedEx(const char* label_id, const Getter1& getter1, const Getter2& getter2, bool fit2) {
    if (BeginItem(label_id, ImPlotCol_Fill)) {
        if (FitThisFrame()) {
            for (int i = 0; i < getter1.Count; ++i)
                FitPoint(getter1(i));
            if (fit2) {
                for (int i = 0; i < getter2.Count; ++i)
                    FitPoint(getter2(i));
            }
        }
        const ImPlotNextItemData& s = GetItemData();
        ImDrawList & DrawList = *GetPlotDrawList();
        if (s.RenderFill) {
            ImU32 col = ImGui::GetColorU32(s.Colors[ImPlotCol_Fill]);
            switch (GetCurrentScale()) {
                case ImPlotScale_LinLin: RenderPrimitives(ShadedRenderer<Getter1,Getter2,TransformerLinLin>(getter1,getter2,TransformerLinLin(), col), DrawList, GImPlot->CurrentPlot->PlotRect); break;
                case ImPlotScale_LogLin: RenderPrimitives(ShadedRenderer<Getter1,Getter2,TransformerLogLin>(getter1,getter2,TransformerLogLin(), col), DrawList, GImPlot->CurrentPlot->PlotRect); break;
                case ImPlotScale_LinLog: RenderPrimitives(ShadedRenderer<Getter1,Getter2,TransformerLinLog>(getter1,getter2,TransformerLinLog(), col), DrawList, GImPlot->CurrentPlot->PlotRect); break;
                case ImPlotScale_LogLog: RenderPrimitives(ShadedRenderer<Getter1,Getter2,TransformerLogLog>(getter1,getter2,TransformerLogLog(), col), DrawList, GImPlot->CurrentPlot->PlotRect); break;
            }
        }
        EndItem();
    }
}

template <typename T>
void PlotShaded(const char* label_id, const T* values, int count, double y_ref, double xscale, double x0, int offset, int stride) {
    bool fit2 = true;
    if (!(y_ref > -DBL_MAX)) { // filters out nans too
        fit2 = false;
        y_ref = GetPlotLimits(IMPLOT_AUTO,IMPLOT_AUTO).Y.Min;
    }
    if (!(y_ref < DBL_MAX)) { // filters out nans too
        fit2 = false;
        y_ref = GetPlotLimits(IMPLOT_AUTO,IMPLOT_AUTO).Y.Max;
    }
    GetterXY<GetterLin,GetterIdx<T>> getter1(GetterLin(xscale,x0),GetterIdx<T>(values,count,offset,stride),count);
    GetterXY<GetterLin,GetterRef>    getter2(GetterLin(xscale,x0),GetterRef(y_ref),count);
    PlotShadedEx(label_id, getter1, getter2, fit2);
}

template IMPLOT_API void PlotShaded<ImS8>(const char* label_id, const ImS8* values, int count, double y_ref, double xscale, double x0, int offset, int stride);
template IMPLOT_API void PlotShaded<ImU8>(const char* label_id, const ImU8* values, int count, double y_ref, double xscale, double x0, int offset, int stride);
template IMPLOT_API void PlotShaded<ImS16>(const char* label_id, const ImS16* values, int count, double y_ref, double xscale, double x0, int offset, int stride);
template IMPLOT_API void PlotShaded<ImU16>(const char* label_id, const ImU16* values, int count, double y_ref, double xscale, double x0, int offset, int stride);
template IMPLOT_API void PlotShaded<ImS32>(const char* label_id, const ImS32* values, int count, double y_ref, double xscale, double x0, int offset, int stride);
template IMPLOT_API void PlotShaded<ImU32>(const char* label_id, const ImU32* values, int count, double y_ref, double xscale, double x0, int offset, int stride);
template IMPLOT_API void PlotShaded<ImS64>(const char* label_id, const ImS64* values, int count, double y_ref, double xscale, double x0, int offset, int stride);
template IMPLOT_API void PlotShaded<ImU64>(const char* label_id, const ImU64* values, int count, double y_ref, double xscale, double x0, int offset, int stride);
template IMPLOT_API void PlotShaded<float>(const char* label_id, const float* values, int count, double y_ref, double xscale, double x0, int offset, int stride);
template IMPLOT_API void PlotShaded<double>(const char* label_id, const double* values, int count, double y_ref, double xscale, double x0, int offset, int stride);

template <typename T>
void PlotShaded(const char* label_id, const T* xs, const T* ys, int count, double y_ref, int offset, int stride) {
    bool fit2 = true;
    if (!(y_ref > -DBL_MAX)) { // filters out nans too
        fit2 = false;
        y_ref = GetPlotLimits(IMPLOT_AUTO,IMPLOT_AUTO).Y.Min;
    }
    if (!(y_ref < DBL_MAX)) { // filters out nans too
        fit2 = false;
        y_ref = GetPlotLimits(IMPLOT_AUTO,IMPLOT_AUTO).Y.Max;
    }
    GetterXY<GetterIdx<T>,GetterIdx<T>> getter1(GetterIdx<T>(xs,count,offset,stride),GetterIdx<T>(ys,count,offset,stride),count);
    GetterXY<GetterIdx<T>,GetterRef>    getter2(GetterIdx<T>(xs,count,offset,stride),GetterRef(y_ref),count);
    PlotShadedEx(label_id, getter1, getter2, fit2);
}

template IMPLOT_API void PlotShaded<ImS8>(const char* label_id, const ImS8* xs, const ImS8* ys, int count, double y_ref, int offset, int stride);
template IMPLOT_API void PlotShaded<ImU8>(const char* label_id, const ImU8* xs, const ImU8* ys, int count, double y_ref, int offset, int stride);
template IMPLOT_API void PlotShaded<ImS16>(const char* label_id, const ImS16* xs, const ImS16* ys, int count, double y_ref, int offset, int stride);
template IMPLOT_API void PlotShaded<ImU16>(const char* label_id, const ImU16* xs, const ImU16* ys, int count, double y_ref, int offset, int stride);
template IMPLOT_API void PlotShaded<ImS32>(const char* label_id, const ImS32* xs, const ImS32* ys, int count, double y_ref, int offset, int stride);
template IMPLOT_API void PlotShaded<ImU32>(const char* label_id, const ImU32* xs, const ImU32* ys, int count, double y_ref, int offset, int stride);
template IMPLOT_API void PlotShaded<ImS64>(const char* label_id, const ImS64* xs, const ImS64* ys, int count, double y_ref, int offset, int stride);
template IMPLOT_API void PlotShaded<ImU64>(const char* label_id, const ImU64* xs, const ImU64* ys, int count, double y_ref, int offset, int stride);
template IMPLOT_API void PlotShaded<float>(const char* label_id, const float* xs, const float* ys, int count, double y_ref, int offset, int stride);
template IMPLOT_API void PlotShaded<double>(const char* label_id, const double* xs, const double* ys, int count, double y_ref, int offset, int stride);

template <typename T>
void PlotShaded(const char* label_id, const T* xs, const T* ys1, const T* ys2, int count, int offset, int stride) {
    GetterXY<GetterIdx<T>,GetterIdx<T>> getter1(GetterIdx<T>(xs,count,offset,stride),GetterIdx<T>(ys1,count,offset,stride),count);
    GetterXY<GetterIdx<T>,GetterIdx<T>> getter2(GetterIdx<T>(xs,count,offset,stride),GetterIdx<T>(ys2,count,offset,stride),count);
    PlotShadedEx(label_id, getter1, getter2, true);
}

template IMPLOT_API void PlotShaded<ImS8>(const char* label_id, const ImS8* xs, const ImS8* ys1, const ImS8* ys2, int count, int offset, int stride);
template IMPLOT_API void PlotShaded<ImU8>(const char* label_id, const ImU8* xs, const ImU8* ys1, const ImU8* ys2, int count, int offset, int stride);
template IMPLOT_API void PlotShaded<ImS16>(const char* label_id, const ImS16* xs, const ImS16* ys1, const ImS16* ys2, int count, int offset, int stride);
template IMPLOT_API void PlotShaded<ImU16>(const char* label_id, const ImU16* xs, const ImU16* ys1, const ImU16* ys2, int count, int offset, int stride);
template IMPLOT_API void PlotShaded<ImS32>(const char* label_id, const ImS32* xs, const ImS32* ys1, const ImS32* ys2, int count, int offset, int stride);
template IMPLOT_API void PlotShaded<ImU32>(const char* label_id, const ImU32* xs, const ImU32* ys1, const ImU32* ys2, int count, int offset, int stride);
template IMPLOT_API void PlotShaded<ImS64>(const char* label_id, const ImS64* xs, const ImS64* ys1, const ImS64* ys2, int count, int offset, int stride);
template IMPLOT_API void PlotShaded<ImU64>(const char* label_id, const ImU64* xs, const ImU64* ys1, const ImU64* ys2, int count, int offset, int stride);
template IMPLOT_API void PlotShaded<float>(const char* label_id, const float* xs, const float* ys1, const float* ys2, int count, int offset, int stride);
template IMPLOT_API void PlotShaded<double>(const char* label_id, const double* xs, const double* ys1, const double* ys2, int count, int offset, int stride);

// custom
void PlotShadedG(const char* label_id, ImPlotGetter getter_func1, void* data1, ImPlotGetter getter_func2, void* data2, int count) {
    GetterFuncPtr getter1(getter_func1, data1, count);
    GetterFuncPtr getter2(getter_func2, data2, count);
    PlotShadedEx(label_id, getter1, getter2, true);
}

//-----------------------------------------------------------------------------
// PLOT BAR
//-----------------------------------------------------------------------------

// TODO: Migrate to RenderPrimitives

template <typename Getter1, typename Getter2>
void PlotBarsEx(const char* label_id, const Getter1& getter1, const Getter2 getter2, double width) {
    if (BeginItem(label_id, ImPlotCol_Fill)) {
        const double half_width = width / 2;
        if (FitThisFrame()) {
            for (int i = 0; i < getter1.Count; ++i) {
                ImPlotPoint p1 = getter1(i);
                ImPlotPoint p2 = getter2(i);
                FitPoint(ImPlotPoint(p1.x - half_width, p1.y));
                FitPoint(ImPlotPoint(p2.x + half_width, p2.y));
            }
        }
        const ImPlotNextItemData& s = GetItemData();
        ImDrawList& DrawList = *GetPlotDrawList();
        ImU32 col_line  = ImGui::GetColorU32(s.Colors[ImPlotCol_Line]);
        ImU32 col_fill  = ImGui::GetColorU32(s.Colors[ImPlotCol_Fill]);
        bool  rend_line = s.RenderLine;
        if (s.RenderFill && col_line == col_fill)
            rend_line = false;
        for (int i = 0; i < getter1.Count; ++i) {
            ImPlotPoint p1 = getter1(i);
            ImPlotPoint p2 = getter2(i);
            if (p1.y == p2.y)
                continue;
            ImVec2 a = PlotToPixels(p1.x - half_width, p1.y,IMPLOT_AUTO,IMPLOT_AUTO);
            ImVec2 b = PlotToPixels(p2.x + half_width, p2.y,IMPLOT_AUTO,IMPLOT_AUTO);
            float width_px = ImAbs(a.x-b.x);
            if (width_px < 1.0f) {
                a.x += a.x > b.x ? (1-width_px) / 2 : (width_px-1) / 2;
                b.x += b.x > a.x ? (1-width_px) / 2 : (width_px-1) / 2;
            }
            // a.x = IM_ROUND(a.x);
            // b.x = IM_ROUND(b.x);
            if (s.RenderFill)
                DrawList.AddRectFilled(a, b, col_fill);
            if (rend_line)
                DrawList.AddRect(a, b, col_line, 0, ImDrawFlags_RoundCornersAll, s.LineWeight);
        }
        EndItem();
    }
}

template <typename T>
void PlotBars(const char* label_id, const T* values, int count, double width, double shift, int offset, int stride) {
    GetterXY<GetterLin,GetterIdx<T>> getter1(GetterLin(1.0,shift),GetterIdx<T>(values,count,offset,stride),count);
    GetterXY<GetterLin,GetterRef>    getter2(GetterLin(1.0,shift),GetterRef(0),count);
    PlotBarsEx(label_id, getter1, getter2, width);
}

template IMPLOT_API void PlotBars<ImS8>(const char* label_id, const ImS8* values, int count, double width, double shift, int offset, int stride);
template IMPLOT_API void PlotBars<ImU8>(const char* label_id, const ImU8* values, int count, double width, double shift, int offset, int stride);
template IMPLOT_API void PlotBars<ImS16>(const char* label_id, const ImS16* values, int count, double width, double shift, int offset, int stride);
template IMPLOT_API void PlotBars<ImU16>(const char* label_id, const ImU16* values, int count, double width, double shift, int offset, int stride);
template IMPLOT_API void PlotBars<ImS32>(const char* label_id, const ImS32* values, int count, double width, double shift, int offset, int stride);
template IMPLOT_API void PlotBars<ImU32>(const char* label_id, const ImU32* values, int count, double width, double shift, int offset, int stride);
template IMPLOT_API void PlotBars<ImS64>(const char* label_id, const ImS64* values, int count, double width, double shift, int offset, int stride);
template IMPLOT_API void PlotBars<ImU64>(const char* label_id, const ImU64* values, int count, double width, double shift, int offset, int stride);
template IMPLOT_API void PlotBars<float>(const char* label_id, const float* values, int count, double width, double shift, int offset, int stride);
template IMPLOT_API void PlotBars<double>(const char* label_id, const double* values, int count, double width, double shift, int offset, int stride);

template <typename T>
void PlotBars(const char* label_id, const T* xs, const T* ys, int count, double width, int offset, int stride) {
    GetterXY<GetterIdx<T>,GetterIdx<T>> getter1(GetterIdx<T>(xs,count,offset,stride),GetterIdx<T>(ys,count,offset,stride),count);
    GetterXY<GetterIdx<T>,GetterRef>    getter2(GetterIdx<T>(xs,count,offset,stride),GetterRef(0),count);
    PlotBarsEx(label_id, getter1, getter2, width);
}

template IMPLOT_API void PlotBars<ImS8>(const char* label_id, const ImS8* xs, const ImS8* ys, int count, double width, int offset, int stride);
template IMPLOT_API void PlotBars<ImU8>(const char* label_id, const ImU8* xs, const ImU8* ys, int count, double width, int offset, int stride);
template IMPLOT_API void PlotBars<ImS16>(const char* label_id, const ImS16* xs, const ImS16* ys, int count, double width, int offset, int stride);
template IMPLOT_API void PlotBars<ImU16>(const char* label_id, const ImU16* xs, const ImU16* ys, int count, double width, int offset, int stride);
template IMPLOT_API void PlotBars<ImS32>(const char* label_id, const ImS32* xs, const ImS32* ys, int count, double width, int offset, int stride);
template IMPLOT_API void PlotBars<ImU32>(const char* label_id, const ImU32* xs, const ImU32* ys, int count, double width, int offset, int stride);
template IMPLOT_API void PlotBars<ImS64>(const char* label_id, const ImS64* xs, const ImS64* ys, int count, double width, int offset, int stride);
template IMPLOT_API void PlotBars<ImU64>(const char* label_id, const ImU64* xs, const ImU64* ys, int count, double width, int offset, int stride);
template IMPLOT_API void PlotBars<float>(const char* label_id, const float* xs, const float* ys, int count, double width, int offset, int stride);
template IMPLOT_API void PlotBars<double>(const char* label_id, const double* xs, const double* ys, int count, double width, int offset, int stride);

// custom
void PlotBarsG(const char* label_id, ImPlotGetter getter_func, void* data, int count, double width) {
    GetterFuncPtr getter1(getter_func, data, count);
    GetterOverrideY<GetterFuncPtr> getter2(getter1,0);
    PlotBarsEx(label_id, getter1, getter2, width);
}

//-----------------------------------------------------------------------------
// PLOT BAR H
//-----------------------------------------------------------------------------

// TODO: Migrate to RenderPrimitives

template <typename Getter1, typename Getter2>
void PlotBarsHEx(const char* label_id, const Getter1& getter1, const Getter2& getter2, double height) {
    if (BeginItem(label_id, ImPlotCol_Fill)) {
        const double half_height = height / 2;
        if (FitThisFrame()) {
            for (int i = 0; i < getter1.Count; ++i) {
                ImPlotPoint p1 = getter1(i);
                ImPlotPoint p2 = getter2(i);
                FitPoint(ImPlotPoint(p1.x, p1.y - half_height));
                FitPoint(ImPlotPoint(p2.x, p2.y + half_height));
            }
        }
        const ImPlotNextItemData& s = GetItemData();
        ImDrawList& DrawList = *GetPlotDrawList();
        ImU32 col_line  = ImGui::GetColorU32(s.Colors[ImPlotCol_Line]);
        ImU32 col_fill  = ImGui::GetColorU32(s.Colors[ImPlotCol_Fill]);
        bool  rend_line = s.RenderLine;
        if (s.RenderFill && col_line == col_fill)
            rend_line = false;
        for (int i = 0; i < getter1.Count; ++i) {
            ImPlotPoint p1 = getter1(i);
            ImPlotPoint p2 = getter2(i);
            if (p1.x == p2.x)
                continue;
            ImVec2 a = PlotToPixels(p1.x, p1.y - half_height,IMPLOT_AUTO,IMPLOT_AUTO);
            ImVec2 b = PlotToPixels(p2.x, p2.y + half_height,IMPLOT_AUTO,IMPLOT_AUTO);
            if (s.RenderFill)
                DrawList.AddRectFilled(a, b, col_fill);
            if (rend_line)
                DrawList.AddRect(a, b, col_line, 0, ImDrawFlags_RoundCornersAll, s.LineWeight);
        }
        EndItem();
    }
}

template <typename T>
void PlotBarsH(const char* label_id, const T* values, int count, double height, double shift, int offset, int stride) {
    GetterXY<GetterIdx<T>,GetterLin> getter1(GetterIdx<T>(values,count,offset,stride),GetterLin(1.0,shift),count);
    GetterXY<GetterRef,GetterLin>    getter2(GetterRef(0),GetterLin(1.0,shift),count);
    PlotBarsHEx(label_id, getter1, getter2, height);
}

template IMPLOT_API void PlotBarsH<ImS8>(const char* label_id, const ImS8* values, int count, double height, double shift, int offset, int stride);
template IMPLOT_API void PlotBarsH<ImU8>(const char* label_id, const ImU8* values, int count, double height, double shift, int offset, int stride);
template IMPLOT_API void PlotBarsH<ImS16>(const char* label_id, const ImS16* values, int count, double height, double shift, int offset, int stride);
template IMPLOT_API void PlotBarsH<ImU16>(const char* label_id, const ImU16* values, int count, double height, double shift, int offset, int stride);
template IMPLOT_API void PlotBarsH<ImS32>(const char* label_id, const ImS32* values, int count, double height, double shift, int offset, int stride);
template IMPLOT_API void PlotBarsH<ImU32>(const char* label_id, const ImU32* values, int count, double height, double shift, int offset, int stride);
template IMPLOT_API void PlotBarsH<ImS64>(const char* label_id, const ImS64* values, int count, double height, double shift, int offset, int stride);
template IMPLOT_API void PlotBarsH<ImU64>(const char* label_id, const ImU64* values, int count, double height, double shift, int offset, int stride);
template IMPLOT_API void PlotBarsH<float>(const char* label_id, const float* values, int count, double height, double shift, int offset, int stride);
template IMPLOT_API void PlotBarsH<double>(const char* label_id, const double* values, int count, double height, double shift, int offset, int stride);

template <typename T>
void PlotBarsH(const char* label_id, const T* xs, const T* ys, int count, double height, int offset, int stride) {
    GetterXY<GetterIdx<T>,GetterIdx<T>> getter1(GetterIdx<T>(xs,count,offset,stride),GetterIdx<T>(ys,count,offset,stride),count);
    GetterXY<GetterRef,   GetterIdx<T>> getter2(GetterRef(0),GetterIdx<T>(ys,count,offset,stride),count);
    PlotBarsHEx(label_id, getter1, getter2, height);
}

template IMPLOT_API void PlotBarsH<ImS8>(const char* label_id, const ImS8* xs, const ImS8* ys, int count, double height, int offset, int stride);
template IMPLOT_API void PlotBarsH<ImU8>(const char* label_id, const ImU8* xs, const ImU8* ys, int count, double height, int offset, int stride);
template IMPLOT_API void PlotBarsH<ImS16>(const char* label_id, const ImS16* xs, const ImS16* ys, int count, double height, int offset, int stride);
template IMPLOT_API void PlotBarsH<ImU16>(const char* label_id, const ImU16* xs, const ImU16* ys, int count, double height, int offset, int stride);
template IMPLOT_API void PlotBarsH<ImS32>(const char* label_id, const ImS32* xs, const ImS32* ys, int count, double height, int offset, int stride);
template IMPLOT_API void PlotBarsH<ImU32>(const char* label_id, const ImU32* xs, const ImU32* ys, int count, double height, int offset, int stride);
template IMPLOT_API void PlotBarsH<ImS64>(const char* label_id, const ImS64* xs, const ImS64* ys, int count, double height, int offset, int stride);
template IMPLOT_API void PlotBarsH<ImU64>(const char* label_id, const ImU64* xs, const ImU64* ys, int count, double height, int offset, int stride);
template IMPLOT_API void PlotBarsH<float>(const char* label_id, const float* xs, const float* ys, int count, double height, int offset, int stride);
template IMPLOT_API void PlotBarsH<double>(const char* label_id, const double* xs, const double* ys, int count, double height, int offset, int stride);

// custom
void PlotBarsHG(const char* label_id, ImPlotGetter getter_func, void* data, int count, double height) {
    GetterFuncPtr getter1(getter_func, data, count);
    GetterOverrideX<GetterFuncPtr> getter2(getter1,0);
    PlotBarsHEx(label_id, getter1, getter2, height);
}

//-----------------------------------------------------------------------------
// PLOT BAR GROUPS
//-----------------------------------------------------------------------------

template <typename T>
void PlotBarGroups(const char* const label_ids[], const T* values, int items, int groups, double width, double shift, ImPlotBarGroupsFlags flags) {
    if (ImHasFlag(flags, ImPlotBarGroupsFlags_Stacked)) {
        SetupLock();
        GImPlot->TempDouble1.resize(4*groups);
        double* temp = GImPlot->TempDouble1.Data;
        double* neg =      &temp[0];
        double* pos =      &temp[groups];
        double* curr_min = &temp[groups*2];
        double* curr_max = &temp[groups*3];
        for (int g = 0; g < groups*2; ++g)
            temp[g] = 0;
        for (int i = 0; i < items; ++i) {
            if (!IsItemHidden(label_ids[i])) {
                for (int g = 0; g < groups; ++g) {
                    double v = (double)values[i*groups+g];
                    if (v > 0) {
                        curr_min[g] = pos[g];
                        curr_max[g] = curr_min[g] + v;
                        pos[g]      += v;
                    }
                    else {
                        curr_max[g] = neg[g];
                        curr_min[g] = curr_max[g] + v;
                        neg[g]      += v;
                    }
                }
            }
            GetterXY<GetterLin,GetterIdx<double>> getter1(GetterLin(1.0,shift),GetterIdx<double>(curr_min,groups),groups);
            GetterXY<GetterLin,GetterIdx<double>> getter2(GetterLin(1.0,shift),GetterIdx<double>(curr_max,groups),groups);
            PlotBarsEx(label_ids[i],getter1,getter2,width);
        }
    }
    else {
        const double subwidth = width / items;
        for (int i = 0; i < items; ++i) {
            const double subshift = (i+0.5)*subwidth - width/2;
            PlotBars(label_ids[i],&values[i*groups],groups,subwidth,subshift+shift);
        }
    }
}

template IMPLOT_API void PlotBarGroups<ImS8>(const char* const label_ids[], const ImS8* values, int items, int groups, double width, double shift, ImPlotBarGroupsFlags flags);
template IMPLOT_API void PlotBarGroups<ImU8>(const char* const label_ids[], const ImU8* values, int items, int groups, double width, double shift, ImPlotBarGroupsFlags flags);
template IMPLOT_API void PlotBarGroups<ImS16>(const char* const label_ids[], const ImS16* values, int items, int groups, double width, double shift, ImPlotBarGroupsFlags flags);
template IMPLOT_API void PlotBarGroups<ImU16>(const char* const label_ids[], const ImU16* values, int items, int groups, double width, double shift, ImPlotBarGroupsFlags flags);
template IMPLOT_API void PlotBarGroups<ImS32>(const char* const label_ids[], const ImS32* values, int items, int groups, double width, double shift, ImPlotBarGroupsFlags flags);
template IMPLOT_API void PlotBarGroups<ImU32>(const char* const label_ids[], const ImU32* values, int items, int groups, double width, double shift, ImPlotBarGroupsFlags flags);
template IMPLOT_API void PlotBarGroups<ImS64>(const char* const label_ids[], const ImS64* values, int items, int groups, double width, double shift, ImPlotBarGroupsFlags flags);
template IMPLOT_API void PlotBarGroups<ImU64>(const char* const label_ids[], const ImU64* values, int items, int groups, double width, double shift, ImPlotBarGroupsFlags flags);
template IMPLOT_API void PlotBarGroups<float>(const char* const label_ids[], const float* values, int items, int groups, double width, double shift, ImPlotBarGroupsFlags flags);
template IMPLOT_API void PlotBarGroups<double>(const char* const label_ids[], const double* values, int items, int groups, double width, double shift, ImPlotBarGroupsFlags flags);

template <typename T>
void PlotBarGroupsH(const char* const label_ids[], const T* values, int items, int groups, double height, double shift, ImPlotBarGroupsFlags flags) {
    if (ImHasFlag(flags, ImPlotBarGroupsFlags_Stacked)) {
        SetupLock();
        GImPlot->TempDouble1.resize(4*groups);
        double* temp = GImPlot->TempDouble1.Data;
        double* neg =      &temp[0];
        double* pos =      &temp[groups];
        double* curr_min = &temp[groups*2];
        double* curr_max = &temp[groups*3];
        for (int g = 0; g < groups*2; ++g)
            temp[g] = 0;
        for (int i = 0; i < items; ++i) {
            if (!IsItemHidden(label_ids[i])) {
                for (int g = 0; g < groups; ++g) {
                    double v = (double)values[i*groups+g];
                    if (v > 0) {
                        curr_min[g] = pos[g];
                        curr_max[g] = curr_min[g] + v;
                        pos[g]      += v;
                    }
                    else {
                        curr_max[g] = neg[g];
                        curr_min[g] = curr_max[g] + v;
                        neg[g]      += v;
                    }
                }
            }
            GetterXY<GetterIdx<double>,GetterLin> getter1(GetterIdx<double>(curr_min,groups),GetterLin(1.0,shift),groups);
            GetterXY<GetterIdx<double>,GetterLin> getter2(GetterIdx<double>(curr_max,groups),GetterLin(1.0,shift),groups);
            PlotBarsHEx(label_ids[i],getter1,getter2,height);
        }
    }
    else {
        const double subheight = height / items;
        for (int i = 0; i < items; ++i) {
            const double subshift = (i+0.5)*subheight - height/2;
            PlotBarsH(label_ids[i],&values[i*groups],groups,subheight,subshift+shift);
        }
    }
}

template IMPLOT_API void PlotBarGroupsH<ImS8>(const char* const label_ids[], const ImS8* values, int items, int groups, double width, double shift, ImPlotBarGroupsFlags flags);
template IMPLOT_API void PlotBarGroupsH<ImU8>(const char* const label_ids[], const ImU8* values, int items, int groups, double width, double shift, ImPlotBarGroupsFlags flags);
template IMPLOT_API void PlotBarGroupsH<ImS16>(const char* const label_ids[], const ImS16* values, int items, int groups, double width, double shift, ImPlotBarGroupsFlags flags);
template IMPLOT_API void PlotBarGroupsH<ImU16>(const char* const label_ids[], const ImU16* values, int items, int groups, double width, double shift, ImPlotBarGroupsFlags flags);
template IMPLOT_API void PlotBarGroupsH<ImS32>(const char* const label_ids[], const ImS32* values, int items, int groups, double width, double shift, ImPlotBarGroupsFlags flags);
template IMPLOT_API void PlotBarGroupsH<ImU32>(const char* const label_ids[], const ImU32* values, int items, int groups, double width, double shift, ImPlotBarGroupsFlags flags);
template IMPLOT_API void PlotBarGroupsH<ImS64>(const char* const label_ids[], const ImS64* values, int items, int groups, double width, double shift, ImPlotBarGroupsFlags flags);
template IMPLOT_API void PlotBarGroupsH<ImU64>(const char* const label_ids[], const ImU64* values, int items, int groups, double width, double shift, ImPlotBarGroupsFlags flags);
template IMPLOT_API void PlotBarGroupsH<float>(const char* const label_ids[], const float* values, int items, int groups, double width, double shift, ImPlotBarGroupsFlags flags);
template IMPLOT_API void PlotBarGroupsH<double>(const char* const label_ids[], const double* values, int items, int groups, double width, double shift, ImPlotBarGroupsFlags flags);

//-----------------------------------------------------------------------------
// PLOT ERROR BARS
//-----------------------------------------------------------------------------

template <typename Getter>
void PlotErrorBarsEx(const char* label_id, const Getter& getter) {
    if (BeginItem(label_id)) {
        if (FitThisFrame()) {
            for (int i = 0; i < getter.Count; ++i) {
                ImPlotPointError e = getter(i);
                FitPoint(ImPlotPoint(e.X , e.Y - e.Neg));
                FitPoint(ImPlotPoint(e.X , e.Y + e.Pos ));
            }
        }
        const ImPlotNextItemData& s = GetItemData();
        ImDrawList& DrawList = *GetPlotDrawList();
        const ImU32 col = ImGui::GetColorU32(s.Colors[ImPlotCol_ErrorBar]);
        const bool rend_whisker  = s.ErrorBarSize > 0;
        const float half_whisker = s.ErrorBarSize * 0.5f;
        for (int i = 0; i < getter.Count; ++i) {
            ImPlotPointError e = getter(i);
            ImVec2 p1 = PlotToPixels(e.X, e.Y - e.Neg,IMPLOT_AUTO,IMPLOT_AUTO);
            ImVec2 p2 = PlotToPixels(e.X, e.Y + e.Pos,IMPLOT_AUTO,IMPLOT_AUTO);
            DrawList.AddLine(p1,p2,col, s.ErrorBarWeight);
            if (rend_whisker) {
                DrawList.AddLine(p1 - ImVec2(half_whisker, 0), p1 + ImVec2(half_whisker, 0), col, s.ErrorBarWeight);
                DrawList.AddLine(p2 - ImVec2(half_whisker, 0), p2 + ImVec2(half_whisker, 0), col, s.ErrorBarWeight);
            }
        }
        EndItem();
    }
}

template <typename T>
void PlotErrorBars(const char* label_id, const T* xs, const T* ys, const T* err, int count, int offset, int stride) {
    GetterError<T> getter(xs, ys, err, err, count, offset, stride);
    PlotErrorBarsEx(label_id, getter);
}

template IMPLOT_API void PlotErrorBars<ImS8>(const char* label_id, const ImS8* xs, const ImS8* ys, const ImS8* err, int count, int offset, int stride);
template IMPLOT_API void PlotErrorBars<ImU8>(const char* label_id, const ImU8* xs, const ImU8* ys, const ImU8* err, int count, int offset, int stride);
template IMPLOT_API void PlotErrorBars<ImS16>(const char* label_id, const ImS16* xs, const ImS16* ys, const ImS16* err, int count, int offset, int stride);
template IMPLOT_API void PlotErrorBars<ImU16>(const char* label_id, const ImU16* xs, const ImU16* ys, const ImU16* err, int count, int offset, int stride);
template IMPLOT_API void PlotErrorBars<ImS32>(const char* label_id, const ImS32* xs, const ImS32* ys, const ImS32* err, int count, int offset, int stride);
template IMPLOT_API void PlotErrorBars<ImU32>(const char* label_id, const ImU32* xs, const ImU32* ys, const ImU32* err, int count, int offset, int stride);
template IMPLOT_API void PlotErrorBars<ImS64>(const char* label_id, const ImS64* xs, const ImS64* ys, const ImS64* err, int count, int offset, int stride);
template IMPLOT_API void PlotErrorBars<ImU64>(const char* label_id, const ImU64* xs, const ImU64* ys, const ImU64* err, int count, int offset, int stride);
template IMPLOT_API void PlotErrorBars<float>(const char* label_id, const float* xs, const float* ys, const float* err, int count, int offset, int stride);
template IMPLOT_API void PlotErrorBars<double>(const char* label_id, const double* xs, const double* ys, const double* err, int count, int offset, int stride);

template <typename T>
void PlotErrorBars(const char* label_id, const T* xs, const T* ys, const T* neg, const T* pos, int count, int offset, int stride) {
    GetterError<T> getter(xs, ys, neg, pos, count, offset, stride);
    PlotErrorBarsEx(label_id, getter);
}

template IMPLOT_API void PlotErrorBars<ImS8>(const char* label_id, const ImS8* xs, const ImS8* ys, const ImS8* neg, const ImS8* pos, int count, int offset, int stride);
template IMPLOT_API void PlotErrorBars<ImU8>(const char* label_id, const ImU8* xs, const ImU8* ys, const ImU8* neg, const ImU8* pos, int count, int offset, int stride);
template IMPLOT_API void PlotErrorBars<ImS16>(const char* label_id, const ImS16* xs, const ImS16* ys, const ImS16* neg, const ImS16* pos, int count, int offset, int stride);
template IMPLOT_API void PlotErrorBars<ImU16>(const char* label_id, const ImU16* xs, const ImU16* ys, const ImU16* neg, const ImU16* pos, int count, int offset, int stride);
template IMPLOT_API void PlotErrorBars<ImS32>(const char* label_id, const ImS32* xs, const ImS32* ys, const ImS32* neg, const ImS32* pos, int count, int offset, int stride);
template IMPLOT_API void PlotErrorBars<ImU32>(const char* label_id, const ImU32* xs, const ImU32* ys, const ImU32* neg, const ImU32* pos, int count, int offset, int stride);
template IMPLOT_API void PlotErrorBars<ImS64>(const char* label_id, const ImS64* xs, const ImS64* ys, const ImS64* neg, const ImS64* pos, int count, int offset, int stride);
template IMPLOT_API void PlotErrorBars<ImU64>(const char* label_id, const ImU64* xs, const ImU64* ys, const ImU64* neg, const ImU64* pos, int count, int offset, int stride);
template IMPLOT_API void PlotErrorBars<float>(const char* label_id, const float* xs, const float* ys, const float* neg, const float* pos, int count, int offset, int stride);
template IMPLOT_API void PlotErrorBars<double>(const char* label_id, const double* xs, const double* ys, const double* neg, const double* pos, int count, int offset, int stride);

//-----------------------------------------------------------------------------
// PLOT ERROR BARS H
//-----------------------------------------------------------------------------

template <typename Getter>
void PlotErrorBarsHEx(const char* label_id, const Getter& getter) {
    if (BeginItem(label_id)) {
        if (FitThisFrame()) {
            for (int i = 0; i < getter.Count; ++i) {
                ImPlotPointError e = getter(i);
                FitPoint(ImPlotPoint(e.X - e.Neg, e.Y));
                FitPoint(ImPlotPoint(e.X + e.Pos, e.Y));
            }
        }
        const ImPlotNextItemData& s = GetItemData();
        ImDrawList& DrawList = *GetPlotDrawList();
        const ImU32 col = ImGui::GetColorU32(s.Colors[ImPlotCol_ErrorBar]);
        const bool rend_whisker  = s.ErrorBarSize > 0;
        const float half_whisker = s.ErrorBarSize * 0.5f;
        for (int i = 0; i < getter.Count; ++i) {
            ImPlotPointError e = getter(i);
            ImVec2 p1 = PlotToPixels(e.X - e.Neg, e.Y,IMPLOT_AUTO,IMPLOT_AUTO);
            ImVec2 p2 = PlotToPixels(e.X + e.Pos, e.Y,IMPLOT_AUTO,IMPLOT_AUTO);
            DrawList.AddLine(p1, p2, col, s.ErrorBarWeight);
            if (rend_whisker) {
                DrawList.AddLine(p1 - ImVec2(0, half_whisker), p1 + ImVec2(0, half_whisker), col, s.ErrorBarWeight);
                DrawList.AddLine(p2 - ImVec2(0, half_whisker), p2 + ImVec2(0, half_whisker), col, s.ErrorBarWeight);
            }
        }
        EndItem();
    }
}

template <typename T>
void PlotErrorBarsH(const char* label_id, const T* xs, const T* ys, const T* err, int count, int offset, int stride) {
    GetterError<T> getter(xs, ys, err, err, count, offset, stride);
    PlotErrorBarsHEx(label_id, getter);
}

template IMPLOT_API void PlotErrorBarsH<ImS8>(const char* label_id, const ImS8* xs, const ImS8* ys, const ImS8* err, int count, int offset, int stride);
template IMPLOT_API void PlotErrorBarsH<ImU8>(const char* label_id, const ImU8* xs, const ImU8* ys, const ImU8* err, int count, int offset, int stride);
template IMPLOT_API void PlotErrorBarsH<ImS16>(const char* label_id, const ImS16* xs, const ImS16* ys, const ImS16* err, int count, int offset, int stride);
template IMPLOT_API void PlotErrorBarsH<ImU16>(const char* label_id, const ImU16* xs, const ImU16* ys, const ImU16* err, int count, int offset, int stride);
template IMPLOT_API void PlotErrorBarsH<ImS32>(const char* label_id, const ImS32* xs, const ImS32* ys, const ImS32* err, int count, int offset, int stride);
template IMPLOT_API void PlotErrorBarsH<ImU32>(const char* label_id, const ImU32* xs, const ImU32* ys, const ImU32* err, int count, int offset, int stride);
template IMPLOT_API void PlotErrorBarsH<ImS64>(const char* label_id, const ImS64* xs, const ImS64* ys, const ImS64* err, int count, int offset, int stride);
template IMPLOT_API void PlotErrorBarsH<ImU64>(const char* label_id, const ImU64* xs, const ImU64* ys, const ImU64* err, int count, int offset, int stride);
template IMPLOT_API void PlotErrorBarsH<float>(const char* label_id, const float* xs, const float* ys, const float* err, int count, int offset, int stride);
template IMPLOT_API void PlotErrorBarsH<double>(const char* label_id, const double* xs, const double* ys, const double* err, int count, int offset, int stride);

template <typename T>
void PlotErrorBarsH(const char* label_id, const T* xs, const T* ys, const T* neg, const T* pos, int count, int offset, int stride) {
    GetterError<T> getter(xs, ys, neg, pos, count, offset, stride);
    PlotErrorBarsHEx(label_id, getter);
}

template IMPLOT_API void PlotErrorBarsH<ImS8>(const char* label_id, const ImS8* xs, const ImS8* ys, const ImS8* neg, const ImS8* pos, int count, int offset, int stride);
template IMPLOT_API void PlotErrorBarsH<ImU8>(const char* label_id, const ImU8* xs, const ImU8* ys, const ImU8* neg, const ImU8* pos, int count, int offset, int stride);
template IMPLOT_API void PlotErrorBarsH<ImS16>(const char* label_id, const ImS16* xs, const ImS16* ys, const ImS16* neg, const ImS16* pos, int count, int offset, int stride);
template IMPLOT_API void PlotErrorBarsH<ImU16>(const char* label_id, const ImU16* xs, const ImU16* ys, const ImU16* neg, const ImU16* pos, int count, int offset, int stride);
template IMPLOT_API void PlotErrorBarsH<ImS32>(const char* label_id, const ImS32* xs, const ImS32* ys, const ImS32* neg, const ImS32* pos, int count, int offset, int stride);
template IMPLOT_API void PlotErrorBarsH<ImU32>(const char* label_id, const ImU32* xs, const ImU32* ys, const ImU32* neg, const ImU32* pos, int count, int offset, int stride);
template IMPLOT_API void PlotErrorBarsH<ImS64>(const char* label_id, const ImS64* xs, const ImS64* ys, const ImS64* neg, const ImS64* pos, int count, int offset, int stride);
template IMPLOT_API void PlotErrorBarsH<ImU64>(const char* label_id, const ImU64* xs, const ImU64* ys, const ImU64* neg, const ImU64* pos, int count, int offset, int stride);
template IMPLOT_API void PlotErrorBarsH<float>(const char* label_id, const float* xs, const float* ys, const float* neg, const float* pos, int count, int offset, int stride);
template IMPLOT_API void PlotErrorBarsH<double>(const char* label_id, const double* xs, const double* ys, const double* neg, const double* pos, int count, int offset, int stride);

//-----------------------------------------------------------------------------
// PLOT STEMS
//-----------------------------------------------------------------------------

template <typename GetterM, typename GetterB>
IMPLOT_INLINE void PlotStemsEx(const char* label_id, const GetterM& get_mark, const GetterB& get_base) {
    if (BeginItem(label_id, ImPlotCol_Line)) {
        if (FitThisFrame()) {
            for (int i = 0; i < get_base.Count; ++i) {
                FitPoint(get_mark(i));
                FitPoint(get_base(i));
            }
        }
        const ImPlotNextItemData& s = GetItemData();
        ImDrawList& DrawList = *GetPlotDrawList();
        // render stems
        if (s.RenderLine) {
            const ImU32 col_line = ImGui::GetColorU32(s.Colors[ImPlotCol_Line]);
            switch (GetCurrentScale()) {
                case ImPlotScale_LinLin: RenderLineSegments(get_mark, get_base, TransformerLinLin(), DrawList, s.LineWeight, col_line); break;
                case ImPlotScale_LogLin: RenderLineSegments(get_mark, get_base, TransformerLogLin(), DrawList, s.LineWeight, col_line); break;
                case ImPlotScale_LinLog: RenderLineSegments(get_mark, get_base, TransformerLinLog(), DrawList, s.LineWeight, col_line); break;
                case ImPlotScale_LogLog: RenderLineSegments(get_mark, get_base, TransformerLogLog(), DrawList, s.LineWeight, col_line); break;
            }
        }
        // render markers
        ImPlotMarker marker = s.Marker == ImPlotMarker_None ? ImPlotMarker_Circle : s.Marker;
        if (marker != ImPlotMarker_None) {
            PopPlotClipRect();
            PushPlotClipRect(s.MarkerSize);
            const ImU32 col_line = ImGui::GetColorU32(s.Colors[ImPlotCol_MarkerOutline]);
            const ImU32 col_fill = ImGui::GetColorU32(s.Colors[ImPlotCol_MarkerFill]);
            switch (GetCurrentScale()) {
                case ImPlotScale_LinLin: RenderMarkers(get_mark, TransformerLinLin(), DrawList, marker, s.MarkerSize, s.RenderMarkerLine, col_line, s.MarkerWeight, s.RenderMarkerFill, col_fill); break;
                case ImPlotScale_LogLin: RenderMarkers(get_mark, TransformerLogLin(), DrawList, marker, s.MarkerSize, s.RenderMarkerLine, col_line, s.MarkerWeight, s.RenderMarkerFill, col_fill); break;
                case ImPlotScale_LinLog: RenderMarkers(get_mark, TransformerLinLog(), DrawList, marker, s.MarkerSize, s.RenderMarkerLine, col_line, s.MarkerWeight, s.RenderMarkerFill, col_fill); break;
                case ImPlotScale_LogLog: RenderMarkers(get_mark, TransformerLogLog(), DrawList, marker, s.MarkerSize, s.RenderMarkerLine, col_line, s.MarkerWeight, s.RenderMarkerFill, col_fill); break;
            }
        }
        EndItem();
    }
}

template <typename T>
void PlotStems(const char* label_id, const T* values, int count, double y_ref, double xscale, double x0, int offset, int stride) {
    GetterXY<GetterLin,GetterIdx<T>> get_mark(GetterLin(xscale,x0),GetterIdx<T>(values,count,offset,stride),count);
    GetterXY<GetterLin,GetterRef>    get_base(GetterLin(xscale,x0),GetterRef(y_ref),count);
    PlotStemsEx(label_id, get_mark, get_base);
}

template IMPLOT_API void PlotStems<ImS8>(const char* label_id, const ImS8* values, int count, double y_ref, double xscale, double x0, int offset, int stride);
template IMPLOT_API void PlotStems<ImU8>(const char* label_id, const ImU8* values, int count, double y_ref, double xscale, double x0, int offset, int stride);
template IMPLOT_API void PlotStems<ImS16>(const char* label_id, const ImS16* values, int count, double y_ref, double xscale, double x0, int offset, int stride);
template IMPLOT_API void PlotStems<ImU16>(const char* label_id, const ImU16* values, int count, double y_ref, double xscale, double x0, int offset, int stride);
template IMPLOT_API void PlotStems<ImS32>(const char* label_id, const ImS32* values, int count, double y_ref, double xscale, double x0, int offset, int stride);
template IMPLOT_API void PlotStems<ImU32>(const char* label_id, const ImU32* values, int count, double y_ref, double xscale, double x0, int offset, int stride);
template IMPLOT_API void PlotStems<ImS64>(const char* label_id, const ImS64* values, int count, double y_ref, double xscale, double x0, int offset, int stride);
template IMPLOT_API void PlotStems<ImU64>(const char* label_id, const ImU64* values, int count, double y_ref, double xscale, double x0, int offset, int stride);
template IMPLOT_API void PlotStems<float>(const char* label_id, const float* values, int count, double y_ref, double xscale, double x0, int offset, int stride);
template IMPLOT_API void PlotStems<double>(const char* label_id, const double* values, int count, double y_ref, double xscale, double x0, int offset, int stride);

template <typename T>
void PlotStems(const char* label_id, const T* xs, const T* ys, int count, double y_ref, int offset, int stride) {
    GetterXY<GetterIdx<T>,GetterIdx<T>> get_mark(GetterIdx<T>(xs,count,offset,stride),GetterIdx<T>(ys,count,offset,stride),count);
    GetterXY<GetterIdx<T>,GetterRef>    get_base(GetterIdx<T>(xs,count,offset,stride),GetterRef(y_ref),count);
    PlotStemsEx(label_id, get_mark, get_base);
}

template IMPLOT_API void PlotStems<ImS8>(const char* label_id, const ImS8* xs, const ImS8* ys, int count, double y_ref, int offset, int stride);
template IMPLOT_API void PlotStems<ImU8>(const char* label_id, const ImU8* xs, const ImU8* ys, int count, double y_ref, int offset, int stride);
template IMPLOT_API void PlotStems<ImS16>(const char* label_id, const ImS16* xs, const ImS16* ys, int count, double y_ref, int offset, int stride);
template IMPLOT_API void PlotStems<ImU16>(const char* label_id, const ImU16* xs, const ImU16* ys, int count, double y_ref, int offset, int stride);
template IMPLOT_API void PlotStems<ImS32>(const char* label_id, const ImS32* xs, const ImS32* ys, int count, double y_ref, int offset, int stride);
template IMPLOT_API void PlotStems<ImU32>(const char* label_id, const ImU32* xs, const ImU32* ys, int count, double y_ref, int offset, int stride);
template IMPLOT_API void PlotStems<ImS64>(const char* label_id, const ImS64* xs, const ImS64* ys, int count, double y_ref, int offset, int stride);
template IMPLOT_API void PlotStems<ImU64>(const char* label_id, const ImU64* xs, const ImU64* ys, int count, double y_ref, int offset, int stride);
template IMPLOT_API void PlotStems<float>(const char* label_id, const float* xs, const float* ys, int count, double y_ref, int offset, int stride);
template IMPLOT_API void PlotStems<double>(const char* label_id, const double* xs, const double* ys, int count, double y_ref, int offset, int stride);

//-----------------------------------------------------------------------------
// INFINITE LINES
//-----------------------------------------------------------------------------

template <typename T>
void PlotVLines(const char* label_id, const T* xs, int count, int offset, int stride) {
    if (BeginItem(label_id, ImPlotCol_Line)) {
        const ImPlotRect lims = GetPlotLimits(IMPLOT_AUTO,IMPLOT_AUTO);
        GetterXY<GetterIdx<T>,GetterRef> get_min(GetterIdx<T>(xs,count,offset,stride),GetterRef(lims.Y.Min),count);
        GetterXY<GetterIdx<T>,GetterRef> get_max(GetterIdx<T>(xs,count,offset,stride),GetterRef(lims.Y.Max),count);
        if (FitThisFrame()) {
            for (int i = 0; i < get_min.Count; ++i)
                FitPointX(get_min(i).x);
        }
        const ImPlotNextItemData& s = GetItemData();
        ImDrawList& DrawList = *GetPlotDrawList();
        // render stems
        if (s.RenderLine) {
            const ImU32 col_line = ImGui::GetColorU32(s.Colors[ImPlotCol_Line]);
            switch (GetCurrentScale()) {
                case ImPlotScale_LinLin: RenderLineSegments(get_min, get_max, TransformerLinLin(), DrawList, s.LineWeight, col_line); break;
                case ImPlotScale_LogLin: RenderLineSegments(get_min, get_max, TransformerLogLin(), DrawList, s.LineWeight, col_line); break;
                case ImPlotScale_LinLog: RenderLineSegments(get_min, get_max, TransformerLinLog(), DrawList, s.LineWeight, col_line); break;
                case ImPlotScale_LogLog: RenderLineSegments(get_min, get_max, TransformerLogLog(), DrawList, s.LineWeight, col_line); break;
            }
        }
        EndItem();
    }
}

template IMPLOT_API void PlotVLines<ImS8>(const char* label_id, const ImS8* xs, int count, int offset, int stride);
template IMPLOT_API void PlotVLines<ImU8>(const char* label_id, const ImU8* xs, int count, int offset, int stride);
template IMPLOT_API void PlotVLines<ImS16>(const char* label_id, const ImS16* xs, int count, int offset, int stride);
template IMPLOT_API void PlotVLines<ImU16>(const char* label_id, const ImU16* xs, int count, int offset, int stride);
template IMPLOT_API void PlotVLines<ImS32>(const char* label_id, const ImS32* xs, int count, int offset, int stride);
template IMPLOT_API void PlotVLines<ImU32>(const char* label_id, const ImU32* xs, int count, int offset, int stride);
template IMPLOT_API void PlotVLines<ImS64>(const char* label_id, const ImS64* xs, int count, int offset, int stride);
template IMPLOT_API void PlotVLines<ImU64>(const char* label_id, const ImU64* xs, int count, int offset, int stride);
template IMPLOT_API void PlotVLines<float>(const char* label_id, const float* xs, int count, int offset, int stride);
template IMPLOT_API void PlotVLines<double>(const char* label_id, const double* xs, int count, int offset, int stride);


template <typename T>
void PlotHLines(const char* label_id, const T* ys, int count, int offset, int stride) {
    if (BeginItem(label_id, ImPlotCol_Line)) {
        const ImPlotRect lims = GetPlotLimits(IMPLOT_AUTO,IMPLOT_AUTO);
        GetterXY<GetterRef,GetterIdx<T>> get_min(GetterRef(lims.X.Min),GetterIdx<T>(ys,count,offset,stride),count);
        GetterXY<GetterRef,GetterIdx<T>> get_max(GetterRef(lims.X.Max),GetterIdx<T>(ys,count,offset,stride),count);
        if (FitThisFrame()) {
            for (int i = 0; i < get_min.Count; ++i)
                FitPointY(get_min(i).y);
        }
        const ImPlotNextItemData& s = GetItemData();
        ImDrawList& DrawList = *GetPlotDrawList();
        // render stems
        if (s.RenderLine) {
            const ImU32 col_line = ImGui::GetColorU32(s.Colors[ImPlotCol_Line]);
            switch (GetCurrentScale()) {
                case ImPlotScale_LinLin: RenderLineSegments(get_min, get_max, TransformerLinLin(), DrawList, s.LineWeight, col_line); break;
                case ImPlotScale_LogLin: RenderLineSegments(get_min, get_max, TransformerLogLin(), DrawList, s.LineWeight, col_line); break;
                case ImPlotScale_LinLog: RenderLineSegments(get_min, get_max, TransformerLinLog(), DrawList, s.LineWeight, col_line); break;
                case ImPlotScale_LogLog: RenderLineSegments(get_min, get_max, TransformerLogLog(), DrawList, s.LineWeight, col_line); break;
            }
        }
        EndItem();
    }
}

template IMPLOT_API void PlotHLines<ImS8>(const char* label_id, const ImS8* ys, int count, int offset, int stride);
template IMPLOT_API void PlotHLines<ImU8>(const char* label_id, const ImU8* ys, int count, int offset, int stride);
template IMPLOT_API void PlotHLines<ImS16>(const char* label_id, const ImS16* ys, int count, int offset, int stride);
template IMPLOT_API void PlotHLines<ImU16>(const char* label_id, const ImU16* ys, int count, int offset, int stride);
template IMPLOT_API void PlotHLines<ImS32>(const char* label_id, const ImS32* ys, int count, int offset, int stride);
template IMPLOT_API void PlotHLines<ImU32>(const char* label_id, const ImU32* ys, int count, int offset, int stride);
template IMPLOT_API void PlotHLines<ImS64>(const char* label_id, const ImS64* ys, int count, int offset, int stride);
template IMPLOT_API void PlotHLines<ImU64>(const char* label_id, const ImU64* ys, int count, int offset, int stride);
template IMPLOT_API void PlotHLines<float>(const char* label_id, const float* ys, int count, int offset, int stride);
template IMPLOT_API void PlotHLines<double>(const char* label_id, const double* ys, int count, int offset, int stride);

//-----------------------------------------------------------------------------
// PLOT PIE CHART
//-----------------------------------------------------------------------------

IMPLOT_INLINE void RenderPieSlice(ImDrawList& DrawList, const ImPlotPoint& center, double radius, double a0, double a1, ImU32 col) {
    static const float resolution = 50 / (2 * IM_PI);
    static ImVec2 buffer[50];
    buffer[0] = PlotToPixels(center,IMPLOT_AUTO,IMPLOT_AUTO);
    int n = ImMax(3, (int)((a1 - a0) * resolution));
    double da = (a1 - a0) / (n - 1);
    for (int i = 0; i < n; ++i) {
        double a = a0 + i * da;
        buffer[i + 1] = PlotToPixels(center.x + radius * cos(a), center.y + radius * sin(a),IMPLOT_AUTO,IMPLOT_AUTO);
    }
    DrawList.AddConvexPolyFilled(buffer, n + 1, col);
}

template <typename T>
void PlotPieChart(const char* const label_ids[], const T* values, int count, double x, double y, double radius, bool normalize, const char* fmt, double angle0) {
    IM_ASSERT_USER_ERROR(GImPlot->CurrentPlot != NULL, "PlotPieChart() needs to be called between BeginPlot() and EndPlot()!");
    ImDrawList & DrawList = *GetPlotDrawList();
    double sum = 0;
    for (int i = 0; i < count; ++i)
        sum += (double)values[i];
    normalize = normalize || sum > 1.0;
    ImPlotPoint center(x,y);
    PushPlotClipRect();
    double a0 = angle0 * 2 * IM_PI / 360.0;
    double a1 = angle0 * 2 * IM_PI / 360.0;
    for (int i = 0; i < count; ++i) {
        double percent = normalize ? (double)values[i] / sum : (double)values[i];
        a1 = a0 + 2 * IM_PI * percent;
        if (BeginItem(label_ids[i])) {
            if (FitThisFrame()) {
                FitPoint(ImPlotPoint(x-radius,y-radius));
                FitPoint(ImPlotPoint(x+radius,y+radius));
            }
            ImU32 col = GetCurrentItem()->Color;
            if (percent < 0.5) {
                RenderPieSlice(DrawList, center, radius, a0, a1, col);
            }
            else  {
                RenderPieSlice(DrawList, center, radius, a0, a0 + (a1 - a0) * 0.5, col);
                RenderPieSlice(DrawList, center, radius, a0 + (a1 - a0) * 0.5, a1, col);
            }
            EndItem();
        }
        a0 = a1;
    }
    if (fmt != NULL) {
        a0 = angle0 * 2 * IM_PI / 360.0;
        a1 = angle0 * 2 * IM_PI / 360.0;
        char buffer[32];
        for (int i = 0; i < count; ++i) {
            ImPlotItem* item = GetItem(label_ids[i]);
            double percent = normalize ? (double)values[i] / sum : (double)values[i];
            a1 = a0 + 2 * IM_PI * percent;
            if (item->Show) {
                ImFormatString(buffer, 32, fmt, (double)values[i]);
                ImVec2 size = ImGui::CalcTextSize(buffer);
                double angle = a0 + (a1 - a0) * 0.5;
                ImVec2 pos = PlotToPixels(center.x + 0.5 * radius * cos(angle), center.y + 0.5 * radius * sin(angle),IMPLOT_AUTO,IMPLOT_AUTO);
                ImU32 col  = CalcTextColor(ImGui::ColorConvertU32ToFloat4(item->Color));
                DrawList.AddText(pos - size * 0.5f, col, buffer);
            }
            a0 = a1;
        }
    }
    PopPlotClipRect();
}

template IMPLOT_API void PlotPieChart<ImS8>(const char* const label_ids[], const ImS8* values, int count, double x, double y, double radius, bool normalize, const char* fmt, double angle0);
template IMPLOT_API void PlotPieChart<ImU8>(const char* const label_ids[], const ImU8* values, int count, double x, double y, double radius, bool normalize, const char* fmt, double angle0);
template IMPLOT_API void PlotPieChart<ImS16>(const char* const label_ids[], const ImS16* values, int count, double x, double y, double radius, bool normalize, const char* fmt, double angle0);
template IMPLOT_API void PlotPieChart<ImU16>(const char* const label_ids[], const ImU16* values, int count, double x, double y, double radius, bool normalize, const char* fmt, double angle0);
template IMPLOT_API void PlotPieChart<ImS32>(const char* const label_ids[], const ImS32* values, int count, double x, double y, double radius, bool normalize, const char* fmt, double angle0);
template IMPLOT_API void PlotPieChart<ImU32>(const char* const label_ids[], const ImU32* values, int count, double x, double y, double radius, bool normalize, const char* fmt, double angle0);
template IMPLOT_API void PlotPieChart<ImS64>(const char* const label_ids[], const ImS64* values, int count, double x, double y, double radius, bool normalize, const char* fmt, double angle0);
template IMPLOT_API void PlotPieChart<ImU64>(const char* const label_ids[], const ImU64* values, int count, double x, double y, double radius, bool normalize, const char* fmt, double angle0);
template IMPLOT_API void PlotPieChart<float>(const char* const label_ids[], const float* values, int count, double x, double y, double radius, bool normalize, const char* fmt, double angle0);
template IMPLOT_API void PlotPieChart<double>(const char* const label_ids[], const double* values, int count, double x, double y, double radius, bool normalize, const char* fmt, double angle0);

//-----------------------------------------------------------------------------
// PLOT HEATMAP
//-----------------------------------------------------------------------------

struct RectInfo {
    ImPlotPoint Min, Max;
    ImU32 Color;
};

template <typename TGetter, typename TTransformer>
struct RectRenderer {
    IMPLOT_INLINE RectRenderer(const TGetter& getter, const TTransformer& transformer) :
        Getter(getter),
        Transformer(transformer),
        Prims(Getter.Count)
    {}
    IMPLOT_INLINE bool operator()(ImDrawList& DrawList, const ImRect& cull_rect, const ImVec2& uv, int prim) const {
        RectInfo rect = Getter(prim);
        ImVec2 P1 = Transformer(rect.Min);
        ImVec2 P2 = Transformer(rect.Max);

        if ((rect.Color & IM_COL32_A_MASK) == 0 || !cull_rect.Overlaps(ImRect(ImMin(P1, P2), ImMax(P1, P2))))
            return false;

        DrawList._VtxWritePtr[0].pos   = P1;
        DrawList._VtxWritePtr[0].uv    = uv;
        DrawList._VtxWritePtr[0].col   = rect.Color;
        DrawList._VtxWritePtr[1].pos.x = P1.x;
        DrawList._VtxWritePtr[1].pos.y = P2.y;
        DrawList._VtxWritePtr[1].uv    = uv;
        DrawList._VtxWritePtr[1].col   = rect.Color;
        DrawList._VtxWritePtr[2].pos   = P2;
        DrawList._VtxWritePtr[2].uv    = uv;
        DrawList._VtxWritePtr[2].col   = rect.Color;
        DrawList._VtxWritePtr[3].pos.x = P2.x;
        DrawList._VtxWritePtr[3].pos.y = P1.y;
        DrawList._VtxWritePtr[3].uv    = uv;
        DrawList._VtxWritePtr[3].col   = rect.Color;
        DrawList._VtxWritePtr += 4;
        DrawList._IdxWritePtr[0] = (ImDrawIdx)(DrawList._VtxCurrentIdx);
        DrawList._IdxWritePtr[1] = (ImDrawIdx)(DrawList._VtxCurrentIdx + 1);
        DrawList._IdxWritePtr[2] = (ImDrawIdx)(DrawList._VtxCurrentIdx + 3);
        DrawList._IdxWritePtr[3] = (ImDrawIdx)(DrawList._VtxCurrentIdx + 1);
        DrawList._IdxWritePtr[4] = (ImDrawIdx)(DrawList._VtxCurrentIdx + 2);
        DrawList._IdxWritePtr[5] = (ImDrawIdx)(DrawList._VtxCurrentIdx + 3);
        DrawList._IdxWritePtr   += 6;
        DrawList._VtxCurrentIdx += 4;
        return true;
    }
    const TGetter& Getter;
    const TTransformer& Transformer;
    const int Prims;
    static const int IdxConsumed = 6;
    static const int VtxConsumed = 4;
};

template <typename T>
struct GetterHeatmap {
    GetterHeatmap(const T* values, int rows, int cols, double scale_min, double scale_max, double width, double height, double xref, double yref, double ydir) :
        Values(values),
        Count(rows*cols),
        Rows(rows),
        Cols(cols),
        ScaleMin(scale_min),
        ScaleMax(scale_max),
        Width(width),
        Height(height),
        XRef(xref),
        YRef(yref),
        YDir(ydir),
        HalfSize(Width*0.5, Height*0.5)
    { }

    template <typename I> IMPLOT_INLINE RectInfo operator()(I idx) const {
        double val = (double)Values[idx];
        const int r = idx / Cols;
        const int c = idx % Cols;
        const ImPlotPoint p(XRef + HalfSize.x + c*Width, YRef + YDir * (HalfSize.y + r*Height));
        RectInfo rect;
        rect.Min.x = p.x - HalfSize.x;
        rect.Min.y = p.y - HalfSize.y;
        rect.Max.x = p.x + HalfSize.x;
        rect.Max.y = p.y + HalfSize.y;
        const float t = ImClamp((float)ImRemap01(val, ScaleMin, ScaleMax),0.0f,1.0f);
        rect.Color = GImPlot->ColormapData.LerpTable(GImPlot->Style.Colormap, t);
        return rect;
    }
    const T* const Values;
    const int Count, Rows, Cols;
    const double ScaleMin, ScaleMax, Width, Height, XRef, YRef, YDir;
    const ImPlotPoint HalfSize;
};

template <typename T, typename Transformer>
void RenderHeatmap(Transformer transformer, ImDrawList& DrawList, const T* values, int rows, int cols, double scale_min, double scale_max, const char* fmt, const ImPlotPoint& bounds_min, const ImPlotPoint& bounds_max, bool reverse_y) {
    ImPlotContext& gp = *GImPlot;
    if (scale_min == 0 && scale_max == 0) {
        T temp_min, temp_max;
        ImMinMaxArray(values,rows*cols,&temp_min,&temp_max);
        scale_min = (double)temp_min;
        scale_max = (double)temp_max;
    }
    if (scale_min == scale_max) {
        ImVec2 a = transformer(bounds_min);
        ImVec2 b = transformer(bounds_max);
        ImU32  col = GetColormapColorU32(0,gp.Style.Colormap);
        DrawList.AddRectFilled(a, b, col);
        return;
    }
    const double yref = reverse_y ? bounds_max.y : bounds_min.y;
    const double ydir = reverse_y ? -1 : 1;
    GetterHeatmap<T> getter(values, rows, cols, scale_min, scale_max, (bounds_max.x - bounds_min.x) / cols, (bounds_max.y - bounds_min.y) / rows, bounds_min.x, yref, ydir);
    switch (GetCurrentScale()) {
        case ImPlotScale_LinLin: RenderPrimitives(RectRenderer<GetterHeatmap<T>, TransformerLinLin>(getter, TransformerLinLin()), DrawList, gp.CurrentPlot->PlotRect); break;
        case ImPlotScale_LogLin: RenderPrimitives(RectRenderer<GetterHeatmap<T>, TransformerLogLin>(getter, TransformerLogLin()), DrawList, gp.CurrentPlot->PlotRect); break;;
        case ImPlotScale_LinLog: RenderPrimitives(RectRenderer<GetterHeatmap<T>, TransformerLinLog>(getter, TransformerLinLog()), DrawList, gp.CurrentPlot->PlotRect); break;;
        case ImPlotScale_LogLog: RenderPrimitives(RectRenderer<GetterHeatmap<T>, TransformerLogLog>(getter, TransformerLogLog()), DrawList, gp.CurrentPlot->PlotRect); break;;
    }
    if (fmt != NULL) {
        const double w = (bounds_max.x - bounds_min.x) / cols;
        const double h = (bounds_max.y - bounds_min.y) / rows;
        const ImPlotPoint half_size(w*0.5,h*0.5);
        int i = 0;
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                ImPlotPoint p;
                p.x = bounds_min.x + 0.5*w + c*w;
                p.y = yref + ydir * (0.5*h + r*h);
                ImVec2 px = transformer(p);
                char buff[32];
                ImFormatString(buff, 32, fmt, values[i]);
                ImVec2 size = ImGui::CalcTextSize(buff);
                double t = ImClamp(ImRemap01((double)values[i], scale_min, scale_max),0.0,1.0);
                ImVec4 color = SampleColormap((float)t);
                ImU32 col = CalcTextColor(color);
                DrawList.AddText(px - size * 0.5f, col, buff);
                i++;
            }
        }
    }
}

template <typename T>
void PlotHeatmap(const char* label_id, const T* values, int rows, int cols, double scale_min, double scale_max, const char* fmt, const ImPlotPoint& bounds_min, const ImPlotPoint& bounds_max) {
    if (BeginItem(label_id)) {
        if (FitThisFrame()) {
            FitPoint(bounds_min);
            FitPoint(bounds_max);
        }
        ImDrawList& DrawList = *GetPlotDrawList();
        switch (GetCurrentScale()) {
            case ImPlotScale_LinLin: RenderHeatmap(TransformerLinLin(), DrawList, values, rows, cols, scale_min, scale_max, fmt, bounds_min, bounds_max, true); break;
            case ImPlotScale_LogLin: RenderHeatmap(TransformerLogLin(), DrawList, values, rows, cols, scale_min, scale_max, fmt, bounds_min, bounds_max, true); break;
            case ImPlotScale_LinLog: RenderHeatmap(TransformerLinLog(), DrawList, values, rows, cols, scale_min, scale_max, fmt, bounds_min, bounds_max, true); break;
            case ImPlotScale_LogLog: RenderHeatmap(TransformerLogLog(), DrawList, values, rows, cols, scale_min, scale_max, fmt, bounds_min, bounds_max, true); break;
        }
        EndItem();
    }
}

template IMPLOT_API void PlotHeatmap<ImS8>(const char* label_id, const ImS8* values, int rows, int cols, double scale_min, double scale_max, const char* fmt, const ImPlotPoint& bounds_min, const ImPlotPoint& bounds_max);
template IMPLOT_API void PlotHeatmap<ImU8>(const char* label_id, const ImU8* values, int rows, int cols, double scale_min, double scale_max, const char* fmt, const ImPlotPoint& bounds_min, const ImPlotPoint& bounds_max);
template IMPLOT_API void PlotHeatmap<ImS16>(const char* label_id, const ImS16* values, int rows, int cols, double scale_min, double scale_max, const char* fmt, const ImPlotPoint& bounds_min, const ImPlotPoint& bounds_max);
template IMPLOT_API void PlotHeatmap<ImU16>(const char* label_id, const ImU16* values, int rows, int cols, double scale_min, double scale_max, const char* fmt, const ImPlotPoint& bounds_min, const ImPlotPoint& bounds_max);
template IMPLOT_API void PlotHeatmap<ImS32>(const char* label_id, const ImS32* values, int rows, int cols, double scale_min, double scale_max, const char* fmt, const ImPlotPoint& bounds_min, const ImPlotPoint& bounds_max);
template IMPLOT_API void PlotHeatmap<ImU32>(const char* label_id, const ImU32* values, int rows, int cols, double scale_min, double scale_max, const char* fmt, const ImPlotPoint& bounds_min, const ImPlotPoint& bounds_max);
template IMPLOT_API void PlotHeatmap<ImS64>(const char* label_id, const ImS64* values, int rows, int cols, double scale_min, double scale_max, const char* fmt, const ImPlotPoint& bounds_min, const ImPlotPoint& bounds_max);
template IMPLOT_API void PlotHeatmap<ImU64>(const char* label_id, const ImU64* values, int rows, int cols, double scale_min, double scale_max, const char* fmt, const ImPlotPoint& bounds_min, const ImPlotPoint& bounds_max);
template IMPLOT_API void PlotHeatmap<float>(const char* label_id, const float* values, int rows, int cols, double scale_min, double scale_max, const char* fmt, const ImPlotPoint& bounds_min, const ImPlotPoint& bounds_max);
template IMPLOT_API void PlotHeatmap<double>(const char* label_id, const double* values, int rows, int cols, double scale_min, double scale_max, const char* fmt, const ImPlotPoint& bounds_min, const ImPlotPoint& bounds_max);

//-----------------------------------------------------------------------------
// PLOT HISTOGRAM
//-----------------------------------------------------------------------------

template <typename T>
double PlotHistogram(const char* label_id, const T* values, int count, int bins, bool cumulative, bool density, ImPlotRange range, bool outliers, double bar_scale) {

    if (count <= 0 || bins == 0)
        return 0;

    if (range.Min == 0 && range.Max == 0) {
        T Min, Max;
        ImMinMaxArray(values, count, &Min, &Max);
        range.Min = (double)Min;
        range.Max = (double)Max;
    }

    double width;
    if (bins < 0)
        CalculateBins(values, count, bins, range, bins, width);
    else
        width = range.Size() / bins;

    ImVector<double>& bin_centers = GImPlot->TempDouble1;
    ImVector<double>& bin_counts  = GImPlot->TempDouble2;
    bin_centers.resize(bins);
    bin_counts.resize(bins);
    int below = 0;

    for (int b = 0; b < bins; ++b) {
        bin_centers[b] = range.Min + b * width + width * 0.5;
        bin_counts[b] = 0;
    }
    int counted = 0;
    double max_count = 0;
    for (int i = 0; i < count; ++i) {
        double val = (double)values[i];
        if (range.Contains(val)) {
            const int b = ImClamp((int)((val - range.Min) / width), 0, bins - 1);
            bin_counts[b] += 1.0;
            if (bin_counts[b] > max_count)
                max_count = bin_counts[b];
            counted++;
        }
        else if (val < range.Min) {
            below++;
        }
    }
    if (cumulative && density) {
        if (outliers)
            bin_counts[0] += below;
        for (int b = 1; b < bins; ++b)
            bin_counts[b] += bin_counts[b-1];
        double scale = 1.0 / (outliers ? count : counted);
        for (int b = 0; b < bins; ++b)
            bin_counts[b] *= scale;
        max_count = bin_counts[bins-1];
    }
    else if (cumulative) {
        if (outliers)
            bin_counts[0] += below;
        for (int b = 1; b < bins; ++b)
            bin_counts[b] += bin_counts[b-1];
        max_count = bin_counts[bins-1];
    }
    else if (density) {
        double scale = 1.0 / ((outliers ? count : counted) * width);
        for (int b = 0; b < bins; ++b)
            bin_counts[b] *= scale;
        max_count *= scale;
    }
    PlotBars(label_id, &bin_centers.Data[0], &bin_counts.Data[0], bins, bar_scale*width);
    return max_count;
}

template IMPLOT_API double PlotHistogram<ImS8>(const char* label_id, const ImS8* values, int count, int bins, bool cumulative, bool density, ImPlotRange range, bool outliers, double bar_scale);
template IMPLOT_API double PlotHistogram<ImU8>(const char* label_id, const ImU8* values, int count, int bins, bool cumulative, bool density, ImPlotRange range, bool outliers, double bar_scale);
template IMPLOT_API double PlotHistogram<ImS16>(const char* label_id, const ImS16* values, int count, int bins, bool cumulative, bool density, ImPlotRange range, bool outliers, double bar_scale);
template IMPLOT_API double PlotHistogram<ImU16>(const char* label_id, const ImU16* values, int count, int bins, bool cumulative, bool density, ImPlotRange range, bool outliers, double bar_scale);
template IMPLOT_API double PlotHistogram<ImS32>(const char* label_id, const ImS32* values, int count, int bins, bool cumulative, bool density, ImPlotRange range, bool outliers, double bar_scale);
template IMPLOT_API double PlotHistogram<ImU32>(const char* label_id, const ImU32* values, int count, int bins, bool cumulative, bool density, ImPlotRange range, bool outliers, double bar_scale);
template IMPLOT_API double PlotHistogram<ImS64>(const char* label_id, const ImS64* values, int count, int bins, bool cumulative, bool density, ImPlotRange range, bool outliers, double bar_scale);
template IMPLOT_API double PlotHistogram<ImU64>(const char* label_id, const ImU64* values, int count, int bins, bool cumulative, bool density, ImPlotRange range, bool outliers, double bar_scale);
template IMPLOT_API double PlotHistogram<float>(const char* label_id, const float* values, int count, int bins, bool cumulative, bool density, ImPlotRange range, bool outliers, double bar_scale);
template IMPLOT_API double PlotHistogram<double>(const char* label_id, const double* values, int count, int bins, bool cumulative, bool density, ImPlotRange range, bool outliers, double bar_scale);

//-----------------------------------------------------------------------------
// PLOT HISTOGRAM 2D
//-----------------------------------------------------------------------------

template <typename T>
double PlotHistogram2D(const char* label_id, const T* xs, const T* ys, int count, int x_bins, int y_bins, bool density, ImPlotRect range, bool outliers) {

    if (count <= 0 || x_bins == 0 || y_bins == 0)
        return 0;

    if (range.X.Min == 0 && range.X.Max == 0) {
        T Min, Max;
        ImMinMaxArray(xs, count, &Min, &Max);
        range.X.Min = (double)Min;
        range.X.Max = (double)Max;
    }
    if (range.Y.Min == 0 && range.Y.Max == 0) {
        T Min, Max;
        ImMinMaxArray(ys, count, &Min, &Max);
        range.Y.Min = (double)Min;
        range.Y.Max = (double)Max;
    }

    double width, height;
    if (x_bins < 0)
        CalculateBins(xs, count, x_bins, range.X, x_bins, width);
    else
        width = range.X.Size() / x_bins;
    if (y_bins < 0)
        CalculateBins(ys, count, y_bins, range.Y, y_bins, height);
    else
        height = range.Y.Size() / y_bins;

    const int bins = x_bins * y_bins;

    ImVector<double>& bin_counts = GImPlot->TempDouble1;
    bin_counts.resize(bins);

    for (int b = 0; b < bins; ++b)
        bin_counts[b] = 0;

    int counted = 0;
    double max_count = 0;
    for (int i = 0; i < count; ++i) {
        if (range.Contains((double)xs[i], (double)ys[i])) {
            const int xb = ImClamp( (int)((double)(xs[i] - range.X.Min) / width)  , 0, x_bins - 1);
            const int yb = ImClamp( (int)((double)(ys[i] - range.Y.Min) / height) , 0, y_bins - 1);
            const int b  = yb * x_bins + xb;
            bin_counts[b] += 1.0;
            if (bin_counts[b] > max_count)
                max_count = bin_counts[b];
            counted++;
        }
    }
    if (density) {
        double scale = 1.0 / ((outliers ? count : counted) * width * height);
        for (int b = 0; b < bins; ++b)
            bin_counts[b] *= scale;
        max_count *= scale;
    }

    if (BeginItem(label_id)) {
        if (FitThisFrame()) {
            FitPoint(range.Min());
            FitPoint(range.Max());
        }
        ImDrawList& DrawList = *GetPlotDrawList();
        switch (GetCurrentScale()) {
            case ImPlotScale_LinLin: RenderHeatmap(TransformerLinLin(), DrawList, &bin_counts.Data[0], y_bins, x_bins, 0, max_count, NULL, range.Min(), range.Max(), false); break;
            case ImPlotScale_LogLin: RenderHeatmap(TransformerLogLin(), DrawList, &bin_counts.Data[0], y_bins, x_bins, 0, max_count, NULL, range.Min(), range.Max(), false); break;
            case ImPlotScale_LinLog: RenderHeatmap(TransformerLinLog(), DrawList, &bin_counts.Data[0], y_bins, x_bins, 0, max_count, NULL, range.Min(), range.Max(), false); break;
            case ImPlotScale_LogLog: RenderHeatmap(TransformerLogLog(), DrawList, &bin_counts.Data[0], y_bins, x_bins, 0, max_count, NULL, range.Min(), range.Max(), false); break;
        }
        EndItem();
    }
    return max_count;
}

template IMPLOT_API double PlotHistogram2D<ImS8>(const char* label_id,   const ImS8*   xs, const ImS8*   ys, int count, int x_bins, int y_bins, bool density, ImPlotRect range, bool outliers);
template IMPLOT_API double PlotHistogram2D<ImU8>(const char* label_id,   const ImU8*   xs, const ImU8*   ys, int count, int x_bins, int y_bins, bool density, ImPlotRect range, bool outliers);
template IMPLOT_API double PlotHistogram2D<ImS16>(const char* label_id,  const ImS16*  xs, const ImS16*  ys, int count, int x_bins, int y_bins, bool density, ImPlotRect range, bool outliers);
template IMPLOT_API double PlotHistogram2D<ImU16>(const char* label_id,  const ImU16*  xs, const ImU16*  ys, int count, int x_bins, int y_bins, bool density, ImPlotRect range, bool outliers);
template IMPLOT_API double PlotHistogram2D<ImS32>(const char* label_id,  const ImS32*  xs, const ImS32*  ys, int count, int x_bins, int y_bins, bool density, ImPlotRect range, bool outliers);
template IMPLOT_API double PlotHistogram2D<ImU32>(const char* label_id,  const ImU32*  xs, const ImU32*  ys, int count, int x_bins, int y_bins, bool density, ImPlotRect range, bool outliers);
template IMPLOT_API double PlotHistogram2D<ImS64>(const char* label_id,  const ImS64*  xs, const ImS64*  ys, int count, int x_bins, int y_bins, bool density, ImPlotRect range, bool outliers);
template IMPLOT_API double PlotHistogram2D<ImU64>(const char* label_id,  const ImU64*  xs, const ImU64*  ys, int count, int x_bins, int y_bins, bool density, ImPlotRect range, bool outliers);
template IMPLOT_API double PlotHistogram2D<float>(const char* label_id,  const float*  xs, const float*  ys, int count, int x_bins, int y_bins, bool density, ImPlotRect range, bool outliers);
template IMPLOT_API double PlotHistogram2D<double>(const char* label_id, const double* xs, const double* ys, int count, int x_bins, int y_bins, bool density, ImPlotRect range, bool outliers);

//-----------------------------------------------------------------------------
// PLOT DIGITAL
//-----------------------------------------------------------------------------

// TODO: Make this behave like all the other plot types (.e. not fixed in y axis)

template <typename Getter>
IMPLOT_INLINE void PlotDigitalEx(const char* label_id, Getter getter) {
    if (BeginItem(label_id, ImPlotCol_Fill)) {
        ImPlotContext& gp = *GImPlot;
        ImDrawList& DrawList = *GetPlotDrawList();
        const ImPlotNextItemData& s = GetItemData();
        if (getter.Count > 1 && s.RenderFill) {
            ImPlotPlot& plot   = *gp.CurrentPlot;
            ImPlotAxis& x_axis = plot.Axes[plot.CurrentX];
            ImPlotAxis& y_axis = plot.Axes[plot.CurrentY];

            int pixYMax = 0;
            ImPlotPoint itemData1 = getter(0);
            for (int i = 0; i < getter.Count; ++i) {
                ImPlotPoint itemData2 = getter(i);
                if (ImNanOrInf(itemData1.y)) {
                    itemData1 = itemData2;
                    continue;
                }
                if (ImNanOrInf(itemData2.y)) itemData2.y = ImConstrainNan(ImConstrainInf(itemData2.y));
                int pixY_0 = (int)(s.LineWeight);
                itemData1.y = ImMax(0.0, itemData1.y);
                float pixY_1_float = s.DigitalBitHeight * (float)itemData1.y;
                int pixY_1 = (int)(pixY_1_float); //allow only positive values
                int pixY_chPosOffset = (int)(ImMax(s.DigitalBitHeight, pixY_1_float) + s.DigitalBitGap);
                pixYMax = ImMax(pixYMax, pixY_chPosOffset);
                ImVec2 pMin = PlotToPixels(itemData1,IMPLOT_AUTO,IMPLOT_AUTO);
                ImVec2 pMax = PlotToPixels(itemData2,IMPLOT_AUTO,IMPLOT_AUTO);
                int pixY_Offset = 0; //20 pixel from bottom due to mouse cursor label
                pMin.y = (y_axis.PixelMin) + ((-gp.DigitalPlotOffset)                   - pixY_Offset);
                pMax.y = (y_axis.PixelMin) + ((-gp.DigitalPlotOffset) - pixY_0 - pixY_1 - pixY_Offset);
                //plot only one rectangle for same digital state
                while (((i+2) < getter.Count) && (itemData1.y == itemData2.y)) {
                    const int in = (i + 1);
                    itemData2 = getter(in);
                    if (ImNanOrInf(itemData2.y)) break;
                    pMax.x = PlotToPixels(itemData2,IMPLOT_AUTO,IMPLOT_AUTO).x;
                    i++;
                }
                //do not extend plot outside plot range
                if (pMin.x < x_axis.PixelMin) pMin.x = x_axis.PixelMin;
                if (pMax.x < x_axis.PixelMin) pMax.x = x_axis.PixelMin;
                if (pMin.x > x_axis.PixelMax) pMin.x = x_axis.PixelMax;
                if (pMax.x > x_axis.PixelMax) pMax.x = x_axis.PixelMax;
                //plot a rectangle that extends up to x2 with y1 height
                if ((pMax.x > pMin.x) && (gp.CurrentPlot->PlotRect.Contains(pMin) || gp.CurrentPlot->PlotRect.Contains(pMax))) {
                    // ImVec4 colAlpha = item->Color;
                    // colAlpha.w = item->Highlight ? 1.0f : 0.9f;
                    DrawList.AddRectFilled(pMin, pMax, ImGui::GetColorU32(s.Colors[ImPlotCol_Fill]));
                }
                itemData1 = itemData2;
            }
            gp.DigitalPlotItemCnt++;
            gp.DigitalPlotOffset += pixYMax;
        }
        EndItem();
    }
}


template <typename T>
void PlotDigital(const char* label_id, const T* xs, const T* ys, int count, int offset, int stride) {
    GetterXY<GetterIdx<T>,GetterIdx<T>> getter(GetterIdx<T>(xs,count,offset,stride),GetterIdx<T>(ys,count,offset,stride),count);
    return PlotDigitalEx(label_id, getter);
}

template IMPLOT_API void PlotDigital<ImS8>(const char* label_id, const ImS8* xs, const ImS8* ys, int count, int offset, int stride);
template IMPLOT_API void PlotDigital<ImU8>(const char* label_id, const ImU8* xs, const ImU8* ys, int count, int offset, int stride);
template IMPLOT_API void PlotDigital<ImS16>(const char* label_id, const ImS16* xs, const ImS16* ys, int count, int offset, int stride);
template IMPLOT_API void PlotDigital<ImU16>(const char* label_id, const ImU16* xs, const ImU16* ys, int count, int offset, int stride);
template IMPLOT_API void PlotDigital<ImS32>(const char* label_id, const ImS32* xs, const ImS32* ys, int count, int offset, int stride);
template IMPLOT_API void PlotDigital<ImU32>(const char* label_id, const ImU32* xs, const ImU32* ys, int count, int offset, int stride);
template IMPLOT_API void PlotDigital<ImS64>(const char* label_id, const ImS64* xs, const ImS64* ys, int count, int offset, int stride);
template IMPLOT_API void PlotDigital<ImU64>(const char* label_id, const ImU64* xs, const ImU64* ys, int count, int offset, int stride);
template IMPLOT_API void PlotDigital<float>(const char* label_id, const float* xs, const float* ys, int count, int offset, int stride);
template IMPLOT_API void PlotDigital<double>(const char* label_id, const double* xs, const double* ys, int count, int offset, int stride);

// custom
void PlotDigitalG(const char* label_id, ImPlotGetter getter_func, void* data, int count) {
    GetterFuncPtr getter(getter_func,data,count);
    return PlotDigitalEx(label_id, getter);
}

//-----------------------------------------------------------------------------
// PLOT IMAGE
//-----------------------------------------------------------------------------

void PlotImage(const char* label_id, ImTextureID user_texture_id, const ImPlotPoint& bmin, const ImPlotPoint& bmax, const ImVec2& uv0, const ImVec2& uv1, const ImVec4& tint_col) {
    if (BeginItem(label_id)) {
        if (FitThisFrame()) {
            FitPoint(bmin);
            FitPoint(bmax);
        }
        ImU32 tint_col32 = ImGui::ColorConvertFloat4ToU32(tint_col);
        GetCurrentItem()->Color = tint_col32;
        ImDrawList& DrawList = *GetPlotDrawList();
        ImVec2 p1 = PlotToPixels(bmin.x, bmax.y,IMPLOT_AUTO,IMPLOT_AUTO);
        ImVec2 p2 = PlotToPixels(bmax.x, bmin.y,IMPLOT_AUTO,IMPLOT_AUTO);
        PushPlotClipRect();
        DrawList.AddImage(user_texture_id, p1, p2, uv0, uv1, tint_col32);
        PopPlotClipRect();
        EndItem();
    }
}

//-----------------------------------------------------------------------------
// PLOT TEXT
//-----------------------------------------------------------------------------

// double
void PlotText(const char* text, double x, double y, bool vertical, const ImVec2& pixel_offset) {
    IM_ASSERT_USER_ERROR(GImPlot->CurrentPlot != NULL, "PlotText() needs to be called between BeginPlot() and EndPlot()!");
    SetupLock();
    ImDrawList & DrawList = *GetPlotDrawList();
    PushPlotClipRect();
    ImU32 colTxt = GetStyleColorU32(ImPlotCol_InlayText);
    if (vertical) {
        ImVec2 siz = CalcTextSizeVertical(text) * 0.5f;
        ImVec2 ctr = siz * 0.5f;
        ImVec2 pos = PlotToPixels(ImPlotPoint(x,y),IMPLOT_AUTO,IMPLOT_AUTO) + ImVec2(-ctr.x, ctr.y) + pixel_offset;
        if (FitThisFrame()) {
            FitPoint(PixelsToPlot(pos));
            FitPoint(PixelsToPlot(pos.x + siz.x, pos.y - siz.y));
        }
        AddTextVertical(&DrawList, pos, colTxt, text);
    }
    else {
        ImVec2 siz = ImGui::CalcTextSize(text);
        ImVec2 pos = PlotToPixels(ImPlotPoint(x,y),IMPLOT_AUTO,IMPLOT_AUTO) - siz * 0.5f + pixel_offset;
        if (FitThisFrame()) {
            FitPoint(PixelsToPlot(pos));
            FitPoint(PixelsToPlot(pos+siz));
        }
        DrawList.AddText(pos, colTxt, text);
    }
    PopPlotClipRect();
}

//-----------------------------------------------------------------------------
// PLOT DUMMY
//-----------------------------------------------------------------------------

void PlotDummy(const char* label_id) {
    if (BeginItem(label_id, ImPlotCol_Line))
        EndItem();
}

} // namespace ImPlot
