// the structure of this file:
//
// [SECTION] internal data structures
// [SECTION] global struct
// [SECTION] editor context definition
// [SECTION] draw list helper
// [SECTION] ObjectPool implementation
// [SECTION] ui state logic
// [SECTION] render helpers
// [SECTION] API implementation

#include "imnodes.h"

#include <imgui.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>

// Check minimum ImGui version
#define MINIMUM_COMPATIBLE_IMGUI_VERSION 17400
#if IMGUI_VERSION_NUM < MINIMUM_COMPATIBLE_IMGUI_VERSION
#error "Minimum ImGui version requirement not met -- please use a newer version!"
#endif

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <new>
#include <stdint.h>
#include <string.h> // strlen, strncmp
#include <stdio.h>  // for fwrite, ssprintf, sscanf
#include <stdlib.h>

namespace imnodes
{
namespace
{
enum ScopeFlags
{
    Scope_None = 1,
    Scope_Editor = 1 << 1,
    Scope_Node = 1 << 2,
    Scope_Attribute = 1 << 3
};

enum AttributeType
{
    AttributeType_None,
    AttributeType_Input,
    AttributeType_Output
};

enum ElementStateChange
{
    ElementStateChange_None = 0,
    ElementStateChange_LinkStarted = 1 << 0,
    ElementStateChange_LinkDropped = 1 << 1,
    ElementStateChange_LinkCreated = 1 << 2
};

// [SECTION] internal data structures

// The object T must have the following interface:
//
// struct T
// {
//     T();
//
//     int id;
// };
template<typename T>
struct ObjectPool
{
    ImVector<T> pool;
    ImVector<bool> in_use;
    ImVector<int> free_list;
    ImGuiStorage id_map;

    ObjectPool() : pool(), in_use(), free_list(), id_map() {}
};

// Emulates std::optional<int> using the sentinel value `invalid_index`.
struct OptionalIndex
{
    OptionalIndex() : m_index(invalid_index) {}
    OptionalIndex(const int value) : m_index(value) {}

    // Observers

    inline bool has_value() const { return m_index != invalid_index; }

    inline int value() const
    {
        assert(has_value());
        return m_index;
    }

    // Modifiers

    inline OptionalIndex& operator=(const int value)
    {
        m_index = value;
        return *this;
    }

    inline void reset() { m_index = invalid_index; }

    inline bool operator==(const OptionalIndex& rhs) const { return m_index == rhs.m_index; }

    inline bool operator==(const int rhs) const { return m_index == rhs; }

    static const int invalid_index = -1;

private:
    int m_index;
};

struct NodeData
{
    int id;
    ImVec2 origin; // The node origin is in editor space
    ImRect title_bar_content_rect;
    ImRect rect;

    struct
    {
        ImU32 background, background_hovered, background_selected, outline, titlebar,
            titlebar_hovered, titlebar_selected;
    } color_style;

    struct
    {
        float corner_rounding;
        ImVec2 padding;
        float border_thickness;
    } layout_style;

    ImVector<int> pin_indices;
    bool draggable;

    NodeData(const int node_id)
        : id(node_id), origin(100.0f, 100.0f), title_bar_content_rect(),
          rect(ImVec2(0.0f, 0.0f), ImVec2(0.0f, 0.0f)), color_style(), layout_style(),
          pin_indices(), draggable(true)
    {
    }

    ~NodeData() { id = INT_MIN; }
};

struct PinData
{
    int id;
    int parent_node_idx;
    ImRect attribute_rect;
    AttributeType type;
    PinShape shape;
    ImVec2 pos; // screen-space coordinates
    int flags;

    struct
    {
        ImU32 background, hovered;
    } color_style;

    PinData(const int pin_id)
        : id(pin_id), parent_node_idx(), attribute_rect(), type(AttributeType_None),
          shape(PinShape_CircleFilled), pos(), flags(AttributeFlags_None), color_style()
    {
    }
};

struct LinkData
{
    int id;
    int start_pin_idx, end_pin_idx;

    struct
    {
        ImU32 base, hovered, selected;
    } color_style;

    LinkData(const int link_id) : id(link_id), start_pin_idx(), end_pin_idx(), color_style() {}
};

struct LinkPredicate
{
    bool operator()(const LinkData& lhs, const LinkData& rhs) const
    {
        // Do a unique compare by sorting the pins' addresses.
        // This catches duplicate links, whether they are in the
        // same direction or not.
        // Sorting by pin index should have the uniqueness guarantees as sorting
        // by id -- each unique id will get one slot in the link pool array.

        int lhs_start = lhs.start_pin_idx;
        int lhs_end = lhs.end_pin_idx;
        int rhs_start = rhs.start_pin_idx;
        int rhs_end = rhs.end_pin_idx;

        if (lhs_start > lhs_end)
        {
            ImSwap(lhs_start, lhs_end);
        }

        if (rhs_start > rhs_end)
        {
            ImSwap(rhs_start, rhs_end);
        }

        return lhs_start == rhs_start && lhs_end == rhs_end;
    }
};

struct BezierCurve
{
    // the curve control points
    ImVec2 p0, p1, p2, p3;
};

struct LinkBezierData
{
    BezierCurve bezier;
    int num_segments;
};

enum ClickInteractionType
{
    ClickInteractionType_Node,
    ClickInteractionType_Link,
    ClickInteractionType_LinkCreation,
    ClickInteractionType_Panning,
    ClickInteractionType_BoxSelection,
    ClickInteractionType_None
};

enum LinkCreationType
{
    LinkCreationType_Standard,
    LinkCreationType_FromDetach
};

struct ClickInteractionState
{
    struct
    {
        int start_pin_idx;
        OptionalIndex end_pin_idx;
        LinkCreationType link_creation_type;
    } link_creation;

    struct
    {
        ImRect rect;
    } box_selector;
};

struct ColorStyleElement
{
    ImU32 color;
    ColorStyle item;

    ColorStyleElement(const ImU32 c, const ColorStyle s) : color(c), item(s) {}
};

struct StyleElement
{
    StyleVar item;
    float value;

    StyleElement(const float value, const StyleVar variable) : item(variable), value(value) {}
};

// [SECTION] global struct
// this stores data which only lives for one frame
struct
{
    EditorContext* default_editor_ctx;
    EditorContext* editor_ctx;

    // Canvas draw list and helper state
    ImDrawList* canvas_draw_list;
    ImGuiStorage node_idx_to_submission_idx;
    ImVector<int> node_idx_submission_order;
    ImVector<int> node_indices_overlapping_with_mouse;

    // Canvas extents
    ImVec2 canvas_origin_screen_space;
    ImRect canvas_rect_screen_space;

    // Debug helpers
    ScopeFlags current_scope;

    // Configuration state
    IO io;
    Style style;
    ImVector<ColorStyleElement> color_modifier_stack;
    ImVector<StyleElement> style_modifier_stack;
    ImGuiTextBuffer text_buffer;

    int current_attribute_flags;
    ImVector<int> attribute_flag_stack;

    // UI element state
    int current_node_idx;
    int current_pin_idx;
    int current_attribute_id;

    OptionalIndex hovered_node_idx;
    OptionalIndex interactive_node_idx;
    OptionalIndex hovered_link_idx;
    OptionalIndex hovered_pin_idx;
    int hovered_pin_flags;

    OptionalIndex deleted_link_idx;
    OptionalIndex snap_link_idx;

    // Event helper state
    int element_state_change;

    int active_attribute_id;
    bool active_attribute;

    // ImGui::IO cache

    ImVec2 mouse_pos;

    bool left_mouse_clicked;
    bool left_mouse_released;
    bool middle_mouse_clicked;
    bool left_mouse_dragging;
    bool middle_mouse_dragging;
} g;

EditorContext& editor_context_get()
{
    // No editor context was set! Did you forget to call imnodes::Initialize?
    assert(g.editor_ctx != NULL);
    return *g.editor_ctx;
}

inline bool is_mouse_hovering_near_point(const ImVec2& point, float radius)
{
    ImVec2 delta = g.mouse_pos - point;
    return (delta.x * delta.x + delta.y * delta.y) < (radius * radius);
}

inline ImVec2 eval_bezier(float t, const BezierCurve& bezier)
{
    // B(t) = (1-t)**3 p0 + 3(1 - t)**2 t P1 + 3(1-t)t**2 P2 + t**3 P3
    return ImVec2(
        (1 - t) * (1 - t) * (1 - t) * bezier.p0.x + 3 * (1 - t) * (1 - t) * t * bezier.p1.x +
            3 * (1 - t) * t * t * bezier.p2.x + t * t * t * bezier.p3.x,
        (1 - t) * (1 - t) * (1 - t) * bezier.p0.y + 3 * (1 - t) * (1 - t) * t * bezier.p1.y +
            3 * (1 - t) * t * t * bezier.p2.y + t * t * t * bezier.p3.y);
}

// Calculates the closest point along each bezier curve segment.
ImVec2 get_closest_point_on_cubic_bezier(
    const int num_segments,
    const ImVec2& p,
    const BezierCurve& bezier)
{
    IM_ASSERT(num_segments > 0);
    ImVec2 p_last = bezier.p0;
    ImVec2 p_closest;
    float p_closest_dist = FLT_MAX;
    float t_step = 1.0f / (float)num_segments;
    for (int i = 1; i <= num_segments; ++i)
    {
        ImVec2 p_current = eval_bezier(t_step * i, bezier);
        ImVec2 p_line = ImLineClosestPoint(p_last, p_current, p);
        float dist = ImLengthSqr(p - p_line);
        if (dist < p_closest_dist)
        {
            p_closest = p_line;
            p_closest_dist = dist;
        }
        p_last = p_current;
    }
    return p_closest;
}

inline float get_distance_to_cubic_bezier(
    const ImVec2& pos,
    const BezierCurve& bezier,
    const int num_segments)
{
    const ImVec2 point_on_curve = get_closest_point_on_cubic_bezier(num_segments, pos, bezier);

    const ImVec2 to_curve = point_on_curve - pos;
    return ImSqrt(ImLengthSqr(to_curve));
}

inline ImRect get_containing_rect_for_bezier_curve(const BezierCurve& bezier)
{
    const ImVec2 min = ImVec2(ImMin(bezier.p0.x, bezier.p3.x), ImMin(bezier.p0.y, bezier.p3.y));
    const ImVec2 max = ImVec2(ImMax(bezier.p0.x, bezier.p3.x), ImMax(bezier.p0.y, bezier.p3.y));

    const float hover_distance = g.style.link_hover_distance;

    ImRect rect(min, max);
    rect.Add(bezier.p1);
    rect.Add(bezier.p2);
    rect.Expand(ImVec2(hover_distance, hover_distance));

    return rect;
}

inline LinkBezierData get_link_renderable(
    ImVec2 start,
    ImVec2 end,
    const AttributeType start_type,
    const float line_segments_per_length)
{
    assert((start_type == AttributeType_Input) || (start_type == AttributeType_Output));
    if (start_type == AttributeType_Input)
    {
        ImSwap(start, end);
    }

    const float link_length = ImSqrt(ImLengthSqr(end - start));
    const ImVec2 offset = ImVec2(0.25f * link_length, 0.f);
    LinkBezierData link_data;
    link_data.bezier.p0 = start;
    link_data.bezier.p1 = start + offset;
    link_data.bezier.p2 = end - offset;
    link_data.bezier.p3 = end;
    link_data.num_segments = ImMax(static_cast<int>(link_length * line_segments_per_length), 1);
    return link_data;
}

inline bool is_mouse_hovering_near_link(const BezierCurve& bezier, const int num_segments)
{
    const ImVec2 mouse_pos = g.mouse_pos;

    // First, do a simple bounding box test against the box containing the link
    // to see whether calculating the distance to the link is worth doing.
    const ImRect link_rect = get_containing_rect_for_bezier_curve(bezier);

    if (link_rect.Contains(mouse_pos))
    {
        const float distance = get_distance_to_cubic_bezier(mouse_pos, bezier, num_segments);
        if (distance < g.style.link_hover_distance)
        {
            return true;
        }
    }

    return false;
}

inline float eval_implicit_line_eq(const ImVec2& p1, const ImVec2& p2, const ImVec2& p)
{
    return (p2.y - p1.y) * p.x + (p1.x - p2.x) * p.y + (p2.x * p1.y - p1.x * p2.y);
}

inline int sign(float val) { return int(val > 0.0f) - int(val < 0.0f); }

inline bool rectangle_overlaps_line_segment(const ImRect& rect, const ImVec2& p1, const ImVec2& p2)
{
    // Trivial case: rectangle contains an endpoint
    if (rect.Contains(p1) || rect.Contains(p2))
    {
        return true;
    }

    // Flip rectangle if necessary
    ImRect flip_rect = rect;

    if (flip_rect.Min.x > flip_rect.Max.x)
    {
        ImSwap(flip_rect.Min.x, flip_rect.Max.x);
    }

    if (flip_rect.Min.y > flip_rect.Max.y)
    {
        ImSwap(flip_rect.Min.y, flip_rect.Max.y);
    }

    // Trivial case: line segment lies to one particular side of rectangle
    if ((p1.x < flip_rect.Min.x && p2.x < flip_rect.Min.x) ||
        (p1.x > flip_rect.Max.x && p2.x > flip_rect.Max.x) ||
        (p1.y < flip_rect.Min.y && p2.y < flip_rect.Min.y) ||
        (p1.y > flip_rect.Max.y && p2.y > flip_rect.Max.y))
    {
        return false;
    }

    const int corner_signs[4] = {
        sign(eval_implicit_line_eq(p1, p2, flip_rect.Min)),
        sign(eval_implicit_line_eq(p1, p2, ImVec2(flip_rect.Max.x, flip_rect.Min.y))),
        sign(eval_implicit_line_eq(p1, p2, ImVec2(flip_rect.Min.x, flip_rect.Max.y))),
        sign(eval_implicit_line_eq(p1, p2, flip_rect.Max))};

    int sum = 0;
    int sum_abs = 0;

    for (int i = 0; i < 4; ++i)
    {
        sum += corner_signs[i];
        sum_abs += abs(corner_signs[i]);
    }

    // At least one corner of rectangle lies on a different side of line segment
    return abs(sum) != sum_abs;
}

inline bool rectangle_overlaps_bezier(const ImRect& rectangle, const LinkBezierData& link_data)
{
    ImVec2 current = eval_bezier(0.f, link_data.bezier);
    const float dt = 1.0f / link_data.num_segments;
    for (int s = 0; s < link_data.num_segments; ++s)
    {
        ImVec2 next = eval_bezier(static_cast<float>((s + 1) * dt), link_data.bezier);
        if (rectangle_overlaps_line_segment(rectangle, current, next))
        {
            return true;
        }
        current = next;
    }
    return false;
}

inline bool rectangle_overlaps_link(
    const ImRect& rectangle,
    const ImVec2& start,
    const ImVec2& end,
    const AttributeType start_type)
{
    // First level: simple rejection test via rectangle overlap:

    ImRect lrect = ImRect(start, end);
    if (lrect.Min.x > lrect.Max.x)
    {
        ImSwap(lrect.Min.x, lrect.Max.x);
    }

    if (lrect.Min.y > lrect.Max.y)
    {
        ImSwap(lrect.Min.y, lrect.Max.y);
    }

    if (rectangle.Overlaps(lrect))
    {
        // First, check if either one or both endpoinds are trivially contained
        // in the rectangle

        if (rectangle.Contains(start) || rectangle.Contains(end))
        {
            return true;
        }

        // Second level of refinement: do a more expensive test against the
        // link

        const LinkBezierData link_data =
            get_link_renderable(start, end, start_type, g.style.link_line_segments_per_length);
        return rectangle_overlaps_bezier(rectangle, link_data);
    }

    return false;
}
} // namespace

// [SECTION] editor context definition

struct EditorContext
{
    ObjectPool<NodeData> nodes;
    ObjectPool<PinData> pins;
    ObjectPool<LinkData> links;

