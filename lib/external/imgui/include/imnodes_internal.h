#pragma once

#include "imnodes.h"

#include <imgui.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>

#include <limits.h>

// the structure of this file:
//
// [SECTION] internal enums
// [SECTION] internal data structures
// [SECTION] global and editor context structs
// [SECTION] object pool implementation

struct ImNodesContext;

extern ImNodesContext* GImNodes;

// [SECTION] internal enums

typedef int ImNodesScope;
typedef int ImNodesAttributeType;
typedef int ImNodesUIState;
typedef int ImNodesClickInteractionType;
typedef int ImNodesLinkCreationType;

enum ImNodesScope_
{
    ImNodesScope_None = 1,
    ImNodesScope_Editor = 1 << 1,
    ImNodesScope_Node = 1 << 2,
    ImNodesScope_Attribute = 1 << 3
};

enum ImNodesAttributeType_
{
    ImNodesAttributeType_None,
    ImNodesAttributeType_Input,
    ImNodesAttributeType_Output
};

enum ImNodesUIState_
{
    ImNodesUIState_None = 0,
    ImNodesUIState_LinkStarted = 1 << 0,
    ImNodesUIState_LinkDropped = 1 << 1,
    ImNodesUIState_LinkCreated = 1 << 2
};

enum ImNodesClickInteractionType_
{
    ImNodesClickInteractionType_Node,
    ImNodesClickInteractionType_Link,
    ImNodesClickInteractionType_LinkCreation,
    ImNodesClickInteractionType_Panning,
    ImNodesClickInteractionType_BoxSelection,
    ImNodesClickInteractionType_ImGuiItem,
    ImNodesClickInteractionType_None
};

enum ImNodesLinkCreationType_
{
    ImNodesLinkCreationType_Standard,
    ImNodesLinkCreationType_FromDetach
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
struct ImObjectPool
{
    ImVector<T>    Pool;
    ImVector<bool> InUse;
    ImVector<int>  FreeList;
    ImGuiStorage   IdMap;

    ImObjectPool() : Pool(), InUse(), FreeList(), IdMap() {}
};

// Emulates std::optional<int> using the sentinel value `INVALID_INDEX`.
struct ImOptionalIndex
{
    ImOptionalIndex() : _Index(INVALID_INDEX) {}
    ImOptionalIndex(const int value) : _Index(value) {}

    // Observers

    inline bool HasValue() const { return _Index != INVALID_INDEX; }

    inline int Value() const
    {
        IM_ASSERT(HasValue());
        return _Index;
    }

    // Modifiers

    inline ImOptionalIndex& operator=(const int value)
    {
        _Index = value;
        return *this;
    }

    inline void Reset() { _Index = INVALID_INDEX; }

    inline bool operator==(const ImOptionalIndex& rhs) const { return _Index == rhs._Index; }

    inline bool operator==(const int rhs) const { return _Index == rhs; }

    inline bool operator!=(const ImOptionalIndex& rhs) const { return _Index != rhs._Index; }

    inline bool operator!=(const int rhs) const { return _Index != rhs; }

    static const int INVALID_INDEX = -1;

private:
    int _Index;
};

struct ImNodeData
{
    int    Id;
    ImVec2 Origin; // The node origin is in editor space
    ImRect TitleBarContentRect;
    ImRect Rect;

    struct
    {
        ImU32 Background, BackgroundHovered, BackgroundSelected, Outline, Titlebar, TitlebarHovered,
            TitlebarSelected;
    } ColorStyle;

    struct
    {
        float  CornerRounding;
        ImVec2 Padding;
        float  BorderThickness;
    } LayoutStyle;

    ImVector<int> PinIndices;
    bool          Draggable;

    ImNodeData(const int node_id)
        : Id(node_id), Origin(0.0f, 0.0f), TitleBarContentRect(),
          Rect(ImVec2(0.0f, 0.0f), ImVec2(0.0f, 0.0f)), ColorStyle(), LayoutStyle(), PinIndices(),
          Draggable(true)
    {
    }

    ~ImNodeData() { Id = INT_MIN; }
};

struct ImPinData
{
    int                  Id;
    int                  ParentNodeIdx;
    ImRect               AttributeRect;
    ImNodesAttributeType Type;
    ImNodesPinShape      Shape;
    ImVec2               Pos; // screen-space coordinates
    int                  Flags;

    struct
    {
        ImU32 Background, Hovered;
    } ColorStyle;

    ImPinData(const int pin_id)
        : Id(pin_id), ParentNodeIdx(), AttributeRect(), Type(ImNodesAttributeType_None),
          Shape(ImNodesPinShape_CircleFilled), Pos(), Flags(ImNodesAttributeFlags_None),
          ColorStyle()
    {
    }
};

struct ImLinkData
{
    int Id;
    int StartPinIdx, EndPinIdx;

    struct
    {
        ImU32 Base, Hovered, Selected;
    } ColorStyle;

