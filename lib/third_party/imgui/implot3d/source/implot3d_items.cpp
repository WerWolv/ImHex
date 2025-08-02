//--------------------------------------------------
// ImPlot3D v0.3 WIP
// implot3d_items.cpp
// Date: 2024-11-26
// Author: Breno Cunha Queiroz (brenocq.com)
//
// Acknowledgments:
//  ImPlot3D is heavily inspired by ImPlot
//  (https://github.com/epezent/implot) by Evan Pezent,
//  and follows a similar code style and structure to
//  maintain consistency with ImPlot's API.
//--------------------------------------------------

// Table of Contents:
// [SECTION] Includes
// [SECTION] Macros & Defines
// [SECTION] Template instantiation utility
// [SECTION] Item Utils
// [SECTION] Draw Utils
// [SECTION] Renderers
// [SECTION] Indexers
// [SECTION] Getters
// [SECTION] RenderPrimitives
// [SECTION] Markers
// [SECTION] PlotScatter
// [SECTION] PlotLine
// [SECTION] PlotTriangle
// [SECTION] PlotQuad
// [SECTION] PlotSurface
// [SECTION] PlotMesh
// [SECTION] PlotImage
// [SECTION] PlotText

//-----------------------------------------------------------------------------
// [SECTION] Includes
//-----------------------------------------------------------------------------

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include "implot3d.h"
#include "implot3d_internal.h"

#ifndef IMGUI_DISABLE

//-----------------------------------------------------------------------------
// [SECTION] Macros & Defines
//-----------------------------------------------------------------------------

#define SQRT_1_2 0.70710678118f
#define SQRT_3_2 0.86602540378f

// clang-format off
#ifndef IMPLOT3D_NO_FORCE_INLINE
    #ifdef _MSC_VER
        #define IMPLOT3D_INLINE __forceinline
    #elif defined(__GNUC__)
        #define IMPLOT3D_INLINE inline __attribute__((__always_inline__))
    #elif defined(__CLANG__)
        #if __has_attribute(__always_inline__)
            #define IMPLOT3D_INLINE inline __attribute__((__always_inline__))
        #else
            #define IMPLOT3D_INLINE inline
        #endif
    #else
        #define IMPLOT3D_INLINE inline
    #endif
#else
    #define IMPLOT3D_INLINE inline
#endif
// clang-format on

#define IMPLOT3D_NORMALIZE2F(VX, VY)                                                                                                                 \
    do {                                                                                                                                             \
        float d2 = VX * VX + VY * VY;                                                                                                                \
        if (d2 > 0.0f) {                                                                                                                             \
            float inv_len = ImRsqrt(d2);                                                                                                             \
            VX *= inv_len;                                                                                                                           \
            VY *= inv_len;                                                                                                                           \
        }                                                                                                                                            \
    } while (0)

IMPLOT3D_INLINE void GetLineRenderProps(const ImDrawList3D& draw_list_3d, float& half_weight, ImVec2& tex_uv0, ImVec2& tex_uv1) {
    const bool aa = ImPlot3D::ImHasFlag(draw_list_3d._Flags, ImDrawListFlags_AntiAliasedLines) &&
                    ImPlot3D::ImHasFlag(draw_list_3d._Flags, ImDrawListFlags_AntiAliasedLinesUseTex);
    if (aa) {
        ImVec4 tex_uvs = draw_list_3d._SharedData->TexUvLines[(int)(half_weight * 2)];
        tex_uv0 = ImVec2(tex_uvs.x, tex_uvs.y);
        tex_uv1 = ImVec2(tex_uvs.z, tex_uvs.w);
        half_weight += 1;
    } else {
        tex_uv0 = tex_uv1 = draw_list_3d._SharedData->TexUvWhitePixel;
    }
}

//-----------------------------------------------------------------------------
// [SECTION] Template instantiation utility
//-----------------------------------------------------------------------------

// By default, templates are instantiated for `float`, `double`, and for the following integer types, which are defined in imgui.h:
//     signed char         ImS8;   // 8-bit signed integer
//     unsigned char       ImU8;   // 8-bit unsigned integer
//     signed short        ImS16;  // 16-bit signed integer
//     unsigned short      ImU16;  // 16-bit unsigned integer
//     signed int          ImS32;  // 32-bit signed integer == int
//     unsigned int        ImU32;  // 32-bit unsigned integer
//     signed   long long  ImS64;  // 64-bit signed integer
//     unsigned long long  ImU64;  // 64-bit unsigned integer
// (note: this list does *not* include `long`, `unsigned long` and `long double`)
//
// You can customize the supported types by defining IMPLOT3D_CUSTOM_NUMERIC_TYPES at compile time to define your own type list.
//    As an example, you could use the compile time define given by the line below in order to support only float and double.
//        -DIMPLOT3D_CUSTOM_NUMERIC_TYPES="(float)(double)"
//    In order to support all known C++ types, use:
//        -DIMPLOT3D_CUSTOM_NUMERIC_TYPES="(signed char)(unsigned char)(signed short)(unsigned short)(signed int)(unsigned int)(signed long)(unsigned
//        long)(signed long long)(unsigned long long)(float)(double)(long double)"

#ifdef IMPLOT3D_CUSTOM_NUMERIC_TYPES
#define IMPLOT3D_NUMERIC_TYPES IMPLOT3D_CUSTOM_NUMERIC_TYPES
#else
#define IMPLOT3D_NUMERIC_TYPES (ImS8)(ImU8)(ImS16)(ImU16)(ImS32)(ImU32)(ImS64)(ImU64)(float)(double)
#endif

// CALL_INSTANTIATE_FOR_NUMERIC_TYPES will duplicate the template instantiation code `INSTANTIATE_MACRO(T)` on supported types.
#define _CAT(x, y) _CAT_(x, y)
#define _CAT_(x, y) x##y
#define _INSTANTIATE_FOR_NUMERIC_TYPES(chain) _CAT(_INSTANTIATE_FOR_NUMERIC_TYPES_1 chain, _END)
#define _INSTANTIATE_FOR_NUMERIC_TYPES_1(T) INSTANTIATE_MACRO(T) _INSTANTIATE_FOR_NUMERIC_TYPES_2
#define _INSTANTIATE_FOR_NUMERIC_TYPES_2(T) INSTANTIATE_MACRO(T) _INSTANTIATE_FOR_NUMERIC_TYPES_1
#define _INSTANTIATE_FOR_NUMERIC_TYPES_1_END
#define _INSTANTIATE_FOR_NUMERIC_TYPES_2_END
#define CALL_INSTANTIATE_FOR_NUMERIC_TYPES() _INSTANTIATE_FOR_NUMERIC_TYPES(IMPLOT3D_NUMERIC_TYPES)