    ImVector<int> node_depth_order;

    // ui related fields
    ImVec2 panning;

    ImVector<int> selected_node_indices;
    ImVector<int> selected_link_indices;

    ClickInteractionType click_interaction_type;
    ClickInteractionState click_interaction_state;

    EditorContext()
        : nodes(), pins(), links(), panning(0.f, 0.f), selected_node_indices(),
          selected_link_indices(), click_interaction_type(ClickInteractionType_None),
          click_interaction_state()
    {
    }
};

namespace
{
// [SECTION] draw list helper

void ImDrawList_grow_channels(ImDrawList* draw_list, const int num_channels)
{
    ImDrawListSplitter& splitter = draw_list->_Splitter;

    if (splitter._Count == 1)
    {
        splitter.Split(draw_list, num_channels + 1);
        return;
    }

    // NOTE: this logic has been lifted from ImDrawListSplitter::Split with slight modifications
    // to allow nested splits. The main modification is that we only create new ImDrawChannel
    // instances after splitter._Count, instead of over the whole splitter._Channels array like
    // the regular ImDrawListSplitter::Split method does.

    const int old_channel_capacity = splitter._Channels.Size;
    // NOTE: _Channels is not resized down, and therefore _Count <= _Channels.size()!
    const int old_channel_count = splitter._Count;
    const int requested_channel_count = old_channel_count + num_channels;
    if (old_channel_capacity < old_channel_count + num_channels)
    {
        splitter._Channels.resize(requested_channel_count);
    }

    splitter._Count = requested_channel_count;

    for (int i = old_channel_count; i < requested_channel_count; ++i)
    {
        ImDrawChannel& channel = splitter._Channels[i];

        // If we're inside the old capacity region of the array, we need to reuse the existing
        // memory of the command and index buffers.
        if (i < old_channel_capacity)
        {
            channel._CmdBuffer.resize(0);
            channel._IdxBuffer.resize(0);
        }
        // Else, we need to construct new draw channels.
        else
        {
            IM_PLACEMENT_NEW(&channel) ImDrawChannel();
        }

        {
            ImDrawCmd draw_cmd;
            draw_cmd.ClipRect = draw_list->_ClipRectStack.back();
            draw_cmd.TextureId = draw_list->_TextureIdStack.back();
            channel._CmdBuffer.push_back(draw_cmd);
        }
    }
}

void ImDrawListSplitter_swap_channels(
    ImDrawListSplitter& splitter,
    const int lhs_idx,
    const int rhs_idx)
{
    if (lhs_idx == rhs_idx)
    {
        return;
    }

    assert(lhs_idx >= 0 && lhs_idx < splitter._Count);
    assert(rhs_idx >= 0 && rhs_idx < splitter._Count);

    ImDrawChannel& lhs_channel = splitter._Channels[lhs_idx];
    ImDrawChannel& rhs_channel = splitter._Channels[rhs_idx];
    lhs_channel._CmdBuffer.swap(rhs_channel._CmdBuffer);
    lhs_channel._IdxBuffer.swap(rhs_channel._IdxBuffer);

    const int current_channel = splitter._Current;

    if (current_channel == lhs_idx)
    {
        splitter._Current = rhs_idx;
    }
    else if (current_channel == rhs_idx)
    {
        splitter._Current = lhs_idx;
    }
}

void draw_list_set(ImDrawList* window_draw_list)
{
    g.canvas_draw_list = window_draw_list;
    g.node_idx_to_submission_idx.Clear();
    g.node_idx_submission_order.clear();
}

// The draw list channels are structured as follows. First we have our base channel, the canvas grid
// on which we render the grid lines in BeginNodeEditor(). The base channel is the reason
// draw_list_submission_idx_to_background_channel_idx offsets the index by one. Each BeginNode()
// call appends two new draw channels, for the node background and foreground. The node foreground
// is the channel into which the node's ImGui content is rendered. Finally, in EndNodeEditor() we
// append one last draw channel for rendering the selection box and the incomplete link on top of
// everything else.
//
// +----------+----------+----------+----------+----------+----------+
// |          |          |          |          |          |          |
// |canvas    |node      |node      |...       |...       |click     |
// |grid      |background|foreground|          |          |interaction
// |          |          |          |          |          |          |
// +----------+----------+----------+----------+----------+----------+
//            |                     |
//            |   submission idx    |
//            |                     |
//            -----------------------

void draw_list_add_node(const int node_idx)
{
    g.node_idx_to_submission_idx.SetInt(
        static_cast<ImGuiID>(node_idx), g.node_idx_submission_order.Size);
    g.node_idx_submission_order.push_back(node_idx);
    ImDrawList_grow_channels(g.canvas_draw_list, 2);
}

void draw_list_append_click_interaction_channel()
{
    // NOTE: don't use this function outside of EndNodeEditor. Using this before all nodes have been
    // added will screw up the node draw order.
    ImDrawList_grow_channels(g.canvas_draw_list, 1);
}

int draw_list_submission_idx_to_background_channel_idx(const int submission_idx)
{
    // NOTE: the first channel is the canvas background, i.e. the grid
    return 1 + 2 * submission_idx;
}

int draw_list_submission_idx_to_foreground_channel_idx(const int submission_idx)
{
    return draw_list_submission_idx_to_background_channel_idx(submission_idx) + 1;
}

void draw_list_activate_click_interaction_channel()
{
    g.canvas_draw_list->_Splitter.SetCurrentChannel(
        g.canvas_draw_list, g.canvas_draw_list->_Splitter._Count - 1);
}

void draw_list_activate_current_node_foreground()
{
    const int foreground_channel_idx =
        draw_list_submission_idx_to_foreground_channel_idx(g.node_idx_submission_order.Size - 1);
    g.canvas_draw_list->_Splitter.SetCurrentChannel(g.canvas_draw_list, foreground_channel_idx);
}

void draw_list_activate_node_background(const int node_idx)
{
    const int submission_idx =
        g.node_idx_to_submission_idx.GetInt(static_cast<ImGuiID>(node_idx), -1);
    // There is a discrepancy in the submitted node count and the rendered node count! Did you call
    // one of the following functions
    // * EditorContextMoveToNode
    // * SetNodeScreenSpacePos
    // * SetNodeGridSpacePos
    // * SetNodeDraggable
    // after the BeginNode/EndNode function calls?
    assert(submission_idx != -1);
    const int background_channel_idx =
        draw_list_submission_idx_to_background_channel_idx(submission_idx);
    g.canvas_draw_list->_Splitter.SetCurrentChannel(g.canvas_draw_list, background_channel_idx);
}

void draw_list_swap_submission_indices(const int lhs_idx, const int rhs_idx)
{
    assert(lhs_idx != rhs_idx);

    const int lhs_foreground_channel_idx =
        draw_list_submission_idx_to_foreground_channel_idx(lhs_idx);
    const int lhs_background_channel_idx =
        draw_list_submission_idx_to_background_channel_idx(lhs_idx);
    const int rhs_foreground_channel_idx =
        draw_list_submission_idx_to_foreground_channel_idx(rhs_idx);
    const int rhs_background_channel_idx =
        draw_list_submission_idx_to_background_channel_idx(rhs_idx);

    ImDrawListSplitter_swap_channels(
        g.canvas_draw_list->_Splitter, lhs_background_channel_idx, rhs_background_channel_idx);
    ImDrawListSplitter_swap_channels(
        g.canvas_draw_list->_Splitter, lhs_foreground_channel_idx, rhs_foreground_channel_idx);
}

void draw_list_sort_channels_by_depth(const ImVector<int>& node_idx_depth_order)
{
    if (g.node_idx_to_submission_idx.Data.Size < 2)
    {
        return;
    }

    assert(node_idx_depth_order.Size == g.node_idx_submission_order.Size);

    int start_idx = node_idx_depth_order.Size - 1;

    while (node_idx_depth_order[start_idx] == g.node_idx_submission_order[start_idx])
    {
        if (--start_idx == 0)
        {
            // early out if submission order and depth order are the same
            return;
        }
    }

    // TODO: this is an O(N^2) algorithm. It might be worthwhile revisiting this to see if the time
    // complexity can be reduced.

    for (int depth_idx = start_idx; depth_idx > 0; --depth_idx)
    {
        const int node_idx = node_idx_depth_order[depth_idx];

        // Find the current index of the node_idx in the submission order array
        int submission_idx = -1;
        for (int i = 0; i < g.node_idx_submission_order.Size; ++i)
        {
            if (g.node_idx_submission_order[i] == node_idx)
            {
                submission_idx = i;
                break;
            }
        }
        assert(submission_idx >= 0);

        if (submission_idx == depth_idx)
        {
            continue;
        }

        for (int j = submission_idx; j < depth_idx; ++j)
        {
            draw_list_swap_submission_indices(j, j + 1);
            ImSwap(g.node_idx_submission_order[j], g.node_idx_submission_order[j + 1]);
        }
    }
}

// [SECTION] ObjectPool implementation

template<typename T>
int object_pool_find(ObjectPool<T>& objects, const int id)
{
    const int index = objects.id_map.GetInt(static_cast<ImGuiID>(id), -1);
    return index;
}

template<typename T>
void object_pool_update(ObjectPool<T>& objects)
{
    objects.free_list.clear();
    for (int i = 0; i < objects.in_use.size(); ++i)
    {
        if (!objects.in_use[i])
        {
            objects.id_map.SetInt(objects.pool[i].id, -1);
            objects.free_list.push_back(i);
            (objects.pool.Data + i)->~T();
        }
    }
}

template<>
void object_pool_update(ObjectPool<NodeData>& nodes)
{
    nodes.free_list.clear();
    for (int i = 0; i < nodes.in_use.size(); ++i)
    {
        if (nodes.in_use[i])
        {
            nodes.pool[i].pin_indices.clear();
        }
        else
        {
            const int previous_id = nodes.pool[i].id;
            const int previous_idx = nodes.id_map.GetInt(previous_id, -1);

            if (previous_idx != -1)
            {
                assert(previous_idx == i);
                // Remove node idx form depth stack the first time we detect that this idx slot is
                // unused
                ImVector<int>& depth_stack = editor_context_get().node_depth_order;
                const int* const elem = depth_stack.find(i);
                assert(elem != depth_stack.end());
                depth_stack.erase(elem);
            }

            nodes.id_map.SetInt(previous_id, -1);
            nodes.free_list.push_back(i);
            (nodes.pool.Data + i)->~NodeData();
        }
    }
}

template<typename T>
void object_pool_reset(ObjectPool<T>& objects)
{
    if (!objects.in_use.empty())
    {
        memset(objects.in_use.Data, 0, objects.in_use.size_in_bytes());
    }
}

template<typename T>
int object_pool_find_or_create_index(ObjectPool<T>& objects, const int id)
{
    int index = objects.id_map.GetInt(static_cast<ImGuiID>(id), -1);

    // Construct new object
    if (index == -1)
    {
        if (objects.free_list.empty())
        {
            index = objects.pool.size();
            IM_ASSERT(objects.pool.size() == objects.in_use.size());
            const int new_size = objects.pool.size() + 1;
            objects.pool.resize(new_size);
            objects.in_use.resize(new_size);
        }
        else
        {
            index = objects.free_list.back();
            objects.free_list.pop_back();
        }
        IM_PLACEMENT_NEW(objects.pool.Data + index) T(id);
        objects.id_map.SetInt(static_cast<ImGuiID>(id), index);
    }

    // Flag it as used
    objects.in_use[index] = true;

    return index;
}

template<>
int object_pool_find_or_create_index(ObjectPool<NodeData>& nodes, const int node_id)
{
    int node_idx = nodes.id_map.GetInt(static_cast<ImGuiID>(node_id), -1);

    // Construct new node
    if (node_idx == -1)
    {
        if (nodes.free_list.empty())
        {
            node_idx = nodes.pool.size();
            IM_ASSERT(nodes.pool.size() == nodes.in_use.size());
            const int new_size = nodes.pool.size() + 1;
            nodes.pool.resize(new_size);
            nodes.in_use.resize(new_size);
        }
        else
        {
            node_idx = nodes.free_list.back();
            nodes.free_list.pop_back();
        }
        IM_PLACEMENT_NEW(nodes.pool.Data + node_idx) NodeData(node_id);
        nodes.id_map.SetInt(static_cast<ImGuiID>(node_id), node_idx);

        EditorContext& editor = editor_context_get();
        editor.node_depth_order.push_back(node_idx);
    }

    // Flag node as used
    nodes.in_use[node_idx] = true;

    return node_idx;
}

template<typename T>
T& object_pool_find_or_create_object(ObjectPool<T>& objects, const int id)
{
    const int index = object_pool_find_or_create_index(objects, id);
    return objects.pool[index];
}

// [SECTION] ui state logic

ImVec2 get_screen_space_pin_coordinates(
    const ImRect& node_rect,
    const ImRect& attribute_rect,
    const AttributeType type)
{
    assert(type == AttributeType_Input || type == AttributeType_Output);
    const float x = type == AttributeType_Input ? (node_rect.Min.x - g.style.pin_offset)
                                                : (node_rect.Max.x + g.style.pin_offset);
    return ImVec2(x, 0.5f * (attribute_rect.Min.y + attribute_rect.Max.y));
}

ImVec2 get_screen_space_pin_coordinates(const EditorContext& editor, const PinData& pin)
{
    const ImRect& parent_node_rect = editor.nodes.pool[pin.parent_node_idx].rect;
    return get_screen_space_pin_coordinates(parent_node_rect, pin.attribute_rect, pin.type);
}

// These functions are here, and not members of the BoxSelector struct, because
// implementing a C API in C++ is frustrating. EditorContext has a BoxSelector
// field, but the state changes depend on the editor. So, these are implemented
// as C-style free functions so that the code is not too much of a mish-mash of
// C functions and C++ method definitions.

bool mouse_in_canvas()
{
    return g.canvas_rect_screen_space.Contains(ImGui::GetMousePos()) && ImGui::IsWindowHovered();
}

void begin_node_selection(EditorContext& editor, const int node_idx)
{
    // Don't start selecting a node if we are e.g. already creating and dragging
    // a new link! New link creation can happen when the mouse is clicked over
    // a node, but within the hover radius of a pin.
    if (editor.click_interaction_type != ClickInteractionType_None)
    {
        return;
    }

    editor.click_interaction_type = ClickInteractionType_Node;
    // If the node is not already contained in the selection, then we want only
    // the interaction node to be selected, effective immediately.
    //
    // Otherwise, we want to allow for the possibility of multiple nodes to be
    // moved at once.
    if (!editor.selected_node_indices.contains(node_idx))
    {
        editor.selected_node_indices.clear();
        editor.selected_link_indices.clear();
        editor.selected_node_indices.push_back(node_idx);

        // Ensure that individually selected nodes get rendered on top
        ImVector<int>& depth_stack = editor.node_depth_order;
        const int* const elem = depth_stack.find(node_idx);
        assert(elem != depth_stack.end());
        depth_stack.erase(elem);
        depth_stack.push_back(node_idx);
    }
}

void begin_link_selection(EditorContext& editor, const int link_idx)
{
    editor.click_interaction_type = ClickInteractionType_Link;
    // When a link is selected, clear all other selections, and insert the link
    // as the sole selection.
    editor.selected_node_indices.clear();
    editor.selected_link_indices.clear();
    editor.selected_link_indices.push_back(link_idx);
}

void begin_link_detach(EditorContext& editor, const int link_idx, const int detach_pin_idx)
{
    const LinkData& link = editor.links.pool[link_idx];
    ClickInteractionState& state = editor.click_interaction_state;
    state.link_creation.end_pin_idx.reset();
    state.link_creation.start_pin_idx =
        detach_pin_idx == link.start_pin_idx ? link.end_pin_idx : link.start_pin_idx;
    g.deleted_link_idx = link_idx;
}

void begin_link_interaction(EditorContext& editor, const int link_idx)
{
    // First check if we are clicking a link in the vicinity of a pin.
    // This may result in a link detach via click and drag.
    if (editor.click_interaction_type == ClickInteractionType_LinkCreation)
    {
        if ((g.hovered_pin_flags & AttributeFlags_EnableLinkDetachWithDragClick) != 0)
        {
            begin_link_detach(editor, link_idx, g.hovered_pin_idx.value());
            editor.click_interaction_state.link_creation.link_creation_type =
                LinkCreationType_FromDetach;
        }
    }
    // If we aren't near a pin, check if we are clicking the link with the
    // modifier pressed. This may also result in a link detach via clicking.
    else
    {
        const bool modifier_pressed = g.io.link_detach_with_modifier_click.modifier == NULL
                                          ? false
                                          : *g.io.link_detach_with_modifier_click.modifier;

        if (modifier_pressed)
        {
            const LinkData& link = editor.links.pool[link_idx];
            const PinData& start_pin = editor.pins.pool[link.start_pin_idx];
            const PinData& end_pin = editor.pins.pool[link.end_pin_idx];
            const ImVec2& mouse_pos = g.mouse_pos;
            const float dist_to_start = ImLengthSqr(start_pin.pos - mouse_pos);
            const float dist_to_end = ImLengthSqr(end_pin.pos - mouse_pos);
            const int closest_pin_idx =
                dist_to_start < dist_to_end ? link.start_pin_idx : link.end_pin_idx;

            editor.click_interaction_type = ClickInteractionType_LinkCreation;
            begin_link_detach(editor, link_idx, closest_pin_idx);
        }
        else
        {
            begin_link_selection(editor, link_idx);
        }
    }
}

void begin_link_creation(EditorContext& editor, const int hovered_pin_idx)
{
    editor.click_interaction_type = ClickInteractionType_LinkCreation;
    editor.click_interaction_state.link_creation.start_pin_idx = hovered_pin_idx;
    editor.click_interaction_state.link_creation.end_pin_idx.reset();
    editor.click_interaction_state.link_creation.link_creation_type = LinkCreationType_Standard;
    g.element_state_change |= ElementStateChange_LinkStarted;
}

void begin_canvas_interaction(EditorContext& editor)
{
    const bool any_ui_element_hovered = g.hovered_node_idx.has_value() ||
                                        g.hovered_link_idx.has_value() ||
                                        g.hovered_pin_idx.has_value() || ImGui::IsAnyItemHovered();

    const bool mouse_not_in_canvas = !mouse_in_canvas();

    if (editor.click_interaction_type != ClickInteractionType_None || any_ui_element_hovered ||
        mouse_not_in_canvas)
    {
        return;
    }

    const bool started_panning =
        g.io.emulate_three_button_mouse.enabled
            ? (g.left_mouse_clicked && *g.io.emulate_three_button_mouse.modifier)
            : g.middle_mouse_clicked;

    if (started_panning)
    {
        editor.click_interaction_type = ClickInteractionType_Panning;
    }
    else if (g.left_mouse_clicked)
    {
        editor.click_interaction_type = ClickInteractionType_BoxSelection;
        editor.click_interaction_state.box_selector.rect.Min = g.mouse_pos;
    }
}

void box_selector_update_selection(EditorContext& editor, ImRect box_rect)
{
    // Invert box selector coordinates as needed

    if (box_rect.Min.x > box_rect.Max.x)
    {
        ImSwap(box_rect.Min.x, box_rect.Max.x);
    }

    if (box_rect.Min.y > box_rect.Max.y)
    {
        ImSwap(box_rect.Min.y, box_rect.Max.y);
    }

    // Update node selection

    editor.selected_node_indices.clear();

    // Test for overlap against node rectangles

    for (int node_idx = 0; node_idx < editor.nodes.pool.size(); ++node_idx)
    {
        if (editor.nodes.in_use[node_idx])
        {
            NodeData& node = editor.nodes.pool[node_idx];
            if (box_rect.Overlaps(node.rect))
            {
                editor.selected_node_indices.push_back(node_idx);
            }
        }
    }

    // Update link selection

    editor.selected_link_indices.clear();

    // Test for overlap against links

    for (int link_idx = 0; link_idx < editor.links.pool.size(); ++link_idx)
    {
        if (editor.links.in_use[link_idx])
        {
            const LinkData& link = editor.links.pool[link_idx];

            const PinData& pin_start = editor.pins.pool[link.start_pin_idx];
            const PinData& pin_end = editor.pins.pool[link.end_pin_idx];
            const ImRect& node_start_rect = editor.nodes.pool[pin_start.parent_node_idx].rect;
            const ImRect& node_end_rect = editor.nodes.pool[pin_end.parent_node_idx].rect;

            const ImVec2 start = get_screen_space_pin_coordinates(
                node_start_rect, pin_start.attribute_rect, pin_start.type);
            const ImVec2 end = get_screen_space_pin_coordinates(
                node_end_rect, pin_end.attribute_rect, pin_end.type);

            // Test
            if (rectangle_overlaps_link(box_rect, start, end, pin_start.type))
            {
                editor.selected_link_indices.push_back(link_idx);
            }
        }
    }
}

void translate_selected_nodes(EditorContext& editor)
{
    if (g.left_mouse_dragging)
    {
        const ImGuiIO& io = ImGui::GetIO();
        for (int i = 0; i < editor.selected_node_indices.size(); ++i)
        {
            const int node_idx = editor.selected_node_indices[i];
            NodeData& node = editor.nodes.pool[node_idx];
            if (node.draggable)
            {
                node.origin += io.MouseDelta;
            }
        }
    }
}

OptionalIndex find_duplicate_link(
    const EditorContext& editor,
    const int start_pin_idx,
    const int end_pin_idx)
{
    LinkData test_link(0);
    test_link.start_pin_idx = start_pin_idx;
    test_link.end_pin_idx = end_pin_idx;
    for (int link_idx = 0; link_idx < editor.links.pool.size(); ++link_idx)
    {
        const LinkData& link = editor.links.pool[link_idx];
        if (LinkPredicate()(test_link, link) && editor.links.in_use[link_idx])
        {
            return OptionalIndex(link_idx);
        }
    }

    return OptionalIndex();
}

bool should_link_snap_to_pin(
    const EditorContext& editor,
    const PinData& start_pin,
    const int hovered_pin_idx,
    const OptionalIndex duplicate_link)
{
    const PinData& end_pin = editor.pins.pool[hovered_pin_idx];

    // The end pin must be in a different node
    if (start_pin.parent_node_idx == end_pin.parent_node_idx)
    {
        return false;
    }

    // The end pin must be of a different type
    if (start_pin.type == end_pin.type)
    {
        return false;
    }

    // The link to be created must not be a duplicate, unless it is the link which was created on
    // snap. In that case we want to snap, since we want it to appear visually as if the created
    // link remains snapped to the pin.
    if (duplicate_link.has_value() && !(duplicate_link == g.snap_link_idx))
    {
        return false;
    }

    return true;
}

void click_interaction_update(EditorContext& editor)
{
    switch (editor.click_interaction_type)
    {
    case ClickInteractionType_BoxSelection:
    {
        ImRect& box_rect = editor.click_interaction_state.box_selector.rect;
        box_rect.Max = g.mouse_pos;

        box_selector_update_selection(editor, box_rect);

        const ImU32 box_selector_color = g.style.colors[ColorStyle_BoxSelector];
        const ImU32 box_selector_outline = g.style.colors[ColorStyle_BoxSelectorOutline];
        g.canvas_draw_list->AddRectFilled(box_rect.Min, box_rect.Max, box_selector_color);
        g.canvas_draw_list->AddRect(box_rect.Min, box_rect.Max, box_selector_outline);

        if (g.left_mouse_released)
        {
            ImVector<int>& depth_stack = editor.node_depth_order;
            const ImVector<int>& selected_idxs = editor.selected_node_indices;

            // Bump the selected node indices, in order, to the top of the depth stack.
            // NOTE: this algorithm has worst case time complexity of O(N^2), if the node selection
            // is ~ N (due to selected_idxs.contains()).

            if ((selected_idxs.Size > 0) && (selected_idxs.Size < depth_stack.Size))
            {
                int num_moved = 0; // The number of indices moved. Stop after selected_idxs.Size
                for (int i = 0; i < depth_stack.Size - selected_idxs.Size; ++i)
                {
                    for (int node_idx = depth_stack[i]; selected_idxs.contains(node_idx);
                         node_idx = depth_stack[i])
                    {
                        depth_stack.erase(depth_stack.begin() + static_cast<size_t>(i));
                        depth_stack.push_back(node_idx);
                        ++num_moved;
                    }

                    if (num_moved == selected_idxs.Size)
                    {
                        break;
                    }
                }
            }

            editor.click_interaction_type = ClickInteractionType_None;
        }
    }
    break;
    case ClickInteractionType_Node:
    {
        translate_selected_nodes(editor);

        if (g.left_mouse_released)
        {
            editor.click_interaction_type = ClickInteractionType_None;
        }
    }
    break;
    case ClickInteractionType_Link:
    {
        if (g.left_mouse_released)
        {
            editor.click_interaction_type = ClickInteractionType_None;
        }
    }
    break;
    case ClickInteractionType_LinkCreation:
    {
        const PinData& start_pin =
            editor.pins.pool[editor.click_interaction_state.link_creation.start_pin_idx];

        const OptionalIndex maybe_duplicate_link_idx =
            g.hovered_pin_idx.has_value()
                ? find_duplicate_link(
                      editor,
                      editor.click_interaction_state.link_creation.start_pin_idx,
                      g.hovered_pin_idx.value())
                : OptionalIndex();

        const bool should_snap =
            g.hovered_pin_idx.has_value() &&
            should_link_snap_to_pin(
                editor, start_pin, g.hovered_pin_idx.value(), maybe_duplicate_link_idx);

        // If we created on snap and the hovered pin is empty or changed, then we need signal that
        // the link's state has changed.
        const bool snapping_pin_changed =
            editor.click_interaction_state.link_creation.end_pin_idx.has_value() &&
            !(g.hovered_pin_idx == editor.click_interaction_state.link_creation.end_pin_idx);

        // Detach the link that was created by this link event if it's no longer in snap range
        if (snapping_pin_changed && g.snap_link_idx.has_value())
        {
            begin_link_detach(
                editor,
                g.snap_link_idx.value(),
                editor.click_interaction_state.link_creation.end_pin_idx.value());
        }

        const ImVec2 start_pos = get_screen_space_pin_coordinates(editor, start_pin);
        // If we are within the hover radius of a receiving pin, snap the link
        // endpoint to it
        const ImVec2 end_pos = should_snap
                                   ? get_screen_space_pin_coordinates(
                                         editor, editor.pins.pool[g.hovered_pin_idx.value()])
                                   : g.mouse_pos;

        const LinkBezierData link_data = get_link_renderable(
            start_pos, end_pos, start_pin.type, g.style.link_line_segments_per_length);
#if IMGUI_VERSION_NUM < 18000
        g.canvas_draw_list->AddBezierCurve(
#else
        g.canvas_draw_list->AddBezierCubic(
#endif
            link_data.bezier.p0,
            link_data.bezier.p1,
            link_data.bezier.p2,
            link_data.bezier.p3,
            g.style.colors[ColorStyle_Link],
            g.style.link_thickness,
            link_data.num_segments);

        const bool link_creation_on_snap =
            g.hovered_pin_idx.has_value() && (editor.pins.pool[g.hovered_pin_idx.value()].flags &
                                              AttributeFlags_EnableLinkCreationOnSnap);

        if (!should_snap)
        {
            editor.click_interaction_state.link_creation.end_pin_idx.reset();
        }

        const bool create_link = should_snap && (g.left_mouse_released || link_creation_on_snap);

        if (create_link && !maybe_duplicate_link_idx.has_value())
        {
            // Avoid send OnLinkCreated() events every frame if the snap link is not saved
            // (only applies for EnableLinkCreationOnSnap)
            if (!g.left_mouse_released &&
                editor.click_interaction_state.link_creation.end_pin_idx == g.hovered_pin_idx)
            {
                break;
            }

            g.element_state_change |= ElementStateChange_LinkCreated;
            editor.click_interaction_state.link_creation.end_pin_idx = g.hovered_pin_idx.value();
        }

        if (g.left_mouse_released)
        {
            editor.click_interaction_type = ClickInteractionType_None;
            if (!create_link)
            {
                g.element_state_change |= ElementStateChange_LinkDropped;
            }
        }
    }
    break;
    case ClickInteractionType_Panning:
    {
        const bool dragging =
            g.io.emulate_three_button_mouse.enabled
                ? (g.left_mouse_dragging && (*g.io.emulate_three_button_mouse.modifier))
                : g.middle_mouse_dragging;

        if (dragging)
        {
            editor.panning += ImGui::GetIO().MouseDelta;
        }
        else
        {
            editor.click_interaction_type = ClickInteractionType_None;
        }
    }
    break;
    case ClickInteractionType_None:
        break;
    default:
        assert(!"Unreachable code!");
        break;
    }
}

OptionalIndex resolve_hovered_node(const EditorContext& editor)
{
    if (g.node_indices_overlapping_with_mouse.Size == 0)
    {
        return OptionalIndex();
    }

    int largest_depth_idx = -1;
    int node_idx_on_top = -1;

    const ImVector<int>& depth_stack = editor.node_depth_order;
    for (int i = 0; i < g.node_indices_overlapping_with_mouse.Size; ++i)
    {
        const int node_idx = g.node_indices_overlapping_with_mouse[i];
        for (int depth_idx = 0; depth_idx < depth_stack.Size; ++depth_idx)
        {
            if (depth_stack[depth_idx] == node_idx && (depth_idx > largest_depth_idx))
            {
                largest_depth_idx = depth_idx;
                node_idx_on_top = node_idx;
            }
        }
    }

    assert(node_idx_on_top != -1);
    return OptionalIndex(node_idx_on_top);
}

// [SECTION] render helpers

inline ImVec2 screen_space_to_grid_space(const EditorContext& editor, const ImVec2& v)
{
    return v - g.canvas_origin_screen_space - editor.panning;
}

inline ImVec2 grid_space_to_screen_space(const EditorContext& editor, const ImVec2& v)
{
    return v + g.canvas_origin_screen_space + editor.panning;
}

inline ImVec2 grid_space_to_editor_space(const EditorContext& editor, const ImVec2& v)
{
    return v + editor.panning;
}

inline ImVec2 editor_space_to_grid_space(const EditorContext& editor, const ImVec2& v)
{
    return v - editor.panning;
}

inline ImVec2 editor_space_to_screen_space(const ImVec2& v)
{
    return g.canvas_origin_screen_space + v;
}

inline ImRect get_item_rect() { return ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax()); }

inline ImVec2 get_node_title_bar_origin(const NodeData& node)
{
    return node.origin + node.layout_style.padding;
}

inline ImVec2 get_node_content_origin(const NodeData& node)
{
    const ImVec2 title_bar_height =
        ImVec2(0.f, node.title_bar_content_rect.GetHeight() + 2.0f * node.layout_style.padding.y);
    return node.origin + title_bar_height + node.layout_style.padding;
}

inline ImRect get_node_title_rect(const NodeData& node)
{
    ImRect expanded_title_rect = node.title_bar_content_rect;
    expanded_title_rect.Expand(node.layout_style.padding);

    return ImRect(
        expanded_title_rect.Min,
        expanded_title_rect.Min + ImVec2(node.rect.GetWidth(), 0.f) +
            ImVec2(0.f, expanded_title_rect.GetHeight()));
}

void draw_grid(EditorContext& editor, const ImVec2& canvas_size)
{
    const ImVec2 offset = editor.panning;

    for (float x = fmodf(offset.x, g.style.grid_spacing); x < canvas_size.x;
         x += g.style.grid_spacing)
    {
        g.canvas_draw_list->AddLine(
            editor_space_to_screen_space(ImVec2(x, 0.0f)),
            editor_space_to_screen_space(ImVec2(x, canvas_size.y)),
            g.style.colors[ColorStyle_GridLine]);
    }

    for (float y = fmodf(offset.y, g.style.grid_spacing); y < canvas_size.y;
         y += g.style.grid_spacing)
    {
        g.canvas_draw_list->AddLine(
            editor_space_to_screen_space(ImVec2(0.0f, y)),
            editor_space_to_screen_space(ImVec2(canvas_size.x, y)),
            g.style.colors[ColorStyle_GridLine]);
    }
}

struct QuadOffsets
{
    ImVec2 top_left, bottom_left, bottom_right, top_right;
};

QuadOffsets calculate_quad_offsets(const float side_length)
{
    const float half_side = 0.5f * side_length;

    QuadOffsets offset;

    offset.top_left = ImVec2(-half_side, half_side);
    offset.bottom_left = ImVec2(-half_side, -half_side);
    offset.bottom_right = ImVec2(half_side, -half_side);
    offset.top_right = ImVec2(half_side, half_side);

    return offset;
}

struct TriangleOffsets
{
    ImVec2 top_left, bottom_left, right;
};

TriangleOffsets calculate_triangle_offsets(const float side_length)
{
    // Calculates the Vec2 offsets from an equilateral triangle's midpoint to
    // its vertices. Here is how the left_offset and right_offset are
    // calculated.
    //
    // For an equilateral triangle of side length s, the
    // triangle's height, h, is h = s * sqrt(3) / 2.
    //
    // The length from the base to the midpoint is (1 / 3) * h. The length from
    // the midpoint to the triangle vertex is (2 / 3) * h.
    const float sqrt_3 = sqrtf(3.0f);
    const float left_offset = -0.1666666666667f * sqrt_3 * side_length;
    const float right_offset = 0.333333333333f * sqrt_3 * side_length;
    const float vertical_offset = 0.5f * side_length;

    TriangleOffsets offset;
    offset.top_left = ImVec2(left_offset, vertical_offset);
    offset.bottom_left = ImVec2(left_offset, -vertical_offset);
    offset.right = ImVec2(right_offset, 0.f);

    return offset;
}

void draw_pin_shape(const ImVec2& pin_pos, const PinData& pin, const ImU32 pin_color)
{
    static const int circle_num_segments = 8;

    switch (pin.shape)
    {
    case PinShape_Circle:
    {
        g.canvas_draw_list->AddCircle(
            pin_pos,
            g.style.pin_circle_radius,
            pin_color,
            circle_num_segments,
            g.style.pin_line_thickness);
    }
    break;
    case PinShape_CircleFilled:
    {
        g.canvas_draw_list->AddCircleFilled(
            pin_pos, g.style.pin_circle_radius, pin_color, circle_num_segments);
    }
    break;
    case PinShape_Quad:
    {
        const QuadOffsets offset = calculate_quad_offsets(g.style.pin_quad_side_length);
        g.canvas_draw_list->AddQuad(
            pin_pos + offset.top_left,
            pin_pos + offset.bottom_left,
            pin_pos + offset.bottom_right,
            pin_pos + offset.top_right,
            pin_color,
            g.style.pin_line_thickness);
    }
    break;
    case PinShape_QuadFilled:
    {
        const QuadOffsets offset = calculate_quad_offsets(g.style.pin_quad_side_length);
        g.canvas_draw_list->AddQuadFilled(
            pin_pos + offset.top_left,
            pin_pos + offset.bottom_left,
            pin_pos + offset.bottom_right,
            pin_pos + offset.top_right,
            pin_color);
    }
    break;
    case PinShape_Triangle:
    {
        const TriangleOffsets offset = calculate_triangle_offsets(g.style.pin_triangle_side_length);
        g.canvas_draw_list->AddTriangle(
            pin_pos + offset.top_left,
            pin_pos + offset.bottom_left,
            pin_pos + offset.right,
            pin_color,
            // NOTE: for some weird reason, the line drawn by AddTriangle is
            // much thinner than the lines drawn by AddCircle or AddQuad.
            // Multiplying the line thickness by two seemed to solve the
            // problem at a few different thickness values.
            2.f * g.style.pin_line_thickness);
    }
    break;
    case PinShape_TriangleFilled:
    {
        const TriangleOffsets offset = calculate_triangle_offsets(g.style.pin_triangle_side_length);
        g.canvas_draw_list->AddTriangleFilled(
            pin_pos + offset.top_left,
            pin_pos + offset.bottom_left,
            pin_pos + offset.right,
            pin_color);
    }
    break;
    default:
        assert(!"Invalid PinShape value!");
        break;
    }
}

bool is_pin_hovered(const PinData& pin)
{
    return is_mouse_hovering_near_point(pin.pos, g.style.pin_hover_radius);
}

void draw_pin(EditorContext& editor, const int pin_idx, const bool left_mouse_clicked)
{
    PinData& pin = editor.pins.pool[pin_idx];
    const ImRect& parent_node_rect = editor.nodes.pool[pin.parent_node_idx].rect;

    pin.pos = get_screen_space_pin_coordinates(parent_node_rect, pin.attribute_rect, pin.type);

    ImU32 pin_color = pin.color_style.background;

    const bool pin_hovered = is_pin_hovered(pin) && mouse_in_canvas() &&
                             editor.click_interaction_type != ClickInteractionType_BoxSelection;

    if (pin_hovered)
    {
        g.hovered_pin_idx = pin_idx;
        g.hovered_pin_flags = pin.flags;
        pin_color = pin.color_style.hovered;

        if (left_mouse_clicked)
        {
            begin_link_creation(editor, pin_idx);
        }
    }

    draw_pin_shape(pin.pos, pin, pin_color);
}

// TODO: Separate hover code from drawing code to avoid this unpleasant divergent function
// signature.
bool is_node_hovered(const NodeData& node, const int node_idx, const ObjectPool<PinData> pins)
{
    // We render pins on top of nodes. In order to prevent node interaction when a pin is on top of
    // a node, we just early out here if a pin is hovered.
    for (int i = 0; i < node.pin_indices.size(); ++i)
    {
        const PinData& pin = pins.pool[node.pin_indices[i]];
        if (is_pin_hovered(pin))
        {
            return false;
        }
    }

    return g.hovered_node_idx.has_value() && node_idx == g.hovered_node_idx.value();
}

// TODO: It may be useful to make this an EditorContext method, since this uses
// a lot of editor state. Currently that is just not clear, since we don't pass
// the editor as a part of the function signature.
void draw_node(EditorContext& editor, const int node_idx)
{
    const NodeData& node = editor.nodes.pool[node_idx];
    ImGui::SetCursorPos(node.origin + editor.panning);

    const bool node_hovered = is_node_hovered(node, node_idx, editor.pins) && mouse_in_canvas() &&
                              editor.click_interaction_type != ClickInteractionType_BoxSelection;

    ImU32 node_background = node.color_style.background;
    ImU32 titlebar_background = node.color_style.titlebar;

    if (editor.selected_node_indices.contains(node_idx))
    {
        node_background = node.color_style.background_selected;
        titlebar_background = node.color_style.titlebar_selected;
    }
    else if (node_hovered)
    {
        node_background = node.color_style.background_hovered;
        titlebar_background = node.color_style.titlebar_hovered;
    }

    {
        // node base
        g.canvas_draw_list->AddRectFilled(
            node.rect.Min, node.rect.Max, node_background, node.layout_style.corner_rounding);

        // title bar:
        if (node.title_bar_content_rect.GetHeight() > 0.f)
        {
            ImRect title_bar_rect = get_node_title_rect(node);

            g.canvas_draw_list->AddRectFilled(
                title_bar_rect.Min,
                title_bar_rect.Max,
                titlebar_background,
                node.layout_style.corner_rounding,
                ImDrawCornerFlags_Top);
        }

        if ((g.style.flags & StyleFlags_NodeOutline) != 0)
        {
            g.canvas_draw_list->AddRect(
                node.rect.Min,
                node.rect.Max,
                node.color_style.outline,
                node.layout_style.corner_rounding,
                ImDrawCornerFlags_All,
                node.layout_style.border_thickness);
        }
    }

    for (int i = 0; i < node.pin_indices.size(); ++i)
    {
        draw_pin(editor, node.pin_indices[i], g.left_mouse_clicked);
    }

    if (node_hovered)
    {
        g.hovered_node_idx = node_idx;
        const bool node_ui_interaction = g.interactive_node_idx == node_idx;
        if (g.left_mouse_clicked && !node_ui_interaction)
        {
            begin_node_selection(editor, node_idx);
        }
    }
}

bool is_link_hovered(const LinkBezierData& link_data)
{
    // We render pins and nodes on top of links. In order to prevent link interaction when a pin or
    // node is on top of a link, we just early out here if a pin or node is hovered.
    if (g.hovered_pin_idx.has_value() || g.hovered_node_idx.has_value())
    {
        return false;
    }

    return is_mouse_hovering_near_link(link_data.bezier, link_data.num_segments);
}

void draw_link(EditorContext& editor, const int link_idx)
{
    const LinkData& link = editor.links.pool[link_idx];
    const PinData& start_pin = editor.pins.pool[link.start_pin_idx];
    const PinData& end_pin = editor.pins.pool[link.end_pin_idx];

    const LinkBezierData link_data = get_link_renderable(
        start_pin.pos, end_pin.pos, start_pin.type, g.style.link_line_segments_per_length);

    const bool link_hovered = is_link_hovered(link_data) && mouse_in_canvas() &&
                              editor.click_interaction_type != ClickInteractionType_BoxSelection;

    if (link_hovered)
    {
        g.hovered_link_idx = link_idx;
        if (g.left_mouse_clicked)
        {
            begin_link_interaction(editor, link_idx);
        }
    }

    // It's possible for a link to be deleted in begin_link_interaction. A user
    // may detach a link, resulting in the link wire snapping to the mouse
    // position.
    //
    // In other words, skip rendering the link if it was deleted.
    if (g.deleted_link_idx == link_idx)
    {
        return;
    }

    ImU32 link_color = link.color_style.base;
    if (editor.selected_link_indices.contains(link_idx))
    {
        link_color = link.color_style.selected;
    }
    else if (link_hovered)
    {
        link_color = link.color_style.hovered;
    }

#if IMGUI_VERSION_NUM < 18000
    g.canvas_draw_list->AddBezierCurve(
#else
    g.canvas_draw_list->AddBezierCubic(
#endif
        link_data.bezier.p0,
        link_data.bezier.p1,
        link_data.bezier.p2,
        link_data.bezier.p3,
        link_color,
        g.style.link_thickness,
        link_data.num_segments);
}

void begin_pin_attribute(
    const int id,
    const AttributeType type,
    const PinShape shape,
    const int node_idx)
{
    // Make sure to call BeginNode() before calling
    // BeginAttribute()
    assert(g.current_scope == Scope_Node);
    g.current_scope = Scope_Attribute;

    ImGui::BeginGroup();
    ImGui::PushID(id);

    EditorContext& editor = editor_context_get();

    g.current_attribute_id = id;

    const int pin_idx = object_pool_find_or_create_index(editor.pins, id);
    g.current_pin_idx = pin_idx;
    PinData& pin = editor.pins.pool[pin_idx];
    pin.id = id;
    pin.parent_node_idx = node_idx;
    pin.type = type;
    pin.shape = shape;
    pin.flags = g.current_attribute_flags;
    pin.color_style.background = g.style.colors[ColorStyle_Pin];
    pin.color_style.hovered = g.style.colors[ColorStyle_PinHovered];
}

void end_pin_attribute()
{
    assert(g.current_scope == Scope_Attribute);
    g.current_scope = Scope_Node;

    ImGui::PopID();
    ImGui::EndGroup();

    if (ImGui::IsItemActive())
    {
        g.active_attribute = true;
        g.active_attribute_id = g.current_attribute_id;
        g.interactive_node_idx = g.current_node_idx;
    }

    EditorContext& editor = editor_context_get();
    PinData& pin = editor.pins.pool[g.current_pin_idx];
    NodeData& node = editor.nodes.pool[g.current_node_idx];
    pin.attribute_rect = get_item_rect();
    node.pin_indices.push_back(g.current_pin_idx);
}
} // namespace

// [SECTION] API implementation

IO::EmulateThreeButtonMouse::EmulateThreeButtonMouse() : enabled(false), modifier(NULL) {}

IO::LinkDetachWithModifierClick::LinkDetachWithModifierClick() : modifier(NULL) {}

IO::IO() : emulate_three_button_mouse(), link_detach_with_modifier_click() {}

Style::Style()
    : grid_spacing(32.f), node_corner_rounding(4.f), node_padding_horizontal(8.f),
      node_padding_vertical(8.f), node_border_thickness(1.f), link_thickness(3.f),
      link_line_segments_per_length(0.1f), link_hover_distance(10.f), pin_circle_radius(4.f),
      pin_quad_side_length(7.f), pin_triangle_side_length(9.5), pin_line_thickness(1.f),
      pin_hover_radius(10.f), pin_offset(0.f),
      flags(StyleFlags(StyleFlags_NodeOutline | StyleFlags_GridLines)), colors()
{
}

EditorContext* EditorContextCreate()
{
    void* mem = ImGui::MemAlloc(sizeof(EditorContext));
    new (mem) EditorContext();
    return (EditorContext*)mem;
}

void EditorContextFree(EditorContext* ctx)
{
    ctx->~EditorContext();
    ImGui::MemFree(ctx);
}

void EditorContextSet(EditorContext* ctx) { g.editor_ctx = ctx; }

ImVec2 EditorContextGetPanning()
{
    const EditorContext& editor = editor_context_get();
    return editor.panning;
}

void EditorContextResetPanning(const ImVec2& pos)
{
    EditorContext& editor = editor_context_get();
    editor.panning = pos;
}

void EditorContextMoveToNode(const int node_id)
{
    EditorContext& editor = editor_context_get();
    NodeData& node = object_pool_find_or_create_object(editor.nodes, node_id);

    editor.panning.x = -node.origin.x;
    editor.panning.y = -node.origin.y;
}

void Initialize()
{
    g.canvas_origin_screen_space = ImVec2(0.0f, 0.0f);
    g.canvas_rect_screen_space = ImRect(ImVec2(0.f, 0.f), ImVec2(0.f, 0.f));
    g.current_scope = Scope_None;

    g.current_pin_idx = INT_MAX;
    g.current_node_idx = INT_MAX;

    g.default_editor_ctx = EditorContextCreate();
    EditorContextSet(g.default_editor_ctx);

    const ImGuiIO& io = ImGui::GetIO();
    g.io.emulate_three_button_mouse.modifier = &io.KeyAlt;

    g.current_attribute_flags = AttributeFlags_None;
    g.attribute_flag_stack.push_back(g.current_attribute_flags);

    StyleColorsDark();
}

void Shutdown()
{
    EditorContextFree(g.default_editor_ctx);
    g.editor_ctx = NULL;
    g.default_editor_ctx = NULL;
}

IO& GetIO() { return g.io; }

Style& GetStyle() { return g.style; }

void StyleColorsDark()
{
    g.style.colors[ColorStyle_NodeBackground] = IM_COL32(50, 50, 50, 255);
    g.style.colors[ColorStyle_NodeBackgroundHovered] = IM_COL32(75, 75, 75, 255);
    g.style.colors[ColorStyle_NodeBackgroundSelected] = IM_COL32(75, 75, 75, 255);
    g.style.colors[ColorStyle_NodeOutline] = IM_COL32(100, 100, 100, 255);
    // title bar colors match ImGui's titlebg colors
    g.style.colors[ColorStyle_TitleBar] = IM_COL32(41, 74, 122, 255);
    g.style.colors[ColorStyle_TitleBarHovered] = IM_COL32(66, 150, 250, 255);
    g.style.colors[ColorStyle_TitleBarSelected] = IM_COL32(66, 150, 250, 255);
    // link colors match ImGui's slider grab colors
    g.style.colors[ColorStyle_Link] = IM_COL32(61, 133, 224, 200);
    g.style.colors[ColorStyle_LinkHovered] = IM_COL32(66, 150, 250, 255);
    g.style.colors[ColorStyle_LinkSelected] = IM_COL32(66, 150, 250, 255);
    // pin colors match ImGui's button colors
    g.style.colors[ColorStyle_Pin] = IM_COL32(53, 150, 250, 180);
    g.style.colors[ColorStyle_PinHovered] = IM_COL32(53, 150, 250, 255);

    g.style.colors[ColorStyle_BoxSelector] = IM_COL32(61, 133, 224, 30);
    g.style.colors[ColorStyle_BoxSelectorOutline] = IM_COL32(61, 133, 224, 150);

    g.style.colors[ColorStyle_GridBackground] = IM_COL32(40, 40, 50, 200);
    g.style.colors[ColorStyle_GridLine] = IM_COL32(200, 200, 200, 40);
}

void StyleColorsClassic()
{
    g.style.colors[ColorStyle_NodeBackground] = IM_COL32(50, 50, 50, 255);
    g.style.colors[ColorStyle_NodeBackgroundHovered] = IM_COL32(75, 75, 75, 255);
    g.style.colors[ColorStyle_NodeBackgroundSelected] = IM_COL32(75, 75, 75, 255);
    g.style.colors[ColorStyle_NodeOutline] = IM_COL32(100, 100, 100, 255);
    g.style.colors[ColorStyle_TitleBar] = IM_COL32(69, 69, 138, 255);
    g.style.colors[ColorStyle_TitleBarHovered] = IM_COL32(82, 82, 161, 255);
    g.style.colors[ColorStyle_TitleBarSelected] = IM_COL32(82, 82, 161, 255);
    g.style.colors[ColorStyle_Link] = IM_COL32(255, 255, 255, 100);
    g.style.colors[ColorStyle_LinkHovered] = IM_COL32(105, 99, 204, 153);
    g.style.colors[ColorStyle_LinkSelected] = IM_COL32(105, 99, 204, 153);
    g.style.colors[ColorStyle_Pin] = IM_COL32(89, 102, 156, 170);
    g.style.colors[ColorStyle_PinHovered] = IM_COL32(102, 122, 179, 200);
    g.style.colors[ColorStyle_BoxSelector] = IM_COL32(82, 82, 161, 100);
    g.style.colors[ColorStyle_BoxSelectorOutline] = IM_COL32(82, 82, 161, 255);
    g.style.colors[ColorStyle_GridBackground] = IM_COL32(40, 40, 50, 200);
    g.style.colors[ColorStyle_GridLine] = IM_COL32(200, 200, 200, 40);
}

void StyleColorsLight()
{
    g.style.colors[ColorStyle_NodeBackground] = IM_COL32(240, 240, 240, 255);
    g.style.colors[ColorStyle_NodeBackgroundHovered] = IM_COL32(240, 240, 240, 255);
    g.style.colors[ColorStyle_NodeBackgroundSelected] = IM_COL32(240, 240, 240, 255);
    g.style.colors[ColorStyle_NodeOutline] = IM_COL32(100, 100, 100, 255);
    g.style.colors[ColorStyle_TitleBar] = IM_COL32(248, 248, 248, 255);
    g.style.colors[ColorStyle_TitleBarHovered] = IM_COL32(209, 209, 209, 255);
    g.style.colors[ColorStyle_TitleBarSelected] = IM_COL32(209, 209, 209, 255);
    // original imgui values: 66, 150, 250
    g.style.colors[ColorStyle_Link] = IM_COL32(66, 150, 250, 100);
    // original imgui values: 117, 138, 204
    g.style.colors[ColorStyle_LinkHovered] = IM_COL32(66, 150, 250, 242);
    g.style.colors[ColorStyle_LinkSelected] = IM_COL32(66, 150, 250, 242);
    // original imgui values: 66, 150, 250
    g.style.colors[ColorStyle_Pin] = IM_COL32(66, 150, 250, 160);
    g.style.colors[ColorStyle_PinHovered] = IM_COL32(66, 150, 250, 255);
    g.style.colors[ColorStyle_BoxSelector] = IM_COL32(90, 170, 250, 30);
    g.style.colors[ColorStyle_BoxSelectorOutline] = IM_COL32(90, 170, 250, 150);
    g.style.colors[ColorStyle_GridBackground] = IM_COL32(225, 225, 225, 255);
    g.style.colors[ColorStyle_GridLine] = IM_COL32(180, 180, 180, 100);
    g.style.flags = StyleFlags(StyleFlags_None);
}

void BeginNodeEditor()
{
    assert(g.current_scope == Scope_None);
    g.current_scope = Scope_Editor;

    // Reset state from previous pass

    EditorContext& editor = editor_context_get();
    object_pool_reset(editor.nodes);
    object_pool_reset(editor.pins);
    object_pool_reset(editor.links);

    g.hovered_node_idx.reset();
    g.interactive_node_idx.reset();
    g.hovered_link_idx.reset();
    g.hovered_pin_idx.reset();
    g.hovered_pin_flags = AttributeFlags_None;
    g.deleted_link_idx.reset();
    g.snap_link_idx.reset();

    g.node_indices_overlapping_with_mouse.clear();

    g.element_state_change = ElementStateChange_None;

    g.mouse_pos = ImGui::GetIO().MousePos;
    g.left_mouse_clicked = ImGui::IsMouseClicked(0);
    g.left_mouse_released = ImGui::IsMouseReleased(0);
    g.middle_mouse_clicked = ImGui::IsMouseClicked(2);
    g.left_mouse_dragging = ImGui::IsMouseDragging(0, 0.0f);
    g.middle_mouse_dragging = ImGui::IsMouseDragging(2, 0.0f);

    g.active_attribute = false;

    ImGui::BeginGroup();
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1.f, 1.f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, g.style.colors[ColorStyle_GridBackground]);
        ImGui::BeginChild(
            "scrolling_region",
            ImVec2(0.f, 0.f),
            true,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoScrollWithMouse);
        g.canvas_origin_screen_space = ImGui::GetCursorScreenPos();