    ImLinkData(const int link_id) : Id(link_id), StartPinIdx(), EndPinIdx(), ColorStyle() {}
};

struct ImClickInteractionState
{
    ImNodesClickInteractionType Type;

    struct
    {
        int                     StartPinIdx;
        ImOptionalIndex         EndPinIdx;
        ImNodesLinkCreationType Type;
    } LinkCreation;

    struct
    {
        ImRect Rect; // Coordinates in grid space
    } BoxSelector;

    ImClickInteractionState() : Type(ImNodesClickInteractionType_None) {}
};

struct ImNodesColElement
{
    ImU32      Color;
    ImNodesCol Item;

    ImNodesColElement(const ImU32 c, const ImNodesCol s) : Color(c), Item(s) {}
};

struct ImNodesStyleVarElement
{
    ImNodesStyleVar Item;
    float           FloatValue[2];

    ImNodesStyleVarElement(const ImNodesStyleVar variable, const float value) : Item(variable)
    {
        FloatValue[0] = value;
    }

    ImNodesStyleVarElement(const ImNodesStyleVar variable, const ImVec2 value) : Item(variable)
    {
        FloatValue[0] = value.x;
        FloatValue[1] = value.y;
    }
};

// [SECTION] global and editor context structs

struct ImNodesEditorContext
{
    ImObjectPool<ImNodeData> Nodes;
    ImObjectPool<ImPinData>  Pins;
    ImObjectPool<ImLinkData> Links;

    ImVector<int> NodeDepthOrder;

    // ui related fields
    ImVec2 Panning;
    ImVec2 AutoPanningDelta;
    // Minimum and maximum extents of all content in grid space. Valid after final
    // ImNodes::EndNode() call.
    ImRect GridContentBounds;

    ImVector<int> SelectedNodeIndices;
    ImVector<int> SelectedLinkIndices;

    // Relative origins of selected nodes for snapping of dragged nodes
    ImVector<ImVec2> SelectedNodeOffsets;
    // Offset of the primary node origin relative to the mouse cursor.
    ImVec2           PrimaryNodeOffset;

    ImClickInteractionState ClickInteraction;

    // Mini-map state set by MiniMap()

    bool                                       MiniMapEnabled;
    ImNodesMiniMapLocation                     MiniMapLocation;
    float                                      MiniMapSizeFraction;
    ImNodesMiniMapNodeHoveringCallback         MiniMapNodeHoveringCallback;
    ImNodesMiniMapNodeHoveringCallbackUserData MiniMapNodeHoveringCallbackUserData;

    // Mini-map state set during EndNodeEditor() call

    ImRect MiniMapRectScreenSpace;
    ImRect MiniMapContentScreenSpace;
    float  MiniMapScaling;

    ImNodesEditorContext()
        : Nodes(), Pins(), Links(), Panning(0.f, 0.f), SelectedNodeIndices(), SelectedLinkIndices(),
          SelectedNodeOffsets(), PrimaryNodeOffset(0.f, 0.f), ClickInteraction(),
          MiniMapEnabled(false), MiniMapSizeFraction(0.0f),
          MiniMapNodeHoveringCallback(NULL), MiniMapNodeHoveringCallbackUserData(NULL),
          MiniMapScaling(0.0f)
    {
    }
};

struct ImNodesContext
{
    ImNodesEditorContext* DefaultEditorCtx;
    ImNodesEditorContext* EditorCtx;

    // Canvas draw list and helper state
    ImDrawList*   CanvasDrawList;
    ImGuiStorage  NodeIdxToSubmissionIdx;
    ImVector<int> NodeIdxSubmissionOrder;
    ImVector<int> NodeIndicesOverlappingWithMouse;
    ImVector<int> OccludedPinIndices;

    // Canvas extents
    ImVec2 CanvasOriginScreenSpace;
    ImRect CanvasRectScreenSpace;

    // Debug helpers
    ImNodesScope CurrentScope;

    // Configuration state
    ImNodesIO                        Io;
    ImNodesStyle                     Style;
    ImVector<ImNodesColElement>      ColorModifierStack;
    ImVector<ImNodesStyleVarElement> StyleModifierStack;
    ImGuiTextBuffer                  TextBuffer;

    int           CurrentAttributeFlags;
    ImVector<int> AttributeFlagStack;

    // UI element state
    int CurrentNodeIdx;
    int CurrentPinIdx;
    int CurrentAttributeId;

    ImOptionalIndex HoveredNodeIdx;
    ImOptionalIndex HoveredLinkIdx;
    ImOptionalIndex HoveredPinIdx;

    ImOptionalIndex DeletedLinkIdx;
    ImOptionalIndex SnapLinkIdx;

    // Event helper state
    // TODO: this should be a part of a state machine, and not a member of the global struct.
    // Unclear what parts of the code this relates to.
    int ImNodesUIState;

