#pragma once

#include <stddef.h>

struct ImVec2;

namespace imnodes
{
enum ColorStyle
{
    ColorStyle_NodeBackground = 0,
    ColorStyle_NodeBackgroundHovered,
    ColorStyle_NodeBackgroundSelected,
    ColorStyle_NodeOutline,
    ColorStyle_TitleBar,
    ColorStyle_TitleBarHovered,
    ColorStyle_TitleBarSelected,
    ColorStyle_Link,
    ColorStyle_LinkHovered,
    ColorStyle_LinkSelected,
    ColorStyle_Pin,
    ColorStyle_PinHovered,
    ColorStyle_BoxSelector,
    ColorStyle_BoxSelectorOutline,
    ColorStyle_GridBackground,
    ColorStyle_GridLine,
    ColorStyle_Count
};

enum StyleVar
{
    StyleVar_GridSpacing = 0,
    StyleVar_NodeCornerRounding,
    StyleVar_NodePaddingHorizontal,
    StyleVar_NodePaddingVertical,
    StyleVar_NodeBorderThickness,
    StyleVar_LinkThickness,
    StyleVar_LinkLineSegmentsPerLength,
    StyleVar_LinkHoverDistance,
    StyleVar_PinCircleRadius,
    StyleVar_PinQuadSideLength,
    StyleVar_PinTriangleSideLength,
    StyleVar_PinLineThickness,
    StyleVar_PinHoverRadius,
    StyleVar_PinOffset
};

enum StyleFlags
{
    StyleFlags_None = 0,
    StyleFlags_NodeOutline = 1 << 0,
    StyleFlags_GridLines = 1 << 2
};

// This enum controls the way attribute pins look.
enum PinShape
{
    PinShape_Circle,
    PinShape_CircleFilled,
    PinShape_Triangle,
    PinShape_TriangleFilled,
    PinShape_Quad,
    PinShape_QuadFilled
};

// This enum controls the way the attribute pins behave.
enum AttributeFlags
{
    AttributeFlags_None = 0,
    // Allow detaching a link by left-clicking and dragging the link at a pin it is connected to.
    // NOTE: the user has to actually delete the link for this to work. A deleted link can be
    // detected by calling IsLinkDestroyed() after EndNodeEditor().
    AttributeFlags_EnableLinkDetachWithDragClick = 1 << 0,
    // Visual snapping of an in progress link will trigger IsLink Created/Destroyed events. Allows
    // for previewing the creation of a link while dragging it across attributes. See here for demo:
    // https://github.com/Nelarius/imnodes/issues/41#issuecomment-647132113 NOTE: the user has to
    // actually delete the link for this to work. A deleted link can be detected by calling
    // IsLinkDestroyed() after EndNodeEditor().
    AttributeFlags_EnableLinkCreationOnSnap = 1 << 1
};

struct IO
{
    struct EmulateThreeButtonMouse
    {
        EmulateThreeButtonMouse();

        // Controls whether this feature is enabled or not.
        bool enabled;
        const bool* modifier; // The keyboard modifier to use with the mouse left click. Set to
                              // &ImGuiIO::KeyAlt by default.
    } emulate_three_button_mouse;

    struct LinkDetachWithModifierClick
    {
        LinkDetachWithModifierClick();

        // Pointer to a boolean value indicating when the desired modifier is pressed. Set to NULL
        // by default (i.e. this feature is disabled). To enable the feature, set the link to point
        // to, for example, &ImGuiIO::KeyCtrl.
        //
        // Left-clicking a link with this modifier pressed will detach that link. NOTE: the user has
        // to actually delete the link for this to work. A deleted link can be detected by calling
        // IsLinkDestroyed() after EndNodeEditor().
        const bool* modifier;
    } link_detach_with_modifier_click;

    IO();
};

struct Style
{
    float grid_spacing;

    float node_corner_rounding;
    float node_padding_horizontal;
    float node_padding_vertical;
    float node_border_thickness;

    float link_thickness;
    float link_line_segments_per_length;
    float link_hover_distance;

    // The following variables control the look and behavior of the pins. The default size of each
    // pin shape is balanced to occupy approximately the same surface area on the screen.