        // NOTE: we have to fetch the canvas draw list *after* we call
        // BeginChild(), otherwise the ImGui UI elements are going to be
        // rendered into the parent window draw list.
        draw_list_set(ImGui::GetWindowDrawList());

        {
            const ImVec2 canvas_size = ImGui::GetWindowSize();
            g.canvas_rect_screen_space = ImRect(
                editor_space_to_screen_space(ImVec2(0.f, 0.f)),
                editor_space_to_screen_space(canvas_size));

            if (g.style.flags & StyleFlags_GridLines)
            {
                draw_grid(editor, canvas_size);
            }
        }
    }
}

void EndNodeEditor()
{
    assert(g.current_scope == Scope_Editor);
    g.current_scope = Scope_None;

    EditorContext& editor = editor_context_get();

    // Resolve which node is actually on top and being hovered. This needs to be done before any of
    // the nodes can be rendered.

    g.hovered_node_idx = resolve_hovered_node(editor);

    // Render the nodes and resolve which pin the mouse is hovering over. The hovered pin is needed
    // for handling click interactions.

    for (int node_idx = 0; node_idx < editor.nodes.pool.size(); ++node_idx)
    {
        if (editor.nodes.in_use[node_idx])
        {
            draw_list_activate_node_background(node_idx);
            draw_node(editor, node_idx);
        }
    }

    // In order to render the links underneath the nodes, we want to first select the bottom draw
    // channel.
    g.canvas_draw_list->ChannelsSetCurrent(0);

    for (int link_idx = 0; link_idx < editor.links.pool.size(); ++link_idx)
    {
        if (editor.links.in_use[link_idx])
        {
            draw_link(editor, link_idx);
        }
    }

    // Render the click interaction UI elements (partial links, box selector) on top of everything
    // else.

    draw_list_append_click_interaction_channel();
    draw_list_activate_click_interaction_channel();

    if (g.left_mouse_clicked || g.middle_mouse_clicked)
    {
        begin_canvas_interaction(editor);
    }

    click_interaction_update(editor);

    // At this point, draw commands have been issued for all nodes (and pins). Update the node pool
    // to detect unused node slots and remove those indices from the depth stack before sorting the
    // node draw commands by depth.
    object_pool_update(editor.nodes);
    object_pool_update(editor.pins);

    draw_list_sort_channels_by_depth(editor.node_depth_order);

    // After the links have been rendered, the link pool can be updated as well.
    object_pool_update(editor.links);

    // Finally, merge the draw channels
    g.canvas_draw_list->ChannelsMerge();

    // pop style
    ImGui::EndChild();      // end scrolling region
    ImGui::PopStyleColor(); // pop child window background color
    ImGui::PopStyleVar();   // pop window padding
    ImGui::PopStyleVar();   // pop frame padding
    ImGui::EndGroup();
}