//-----------------------------------------------------------------------------
// [SECTION] Item Utils
//-----------------------------------------------------------------------------
namespace ImPlot3D {

static const float ITEM_HIGHLIGHT_LINE_SCALE = 2.0f;
static const float ITEM_HIGHLIGHT_MARK_SCALE = 1.25f;

bool BeginItem(const char* label_id, ImPlot3DItemFlags flags, ImPlot3DCol recolor_from) {
    ImPlot3DContext& gp = *GImPlot3D;
    IM_ASSERT_USER_ERROR(gp.CurrentPlot != nullptr, "PlotX() needs to be called between BeginPlot() and EndPlot()!");

    // Lock setup
    SetupLock();

    ImPlot3DStyle& style = gp.Style;
    ImPlot3DNextItemData& n = gp.NextItemData;

    // Register item
    bool just_created;
    ImPlot3DItem* item = RegisterOrGetItem(label_id, flags, &just_created);
    // Set current item
    gp.CurrentItem = item;

    // Set/override item color
    if (recolor_from != -1) {
        if (!IsColorAuto(n.Colors[recolor_from]))
            item->Color = ImGui::ColorConvertFloat4ToU32(n.Colors[recolor_from]);
        else if (!IsColorAuto(gp.Style.Colors[recolor_from]))
            item->Color = ImGui::ColorConvertFloat4ToU32(gp.Style.Colors[recolor_from]);
        else if (just_created)
            item->Color = NextColormapColorU32();
    } else if (just_created) {
        item->Color = NextColormapColorU32();
    }

    // Set next item color
    ImVec4 item_color = ImGui::ColorConvertU32ToFloat4(item->Color);
    n.IsAutoLine = IsColorAuto(n.Colors[ImPlot3DCol_Line]) && IsColorAuto(ImPlot3DCol_Line);
    n.IsAutoFill = IsColorAuto(n.Colors[ImPlot3DCol_Fill]) && IsColorAuto(ImPlot3DCol_Fill);
    n.Colors[ImPlot3DCol_Line] = IsColorAuto(n.Colors[ImPlot3DCol_Line])
                                     ? (IsColorAuto(ImPlot3DCol_Line) ? item_color : gp.Style.Colors[ImPlot3DCol_Line])
                                     : n.Colors[ImPlot3DCol_Line];
    n.Colors[ImPlot3DCol_Fill] = IsColorAuto(n.Colors[ImPlot3DCol_Fill])
                                     ? (IsColorAuto(ImPlot3DCol_Fill) ? item_color : gp.Style.Colors[ImPlot3DCol_Fill])
                                     : n.Colors[ImPlot3DCol_Fill];
    n.Colors[ImPlot3DCol_MarkerOutline] =
        IsColorAuto(n.Colors[ImPlot3DCol_MarkerOutline])
            ? (IsColorAuto(ImPlot3DCol_MarkerOutline) ? n.Colors[ImPlot3DCol_Line] : gp.Style.Colors[ImPlot3DCol_MarkerOutline])
            : n.Colors[ImPlot3DCol_MarkerOutline];
    n.Colors[ImPlot3DCol_MarkerFill] =
        IsColorAuto(n.Colors[ImPlot3DCol_MarkerFill])
            ? (IsColorAuto(ImPlot3DCol_MarkerFill) ? n.Colors[ImPlot3DCol_Line] : gp.Style.Colors[ImPlot3DCol_MarkerFill])
            : n.Colors[ImPlot3DCol_MarkerFill];

    // Set size & weight
    n.LineWeight = n.LineWeight < 0.0f ? style.LineWeight : n.LineWeight;
    n.Marker = n.Marker < 0 ? style.Marker : n.Marker;
    n.MarkerSize = n.MarkerSize < 0.0f ? style.MarkerSize : n.MarkerSize;
    n.MarkerWeight = n.MarkerWeight < 0.0f ? style.MarkerWeight : n.MarkerWeight;
    n.FillAlpha = n.FillAlpha < 0 ? gp.Style.FillAlpha : n.FillAlpha;

    // Apply alpha modifiers
    n.Colors[ImPlot3DCol_Fill].w *= n.FillAlpha;
    n.Colors[ImPlot3DCol_MarkerFill].w *= n.FillAlpha;

    // Set render flags
    n.RenderLine = n.Colors[ImPlot3DCol_Line].w > 0 && n.LineWeight > 0;
    n.RenderFill = n.Colors[ImPlot3DCol_Fill].w > 0;
    n.RenderMarkerFill = n.Colors[ImPlot3DCol_MarkerFill].w > 0;
    n.RenderMarkerLine = n.Colors[ImPlot3DCol_MarkerOutline].w > 0 && n.MarkerWeight > 0;

    // Don't render if item is hidden
    if (!item->Show) {
        EndItem();
        return false;
    } else {
        // Legend hover highlight
        if (item->LegendHovered) {
            if (!ImHasFlag(gp.CurrentItems->Legend.Flags, ImPlot3DLegendFlags_NoHighlightItem)) {
                n.LineWeight *= ITEM_HIGHLIGHT_LINE_SCALE;
                n.MarkerSize *= ITEM_HIGHLIGHT_MARK_SCALE;
                n.MarkerWeight *= ITEM_HIGHLIGHT_LINE_SCALE;
            }
        }
    }

    return true;
}

template <typename _Getter>
bool BeginItemEx(const char* label_id, const _Getter& getter, ImPlot3DItemFlags flags = 0, ImPlot3DCol recolor_from = IMPLOT3D_AUTO) {
    if (BeginItem(label_id, flags, recolor_from)) {
        ImPlot3DContext& gp = *GImPlot3D;
        ImPlot3DPlot& plot = *gp.CurrentPlot;
        if (plot.FitThisFrame && !ImHasFlag(flags, ImPlot3DItemFlags_NoFit)) {
            for (int i = 0; i < getter.Count; i++)
                plot.ExtendFit(getter(i));
        }
        return true;
    }
    return false;
}

void EndItem() {
    ImPlot3DContext& gp = *GImPlot3D;
    gp.NextItemData.Reset();
    gp.CurrentItem = nullptr;
}

ImPlot3DItem* RegisterOrGetItem(const char* label_id, ImPlot3DItemFlags flags, bool* just_created) {
    ImPlot3DContext& gp = *GImPlot3D;
    ImPlot3DItemGroup& Items = *gp.CurrentItems;
    ImGuiID id = Items.GetItemID(label_id);
    if (just_created != nullptr)
        *just_created = Items.GetItem(id) == nullptr;
    ImPlot3DItem* item = Items.GetOrAddItem(id);

    // Avoid re-adding the same item to the legend (the legend is reset every frame)
    if (item->SeenThisFrame)
        return item;
    item->SeenThisFrame = true;

    // Add item to the legend
    int idx = Items.GetItemIndex(item);
    item->ID = id;
    if (!ImHasFlag(flags, ImPlot3DItemFlags_NoLegend) && ImGui::FindRenderedTextEnd(label_id, nullptr) != label_id) {
        Items.Legend.Indices.push_back(idx);
        item->NameOffset = Items.Legend.Labels.size();
        Items.Legend.Labels.append(label_id, label_id + strlen(label_id) + 1);
    }
    return item;
}

ImPlot3DItem* GetCurrentItem() {
    ImPlot3DContext& gp = *GImPlot3D;
    return gp.CurrentItem;
}

void BustItemCache() {
    ImPlot3DContext& gp = *GImPlot3D;
    for (int p = 0; p < gp.Plots.GetBufSize(); ++p) {
        ImPlot3DPlot& plot = *gp.Plots.GetByIndex(p);
        plot.Items.Reset();
    }
}

void SetNextLineStyle(const ImVec4& col, float weight) {
    ImPlot3DContext& gp = *GImPlot3D;
    ImPlot3DNextItemData& n = gp.NextItemData;
    n.Colors[ImPlot3DCol_Line] = col;
    n.LineWeight = weight;
}

void SetNextFillStyle(const ImVec4& col, float alpha) {
    ImPlot3DContext& gp = *GImPlot3D;
    ImPlot3DNextItemData& n = gp.NextItemData;
    n.Colors[ImPlot3DCol_Fill] = col;
    n.FillAlpha = alpha;
}

void SetNextMarkerStyle(ImPlot3DMarker marker, float size, const ImVec4& fill, float weight, const ImVec4& outline) {
    ImPlot3DContext& gp = *GImPlot3D;
    ImPlot3DNextItemData& n = gp.NextItemData;
    n.Marker = marker;
    n.Colors[ImPlot3DCol_MarkerFill] = fill;
    n.MarkerSize = size;
    n.Colors[ImPlot3DCol_MarkerOutline] = outline;
    n.MarkerWeight = weight;
}

//-----------------------------------------------------------------------------
// [SECTION] Draw Utils
//-----------------------------------------------------------------------------

IMPLOT3D_INLINE void PrimLine(ImDrawList3D& draw_list_3d, const ImVec2& P1, const ImVec2& P2, float half_weight, ImU32 col, const ImVec2& tex_uv0,
                              const ImVec2& tex_uv1, float z) {
    float dx = P2.x - P1.x;
    float dy = P2.y - P1.y;
    IMPLOT3D_NORMALIZE2F(dx, dy);
    dx *= half_weight;
    dy *= half_weight;
    draw_list_3d._VtxWritePtr[0].pos.x = P1.x + dy;
    draw_list_3d._VtxWritePtr[0].pos.y = P1.y - dx;
    draw_list_3d._VtxWritePtr[0].uv = tex_uv0;
    draw_list_3d._VtxWritePtr[0].col = col;
    draw_list_3d._VtxWritePtr[1].pos.x = P2.x + dy;
    draw_list_3d._VtxWritePtr[1].pos.y = P2.y - dx;
    draw_list_3d._VtxWritePtr[1].uv = tex_uv0;
    draw_list_3d._VtxWritePtr[1].col = col;
    draw_list_3d._VtxWritePtr[2].pos.x = P2.x - dy;
    draw_list_3d._VtxWritePtr[2].pos.y = P2.y + dx;
    draw_list_3d._VtxWritePtr[2].uv = tex_uv1;
    draw_list_3d._VtxWritePtr[2].col = col;
    draw_list_3d._VtxWritePtr[3].pos.x = P1.x - dy;
    draw_list_3d._VtxWritePtr[3].pos.y = P1.y + dx;
    draw_list_3d._VtxWritePtr[3].uv = tex_uv1;
    draw_list_3d._VtxWritePtr[3].col = col;
    draw_list_3d._VtxWritePtr += 4;
    draw_list_3d._IdxWritePtr[0] = (ImDrawIdx)(draw_list_3d._VtxCurrentIdx);
    draw_list_3d._IdxWritePtr[1] = (ImDrawIdx)(draw_list_3d._VtxCurrentIdx + 1);
    draw_list_3d._IdxWritePtr[2] = (ImDrawIdx)(draw_list_3d._VtxCurrentIdx + 2);
    draw_list_3d._IdxWritePtr[3] = (ImDrawIdx)(draw_list_3d._VtxCurrentIdx);
    draw_list_3d._IdxWritePtr[4] = (ImDrawIdx)(draw_list_3d._VtxCurrentIdx + 2);
    draw_list_3d._IdxWritePtr[5] = (ImDrawIdx)(draw_list_3d._VtxCurrentIdx + 3);
    draw_list_3d._IdxWritePtr += 6;
    draw_list_3d._VtxCurrentIdx += 4;
    draw_list_3d._ZWritePtr[0] = z;
    draw_list_3d._ZWritePtr[1] = z;
    draw_list_3d._ZWritePtr += 2;
}

//-----------------------------------------------------------------------------
// [SECTION] Renderers
//-----------------------------------------------------------------------------

float GetPointDepth(ImPlot3DPoint p) {
    ImPlot3DContext& gp = *GImPlot3D;
    ImPlot3DPlot& plot = *gp.CurrentPlot;

    // Adjust for inverted axes before rotation
    if (ImHasFlag(plot.Axes[0].Flags, ImPlot3DAxisFlags_Invert))
        p.x = -p.x;
    if (ImHasFlag(plot.Axes[1].Flags, ImPlot3DAxisFlags_Invert))
        p.y = -p.y;
    if (ImHasFlag(plot.Axes[2].Flags, ImPlot3DAxisFlags_Invert))
        p.z = -p.z;

    ImPlot3DPoint p_rot = plot.Rotation * p;
    return p_rot.z;
}

struct RendererBase {
    RendererBase(int prims, int idx_consumed, int vtx_consumed) : Prims(prims), IdxConsumed(idx_consumed), VtxConsumed(vtx_consumed) {}
    const unsigned int Prims;       // Number of primitives to render
    const unsigned int IdxConsumed; // Number of indices consumed per primitive
    const unsigned int VtxConsumed; // Number of vertices consumed per primitive
};

template <class _Getter> struct RendererMarkersFill : RendererBase {
    RendererMarkersFill(const _Getter& getter, const ImVec2* marker, int count, float size, ImU32 col)
        : RendererBase(getter.Count, (count - 2) * 3, count), Getter(getter), Marker(marker), Count(count), Size(size), Col(col) {}

    void Init(ImDrawList3D& draw_list_3d) const { UV = draw_list_3d._SharedData->TexUvWhitePixel; }

    IMPLOT3D_INLINE bool Render(ImDrawList3D& draw_list_3d, const ImPlot3DBox& cull_box, int prim) const {
        ImPlot3DPoint p_plot = Getter(prim);
        if (!cull_box.Contains(p_plot))
            return false;
        ImVec2 p = PlotToPixels(p_plot);
        // 3 vertices per triangle
        for (int i = 0; i < Count; i++) {
            draw_list_3d._VtxWritePtr[0].pos.x = p.x + Marker[i].x * Size;
            draw_list_3d._VtxWritePtr[0].pos.y = p.y + Marker[i].y * Size;
            draw_list_3d._VtxWritePtr[0].uv = UV;
            draw_list_3d._VtxWritePtr[0].col = Col;
            draw_list_3d._VtxWritePtr++;
        }
        // 3 indices per triangle
        for (int i = 2; i < Count; i++) {
            // Indices
            draw_list_3d._IdxWritePtr[0] = (ImDrawIdx)(draw_list_3d._VtxCurrentIdx);
            draw_list_3d._IdxWritePtr[1] = (ImDrawIdx)(draw_list_3d._VtxCurrentIdx + i - 1);
            draw_list_3d._IdxWritePtr[2] = (ImDrawIdx)(draw_list_3d._VtxCurrentIdx + i);
            draw_list_3d._IdxWritePtr += 3;
            // Z
            draw_list_3d._ZWritePtr[0] = GetPointDepth(p_plot);
            draw_list_3d._ZWritePtr++;
        }
        // Update vertex count
        draw_list_3d._VtxCurrentIdx += (ImDrawIdx)Count;
        return true;
    }
    const _Getter& Getter;
    const ImVec2* Marker;
    const int Count;
    const float Size;
    const ImU32 Col;
    mutable ImVec2 UV;
};

template <class _Getter> struct RendererMarkersLine : RendererBase {
    RendererMarkersLine(const _Getter& getter, const ImVec2* marker, int count, float size, float weight, ImU32 col)
        : RendererBase(getter.Count, count / 2 * 6, count / 2 * 4), Getter(getter), Marker(marker), Count(count),
          HalfWeight(ImMax(1.0f, weight) * 0.5f), Size(size), Col(col) {}

    void Init(ImDrawList3D& draw_list_3d) const { GetLineRenderProps(draw_list_3d, HalfWeight, UV0, UV1); }

    IMPLOT3D_INLINE bool Render(ImDrawList3D& draw_list_3d, const ImPlot3DBox& cull_box, int prim) const {
        ImPlot3DPoint p_plot = Getter(prim);
        if (!cull_box.Contains(p_plot))
            return false;
        ImVec2 p = PlotToPixels(p_plot);
        for (int i = 0; i < Count; i = i + 2) {
            ImVec2 p1(p.x + Marker[i].x * Size, p.y + Marker[i].y * Size);
            ImVec2 p2(p.x + Marker[i + 1].x * Size, p.y + Marker[i + 1].y * Size);
            PrimLine(draw_list_3d, p1, p2, HalfWeight, Col, UV0, UV1, GetPointDepth(p_plot));
        }
        return true;
    }

    const _Getter& Getter;
    const ImVec2* Marker;
    const int Count;
    mutable float HalfWeight;
    const float Size;
    const ImU32 Col;
    mutable ImVec2 UV0;
    mutable ImVec2 UV1;
};

template <class _Getter> struct RendererLineStrip : RendererBase {
    RendererLineStrip(const _Getter& getter, ImU32 col, float weight)
        : RendererBase(getter.Count - 1, 6, 4), Getter(getter), Col(col), HalfWeight(ImMax(1.0f, weight) * 0.5f) {
        // Initialize the first point in plot coordinates
        P1_plot = Getter(0);
    }

    void Init(ImDrawList3D& draw_list_3d) const { GetLineRenderProps(draw_list_3d, HalfWeight, UV0, UV1); }

    IMPLOT3D_INLINE bool Render(ImDrawList3D& draw_list_3d, const ImPlot3DBox& cull_box, int prim) const {
        ImPlot3DPoint P2_plot = Getter(prim + 1);

        // Clip the line segment to the culling box using Liang-Barsky algorithm
        ImPlot3DPoint P1_clipped, P2_clipped;
        bool visible = cull_box.ClipLineSegment(P1_plot, P2_plot, P1_clipped, P2_clipped);

        if (visible) {
            // Convert clipped points to pixel coordinates
            ImVec2 P1_screen = PlotToPixels(P1_clipped);
            ImVec2 P2_screen = PlotToPixels(P2_clipped);
            // Render the line segment
            PrimLine(draw_list_3d, P1_screen, P2_screen, HalfWeight, Col, UV0, UV1, GetPointDepth((P1_plot + P2_plot) * 0.5f));
        }

        // Update for next segment
        P1_plot = P2_plot;

        return visible;
    }

    const _Getter& Getter;
    const ImU32 Col;
    mutable float HalfWeight;
    mutable ImPlot3DPoint P1_plot;
    mutable ImVec2 UV0;
    mutable ImVec2 UV1;
};

template <class _Getter> struct RendererLineStripSkip : RendererBase {
    RendererLineStripSkip(const _Getter& getter, ImU32 col, float weight)
        : RendererBase(getter.Count - 1, 6, 4), Getter(getter), Col(col), HalfWeight(ImMax(1.0f, weight) * 0.5f) {
        // Initialize the first point in plot coordinates
        P1_plot = Getter(0);
    }

    void Init(ImDrawList3D& draw_list_3d) const { GetLineRenderProps(draw_list_3d, HalfWeight, UV0, UV1); }

    IMPLOT3D_INLINE bool Render(ImDrawList3D& draw_list_3d, const ImPlot3DBox& cull_box, int prim) const {
        // Get the next point in plot coordinates
        ImPlot3DPoint P2_plot = Getter(prim + 1);
        bool visible = false;

        // Check for NaNs in P1_plot and P2_plot
        if (!ImNan(P1_plot.x) && !ImNan(P1_plot.y) && !ImNan(P1_plot.z) && !ImNan(P2_plot.x) && !ImNan(P2_plot.y) && !ImNan(P2_plot.z)) {

            // Clip the line segment to the culling box
            ImPlot3DPoint P1_clipped, P2_clipped;
            visible = cull_box.ClipLineSegment(P1_plot, P2_plot, P1_clipped, P2_clipped);

            if (visible) {
                // Convert clipped points to pixel coordinates
                ImVec2 P1_screen = PlotToPixels(P1_clipped);
                ImVec2 P2_screen = PlotToPixels(P2_clipped);
                // Render the line segment
                PrimLine(draw_list_3d, P1_screen, P2_screen, HalfWeight, Col, UV0, UV1, GetPointDepth((P1_plot + P2_plot) * 0.5f));
            }
        }

        // Update P1_plot if P2_plot is valid
        if (!ImNan(P2_plot.x) && !ImNan(P2_plot.y) && !ImNan(P2_plot.z))
            P1_plot = P2_plot;

        return visible;
    }

    const _Getter& Getter;
    const ImU32 Col;
    mutable float HalfWeight;
    mutable ImPlot3DPoint P1_plot;
    mutable ImVec2 UV0;
    mutable ImVec2 UV1;
};

template <class _Getter> struct RendererLineSegments : RendererBase {
    RendererLineSegments(const _Getter& getter, ImU32 col, float weight)
        : RendererBase(getter.Count / 2, 6, 4), Getter(getter), Col(col), HalfWeight(ImMax(1.0f, weight) * 0.5f) {}

    void Init(ImDrawList3D& draw_list_3d) const { GetLineRenderProps(draw_list_3d, HalfWeight, UV0, UV1); }

    IMPLOT3D_INLINE bool Render(ImDrawList3D& draw_list_3d, const ImPlot3DBox& cull_box, int prim) const {
        // Get the segment's endpoints in plot coordinates
        ImPlot3DPoint P1_plot = Getter(prim * 2 + 0);
        ImPlot3DPoint P2_plot = Getter(prim * 2 + 1);

        // Check for NaNs in P1_plot and P2_plot
        if (!ImNan(P1_plot.x) && !ImNan(P1_plot.y) && !ImNan(P1_plot.z) && !ImNan(P2_plot.x) && !ImNan(P2_plot.y) && !ImNan(P2_plot.z)) {

            // Clip the line segment to the culling box
            ImPlot3DPoint P1_clipped, P2_clipped;
            bool visible = cull_box.ClipLineSegment(P1_plot, P2_plot, P1_clipped, P2_clipped);

            if (visible) {
                // Convert clipped points to pixel coordinates
                ImVec2 P1_screen = PlotToPixels(P1_clipped);
                ImVec2 P2_screen = PlotToPixels(P2_clipped);
                // Render the line segment
                PrimLine(draw_list_3d, P1_screen, P2_screen, HalfWeight, Col, UV0, UV1, GetPointDepth((P1_plot + P2_plot) * 0.5f));
            }
            return visible;
        }

        return false;
    }

    const _Getter& Getter;
    const ImU32 Col;
    mutable float HalfWeight;
    mutable ImVec2 UV0;
    mutable ImVec2 UV1;
};

template <class _Getter> struct RendererTriangleFill : RendererBase {
    RendererTriangleFill(const _Getter& getter, ImU32 col) : RendererBase(getter.Count / 3, 3, 3), Getter(getter), Col(col) {}

    void Init(ImDrawList3D& draw_list_3d) const { UV = draw_list_3d._SharedData->TexUvWhitePixel; }

    IMPLOT3D_INLINE bool Render(ImDrawList3D& draw_list_3d, const ImPlot3DBox& cull_box, int prim) const {
        ImPlot3DPoint p_plot[3];
        p_plot[0] = Getter(3 * prim);
        p_plot[1] = Getter(3 * prim + 1);
        p_plot[2] = Getter(3 * prim + 2);

        // Check if the triangle is outside the culling box
        if (!cull_box.Contains(p_plot[0]) && !cull_box.Contains(p_plot[1]) && !cull_box.Contains(p_plot[2]))
            return false;

        // Project the triangle vertices to screen space
        ImVec2 p[3];
        p[0] = PlotToPixels(p_plot[0]);
        p[1] = PlotToPixels(p_plot[1]);
        p[2] = PlotToPixels(p_plot[2]);

        // 3 vertices per triangle
        draw_list_3d._VtxWritePtr[0].pos.x = p[0].x;
        draw_list_3d._VtxWritePtr[0].pos.y = p[0].y;
        draw_list_3d._VtxWritePtr[0].uv = UV;
        draw_list_3d._VtxWritePtr[0].col = Col;
        draw_list_3d._VtxWritePtr[1].pos.x = p[1].x;
        draw_list_3d._VtxWritePtr[1].pos.y = p[1].y;
        draw_list_3d._VtxWritePtr[1].uv = UV;
        draw_list_3d._VtxWritePtr[1].col = Col;
        draw_list_3d._VtxWritePtr[2].pos.x = p[2].x;
        draw_list_3d._VtxWritePtr[2].pos.y = p[2].y;
        draw_list_3d._VtxWritePtr[2].uv = UV;
        draw_list_3d._VtxWritePtr[2].col = Col;
        draw_list_3d._VtxWritePtr += 3;

        // 3 indices per triangle
        draw_list_3d._IdxWritePtr[0] = (ImDrawIdx)(draw_list_3d._VtxCurrentIdx);
        draw_list_3d._IdxWritePtr[1] = (ImDrawIdx)(draw_list_3d._VtxCurrentIdx + 1);
        draw_list_3d._IdxWritePtr[2] = (ImDrawIdx)(draw_list_3d._VtxCurrentIdx + 2);
        draw_list_3d._IdxWritePtr += 3;
        // 1 Z per vertex
        draw_list_3d._ZWritePtr[0] = GetPointDepth((p_plot[0] + p_plot[1] + p_plot[2]) / 3);
        draw_list_3d._ZWritePtr++;

        // Update vertex count
        draw_list_3d._VtxCurrentIdx += 3;

        return true;
    }

    const _Getter& Getter;
    mutable ImVec2 UV;
    const ImU32 Col;
};

template <class _Getter> struct RendererQuadFill : RendererBase {
    RendererQuadFill(const _Getter& getter, ImU32 col) : RendererBase(getter.Count / 4, 6, 4), Getter(getter), Col(col) {}

    void Init(ImDrawList3D& draw_list_3d) const { UV = draw_list_3d._SharedData->TexUvWhitePixel; }

    IMPLOT3D_INLINE bool Render(ImDrawList3D& draw_list_3d, const ImPlot3DBox& cull_box, int prim) const {
        ImPlot3DPoint p_plot[4];
        p_plot[0] = Getter(4 * prim);
        p_plot[1] = Getter(4 * prim + 1);
        p_plot[2] = Getter(4 * prim + 2);
        p_plot[3] = Getter(4 * prim + 3);

        // Check if the quad is outside the culling box
        if (!cull_box.Contains(p_plot[0]) && !cull_box.Contains(p_plot[1]) && !cull_box.Contains(p_plot[2]) && !cull_box.Contains(p_plot[3]))
            return false;

        // Project the quad vertices to screen space
        ImVec2 p[4];
        p[0] = PlotToPixels(p_plot[0]);
        p[1] = PlotToPixels(p_plot[1]);
        p[2] = PlotToPixels(p_plot[2]);
        p[3] = PlotToPixels(p_plot[3]);

        // Add vertices for two triangles
        draw_list_3d._VtxWritePtr[0].pos.x = p[0].x;
        draw_list_3d._VtxWritePtr[0].pos.y = p[0].y;
        draw_list_3d._VtxWritePtr[0].uv = UV;
        draw_list_3d._VtxWritePtr[0].col = Col;

        draw_list_3d._VtxWritePtr[1].pos.x = p[1].x;
        draw_list_3d._VtxWritePtr[1].pos.y = p[1].y;
        draw_list_3d._VtxWritePtr[1].uv = UV;
        draw_list_3d._VtxWritePtr[1].col = Col;

        draw_list_3d._VtxWritePtr[2].pos.x = p[2].x;
        draw_list_3d._VtxWritePtr[2].pos.y = p[2].y;
        draw_list_3d._VtxWritePtr[2].uv = UV;
        draw_list_3d._VtxWritePtr[2].col = Col;

        draw_list_3d._VtxWritePtr[3].pos.x = p[3].x;
        draw_list_3d._VtxWritePtr[3].pos.y = p[3].y;
        draw_list_3d._VtxWritePtr[3].uv = UV;
        draw_list_3d._VtxWritePtr[3].col = Col;

        draw_list_3d._VtxWritePtr += 4;

        // Add indices for two triangles
        draw_list_3d._IdxWritePtr[0] = (ImDrawIdx)(draw_list_3d._VtxCurrentIdx);
        draw_list_3d._IdxWritePtr[1] = (ImDrawIdx)(draw_list_3d._VtxCurrentIdx + 1);
        draw_list_3d._IdxWritePtr[2] = (ImDrawIdx)(draw_list_3d._VtxCurrentIdx + 2);

        draw_list_3d._IdxWritePtr[3] = (ImDrawIdx)(draw_list_3d._VtxCurrentIdx);
        draw_list_3d._IdxWritePtr[4] = (ImDrawIdx)(draw_list_3d._VtxCurrentIdx + 2);
        draw_list_3d._IdxWritePtr[5] = (ImDrawIdx)(draw_list_3d._VtxCurrentIdx + 3);

        draw_list_3d._IdxWritePtr += 6;

        // Add depth value for the quad
        float z = GetPointDepth((p_plot[0] + p_plot[1] + p_plot[2] + p_plot[3]) / 4.0f);
        draw_list_3d._ZWritePtr[0] = z;
        draw_list_3d._ZWritePtr[1] = z;
        draw_list_3d._ZWritePtr += 2;

        // Update vertex count
        draw_list_3d._VtxCurrentIdx += 4;

        return true;
    }

    const _Getter& Getter;
    mutable ImVec2 UV;
    const ImU32 Col;
};

template <class _Getter> struct RendererQuadImage : RendererBase {
    RendererQuadImage(const _Getter& getter, ImTextureRef tex_ref, const ImVec2& uv0, const ImVec2& uv1, const ImVec2& uv2, const ImVec2& uv3,
                      ImU32 col)
        : RendererBase(getter.Count / 4, 6, 4), Getter(getter), TexRef(tex_ref), UV0(uv0), UV1(uv1), UV2(uv2), UV3(uv3), Col(col) {}

    void Init(ImDrawList3D& draw_list_3d) const {}

    IMPLOT3D_INLINE bool Render(ImDrawList3D& draw_list_3d, const ImPlot3DBox& cull_box, int prim) const {
        ImPlot3DPoint p_plot[4];
        p_plot[0] = Getter(4 * prim);
        p_plot[1] = Getter(4 * prim + 1);
        p_plot[2] = Getter(4 * prim + 2);
        p_plot[3] = Getter(4 * prim + 3);

        // Check if the quad is outside the culling box
        if (!cull_box.Contains(p_plot[0]) && !cull_box.Contains(p_plot[1]) && !cull_box.Contains(p_plot[2]) && !cull_box.Contains(p_plot[3]))
            return false;

        // Set texture ID to be used when rendering this quad
        draw_list_3d.SetTexture(TexRef);

        // Project the quad vertices to screen space
        ImVec2 p[4];
        p[0] = PlotToPixels(p_plot[0]);
        p[1] = PlotToPixels(p_plot[1]);
        p[2] = PlotToPixels(p_plot[2]);
        p[3] = PlotToPixels(p_plot[3]);

        // Add vertices for two triangles
        draw_list_3d._VtxWritePtr[0].pos.x = p[0].x;
        draw_list_3d._VtxWritePtr[0].pos.y = p[0].y;
        draw_list_3d._VtxWritePtr[0].uv = UV0;
        draw_list_3d._VtxWritePtr[0].col = Col;

        draw_list_3d._VtxWritePtr[1].pos.x = p[1].x;
        draw_list_3d._VtxWritePtr[1].pos.y = p[1].y;
        draw_list_3d._VtxWritePtr[1].uv = UV1;
        draw_list_3d._VtxWritePtr[1].col = Col;

        draw_list_3d._VtxWritePtr[2].pos.x = p[2].x;
        draw_list_3d._VtxWritePtr[2].pos.y = p[2].y;
        draw_list_3d._VtxWritePtr[2].uv = UV2;
        draw_list_3d._VtxWritePtr[2].col = Col;

        draw_list_3d._VtxWritePtr[3].pos.x = p[3].x;
        draw_list_3d._VtxWritePtr[3].pos.y = p[3].y;
        draw_list_3d._VtxWritePtr[3].uv = UV3;
        draw_list_3d._VtxWritePtr[3].col = Col;

        draw_list_3d._VtxWritePtr += 4;

        // Add indices for two triangles
        draw_list_3d._IdxWritePtr[0] = (ImDrawIdx)(draw_list_3d._VtxCurrentIdx);
        draw_list_3d._IdxWritePtr[1] = (ImDrawIdx)(draw_list_3d._VtxCurrentIdx + 1);
        draw_list_3d._IdxWritePtr[2] = (ImDrawIdx)(draw_list_3d._VtxCurrentIdx + 2);

        draw_list_3d._IdxWritePtr[3] = (ImDrawIdx)(draw_list_3d._VtxCurrentIdx);
        draw_list_3d._IdxWritePtr[4] = (ImDrawIdx)(draw_list_3d._VtxCurrentIdx + 2);
        draw_list_3d._IdxWritePtr[5] = (ImDrawIdx)(draw_list_3d._VtxCurrentIdx + 3);

        draw_list_3d._IdxWritePtr += 6;

        // Add depth value for the quad
        float z = GetPointDepth((p_plot[0] + p_plot[1] + p_plot[2] + p_plot[3]) / 4.0f);
        draw_list_3d._ZWritePtr[0] = z;
        draw_list_3d._ZWritePtr[1] = z;
        draw_list_3d._ZWritePtr += 2;

        // Update vertex count
        draw_list_3d._VtxCurrentIdx += 4;

        // Reset texture ID
        draw_list_3d.ResetTexture();

        return true;
    }

    const _Getter& Getter;
    const ImTextureRef TexRef;
    const ImVec2 UV0, UV1, UV2, UV3;
    const ImU32 Col;
};

template <class _Getter> struct RendererSurfaceFill : RendererBase {
    RendererSurfaceFill(const _Getter& getter, int x_count, int y_count, ImU32 col, double scale_min, double scale_max)
        : RendererBase((x_count - 1) * (y_count - 1), 6, 4), Getter(getter), XCount(x_count), YCount(y_count), Col(col), ScaleMin(scale_min),
          ScaleMax(scale_max) {}

    void Init(ImDrawList3D& draw_list_3d) const {
        UV = draw_list_3d._SharedData->TexUvWhitePixel;

        // Compute min and max values for the colormap (if not solid fill)
        const ImPlot3DNextItemData& n = GetItemData();
        if (n.IsAutoFill) {
            Min = FLT_MAX;
            Max = -FLT_MAX;
            for (int i = 0; i < Getter.Count; i++) {
                float z = Getter(i).z;
                Min = ImMin(Min, z);
                Max = ImMax(Max, z);
            }
        }
    }

    IMPLOT3D_INLINE bool Render(ImDrawList3D& draw_list_3d, const ImPlot3DBox& cull_box, int prim) const {
        int x = prim % (XCount - 1);
        int y = prim / (XCount - 1);

        ImPlot3DPoint p_plot[4];
        p_plot[0] = Getter(x + y * XCount);
        p_plot[1] = Getter(x + 1 + y * XCount);
        p_plot[2] = Getter(x + 1 + (y + 1) * XCount);
        p_plot[3] = Getter(x + (y + 1) * XCount);

        // Check if the quad is outside the culling box
        if (!cull_box.Contains(p_plot[0]) && !cull_box.Contains(p_plot[1]) && !cull_box.Contains(p_plot[2]) && !cull_box.Contains(p_plot[3]))
            return false;

        // Compute colors
        ImU32 cols[4] = {Col, Col, Col, Col};
        const ImPlot3DNextItemData& n = GetItemData();
        if (n.IsAutoFill) {
            float alpha = GImPlot3D->NextItemData.FillAlpha;
            float min = Min;
            float max = Max;
            if (ScaleMin != 0.0 || ScaleMax != 0.0) {
                min = (float)ScaleMin;
                max = (float)ScaleMax;
            }
            for (int i = 0; i < 4; i++) {
                ImVec4 col = SampleColormap(ImClamp(ImRemap01(p_plot[i].z, min, max), 0.0f, 1.0f));
                col.w *= alpha;
                cols[i] = ImGui::ColorConvertFloat4ToU32(col);
            }
        }

        // Project the quad vertices to screen space
        ImVec2 p[4];
        p[0] = PlotToPixels(p_plot[0]);
        p[1] = PlotToPixels(p_plot[1]);
        p[2] = PlotToPixels(p_plot[2]);
        p[3] = PlotToPixels(p_plot[3]);

        // Add vertices for two triangles
        draw_list_3d._VtxWritePtr[0].pos.x = p[0].x;
        draw_list_3d._VtxWritePtr[0].pos.y = p[0].y;
        draw_list_3d._VtxWritePtr[0].uv = UV;
        draw_list_3d._VtxWritePtr[0].col = cols[0];

        draw_list_3d._VtxWritePtr[1].pos.x = p[1].x;
        draw_list_3d._VtxWritePtr[1].pos.y = p[1].y;
        draw_list_3d._VtxWritePtr[1].uv = UV;
        draw_list_3d._VtxWritePtr[1].col = cols[1];

        draw_list_3d._VtxWritePtr[2].pos.x = p[2].x;
        draw_list_3d._VtxWritePtr[2].pos.y = p[2].y;
        draw_list_3d._VtxWritePtr[2].uv = UV;
        draw_list_3d._VtxWritePtr[2].col = cols[2];

        draw_list_3d._VtxWritePtr[3].pos.x = p[3].x;
        draw_list_3d._VtxWritePtr[3].pos.y = p[3].y;
        draw_list_3d._VtxWritePtr[3].uv = UV;
        draw_list_3d._VtxWritePtr[3].col = cols[3];

        draw_list_3d._VtxWritePtr += 4;

        // Add indices for two triangles
        draw_list_3d._IdxWritePtr[0] = (ImDrawIdx)(draw_list_3d._VtxCurrentIdx);
        draw_list_3d._IdxWritePtr[1] = (ImDrawIdx)(draw_list_3d._VtxCurrentIdx + 1);
        draw_list_3d._IdxWritePtr[2] = (ImDrawIdx)(draw_list_3d._VtxCurrentIdx + 2);

        draw_list_3d._IdxWritePtr[3] = (ImDrawIdx)(draw_list_3d._VtxCurrentIdx);
        draw_list_3d._IdxWritePtr[4] = (ImDrawIdx)(draw_list_3d._VtxCurrentIdx + 2);
        draw_list_3d._IdxWritePtr[5] = (ImDrawIdx)(draw_list_3d._VtxCurrentIdx + 3);

        draw_list_3d._IdxWritePtr += 6;

        // Add depth values for the two triangles
        draw_list_3d._ZWritePtr[0] = GetPointDepth((p_plot[0] + p_plot[1] + p_plot[2]) / 3.0f);
        draw_list_3d._ZWritePtr[1] = GetPointDepth((p_plot[0] + p_plot[2] + p_plot[3]) / 3.0f);
        draw_list_3d._ZWritePtr += 2;

        // Update vertex count
        draw_list_3d._VtxCurrentIdx += 4;

        return true;
    }

    const _Getter& Getter;
    mutable ImVec2 UV;
    mutable float Min; // Minimum value for the colormap
    mutable float Max; // Minimum value for the colormap
    const int XCount;
    const int YCount;
    const ImU32 Col;
    const double ScaleMin;
    const double ScaleMax;
};

//-----------------------------------------------------------------------------
// [SECTION] Indexers
//-----------------------------------------------------------------------------

template <typename T> IMPLOT3D_INLINE T IndexData(const T* data, int idx, int count, int offset, int stride) {
    const int s = ((offset == 0) << 0) | ((stride == sizeof(T)) << 1);
    switch (s) {
        case 3: return data[idx];
        case 2: return data[(offset + idx) % count];
        case 1: return *(const T*)(const void*)((const unsigned char*)data + (size_t)((idx))*stride);
        case 0: return *(const T*)(const void*)((const unsigned char*)data + (size_t)((offset + idx) % count) * stride);
        default: return T(0);
    }
}

template <typename T> struct IndexerIdx {
    IndexerIdx(const T* data, int count, int offset = 0, int stride = sizeof(T)) : Data(data), Count(count), Offset(offset), Stride(stride) {}
    template <typename I> IMPLOT3D_INLINE double operator()(I idx) const { return (double)IndexData(Data, idx, Count, Offset, Stride); }
    const T* Data;
    int Count;
    int Offset;
    int Stride;
};

//-----------------------------------------------------------------------------
// [SECTION] Getters
//-----------------------------------------------------------------------------

template <typename _IndexerX, typename _IndexerY, typename _IndexerZ> struct GetterXYZ {
    GetterXYZ(_IndexerX x, _IndexerY y, _IndexerZ z, int count) : IndexerX(x), IndexerY(y), IndexerZ(z), Count(count) {}
    template <typename I> IMPLOT3D_INLINE ImPlot3DPoint operator()(I idx) const {
        return ImPlot3DPoint((float)IndexerX(idx), (float)IndexerY(idx), (float)IndexerZ(idx));
    }
    const _IndexerX IndexerX;
    const _IndexerY IndexerY;
    const _IndexerZ IndexerZ;
    const int Count;
};

template <typename _Getter> struct GetterLoop {
    GetterLoop(_Getter getter) : Getter(getter), Count(getter.Count + 1) {}
    template <typename I> IMPLOT3D_INLINE ImPlot3DPoint operator()(I idx) const {
        idx = idx % (Count - 1);
        return Getter(idx);
    }
    const _Getter Getter;
    const int Count;
};

template <typename _Getter> struct GetterTriangleLines {
    GetterTriangleLines(_Getter getter) : Getter(getter), Count(getter.Count * 2) {}
    template <typename I> IMPLOT3D_INLINE ImPlot3DPoint operator()(I idx) const {
        idx = ((idx % 6 + 1) / 2) % 3 + idx / 6 * 3;
        return Getter(idx);
    }
    const _Getter Getter;
    const int Count;
};

template <typename _Getter> struct GetterQuadLines {
    GetterQuadLines(_Getter getter) : Getter(getter), Count(getter.Count * 2) {}
    template <typename I> IMPLOT3D_INLINE ImPlot3DPoint operator()(I idx) const {
        idx = ((idx % 8 + 1) / 2) % 4 + idx / 8 * 4;
        return Getter(idx);
    }
    const _Getter Getter;
    const int Count;
};

template <typename _Getter> struct GetterSurfaceLines {
    GetterSurfaceLines(_Getter getter, int x_count, int y_count) : Getter(getter), XCount(x_count), YCount(y_count) {
        int horizontal_segments = (XCount - 1) * YCount;
        int vertical_segments = (YCount - 1) * XCount;
        int segments = horizontal_segments + vertical_segments;
        Count = segments * 2; // Each segment has 2 endpoints
    }

    template <typename I> IMPLOT3D_INLINE ImPlot3DPoint operator()(I idx) const {
        // idx is an endpoint index
        int endpoint_i = (int)(idx % 2);
        int segment_i = (int)(idx / 2);

        int horizontal_segments = (XCount - 1) * YCount;

        int px, py;
        if (segment_i < horizontal_segments) {
            // Horizontal segment
            int row = segment_i / (XCount - 1);
            int col = segment_i % (XCount - 1);
            // Endpoint 0 is (col, row), endpoint 1 is (col+1, row)
            px = endpoint_i == 0 ? col : col + 1;
            py = row;
        } else {
            // Vertical segment
            int seg_v = segment_i - horizontal_segments;
            int col = seg_v / (YCount - 1);
            int row = seg_v % (YCount - 1);
            // Endpoint 0 is (col, row), endpoint 1 is (col, row+1)
            px = col;
            py = row + endpoint_i;
        }

        return Getter(py * XCount + px);
    }

    const _Getter Getter;
    int Count;
    const int XCount;
    const int YCount;
};

struct Getter3DPoints {
    Getter3DPoints(const ImPlot3DPoint* points, int count) : Points(points), Count(count) {}
    template <typename I> IMPLOT3D_INLINE ImPlot3DPoint operator()(I idx) const { return Points[idx]; }
    const ImPlot3DPoint* Points;
    const int Count;
};

struct GetterMeshTriangles {
    GetterMeshTriangles(const ImPlot3DPoint* vtx, const unsigned int* idx, int idx_count)
        : Vtx(vtx), Idx(idx), IdxCount(idx_count), TriCount(idx_count / 3), Count(idx_count) {}