    // The circle radius used when the pin shape is either PinShape_Circle or PinShape_CircleFilled.
    float pin_circle_radius;
    // The quad side length used when the shape is either PinShape_Quad or PinShape_QuadFilled.
    float pin_quad_side_length;
    // The equilateral triangle side length used when the pin shape is either PinShape_Triangle or
    // PinShape_TriangleFilled.
    float pin_triangle_side_length;
    // The thickness of the line used when the pin shape is not filled.
    float pin_line_thickness;
    // The radius from the pin's center position inside of which it is detected as being hovered
    // over.
    float pin_hover_radius;
    // Offsets the pins' positions from the edge of the node to the outside of the node.
    float pin_offset;

    // By default, StyleFlags_NodeOutline and StyleFlags_Gridlines are enabled.
    StyleFlags flags;
    // Set these mid-frame using Push/PopColorStyle. You can index this color array with with a
    // ColorStyle enum value.
    unsigned int colors[ColorStyle_Count];

    Style();
};

// An editor context corresponds to a set of nodes in a single workspace (created with a single
// Begin/EndNodeEditor pair)
//
// By default, the library creates an editor context behind the scenes, so using any of the imnodes
// functions doesn't require you to explicitly create a context.
struct EditorContext;

EditorContext* EditorContextCreate();
void EditorContextFree(EditorContext*);
void EditorContextSet(EditorContext*);
ImVec2 EditorContextGetPanning();
void EditorContextResetPanning(const ImVec2& pos);
void EditorContextMoveToNode(const int node_id);

// Initialize the node editor system.
void Initialize();
void Shutdown();

IO& GetIO();

// Returns the global style struct. See the struct declaration for default values.
Style& GetStyle();
// Style presets matching the dear imgui styles of the same name.
void StyleColorsDark(); // on by default
void StyleColorsClassic();
void StyleColorsLight();

// The top-level function call. Call this before calling BeginNode/EndNode. Calling this function
// will result the node editor grid workspace being rendered.
void BeginNodeEditor();
void EndNodeEditor();

// Use PushColorStyle and PopColorStyle to modify Style::colors mid-frame.
void PushColorStyle(ColorStyle item, unsigned int color);
void PopColorStyle();
void PushStyleVar(StyleVar style_item, float value);
void PopStyleVar();

// id can be any positive or negative integer, but INT_MIN is currently reserved for internal use.
void BeginNode(int id);
void EndNode();

ImVec2 GetNodeDimensions(int id);

// Place your node title bar content (such as the node title, using ImGui::Text) between the
// following function calls. These functions have to be called before adding any attributes, or the
// layout of the node will be incorrect.
void BeginNodeTitleBar();
void EndNodeTitleBar();

// Attributes are ImGui UI elements embedded within the node. Attributes can have pin shapes
// rendered next to them. Links are created between pins.
//
// The activity status of an attribute can be checked via the IsAttributeActive() and
// IsAnyAttributeActive() function calls. This is one easy way of checking for any changes made to
// an attribute's drag float UI, for instance.
//
// Each attribute id must be unique.

// Create an input attribute block. The pin is rendered on left side.
void BeginInputAttribute(int id, PinShape shape = PinShape_CircleFilled);
void EndInputAttribute();
// Create an output attribute block. The pin is rendered on the right side.
void BeginOutputAttribute(int id, PinShape shape = PinShape_CircleFilled);
void EndOutputAttribute();
// Create a static attribute block. A static attribute has no pin, and therefore can't be linked to
// anything. However, you can still use IsAttributeActive() and IsAnyAttributeActive() to check for
// attribute activity.
void BeginStaticAttribute(int id);
void EndStaticAttribute();

// Push a single AttributeFlags value. By default, only AttributeFlags_None is set.
void PushAttributeFlag(AttributeFlags flag);
void PopAttributeFlag();

// Render a link between attributes.
// The attributes ids used here must match the ids used in Begin(Input|Output)Attribute function
// calls. The order of start_attr and end_attr doesn't make a difference for rendering the link.
void Link(int id, int start_attribute_id, int end_attribute_id);

// Enable or disable the ability to click and drag a specific node.
void SetNodeDraggable(int node_id, const bool draggable);

// The node's position can be expressed in three coordinate systems:
// * screen space coordinates, -- the origin is the upper left corner of the window.
// * editor space coordinates -- the origin is the upper left corner of the node editor window
// * grid space coordinates, -- the origin is the upper left corner of the node editor window,
// translated by the current editor panning vector (see EditorContextGetPanning() and
// EditorContextResetPanning())

// Use the following functions to get and set the node's coordinates in these coordinate systems.

void SetNodeScreenSpacePos(int node_id, const ImVec2& screen_space_pos);
void SetNodeEditorSpacePos(int node_id, const ImVec2& editor_space_pos);
void SetNodeGridSpacePos(int node_id, const ImVec2& grid_pos);

ImVec2 GetNodeScreenSpacePos(const int node_id);
ImVec2 GetNodeEditorSpacePos(const int node_id);
ImVec2 GetNodeGridSpacePos(const int node_id);

// Returns true if the current node editor canvas is being hovered over by the mouse, and is not
// blocked by any other windows.
bool IsEditorHovered();
// The following functions return true if a UI element is being hovered over by the mouse cursor.
// Assigns the id of the UI element being hovered over to the function argument. Use these functions
// after EndNodeEditor() has been called.
bool IsNodeHovered(int* node_id);
bool IsLinkHovered(int* link_id);
bool IsPinHovered(int* attribute_id);

// Use The following two functions to query the number of selected nodes or links in the current
// editor. Use after calling EndNodeEditor().
int NumSelectedNodes();
int NumSelectedLinks();
// Get the selected node/link ids. The pointer argument should point to an integer array with at
// least as many elements as the respective NumSelectedNodes/NumSelectedLinks function call
// returned.
void GetSelectedNodes(int* node_ids);
void GetSelectedLinks(int* link_ids);

// Clears the list of selected nodes/links. Useful if you want to delete a selected node or link.
void ClearNodeSelection();
void ClearLinkSelection();

// Was the previous attribute active? This will continuously return true while the left mouse button
// is being pressed over the UI content of the attribute.
bool IsAttributeActive();
// Was any attribute active? If so, sets the active attribute id to the output function argument.
bool IsAnyAttributeActive(int* attribute_id = NULL);

// Use the following functions to query a change of state for an existing link, or new link. Call
// these after EndNodeEditor().

// Did the user start dragging a new link from a pin?
bool IsLinkStarted(int* started_at_attribute_id);
// Did the user drop the dragged link before attaching it to a pin?
// There are two different kinds of situations to consider when handling this event:
// 1) a link which is created at a pin and then dropped
// 2) an existing link which is detached from a pin and then dropped
// Use the including_detached_links flag to control whether this function triggers when the user
// detaches a link and drops it.
bool IsLinkDropped(int* started_at_attribute_id = NULL, bool including_detached_links = true);
// Did the user finish creating a new link?
bool IsLinkCreated(
    int* started_at_attribute_id,
    int* ended_at_attribute_id,
    bool* created_from_snap = NULL);
bool IsLinkCreated(
    int* started_at_node_id,
    int* started_at_attribute_id,
    int* ended_at_node_id,
    int* ended_at_attribute_id,
    bool* created_from_snap = NULL);

// Was an existing link detached from a pin by the user? The detached link's id is assigned to the
// output argument link_id.
bool IsLinkDestroyed(int* link_id);

// Use the following functions to write the editor context's state to a string, or directly to a
// file. The editor context is serialized in the INI file format.

const char* SaveCurrentEditorStateToIniString(size_t* data_size = NULL);
const char* SaveEditorStateToIniString(const EditorContext* editor, size_t* data_size = NULL);

void LoadCurrentEditorStateFromIniString(const char* data, size_t data_size);
void LoadEditorStateFromIniString(EditorContext* editor, const char* data, size_t data_size);

void SaveCurrentEditorStateToIniFile(const char* file_name);
void SaveEditorStateToIniFile(const EditorContext* editor, const char* file_name);

void LoadCurrentEditorStateFromIniFile(const char* file_name);
void LoadEditorStateFromIniFile(EditorContext* editor, const char* file_name);
} // namespace imnodes