void BeginNode(const int node_id)
{
    // Remember to call BeginNodeEditor before calling BeginNode
    assert(g.current_scope == Scope_Editor);
    g.current_scope = Scope_Node;

    EditorContext& editor = editor_context_get();

    const int node_idx = object_pool_find_or_create_index(editor.nodes, node_id);
    g.current_node_idx = node_idx;

    NodeData& node = editor.nodes.pool[node_idx];
    node.color_style.background = g.style.colors[ColorStyle_NodeBackground];
    node.color_style.background_hovered = g.style.colors[ColorStyle_NodeBackgroundHovered];
    node.color_style.background_selected = g.style.colors[ColorStyle_NodeBackgroundSelected];
    node.color_style.outline = g.style.colors[ColorStyle_NodeOutline];
    node.color_style.titlebar = g.style.colors[ColorStyle_TitleBar];
    node.color_style.titlebar_hovered = g.style.colors[ColorStyle_TitleBarHovered];
    node.color_style.titlebar_selected = g.style.colors[ColorStyle_TitleBarSelected];
    node.layout_style.corner_rounding = g.style.node_corner_rounding;
    node.layout_style.padding =
        ImVec2(g.style.node_padding_horizontal, g.style.node_padding_vertical);
    node.layout_style.border_thickness = g.style.node_border_thickness;

    // ImGui::SetCursorPos sets the cursor position, local to the current widget
    // (in this case, the child object started in BeginNodeEditor). Use
    // ImGui::SetCursorScreenPos to set the screen space coordinates directly.
    ImGui::SetCursorPos(grid_space_to_editor_space(editor, get_node_title_bar_origin(node)));

    draw_list_add_node(node_idx);
    draw_list_activate_current_node_foreground();

    ImGui::PushID(node.id);
    ImGui::BeginGroup();
}