    template <typename I> IMPLOT3D_INLINE ImPlot3DPoint operator()(I i) const {
        unsigned int vi = Idx[i];
        return Vtx[vi];
    }

    const ImPlot3DPoint* Vtx;
    const unsigned int* Idx;
    int IdxCount;
    int TriCount;
    int Count;
};

//-----------------------------------------------------------------------------
// [SECTION] RenderPrimitives
//-----------------------------------------------------------------------------

/// Renders primitive shapes
template <template <class> class _Renderer, class _Getter, typename... Args> void RenderPrimitives(const _Getter& getter, Args... args) {
    _Renderer<_Getter> renderer(getter, args...);
    ImPlot3DPlot& plot = *GetCurrentPlot();
    ImDrawList3D& draw_list_3d = plot.DrawList;
    ImPlot3DBox cull_box;
    if (ImHasFlag(plot.Flags, ImPlot3DFlags_NoClip)) {
        cull_box.Min = ImPlot3DPoint(-HUGE_VAL, -HUGE_VAL, -HUGE_VAL);
        cull_box.Max = ImPlot3DPoint(HUGE_VAL, HUGE_VAL, HUGE_VAL);
    } else {
        cull_box.Min = plot.RangeMin();
        cull_box.Max = plot.RangeMax();
    }

    // Find how many can be reserved up to end of current draw command's limit
    unsigned int prims_to_render = ImMin(renderer.Prims, (ImDrawList3D::MaxIdx() - draw_list_3d._VtxCurrentIdx) / renderer.VtxConsumed);

    // Reserve vertices and indices to render the primitives
    draw_list_3d.PrimReserve(prims_to_render * renderer.IdxConsumed, prims_to_render * renderer.VtxConsumed);

    // Initialize renderer
    renderer.Init(draw_list_3d);

    // Render primitives
    int num_culled = 0;
    for (unsigned int i = 0; i < prims_to_render; i++)
        if (!renderer.Render(draw_list_3d, cull_box, i))
            num_culled++;
    // Unreserve unused vertices and indices
    draw_list_3d.PrimUnreserve(num_culled * renderer.IdxConsumed, num_culled * renderer.VtxConsumed);
}

//-----------------------------------------------------------------------------
// [SECTION] Markers
//-----------------------------------------------------------------------------

static const ImVec2 MARKER_FILL_CIRCLE[10] = {ImVec2(1.0f, 0.0f),
                                              ImVec2(0.809017f, 0.58778524f),
                                              ImVec2(0.30901697f, 0.95105654f),
                                              ImVec2(-0.30901703f, 0.9510565f),
                                              ImVec2(-0.80901706f, 0.5877852f),
                                              ImVec2(-1.0f, 0.0f),
                                              ImVec2(-0.80901694f, -0.58778536f),
                                              ImVec2(-0.3090171f, -0.9510565f),
                                              ImVec2(0.30901712f, -0.9510565f),
                                              ImVec2(0.80901694f, -0.5877853f)};
static const ImVec2 MARKER_FILL_SQUARE[4] = {ImVec2(SQRT_1_2, SQRT_1_2), ImVec2(SQRT_1_2, -SQRT_1_2), ImVec2(-SQRT_1_2, -SQRT_1_2),
                                             ImVec2(-SQRT_1_2, SQRT_1_2)};
static const ImVec2 MARKER_FILL_DIAMOND[4] = {ImVec2(1, 0), ImVec2(0, -1), ImVec2(-1, 0), ImVec2(0, 1)};
static const ImVec2 MARKER_FILL_UP[3] = {ImVec2(SQRT_3_2, 0.5f), ImVec2(0, -1), ImVec2(-SQRT_3_2, 0.5f)};
static const ImVec2 MARKER_FILL_DOWN[3] = {ImVec2(SQRT_3_2, -0.5f), ImVec2(0, 1), ImVec2(-SQRT_3_2, -0.5f)};
static const ImVec2 MARKER_FILL_LEFT[3] = {ImVec2(-1, 0), ImVec2(0.5, SQRT_3_2), ImVec2(0.5, -SQRT_3_2)};
static const ImVec2 MARKER_FILL_RIGHT[3] = {ImVec2(1, 0), ImVec2(-0.5, SQRT_3_2), ImVec2(-0.5, -SQRT_3_2)};
static const ImVec2 MARKER_LINE_CIRCLE[20] = {ImVec2(1.0f, 0.0f),
                                              ImVec2(0.809017f, 0.58778524f),
                                              ImVec2(0.809017f, 0.58778524f),
                                              ImVec2(0.30901697f, 0.95105654f),
                                              ImVec2(0.30901697f, 0.95105654f),
                                              ImVec2(-0.30901703f, 0.9510565f),
                                              ImVec2(-0.30901703f, 0.9510565f),
                                              ImVec2(-0.80901706f, 0.5877852f),
                                              ImVec2(-0.80901706f, 0.5877852f),
                                              ImVec2(-1.0f, 0.0f),
                                              ImVec2(-1.0f, 0.0f),
                                              ImVec2(-0.80901694f, -0.58778536f),
                                              ImVec2(-0.80901694f, -0.58778536f),
                                              ImVec2(-0.3090171f, -0.9510565f),
                                              ImVec2(-0.3090171f, -0.9510565f),
                                              ImVec2(0.30901712f, -0.9510565f),
                                              ImVec2(0.30901712f, -0.9510565f),
                                              ImVec2(0.80901694f, -0.5877853f),
                                              ImVec2(0.80901694f, -0.5877853f),
                                              ImVec2(1.0f, 0.0f)};
static const ImVec2 MARKER_LINE_SQUARE[8] = {ImVec2(SQRT_1_2, SQRT_1_2),   ImVec2(SQRT_1_2, -SQRT_1_2),  ImVec2(SQRT_1_2, -SQRT_1_2),
                                             ImVec2(-SQRT_1_2, -SQRT_1_2), ImVec2(-SQRT_1_2, -SQRT_1_2), ImVec2(-SQRT_1_2, SQRT_1_2),
                                             ImVec2(-SQRT_1_2, SQRT_1_2),  ImVec2(SQRT_1_2, SQRT_1_2)};
static const ImVec2 MARKER_LINE_DIAMOND[8] = {ImVec2(1, 0),  ImVec2(0, -1), ImVec2(0, -1), ImVec2(-1, 0),
                                              ImVec2(-1, 0), ImVec2(0, 1),  ImVec2(0, 1),  ImVec2(1, 0)};
static const ImVec2 MARKER_LINE_UP[6] = {ImVec2(SQRT_3_2, 0.5f),  ImVec2(0, -1),           ImVec2(0, -1),
                                         ImVec2(-SQRT_3_2, 0.5f), ImVec2(-SQRT_3_2, 0.5f), ImVec2(SQRT_3_2, 0.5f)};
static const ImVec2 MARKER_LINE_DOWN[6] = {ImVec2(SQRT_3_2, -0.5f),  ImVec2(0, 1),           ImVec2(0, 1), ImVec2(-SQRT_3_2, -0.5f),
                                           ImVec2(-SQRT_3_2, -0.5f), ImVec2(SQRT_3_2, -0.5f)};
static const ImVec2 MARKER_LINE_LEFT[6] = {ImVec2(-1, 0),          ImVec2(0.5, SQRT_3_2),  ImVec2(0.5, SQRT_3_2),
                                           ImVec2(0.5, -SQRT_3_2), ImVec2(0.5, -SQRT_3_2), ImVec2(-1, 0)};
static const ImVec2 MARKER_LINE_RIGHT[6] = {
    ImVec2(1, 0), ImVec2(-0.5, SQRT_3_2), ImVec2(-0.5, SQRT_3_2), ImVec2(-0.5, -SQRT_3_2), ImVec2(-0.5, -SQRT_3_2), ImVec2(1, 0)};
static const ImVec2 MARKER_LINE_ASTERISK[6] = {ImVec2(-SQRT_3_2, -0.5f), ImVec2(SQRT_3_2, 0.5f), ImVec2(-SQRT_3_2, 0.5f),
                                               ImVec2(SQRT_3_2, -0.5f),  ImVec2(0, -1),          ImVec2(0, 1)};
static const ImVec2 MARKER_LINE_PLUS[4] = {ImVec2(-1, 0), ImVec2(1, 0), ImVec2(0, -1), ImVec2(0, 1)};
static const ImVec2 MARKER_LINE_CROSS[4] = {ImVec2(-SQRT_1_2, -SQRT_1_2), ImVec2(SQRT_1_2, SQRT_1_2), ImVec2(SQRT_1_2, -SQRT_1_2),
                                            ImVec2(-SQRT_1_2, SQRT_1_2)};

template <typename _Getter> void RenderMarkers(const _Getter& getter, ImPlot3DMarker marker, float size, bool rend_fill, ImU32 col_fill,
                                               bool rend_line, ImU32 col_line, float weight) {
    if (rend_fill) {
        switch (marker) {
            case ImPlot3DMarker_Circle: RenderPrimitives<RendererMarkersFill>(getter, MARKER_FILL_CIRCLE, 10, size, col_fill); break;
            case ImPlot3DMarker_Square: RenderPrimitives<RendererMarkersFill>(getter, MARKER_FILL_SQUARE, 4, size, col_fill); break;
            case ImPlot3DMarker_Diamond: RenderPrimitives<RendererMarkersFill>(getter, MARKER_FILL_DIAMOND, 4, size, col_fill); break;
            case ImPlot3DMarker_Up: RenderPrimitives<RendererMarkersFill>(getter, MARKER_FILL_UP, 3, size, col_fill); break;
            case ImPlot3DMarker_Down: RenderPrimitives<RendererMarkersFill>(getter, MARKER_FILL_DOWN, 3, size, col_fill); break;
            case ImPlot3DMarker_Left: RenderPrimitives<RendererMarkersFill>(getter, MARKER_FILL_LEFT, 3, size, col_fill); break;
            case ImPlot3DMarker_Right: RenderPrimitives<RendererMarkersFill>(getter, MARKER_FILL_RIGHT, 3, size, col_fill); break;
        }
    }
    if (rend_line) {
        switch (marker) {
            case ImPlot3DMarker_Circle: RenderPrimitives<RendererMarkersLine>(getter, MARKER_LINE_CIRCLE, 20, size, weight, col_line); break;
            case ImPlot3DMarker_Square: RenderPrimitives<RendererMarkersLine>(getter, MARKER_LINE_SQUARE, 8, size, weight, col_line); break;
            case ImPlot3DMarker_Diamond: RenderPrimitives<RendererMarkersLine>(getter, MARKER_LINE_DIAMOND, 8, size, weight, col_line); break;
            case ImPlot3DMarker_Up: RenderPrimitives<RendererMarkersLine>(getter, MARKER_LINE_UP, 6, size, weight, col_line); break;
            case ImPlot3DMarker_Down: RenderPrimitives<RendererMarkersLine>(getter, MARKER_LINE_DOWN, 6, size, weight, col_line); break;
            case ImPlot3DMarker_Left: RenderPrimitives<RendererMarkersLine>(getter, MARKER_LINE_LEFT, 6, size, weight, col_line); break;
            case ImPlot3DMarker_Right: RenderPrimitives<RendererMarkersLine>(getter, MARKER_LINE_RIGHT, 6, size, weight, col_line); break;
            case ImPlot3DMarker_Asterisk: RenderPrimitives<RendererMarkersLine>(getter, MARKER_LINE_ASTERISK, 6, size, weight, col_line); break;
            case ImPlot3DMarker_Plus: RenderPrimitives<RendererMarkersLine>(getter, MARKER_LINE_PLUS, 4, size, weight, col_line); break;
            case ImPlot3DMarker_Cross: RenderPrimitives<RendererMarkersLine>(getter, MARKER_LINE_CROSS, 4, size, weight, col_line); break;
        }
    }
}

//-----------------------------------------------------------------------------
// [SECTION] PlotScatter
//-----------------------------------------------------------------------------

template <typename Getter> void PlotScatterEx(const char* label_id, const Getter& getter, ImPlot3DScatterFlags flags) {
    if (BeginItemEx(label_id, getter, flags, ImPlot3DCol_MarkerOutline)) {
        const ImPlot3DNextItemData& n = GetItemData();
        ImPlot3DMarker marker = n.Marker == ImPlot3DMarker_None ? ImPlot3DMarker_Circle : n.Marker;
        const ImU32 col_line = ImGui::GetColorU32(n.Colors[ImPlot3DCol_MarkerOutline]);
        const ImU32 col_fill = ImGui::GetColorU32(n.Colors[ImPlot3DCol_MarkerFill]);
        if (marker != ImPlot3DMarker_None)
            RenderMarkers<Getter>(getter, marker, n.MarkerSize, n.RenderMarkerFill, col_fill, n.RenderMarkerLine, col_line, n.MarkerWeight);
        EndItem();
    }
}

template <typename T>
void PlotScatter(const char* label_id, const T* xs, const T* ys, const T* zs, int count, ImPlot3DScatterFlags flags, int offset, int stride) {
    if (count < 1)
        return;
    GetterXYZ<IndexerIdx<T>, IndexerIdx<T>, IndexerIdx<T>> getter(IndexerIdx<T>(xs, count, offset, stride), IndexerIdx<T>(ys, count, offset, stride),
                                                                  IndexerIdx<T>(zs, count, offset, stride), count);
    return PlotScatterEx(label_id, getter, flags);
}

#define INSTANTIATE_MACRO(T)                                                                                                                         \
    template IMPLOT3D_API void PlotScatter<T>(const char* label_id, const T* xs, const T* ys, const T* zs, int count, ImPlot3DScatterFlags flags,    \
                                              int offset, int stride);
CALL_INSTANTIATE_FOR_NUMERIC_TYPES()
#undef INSTANTIATE_MACRO

//-----------------------------------------------------------------------------
// [SECTION] PlotLine
//-----------------------------------------------------------------------------

template <typename _Getter> void PlotLineEx(const char* label_id, const _Getter& getter, ImPlot3DLineFlags flags) {
    if (BeginItemEx(label_id, getter, flags, ImPlot3DCol_Line)) {
        const ImPlot3DNextItemData& n = GetItemData();
        if (getter.Count >= 2 && n.RenderLine) {
            const ImU32 col_line = ImGui::GetColorU32(n.Colors[ImPlot3DCol_Line]);
            if (ImHasFlag(flags, ImPlot3DLineFlags_Segments)) {
                RenderPrimitives<RendererLineSegments>(getter, col_line, n.LineWeight);
            } else if (ImHasFlag(flags, ImPlot3DLineFlags_Loop)) {
                if (ImHasFlag(flags, ImPlot3DLineFlags_SkipNaN))
                    RenderPrimitives<RendererLineStripSkip>(GetterLoop<_Getter>(getter), col_line, n.LineWeight);
                else
                    RenderPrimitives<RendererLineStrip>(GetterLoop<_Getter>(getter), col_line, n.LineWeight);
            } else {
                if (ImHasFlag(flags, ImPlot3DLineFlags_SkipNaN))
                    RenderPrimitives<RendererLineStripSkip>(getter, col_line, n.LineWeight);
                else
                    RenderPrimitives<RendererLineStrip>(getter, col_line, n.LineWeight);
            }
        }

        // Render markers
        if (n.Marker != ImPlot3DMarker_None) {
            const ImU32 col_line = ImGui::GetColorU32(n.Colors[ImPlot3DCol_MarkerOutline]);
            const ImU32 col_fill = ImGui::GetColorU32(n.Colors[ImPlot3DCol_MarkerFill]);
            RenderMarkers<_Getter>(getter, n.Marker, n.MarkerSize, n.RenderMarkerFill, col_fill, n.RenderMarkerLine, col_line, n.MarkerWeight);
        }
        EndItem();
    }
}

IMPLOT3D_TMP void PlotLine(const char* label_id, const T* xs, const T* ys, const T* zs, int count, ImPlot3DLineFlags flags, int offset, int stride) {
    if (count < 2)
        return;
    GetterXYZ<IndexerIdx<T>, IndexerIdx<T>, IndexerIdx<T>> getter(IndexerIdx<T>(xs, count, offset, stride), IndexerIdx<T>(ys, count, offset, stride),
                                                                  IndexerIdx<T>(zs, count, offset, stride), count);
    return PlotLineEx(label_id, getter, flags);
}

#define INSTANTIATE_MACRO(T)                                                                                                                         \
    template IMPLOT3D_API void PlotLine<T>(const char* label_id, const T* xs, const T* ys, const T* zs, int count, ImPlot3DLineFlags flags,          \
                                           int offset, int stride);
CALL_INSTANTIATE_FOR_NUMERIC_TYPES()
#undef INSTANTIATE_MACRO

//-----------------------------------------------------------------------------
// [SECTION] PlotTriangle
//-----------------------------------------------------------------------------

template <typename _Getter> void PlotTriangleEx(const char* label_id, const _Getter& getter, ImPlot3DTriangleFlags flags) {
    if (BeginItemEx(label_id, getter, flags, ImPlot3DCol_Fill)) {
        const ImPlot3DNextItemData& n = GetItemData();

        // Render fill
        if (getter.Count >= 3 && n.RenderFill && !ImHasFlag(flags, ImPlot3DTriangleFlags_NoFill)) {
            const ImU32 col_fill = ImGui::GetColorU32(n.Colors[ImPlot3DCol_Fill]);
            RenderPrimitives<RendererTriangleFill>(getter, col_fill);
        }

        // Render lines
        if (getter.Count >= 2 && n.RenderLine && !ImHasFlag(flags, ImPlot3DTriangleFlags_NoLines)) {
            const ImU32 col_line = ImGui::GetColorU32(n.Colors[ImPlot3DCol_Line]);
            RenderPrimitives<RendererLineSegments>(GetterTriangleLines<_Getter>(getter), col_line, n.LineWeight);
        }

        // Render markers
        if (n.Marker != ImPlot3DMarker_None && !ImHasFlag(flags, ImPlot3DTriangleFlags_NoMarkers)) {
            const ImU32 col_line = ImGui::GetColorU32(n.Colors[ImPlot3DCol_MarkerOutline]);
            const ImU32 col_fill = ImGui::GetColorU32(n.Colors[ImPlot3DCol_MarkerFill]);
            RenderMarkers<_Getter>(getter, n.Marker, n.MarkerSize, n.RenderMarkerFill, col_fill, n.RenderMarkerLine, col_line, n.MarkerWeight);
        }

        EndItem();
    }
}

IMPLOT3D_TMP void PlotTriangle(const char* label_id, const T* xs, const T* ys, const T* zs, int count, ImPlot3DTriangleFlags flags, int offset,
                               int stride) {
    if (count < 3)
        return;
    GetterXYZ<IndexerIdx<T>, IndexerIdx<T>, IndexerIdx<T>> getter(IndexerIdx<T>(xs, count, offset, stride), IndexerIdx<T>(ys, count, offset, stride),
                                                                  IndexerIdx<T>(zs, count, offset, stride), count);
    return PlotTriangleEx(label_id, getter, flags);
}

#define INSTANTIATE_MACRO(T)                                                                                                                         \
    template IMPLOT3D_API void PlotTriangle<T>(const char* label_id, const T* xs, const T* ys, const T* zs, int count, ImPlot3DTriangleFlags flags,  \
                                               int offset, int stride);
CALL_INSTANTIATE_FOR_NUMERIC_TYPES()
#undef INSTANTIATE_MACRO

//-----------------------------------------------------------------------------
// [SECTION] PlotQuad
//-----------------------------------------------------------------------------

template <typename _Getter> void PlotQuadEx(const char* label_id, const _Getter& getter, ImPlot3DQuadFlags flags) {
    if (BeginItemEx(label_id, getter, flags, ImPlot3DCol_Fill)) {
        const ImPlot3DNextItemData& n = GetItemData();

        // Render fill
        if (getter.Count >= 4 && n.RenderFill && !ImHasFlag(flags, ImPlot3DQuadFlags_NoFill)) {
            const ImU32 col_fill = ImGui::GetColorU32(n.Colors[ImPlot3DCol_Fill]);
            RenderPrimitives<RendererQuadFill>(getter, col_fill);
        }

        // Render lines
        if (getter.Count >= 2 && n.RenderLine && !ImHasFlag(flags, ImPlot3DQuadFlags_NoLines)) {
            const ImU32 col_line = ImGui::GetColorU32(n.Colors[ImPlot3DCol_Line]);
            RenderPrimitives<RendererLineSegments>(GetterQuadLines<_Getter>(getter), col_line, n.LineWeight);
        }

        // Render markers
        if (n.Marker != ImPlot3DMarker_None && !ImHasFlag(flags, ImPlot3DQuadFlags_NoMarkers)) {
            const ImU32 col_line = ImGui::GetColorU32(n.Colors[ImPlot3DCol_MarkerOutline]);
            const ImU32 col_fill = ImGui::GetColorU32(n.Colors[ImPlot3DCol_MarkerFill]);
            RenderMarkers<_Getter>(getter, n.Marker, n.MarkerSize, n.RenderMarkerFill, col_fill, n.RenderMarkerLine, col_line, n.MarkerWeight);
        }

        EndItem();
    }
}

IMPLOT3D_TMP void PlotQuad(const char* label_id, const T* xs, const T* ys, const T* zs, int count, ImPlot3DQuadFlags flags, int offset, int stride) {
    if (count < 3)
        return;
    GetterXYZ<IndexerIdx<T>, IndexerIdx<T>, IndexerIdx<T>> getter(IndexerIdx<T>(xs, count, offset, stride), IndexerIdx<T>(ys, count, offset, stride),
                                                                  IndexerIdx<T>(zs, count, offset, stride), count);
    return PlotQuadEx(label_id, getter, flags);
}

#define INSTANTIATE_MACRO(T)                                                                                                                         \
    template IMPLOT3D_API void PlotQuad<T>(const char* label_id, const T* xs, const T* ys, const T* zs, int count, ImPlot3DQuadFlags flags,          \
                                           int offset, int stride);
CALL_INSTANTIATE_FOR_NUMERIC_TYPES()
#undef INSTANTIATE_MACRO

//-----------------------------------------------------------------------------
// [SECTION] PlotSurface
//-----------------------------------------------------------------------------

template <typename _Getter> void PlotSurfaceEx(const char* label_id, const _Getter& getter, int x_count, int y_count, double scale_min,
                                               double scale_max, ImPlot3DSurfaceFlags flags) {
    if (BeginItemEx(label_id, getter, flags, ImPlot3DCol_Fill)) {
        const ImPlot3DNextItemData& n = GetItemData();

        // Render fill
        if (getter.Count >= 4 && n.RenderFill && !ImHasFlag(flags, ImPlot3DSurfaceFlags_NoFill)) {
            const ImU32 col_fill = ImGui::GetColorU32(n.Colors[ImPlot3DCol_Fill]);
            RenderPrimitives<RendererSurfaceFill>(getter, x_count, y_count, col_fill, scale_min, scale_max);
        }

        // Render lines
        if (getter.Count >= 2 && n.RenderLine && !ImHasFlag(flags, ImPlot3DSurfaceFlags_NoLines)) {
            const ImU32 col_line = ImGui::GetColorU32(n.Colors[ImPlot3DCol_Line]);
            RenderPrimitives<RendererLineSegments>(GetterSurfaceLines<_Getter>(getter, x_count, y_count), col_line, n.LineWeight);
        }

        // Render markers
        if (n.Marker != ImPlot3DMarker_None && !ImHasFlag(flags, ImPlot3DSurfaceFlags_NoMarkers)) {
            const ImU32 col_line = ImGui::GetColorU32(n.Colors[ImPlot3DCol_MarkerOutline]);
            const ImU32 col_fill = ImGui::GetColorU32(n.Colors[ImPlot3DCol_MarkerFill]);
            RenderMarkers<_Getter>(getter, n.Marker, n.MarkerSize, n.RenderMarkerFill, col_fill, n.RenderMarkerLine, col_line, n.MarkerWeight);
        }

        EndItem();
    }
}

IMPLOT3D_TMP void PlotSurface(const char* label_id, const T* xs, const T* ys, const T* zs, int x_count, int y_count, double scale_min,
                              double scale_max, ImPlot3DSurfaceFlags flags, int offset, int stride) {
    int count = x_count * y_count;
    if (count < 4)
        return;
    GetterXYZ<IndexerIdx<T>, IndexerIdx<T>, IndexerIdx<T>> getter(IndexerIdx<T>(xs, count, offset, stride), IndexerIdx<T>(ys, count, offset, stride),
                                                                  IndexerIdx<T>(zs, count, offset, stride), count);
    return PlotSurfaceEx(label_id, getter, x_count, y_count, scale_min, scale_max, flags);
}

#define INSTANTIATE_MACRO(T)                                                                                                                         \
    template IMPLOT3D_API void PlotSurface<T>(const char* label_id, const T* xs, const T* ys, const T* zs, int x_count, int y_count,                 \
                                              double scale_min, double scale_max, ImPlot3DSurfaceFlags flags, int offset, int stride);
CALL_INSTANTIATE_FOR_NUMERIC_TYPES()
#undef INSTANTIATE_MACRO

//-----------------------------------------------------------------------------
// [SECTION] PlotMesh
//-----------------------------------------------------------------------------

void PlotMesh(const char* label_id, const ImPlot3DPoint* vtx, const unsigned int* idx, int vtx_count, int idx_count, ImPlot3DMeshFlags flags) {
    Getter3DPoints getter(vtx, vtx_count);                     // Get vertices
    GetterMeshTriangles getter_triangles(vtx, idx, idx_count); // Get triangle vertices
    if (BeginItemEx(label_id, getter, flags, ImPlot3DCol_Fill)) {
        const ImPlot3DNextItemData& n = GetItemData();

        // Render fill
        if (getter.Count >= 3 && n.RenderFill && !ImHasFlag(flags, ImPlot3DMeshFlags_NoFill)) {
            const ImU32 col_fill = ImGui::GetColorU32(n.Colors[ImPlot3DCol_Fill]);
            RenderPrimitives<RendererTriangleFill>(getter_triangles, col_fill);
        }

        // Render lines
        if (getter.Count >= 2 && n.RenderLine && !n.IsAutoLine && !ImHasFlag(flags, ImPlot3DMeshFlags_NoLines)) {
            const ImU32 col_line = ImGui::GetColorU32(n.Colors[ImPlot3DCol_Line]);
            RenderPrimitives<RendererLineSegments>(GetterTriangleLines<GetterMeshTriangles>(getter_triangles), col_line, n.LineWeight);
        }

        // Render markers
        if (n.Marker != ImPlot3DMarker_None && !ImHasFlag(flags, ImPlot3DMeshFlags_NoMarkers)) {
            const ImU32 col_line = ImGui::GetColorU32(n.Colors[ImPlot3DCol_MarkerOutline]);
            const ImU32 col_fill = ImGui::GetColorU32(n.Colors[ImPlot3DCol_MarkerFill]);
            RenderMarkers(getter, n.Marker, n.MarkerSize, n.RenderMarkerFill, col_fill, n.RenderMarkerLine, col_line, n.MarkerWeight);
        }

        EndItem();
    }
}

//-----------------------------------------------------------------------------
// [SECTION] PlotImage
//-----------------------------------------------------------------------------

IMPLOT3D_API void PlotImage(const char* label_id, ImTextureRef tex_ref, const ImPlot3DPoint& center, const ImPlot3DPoint& axis_u,
                            const ImPlot3DPoint& axis_v, const ImVec2& uv0, const ImVec2& uv1, const ImVec4& tint_col, ImPlot3DImageFlags flags) {
    // Compute corners from center and axes
    ImPlot3DPoint p0 = center - axis_u - axis_v; // Bottom-left
    ImPlot3DPoint p1 = center + axis_u - axis_v; // Bottom-right
    ImPlot3DPoint p2 = center + axis_u + axis_v; // Top-right
    ImPlot3DPoint p3 = center - axis_u + axis_v; // Top-left

    // Map ImPlot-style 2-point UVs into full 4-corner UVs
    ImVec2 uv_0 = uv0;
    ImVec2 uv_1 = ImVec2(uv1.x, uv0.y);
    ImVec2 uv_2 = uv1;
    ImVec2 uv_3 = ImVec2(uv0.x, uv1.y);

    // Delegate to full quad version
    PlotImage(label_id, tex_ref, p0, p1, p2, p3, uv_0, uv_1, uv_2, uv_3, tint_col, flags);
}

IMPLOT3D_API void PlotImage(const char* label_id, ImTextureRef tex_ref, const ImPlot3DPoint& p0, const ImPlot3DPoint& p1, const ImPlot3DPoint& p2,
                            const ImPlot3DPoint& p3, const ImVec2& uv0, const ImVec2& uv1, const ImVec2& uv2, const ImVec2& uv3,
                            const ImVec4& tint_col, ImPlot3DImageFlags flags) {
    ImPlot3DContext& gp = *GImPlot3D;
    IM_ASSERT_USER_ERROR(gp.CurrentPlot != nullptr, "PlotImage() needs to be called between BeginPlot() and EndPlot()!");
    SetupLock();

    ImPlot3DPoint corners[4] = {p0, p1, p2, p3};
    Getter3DPoints getter(corners, 4);

    // Invert Y from UVs
    ImVec2 uv_0 = ImVec2(uv0.x, 1 - uv0.y);
    ImVec2 uv_1 = ImVec2(uv1.x, 1 - uv1.y);
    ImVec2 uv_2 = ImVec2(uv2.x, 1 - uv2.y);
    ImVec2 uv_3 = ImVec2(uv3.x, 1 - uv3.y);

    if (BeginItemEx(label_id, getter, flags)) {
        ImU32 tint_col32 = ImGui::ColorConvertFloat4ToU32(tint_col);
        GetCurrentItem()->Color = tint_col32;

        // Render image
        bool is_transparent = (tint_col32 & IM_COL32_A_MASK) == 0;
        if (!is_transparent)
            RenderPrimitives<RendererQuadImage>(getter, tex_ref, uv_0, uv_1, uv_2, uv_3, tint_col32);

        EndItem();
    }
}

//-----------------------------------------------------------------------------
// [SECTION] PlotText
//-----------------------------------------------------------------------------

void PlotText(const char* text, float x, float y, float z, float angle, const ImVec2& pix_offset) {
    ImPlot3DContext& gp = *GImPlot3D;
    IM_ASSERT_USER_ERROR(gp.CurrentPlot != nullptr, "PlotText() needs to be called between BeginPlot() and EndPlot()!");
    SetupLock();
    ImPlot3DPlot& plot = *gp.CurrentPlot;

    ImPlot3DBox cull_box;
    if (ImHasFlag(plot.Flags, ImPlot3DFlags_NoClip)) {
        cull_box.Min = ImPlot3DPoint(-HUGE_VAL, -HUGE_VAL, -HUGE_VAL);
        cull_box.Max = ImPlot3DPoint(HUGE_VAL, HUGE_VAL, HUGE_VAL);
    } else {
        cull_box.Min = plot.RangeMin();
        cull_box.Max = plot.RangeMax();
    }
    if (!cull_box.Contains(ImPlot3DPoint(x, y, z)))
        return;

    ImVec2 p = PlotToPixels(ImPlot3DPoint(x, y, z));
    p.x += pix_offset.x;
    p.y += pix_offset.y;
    AddTextRotated(GetPlotDrawList(), p, angle, GetStyleColorU32(ImPlot3DCol_InlayText), text);
}

} // namespace ImPlot3D

#endif // #ifndef IMGUI_DISABLE
