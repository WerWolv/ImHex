# ImPlot3D

<p align="center">
<img src="https://github.com/user-attachments/assets/359473d2-73a9-452c-a5f3-cb96e3785dc2" width="270"> <img src="https://github.com/user-attachments/assets/97ec8be4-50f9-428b-b357-25e2479409b8" width="270"> <img src="https://github.com/user-attachments/assets/c212039b-4853-4d26-95a5-5470bf97555e" width="270">
</p>

<p align="center">
<img src="https://github.com/user-attachments/assets/ec7ec42a-3c62-44bf-9275-f735f0304c95" width="270"> <img src="https://github.com/user-attachments/assets/e6bd03fa-6d76-4f3e-8d15-c24a05a5f714" width="270"> <img src="https://github.com/user-attachments/assets/b66ff296-7fbf-4644-9129-37daecca0b62" width="270">
</p>

ImPlot3D is an extension of [Dear ImGui](https://github.com/ocornut/imgui) that provides easy-to-use, high-performance 3D plotting functionality. Inspired by [ImPlot](https://github.com/epezent/implot), it brings a familiar and intuitive API for developers already acquainted with ImPlot. ImPlot3D is designed for rendering 3D plots with customizable markers, lines, surfaces, and meshes, providing an ideal solution for applications requiring visual representation of 3D data.

## 🚀 Features
- GPU-accelerated rendering
- Multiple plot types:
  - Line plots
  - Scatter plots
  - Surface plots
  - Quad plots
  - Triangle plots
  - Mesh plots
  - Text plots
- Rotate, pan, and zoom 3D plots interactively
- Several plot styling options: 10 marker types, adjustable marker sizes, line weights, outline colors, fill colors, etc.
- 16 built-in colormaps and support for and user-added colormaps
- Optional plot titles, axis labels, and grid labels
- Optional and configurable legends with toggle buttons to quickly show/hide plot items
- Default styling based on the current ImGui theme, or completely custom plot styles

## 🛠️ Usage
The ImPlot3D API is designed to feel very similar to Dear ImGui and ImPlot. You start by calling `ImPlot3D::BeginPlot()` to initialize a 3D plot, followed by plotting various data using the `PlotX` functions (e.g., `PlotLine()` , `PlotScatter()` , `PlotSurface()` ). Finally, you end the plot with ` ImPlot3D::EndPlot()` .

```cpp
float x_data[1000] = ...;
float y_data[1000] = ...;
float z_data[1000] = ...;

ImGui::Begin("My Window");
if (ImPlot3D::BeginPlot("My Plot")) {
    ImPlot3D::PlotLine("My Line Plot", x_data, y_data, z_data, 1000);
    ImPlot3D::PlotScatter("My Scatter Plot", x_data, y_data, z_data, 1000);
    ...
    ImPlot3D::EndPlot();
}
ImGui::End();
```

## 🎨 Demos
A comprehensive example showcasing ImPlot3D features can be found in `implot3d_demo.cpp`. Add this file to your project and call `ImPlot3D::ShowDemoWindow()` in your update loop. This demo provides a wide variety of 3D plotting examples, serving as a reference for creating different types of 3D plots. The demo is regularly updated to reflect new features and plot types, so be sure to revisit it with each release!

## ⚙️ Integration
To integrate ImPlot3D into your application, follow these steps:

1. Ensure you have a working Dear ImGui environment. ImPlot3D requires only Dear ImGui to function and does not depend on ImPlot.
2. Add the following source files to your project: `implot3d.h`, `implot3d.cpp`, `implot3d_internal.h`, `implot3d_items.cpp`. Optionally, include `implot3d_demo.cpp` for examples and `implot3d_meshes.cpp` to support pre-loaded meshes.
3. Create and destroy an ImPlot3DContext alongside your ImGuiContext:
  ```cpp
  ImGui::CreateContext();
  ImPlot3D::CreateContext();
  ...
  ImPlot3D::DestroyContext();
  ImGui::DestroyContext();
  ```

You're now ready to start plotting in 3D!

## ⚠️ Extremely Important Note
Dear ImGui, by default, uses 16-bit indexing, which might cause issues with high-density 3D visualizations such as complex surfaces or meshes. This can lead to assertion failures, data truncation, or visual glitches. To avoid these problems, it's recommended to:

- Option 1: Enable 32-bit indices by uncommenting `#define ImDrawIdx unsigned int` in your ImGui imconfig.h file.
- Option 2: Ensure your renderer supports the `ImGuiBackendFlags_RendererHasVtxOffset` flag. Many official ImGui backends already support this functionality.

## 💬 FAQ
#### Why ImPlot3D?
While ImGui excels at building UI, it lacks tools for 3D data visualization. ImPlot3D fills this gap, offering a lightweight, real-time library for 3D plotting, designed with interactivity and ease of use in mind.

Inspired by ImPlot, ImPlot3D provides a similar API, making it easy for existing ImPlot users to adopt. It focuses on real-time, application-level 3D visualizations for debugging, simulations, and data analysis, with performance as a priority.

ImPlot is great for 2D visualizations; ImPlot3D extends this power to 3D, offering the same simplicity and speed.

#### Where can I find documentation?
The API for ImPlot3D is thoroughly commented in `implot3d.h`, and a comprehensive demo file, `implot3d_demo.cpp`, showcases all the features. You are encouraged to explore the demo file as it is regularly updated to reflect new functionality. Additionally, if you're familiar with ImPlot, you'll notice many similarities in usage patterns.

#### How is ImPlot3D different from ImPlot?
ImPlot3D is highly inspired by ImPlot, so if you're already familiar with ImPlot, you'll feel right at home. However, ImPlot3D is specifically built for 3D visualizations, offering interactive 3D rotations, panning, and scaling.

### Do I need ImPlot to use ImPlot3D?
No. ImPlot3D is a standalone library and does not depend on ImPlot. You only need Dear ImGui to get started.

#### Does ImPlot3D support 2D plots?
While you can rotate the 3D view to align with a 2D plane, ImPlot is far better suited for visualizing 2D data. ImPlot3D is specifically designed for 3D plotting and interaction, so we recommend using ImPlot for all your 2D visualization needs.

#### Can I customize the appearance of plots?
Absolutely. ImPlot3D allows you to modify plot styles, including line colors, thickness, fill opacity, and marker sizes. You can also use colormaps for surfaces and customize axis labels, grid styles, and background colors.

#### Can I export 3D plots to an image?
Not currently. You can use your OS's screen capturing tools to save a plot. ImPlot3D is designed for real-time visualization and interaction, not for creating publication-quality renders. For publication-quality output, consider exporting your data to a dedicated 3D rendering tool.

#### Is ImPlot3D suitable for publication-quality visuals?
ImPlot3D prioritizes interactivity and real-time performance. If you need high-quality visualizations, use ImPlot3D for initial exploration and then switch to tools like [MATLAB](https://www.mathworks.com/products/matlab.html), [matplotlib](https://matplotlib.org/), or [ParaView](https://www.paraview.org/) for the final output.

## License
This project is licensed under the MIT License - check [LICENSE](LICENSE) for details.