void EndNode()
{
    assert(g.current_scope == Scope_Node);
    g.current_scope = Scope_Editor;

    EditorContext& editor = editor_context_get();

    // The node's rectangle depends on the ImGui UI group size.
    ImGui::EndGroup();
    ImGui::PopID();

    NodeData& node = editor.nodes.pool[g.current_node_idx];
    node.rect = get_item_rect();
    node.rect.Expand(node.layout_style.padding);

    if (node.rect.Contains(g.mouse_pos))
    {
        g.node_indices_overlapping_with_mouse.push_back(g.current_node_idx);
    }
}

ImVec2 GetNodeDimensions(int node_id)
{
    EditorContext& editor = editor_context_get();
    const int node_idx = object_pool_find(editor.nodes, node_id);
    assert(node_idx != -1); // invalid node_id
    const NodeData& node = editor.nodes.pool[node_idx];
    return node.rect.GetSize();
}

void BeginNodeTitleBar()
{
    assert(g.current_scope == Scope_Node);
    ImGui::BeginGroup();
}

void EndNodeTitleBar()
{
    assert(g.current_scope == Scope_Node);
    ImGui::EndGroup();

    EditorContext& editor = editor_context_get();
    NodeData& node = editor.nodes.pool[g.current_node_idx];
    node.title_bar_content_rect = get_item_rect();

    ImGui::ItemAdd(get_node_title_rect(node), ImGui::GetID("title_bar"));

    ImGui::SetCursorPos(grid_space_to_editor_space(editor, get_node_content_origin(node)));
}

