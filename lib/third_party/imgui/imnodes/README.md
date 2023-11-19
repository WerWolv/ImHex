<h1 align="center">imnodes</h1>

<p align="center">A small, dependency-free node editor extension for <a href="https://github.com/ocornut/imgui">dear imgui</a>.</p>

<p align="center">
  <img src="https://raw.githubusercontent.com/Nelarius/imnodes/master/img/imnodes.gif?token=ADH_jEpqbBrw0nH-BUmOip490dyO2CnRks5cVZllwA%3D%3D">
</p>

[![Build Status](https://github.com/nelarius/imnodes/workflows/Build/badge.svg)](https://github.com/nelarius/imnodes/actions?workflow=Build)

Imnodes aims to provide a simple, immediate-mode interface for creating a node editor within an ImGui window. Imnodes provides simple, customizable building blocks that a user needs to build their node editor.

Features:

* Create nodes, links, and pins in an immediate-mode style. The user controls all the state.
* Nest ImGui widgets inside nodes
* Simple distribution, just copy-paste `imnodes.h`, `imnodes_internal.h`, and `imnodes.cpp` into your project along side ImGui.

## Examples

This repository includes a few example files, under `example/`. They are intended as simple examples giving you an idea of what you can build with imnodes.

If you need to build the examples, you can use the provided CMake script to do so.

```bash
# Initialize the vcpkg submodule
$ git submodule update --init
# Run the generation step and build
$ cmake -B build-release/ -S . -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake
$ cmake --build build-release -- -j
```

Note that this has not been tested on Linux and is likely to fail on the platform.

## A brief tour

Here is a small overview of how the extension is used. For more information on example usage, scroll to the bottom of the README.

Before anything can be done, the library must be initialized. This can be done at the same time as `dear imgui` initialization.

```cpp
ImGui::CreateContext();
ImNodes::CreateContext();

// elsewhere in the code...
ImNodes::DestroyContext();
ImGui::DestroyContext();
```

The node editor is a workspace which contains nodes. The node editor must be instantiated within a window, like any other UI element.

```cpp
ImGui::Begin("node editor");

ImNodes::BeginNodeEditor();
ImNodes::EndNodeEditor();

ImGui::End();
```

Now you should have a workspace with a grid visible in the window. An empty node can now be instantiated:

```cpp
const int hardcoded_node_id = 1;

ImNodes::BeginNodeEditor();

ImNodes::BeginNode(hardcoded_node_id);
ImGui::Dummy(ImVec2(80.0f, 45.0f));
ImNodes::EndNode();

ImNodes::EndNodeEditor();
```

Nodes, like windows in `dear imgui` must be uniquely identified. But we can't use the node titles for identification, because it should be possible to have many nodes of the same name in the workspace. Instead, you just use integers for identification.

Attributes are the UI content of the node. An attribute will have a pin (the little circle) on either side of the node. There are two types of attributes: input, and output attributes. Input attribute pins are on the left side of the node, and output attribute pins are on the right. Like nodes, pins must be uniquely identified.

```cpp
ImNodes::BeginNode(hardcoded_node_id);

const int output_attr_id = 2;
ImNodes::BeginOutputAttribute(output_attr_id);
// in between Begin|EndAttribute calls, you can call ImGui
// UI functions
ImGui::Text("output pin");
ImNodes::EndOutputAttribute();

ImNodes::EndNode();
```

The extension doesn't really care what is in the attribute. It just renders the pin for the attribute, and allows the user to create links between pins.

A title bar can be added to the node using `BeginNodeTitleBar` and `EndNodeTitleBar`. Like attributes, you place your title bar's content between the function calls. Note that these functions have to be called before adding attributes or other `dear imgui` UI elements to the node, since the node's layout is built in order, top-to-bottom.

```cpp
ImNodes::BeginNode(hardcoded_node_id);

ImNodes::BeginNodeTitleBar();
ImGui::TextUnformatted("output node");
ImNodes::EndNodeTitleBar();

// pins and other node UI content omitted...

ImNodes::EndNode();
```

The user has to render their own links between nodes as well. A link is a curve which connects two attributes. A link is just a pair of attribute ids. And like nodes and attributes, links too have to be identified by unique integer values:

```cpp
std::vector<std::pair<int, int>> links;
// elsewhere in the code...
for (int i = 0; i < links.size(); ++i)
{
  const std::pair<int, int> p = links[i];
  // in this case, we just use the array index of the link
  // as the unique identifier
  ImNodes::Link(i, p.first, p.second);
}
```

After `EndNodeEditor` has been called, you can check if a link was created during the frame with the function call `IsLinkCreated`:

```cpp
int start_attr, end_attr;
if (ImNodes::IsLinkCreated(&start_attr, &end_attr))
{
  links.push_back(std::make_pair(start_attr, end_attr));
}
```

In addition to checking for new links, you can also check whether UI elements are being hovered over by the mouse cursor:

```cpp
int node_id;
if (ImNodes::IsNodeHovered(&node_id))
{
  node_hovered = node_id;
}
```

You can also check to see if any node has been selected. Nodes can be clicked on, or they can be selected by clicking and dragging the box selector over them.

```cpp
// Note that since many nodes can be selected at once, we first need to query the number of
// selected nodes before getting them.
const int num_selected_nodes = ImNodes::NumSelectedNodes();
if (num_selected_nodes > 0)
{
  std::vector<int> selected_nodes;
  selected_nodes.resize(num_selected_nodes);
  ImNodes::GetSelectedNodes(selected_nodes.data());
}
```

See `imnodes.h` for more UI event-related functions.

Like `dear imgui`, the style of the UI can be changed. You can set the color style of individual nodes, pins, and links mid-frame by calling `ImNodes::PushColorStyle` and `ImNodes::PopColorStyle`.

```cpp
// set the titlebar color of an individual node
ImNodes::PushColorStyle(
  ImNodesCol_TitleBar, IM_COL32(11, 109, 191, 255));
ImNodes::PushColorStyle(
  ImNodesCol_TitleBarSelected, IM_COL32(81, 148, 204, 255));

ImNodes::BeginNode(hardcoded_node_id);
// node internals here...
ImNodes::EndNode();

ImNodes::PopColorStyle();
ImNodes::PopColorStyle();
```

If the style is not being set mid-frame, `ImNodes::GetStyle` can be called instead, and the values can be set into the style array directly.

```cpp
// set the titlebar color for all nodes
ImNodesStyle& style = ImNodes::GetStyle();
style.colors[ImNodesCol_TitleBar] = IM_COL32(232, 27, 86, 255);
style.colors[ImNodesCol_TitleBarSelected] = IM_COL32(241, 108, 146, 255);
```

To handle quicker navigation of large graphs you can use an interactive mini-map overlay. The mini-map can be zoomed and scrolled. Editor nodes will track the panning of the mini-map accordingly.

```cpp
ImGui::Begin("node editor");

ImNodes::BeginNodeEditor();

// add nodes...

// must be called right before EndNodeEditor
ImNodes::MiniMap();
ImNodes::EndNodeEditor();

ImGui::End();
```

The relative sizing and corner location of the mini-map in the editor space can be specified like so:

```cpp
// MiniMap is a square region with a side length that is 20% the largest editor canvas dimension
// See ImNodesMiniMapLocation_ for other corner locations
ImNodes::MiniMap(0.2f, ImNodesMiniMapLocation_TopRight);
```

The mini-map also supports limited node hovering customization through a user-defined callback.
```cpp
// User callback
void mini_map_node_hovering_callback(int node_id, void* user_data)
{
  ImGui::SetTooltip("This is node %d", node_id);
}

// Later on...
ImNodes::MiniMap(0.2f, ImNodesMiniMapLocation_TopRight, mini_map_node_hovering_callback, custom_user_data);

// 'custom_user_data' can be used to supply extra information needed for drawing within the callback
```

## Customizing ImNodes

ImNodes can be customized by providing an `imnodes_config.h` header and specifying defining `IMNODES_USER_CONFIG=imnodes_config.h` when compiling.

It is currently possible to override the type of the minimap hovering callback function. This is useful when generating bindings for another language.

Here's an example imnodes_config.h, which generates a pybind wrapper for the callback.
```cpp
#pragma once

#include <pybind11/functional.h>

namespace pybind11 {

inline bool PyWrapper_Check(PyObject *o) { return true; }

class wrapper : public object {
public:
    PYBIND11_OBJECT_DEFAULT(wrapper, object, PyWrapper_Check)
    wrapper(void* x) { m_ptr = (PyObject*)x; }
    explicit operator bool() const { return m_ptr != nullptr && m_ptr != Py_None; }
};

} //namespace pybind11

namespace py = pybind11;

#define ImNodesMiniMapNodeHoveringCallback py::wrapper

#define ImNodesMiniMapNodeHoveringCallbackUserData py::wrapper
```

## Known issues

* `ImGui::Separator()` spans the current window span. As a result, using a separator inside a node will result in the separator spilling out of the node into the node editor grid.

## Further information

See the `examples/` directory to see library usage in greater detail.

* simple.cpp is a simple hello-world style program which displays two nodes
* save_load.cpp is enables you to add and remove nodes and links, and serializes/deserializes them, so that the program state is retained between restarting the program
* color_node_editor.cpp is a more complete example, which shows how a simple node editor is implemented with a graph.