    int  ActiveAttributeId;
    bool ActiveAttribute;

    // ImGui::IO cache

    ImVec2 MousePos;

    bool  LeftMouseClicked;
    bool  LeftMouseReleased;
    bool  AltMouseClicked;
    bool  LeftMouseDragging;
    bool  AltMouseDragging;
    float AltMouseScrollDelta;
    bool  MultipleSelectModifier;
};

namespace IMNODES_NAMESPACE
{
static inline ImNodesEditorContext& EditorContextGet()
{
    // No editor context was set! Did you forget to call ImNodes::CreateContext()?
    IM_ASSERT(GImNodes->EditorCtx != NULL);
    return *GImNodes->EditorCtx;
}

// [SECTION] ObjectPool implementation

template<typename T>
static inline int ObjectPoolFind(const ImObjectPool<T>& objects, const int id)
{
    const int index = objects.IdMap.GetInt(static_cast<ImGuiID>(id), -1);
    return index;
}

template<typename T>
static inline void ObjectPoolUpdate(ImObjectPool<T>& objects)
{
    for (int i = 0; i < objects.InUse.size(); ++i)
    {
        const int id = objects.Pool[i].Id;

        if (!objects.InUse[i] && objects.IdMap.GetInt(id, -1) == i)
        {
            objects.IdMap.SetInt(id, -1);
            objects.FreeList.push_back(i);
            (objects.Pool.Data + i)->~T();
        }
    }
}

template<>
inline void ObjectPoolUpdate(ImObjectPool<ImNodeData>& nodes)
{
    for (int i = 0; i < nodes.InUse.size(); ++i)
    {
        if (nodes.InUse[i])
        {
            nodes.Pool[i].PinIndices.clear();
        }
        else
        {
            const int id = nodes.Pool[i].Id;

            if (nodes.IdMap.GetInt(id, -1) == i)
            {
                // Remove node idx form depth stack the first time we detect that this idx slot is
                // unused
                ImVector<int>&   depth_stack = EditorContextGet().NodeDepthOrder;
                const int* const elem = depth_stack.find(i);
                IM_ASSERT(elem != depth_stack.end());
                depth_stack.erase(elem);

                nodes.IdMap.SetInt(id, -1);
                nodes.FreeList.push_back(i);
                (nodes.Pool.Data + i)->~ImNodeData();
            }
        }
    }
}

template<typename T>
static inline void ObjectPoolReset(ImObjectPool<T>& objects)
{
    if (!objects.InUse.empty())
    {
        memset(objects.InUse.Data, 0, objects.InUse.size_in_bytes());
    }
}

template<typename T>
static inline int ObjectPoolFindOrCreateIndex(ImObjectPool<T>& objects, const int id)
{
    int index = objects.IdMap.GetInt(static_cast<ImGuiID>(id), -1);

    // Construct new object
    if (index == -1)
    {
        if (objects.FreeList.empty())
        {
            index = objects.Pool.size();
            IM_ASSERT(objects.Pool.size() == objects.InUse.size());
            const int new_size = objects.Pool.size() + 1;
            objects.Pool.resize(new_size);
            objects.InUse.resize(new_size);
        }
        else
        {
            index = objects.FreeList.back();
            objects.FreeList.pop_back();
        }
        IM_PLACEMENT_NEW(objects.Pool.Data + index) T(id);
        objects.IdMap.SetInt(static_cast<ImGuiID>(id), index);
    }

    // Flag it as used
    objects.InUse[index] = true;

    return index;
}

template<>
inline int ObjectPoolFindOrCreateIndex(ImObjectPool<ImNodeData>& nodes, const int node_id)
{
    int node_idx = nodes.IdMap.GetInt(static_cast<ImGuiID>(node_id), -1);

    // Construct new node
    if (node_idx == -1)
    {
        if (nodes.FreeList.empty())
        {
            node_idx = nodes.Pool.size();
            IM_ASSERT(nodes.Pool.size() == nodes.InUse.size());
            const int new_size = nodes.Pool.size() + 1;
            nodes.Pool.resize(new_size);
            nodes.InUse.resize(new_size);
        }
        else
        {
            node_idx = nodes.FreeList.back();
            nodes.FreeList.pop_back();
        }
        IM_PLACEMENT_NEW(nodes.Pool.Data + node_idx) ImNodeData(node_id);
        nodes.IdMap.SetInt(static_cast<ImGuiID>(node_id), node_idx);

        ImNodesEditorContext& editor = EditorContextGet();
        editor.NodeDepthOrder.push_back(node_idx);
    }

    // Flag node as used
    nodes.InUse[node_idx] = true;

    return node_idx;
}

template<typename T>
static inline T& ObjectPoolFindOrCreateObject(ImObjectPool<T>& objects, const int id)
{
    const int index = ObjectPoolFindOrCreateIndex(objects, id);
    return objects.Pool[index];
}
} // namespace IMNODES_NAMESPACE