void BeginInputAttribute(const int id, const PinShape shape)
{
    begin_pin_attribute(id, AttributeType_Input, shape, g.current_node_idx);
}

void EndInputAttribute() { end_pin_attribute(); }

void BeginOutputAttribute(const int id, const PinShape shape)
{
    begin_pin_attribute(id, AttributeType_Output, shape, g.current_node_idx);
}

void EndOutputAttribute() { end_pin_attribute(); }

void BeginStaticAttribute(const int id)
{
    // Make sure to call BeginNode() before calling BeginAttribute()
    assert(g.current_scope == Scope_Node);
    g.current_scope = Scope_Attribute;

    g.current_attribute_id = id;

    ImGui::BeginGroup();
    ImGui::PushID(id);
}

void EndStaticAttribute()
{
    // Make sure to call BeginNode() before calling BeginAttribute()
    assert(g.current_scope == Scope_Attribute);
    g.current_scope = Scope_Node;

    ImGui::PopID();
    ImGui::EndGroup();

    if (ImGui::IsItemActive())
    {
        g.active_attribute = true;
        g.active_attribute_id = g.current_attribute_id;
        g.interactive_node_idx = g.current_node_idx;
    }
}

void PushAttributeFlag(AttributeFlags flag)
{
    g.current_attribute_flags |= static_cast<int>(flag);
    g.attribute_flag_stack.push_back(g.current_attribute_flags);
}

void PopAttributeFlag()
{
    // PopAttributeFlag called without a matching PushAttributeFlag!
    // The bottom value is always the default value, pushed in Initialize().
    assert(g.attribute_flag_stack.size() > 1);

    g.attribute_flag_stack.pop_back();
    g.current_attribute_flags = g.attribute_flag_stack.back();
}

void Link(int id, const int start_attr_id, const int end_attr_id)
{
    assert(g.current_scope == Scope_Editor);

    EditorContext& editor = editor_context_get();
    LinkData& link = object_pool_find_or_create_object(editor.links, id);
    link.id = id;
    link.start_pin_idx = object_pool_find_or_create_index(editor.pins, start_attr_id);
    link.end_pin_idx = object_pool_find_or_create_index(editor.pins, end_attr_id);
    link.color_style.base = g.style.colors[ColorStyle_Link];
    link.color_style.hovered = g.style.colors[ColorStyle_LinkHovered];
    link.color_style.selected = g.style.colors[ColorStyle_LinkSelected];

    // Check if this link was created by the current link event
    if ((editor.click_interaction_type == ClickInteractionType_LinkCreation &&
         editor.pins.pool[link.end_pin_idx].flags & AttributeFlags_EnableLinkCreationOnSnap &&
         editor.click_interaction_state.link_creation.start_pin_idx == link.start_pin_idx &&
         editor.click_interaction_state.link_creation.end_pin_idx == link.end_pin_idx) ||
        (editor.click_interaction_state.link_creation.start_pin_idx == link.end_pin_idx &&
         editor.click_interaction_state.link_creation.end_pin_idx == link.start_pin_idx))
    {
        g.snap_link_idx = object_pool_find_or_create_index(editor.links, id);
    }
}

void PushColorStyle(ColorStyle item, unsigned int color)
{
    g.color_modifier_stack.push_back(ColorStyleElement(g.style.colors[item], item));
    g.style.colors[item] = color;
}

void PopColorStyle()
{
    assert(g.color_modifier_stack.size() > 0);
    const ColorStyleElement elem = g.color_modifier_stack.back();
    g.style.colors[elem.item] = elem.color;
    g.color_modifier_stack.pop_back();
}

float& lookup_style_var(const StyleVar item)
{
    // TODO: once the switch gets too big and unwieldy to work with, we could do
    // a byte-offset lookup into the Style struct, using the StyleVar as an
    // index. This is how ImGui does it.
    float* style_var = 0;
    switch (item)
    {
    case StyleVar_GridSpacing:
        style_var = &g.style.grid_spacing;
        break;
    case StyleVar_NodeCornerRounding:
        style_var = &g.style.node_corner_rounding;
        break;
    case StyleVar_NodePaddingHorizontal:
        style_var = &g.style.node_padding_horizontal;
        break;
    case StyleVar_NodePaddingVertical:
        style_var = &g.style.node_padding_vertical;
        break;
    case StyleVar_NodeBorderThickness:
        style_var = &g.style.node_border_thickness;
        break;
    case StyleVar_LinkThickness:
        style_var = &g.style.link_thickness;
        break;
    case StyleVar_LinkLineSegmentsPerLength:
        style_var = &g.style.link_line_segments_per_length;
        break;
    case StyleVar_LinkHoverDistance:
        style_var = &g.style.link_hover_distance;
        break;
    case StyleVar_PinCircleRadius:
        style_var = &g.style.pin_circle_radius;
        break;
    case StyleVar_PinQuadSideLength:
        style_var = &g.style.pin_quad_side_length;
        break;
    case StyleVar_PinTriangleSideLength:
        style_var = &g.style.pin_triangle_side_length;
        break;
    case StyleVar_PinLineThickness:
        style_var = &g.style.pin_line_thickness;
        break;
    case StyleVar_PinHoverRadius:
        style_var = &g.style.pin_hover_radius;
        break;
    case StyleVar_PinOffset:
        style_var = &g.style.pin_offset;
        break;
    default:
        assert(!"Invalid StyleVar value!");
    }

    return *style_var;
}

void PushStyleVar(const StyleVar item, const float value)
{
    float& style_var = lookup_style_var(item);
    g.style_modifier_stack.push_back(StyleElement(style_var, item));
    style_var = value;
}

void PopStyleVar()
{
    assert(g.style_modifier_stack.size() > 0);
    const StyleElement style_elem = g.style_modifier_stack.back();
    g.style_modifier_stack.pop_back();
    float& style_var = lookup_style_var(style_elem.item);
    style_var = style_elem.value;
}

void SetNodeScreenSpacePos(int node_id, const ImVec2& screen_space_pos)
{
    EditorContext& editor = editor_context_get();
    NodeData& node = object_pool_find_or_create_object(editor.nodes, node_id);
    node.origin = screen_space_to_grid_space(editor, screen_space_pos);
}

void SetNodeEditorSpacePos(int node_id, const ImVec2& editor_space_pos)
{
    EditorContext& editor = editor_context_get();
    NodeData& node = object_pool_find_or_create_object(editor.nodes, node_id);
    node.origin = editor_space_to_grid_space(editor, editor_space_pos);
}

void SetNodeGridSpacePos(int node_id, const ImVec2& grid_pos)
{
    EditorContext& editor = editor_context_get();
    NodeData& node = object_pool_find_or_create_object(editor.nodes, node_id);
    node.origin = grid_pos;
}

void SetNodeDraggable(int node_id, const bool draggable)
{
    EditorContext& editor = editor_context_get();
    NodeData& node = object_pool_find_or_create_object(editor.nodes, node_id);
    node.draggable = draggable;
}

ImVec2 GetNodeScreenSpacePos(const int node_id)
{
    EditorContext& editor = editor_context_get();
    const int node_idx = object_pool_find(editor.nodes, node_id);
    assert(node_idx != -1);
    NodeData& node = editor.nodes.pool[node_idx];
    return grid_space_to_screen_space(editor, node.origin);
}

ImVec2 GetNodeEditorSpacePos(const int node_id)
{
    EditorContext& editor = editor_context_get();
    const int node_idx = object_pool_find(editor.nodes, node_id);
    assert(node_idx != -1);
    NodeData& node = editor.nodes.pool[node_idx];
    return grid_space_to_editor_space(editor, node.origin);
}

ImVec2 GetNodeGridSpacePos(int node_id)
{
    EditorContext& editor = editor_context_get();
    const int node_idx = object_pool_find(editor.nodes, node_id);
    assert(node_idx != -1);
    NodeData& node = editor.nodes.pool[node_idx];
    return node.origin;
}

bool IsEditorHovered() { return mouse_in_canvas(); }

bool IsNodeHovered(int* const node_id)
{
    assert(g.current_scope == Scope_None);
    assert(node_id != NULL);

    const bool is_hovered = g.hovered_node_idx.has_value();
    if (is_hovered)
    {
        const EditorContext& editor = editor_context_get();
        *node_id = editor.nodes.pool[g.hovered_node_idx.value()].id;
    }
    return is_hovered;
}

bool IsLinkHovered(int* const link_id)
{
    assert(g.current_scope == Scope_None);
    assert(link_id != NULL);

    const bool is_hovered = g.hovered_link_idx.has_value();
    if (is_hovered)
    {
        const EditorContext& editor = editor_context_get();
        *link_id = editor.links.pool[g.hovered_link_idx.value()].id;
    }
    return is_hovered;
}

bool IsPinHovered(int* const attr)
{
    assert(g.current_scope == Scope_None);
    assert(attr != NULL);

    const bool is_hovered = g.hovered_pin_idx.has_value();
    if (is_hovered)
    {
        const EditorContext& editor = editor_context_get();
        *attr = editor.pins.pool[g.hovered_pin_idx.value()].id;
    }
    return is_hovered;
}

int NumSelectedNodes()
{
    assert(g.current_scope == Scope_None);
    const EditorContext& editor = editor_context_get();
    return editor.selected_node_indices.size();
}

int NumSelectedLinks()
{
    assert(g.current_scope == Scope_None);
    const EditorContext& editor = editor_context_get();
    return editor.selected_link_indices.size();
}

void GetSelectedNodes(int* node_ids)
{
    assert(node_ids != NULL);

    const EditorContext& editor = editor_context_get();
    for (int i = 0; i < editor.selected_node_indices.size(); ++i)
    {
        const int node_idx = editor.selected_node_indices[i];
        node_ids[i] = editor.nodes.pool[node_idx].id;
    }
}

void GetSelectedLinks(int* link_ids)
{
    assert(link_ids != NULL);

    const EditorContext& editor = editor_context_get();
    for (int i = 0; i < editor.selected_link_indices.size(); ++i)
    {
        const int link_idx = editor.selected_link_indices[i];
        link_ids[i] = editor.links.pool[link_idx].id;
    }
}

void ClearNodeSelection()
{
    EditorContext& editor = editor_context_get();
    editor.selected_node_indices.clear();
}

void ClearLinkSelection()
{
    EditorContext& editor = editor_context_get();
    editor.selected_link_indices.clear();
}

bool IsAttributeActive()
{
    assert((g.current_scope & Scope_Node) != 0);

    if (!g.active_attribute)
    {
        return false;
    }

    return g.active_attribute_id == g.current_attribute_id;
}

bool IsAnyAttributeActive(int* const attribute_id)
{
    assert((g.current_scope & (Scope_Node | Scope_Attribute)) == 0);

    if (!g.active_attribute)
    {
        return false;
    }

    if (attribute_id != NULL)
    {
        *attribute_id = g.active_attribute_id;
    }

    return true;
}

bool IsLinkStarted(int* const started_at_id)
{
    // Call this function after EndNodeEditor()!
    assert(g.current_scope == Scope_None);
    assert(started_at_id != NULL);

    const bool is_started = (g.element_state_change & ElementStateChange_LinkStarted) != 0;
    if (is_started)
    {
        const EditorContext& editor = editor_context_get();
        const int pin_idx = editor.click_interaction_state.link_creation.start_pin_idx;
        *started_at_id = editor.pins.pool[pin_idx].id;
    }

    return is_started;
}

bool IsLinkDropped(int* const started_at_id, const bool including_detached_links)
{
    // Call this function after EndNodeEditor()!
    assert(g.current_scope == Scope_None);

    const EditorContext& editor = editor_context_get();

    const bool link_dropped = (g.element_state_change & ElementStateChange_LinkDropped) != 0 &&
                              (including_detached_links ||
                               editor.click_interaction_state.link_creation.link_creation_type !=
                                   LinkCreationType_FromDetach);

    if (link_dropped && started_at_id)
    {
        const int pin_idx = editor.click_interaction_state.link_creation.start_pin_idx;
        *started_at_id = editor.pins.pool[pin_idx].id;
    }

    return link_dropped;
}

bool IsLinkCreated(
    int* const started_at_pin_id,
    int* const ended_at_pin_id,
    bool* const created_from_snap)
{
    assert(g.current_scope == Scope_None);
    assert(started_at_pin_id != NULL);
    assert(ended_at_pin_id != NULL);

    const bool is_created = (g.element_state_change & ElementStateChange_LinkCreated) != 0;

    if (is_created)
    {
        const EditorContext& editor = editor_context_get();
        const int start_idx = editor.click_interaction_state.link_creation.start_pin_idx;
        const int end_idx = editor.click_interaction_state.link_creation.end_pin_idx.value();
        const PinData& start_pin = editor.pins.pool[start_idx];
        const PinData& end_pin = editor.pins.pool[end_idx];

        if (start_pin.type == AttributeType_Output)
        {
            *started_at_pin_id = start_pin.id;
            *ended_at_pin_id = end_pin.id;
        }
        else
        {
            *started_at_pin_id = end_pin.id;
            *ended_at_pin_id = start_pin.id;
        }

        if (created_from_snap)
        {
            *created_from_snap = editor.click_interaction_type == ClickInteractionType_LinkCreation;
        }
    }

    return is_created;
}

bool IsLinkCreated(
    int* started_at_node_id,
    int* started_at_pin_id,
    int* ended_at_node_id,
    int* ended_at_pin_id,
    bool* created_from_snap)
{
    assert(g.current_scope == Scope_None);
    assert(started_at_node_id != NULL);
    assert(started_at_pin_id != NULL);
    assert(ended_at_node_id != NULL);
    assert(ended_at_pin_id != NULL);

    const bool is_created = (g.element_state_change & ElementStateChange_LinkCreated) != 0;

    if (is_created)
    {
        const EditorContext& editor = editor_context_get();
        const int start_idx = editor.click_interaction_state.link_creation.start_pin_idx;
        const int end_idx = editor.click_interaction_state.link_creation.end_pin_idx.value();
        const PinData& start_pin = editor.pins.pool[start_idx];
        const NodeData& start_node = editor.nodes.pool[start_pin.parent_node_idx];
        const PinData& end_pin = editor.pins.pool[end_idx];
        const NodeData& end_node = editor.nodes.pool[end_pin.parent_node_idx];

        if (start_pin.type == AttributeType_Output)
        {
            *started_at_pin_id = start_pin.id;
            *started_at_node_id = start_node.id;
            *ended_at_pin_id = end_pin.id;
            *ended_at_node_id = end_node.id;
        }
        else
        {
            *started_at_pin_id = end_pin.id;
            *started_at_node_id = end_node.id;
            *ended_at_pin_id = start_pin.id;
            *ended_at_node_id = start_node.id;
        }

        if (created_from_snap)
        {
            *created_from_snap = editor.click_interaction_type == ClickInteractionType_LinkCreation;
        }
    }

    return is_created;
}

bool IsLinkDestroyed(int* const link_id)
{
    assert(g.current_scope == Scope_None);

    const bool link_destroyed = g.deleted_link_idx.has_value();
    if (link_destroyed)
    {
        const EditorContext& editor = editor_context_get();
        const int link_idx = g.deleted_link_idx.value();
        *link_id = editor.links.pool[link_idx].id;
    }

    return link_destroyed;
}

namespace
{
void node_line_handler(EditorContext& editor, const char* line)
{
    int id;
    int x, y;
    if (sscanf(line, "[node.%i", &id) == 1)
    {
        const int node_idx = object_pool_find_or_create_index(editor.nodes, id);
        g.current_node_idx = node_idx;
        NodeData& node = editor.nodes.pool[node_idx];
        node.id = id;
    }
    else if (sscanf(line, "origin=%i,%i", &x, &y) == 2)
    {
        NodeData& node = editor.nodes.pool[g.current_node_idx];
        node.origin = ImVec2((float)x, (float)y);
    }
}

void editor_line_handler(EditorContext& editor, const char* line)
{
    sscanf(line, "panning=%f,%f", &editor.panning.x, &editor.panning.y);
}
} // namespace

const char* SaveCurrentEditorStateToIniString(size_t* const data_size)
{
    return SaveEditorStateToIniString(&editor_context_get(), data_size);
}

const char* SaveEditorStateToIniString(
    const EditorContext* const editor_ptr,
    size_t* const data_size)
{
    assert(editor_ptr != NULL);
    const EditorContext& editor = *editor_ptr;

    g.text_buffer.clear();
    // TODO: check to make sure that the estimate is the upper bound of element
    g.text_buffer.reserve(64 * editor.nodes.pool.size());

    g.text_buffer.appendf(
        "[editor]\npanning=%i,%i\n", (int)editor.panning.x, (int)editor.panning.y);

    for (int i = 0; i < editor.nodes.pool.size(); i++)
    {
        if (editor.nodes.in_use[i])
        {
            const NodeData& node = editor.nodes.pool[i];
            g.text_buffer.appendf("\n[node.%d]\n", node.id);
            g.text_buffer.appendf("origin=%i,%i\n", (int)node.origin.x, (int)node.origin.y);
        }
    }

    if (data_size != NULL)
    {
        *data_size = g.text_buffer.size();
    }

    return g.text_buffer.c_str();
}

void LoadCurrentEditorStateFromIniString(const char* const data, const size_t data_size)
{
    LoadEditorStateFromIniString(&editor_context_get(), data, data_size);
}

void LoadEditorStateFromIniString(
    EditorContext* const editor_ptr,
    const char* const data,
    const size_t data_size)
{
    if (data_size == 0u)
    {
        return;
    }

    EditorContext& editor = editor_ptr == NULL ? editor_context_get() : *editor_ptr;

    char* buf = (char*)ImGui::MemAlloc(data_size + 1);
    const char* buf_end = buf + data_size;
    memcpy(buf, data, data_size);
    buf[data_size] = 0;

    void (*line_handler)(EditorContext&, const char*);
    line_handler = NULL;
    char* line_end = NULL;
    for (char* line = buf; line < buf_end; line = line_end + 1)
    {
        while (*line == '\n' || *line == '\r')
        {
            line++;
        }
        line_end = line;
        while (line_end < buf_end && *line_end != '\n' && *line_end != '\r')
        {
            line_end++;
        }
        line_end[0] = 0;

        if (*line == ';' || *line == '\0')
        {
            continue;
        }

        if (line[0] == '[' && line_end[-1] == ']')
        {
            line_end[-1] = 0;
            if (strncmp(line + 1, "node", 4) == 0)
            {
                line_handler = node_line_handler;
            }
            else if (strcmp(line + 1, "editor") == 0)
            {
                line_handler = editor_line_handler;
            }
        }

        if (line_handler != NULL)
        {
            line_handler(editor, line);
        }
    }
    ImGui::MemFree(buf);
}

void SaveCurrentEditorStateToIniFile(const char* const file_name)
{
    SaveEditorStateToIniFile(&editor_context_get(), file_name);
}

void SaveEditorStateToIniFile(const EditorContext* const editor, const char* const file_name)
{
    size_t data_size = 0u;
    const char* data = SaveEditorStateToIniString(editor, &data_size);
    FILE* file = ImFileOpen(file_name, "wt");
    if (!file)
    {
        return;
    }

    fwrite(data, sizeof(char), data_size, file);
    fclose(file);
}

void LoadCurrentEditorStateFromIniFile(const char* const file_name)
{
    LoadEditorStateFromIniFile(&editor_context_get(), file_name);
}

void LoadEditorStateFromIniFile(EditorContext* const editor, const char* const file_name)
{
    size_t data_size = 0u;
    char* file_data = (char*)ImFileLoadToMemory(file_name, "rb", &data_size);

    if (!file_data)
    {
        return;
    }

    LoadEditorStateFromIniString(editor, file_data, data_size);
    ImGui::MemFree(file_data);
}
} // namespace imnodes
