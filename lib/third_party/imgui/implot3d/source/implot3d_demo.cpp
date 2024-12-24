//--------------------------------------------------
// ImPlot3D v0.1
// implot3d_demo.cpp
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
// [SECTION] User Namespace
// [SECTION] Helpers
// [SECTION] Plots
// [SECTION] Custom
// [SECTION] Demo Window
// [SECTION] Style Editor
// [SECTION] User Namespace Implementation

#include "implot3d.h"
#include "implot3d_internal.h"

//-----------------------------------------------------------------------------
// [SECTION] User Namespace
//-----------------------------------------------------------------------------

// Encapsulates examples for customizing ImPlot3D
namespace MyImPlot3D {

// Example for Custom Styles section
void StyleSeaborn();

} // namespace MyImPlot3D

namespace ImPlot3D {

//-----------------------------------------------------------------------------
// [SECTION] Helpers
//-----------------------------------------------------------------------------

static void HelpMarker(const char* desc) {
    ImGui::TextDisabled("(?)");
    if (ImGui::BeginItemTooltip()) {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

// Utility structure for realtime plot
struct ScrollingBuffer {
    int MaxSize;
    int Offset;
    ImVector<float> Data;
    ScrollingBuffer(int max_size = 2000) {
        MaxSize = max_size;
        Offset = 0;
        Data.reserve(MaxSize);
    }
    void AddPoint(float x) {
        if (Data.size() < MaxSize)
            Data.push_back(x);
        else {
            Data[Offset] = x;
            Offset = (Offset + 1) % MaxSize;
        }
    }
    void Erase() {
        if (Data.size() > 0) {
            Data.shrink(0);
            Offset = 0;
        }
    }
};

//-----------------------------------------------------------------------------
// [SECTION] Plots
//-----------------------------------------------------------------------------

void DemoLinePlots() {
    static float xs1[1001], ys1[1001], zs1[1001];
    for (int i = 0; i < 1001; i++) {
        xs1[i] = i * 0.001f;
        ys1[i] = 0.5f + 0.5f * cosf(50 * (xs1[i] + (float)ImGui::GetTime() / 10));
        zs1[i] = 0.5f + 0.5f * sinf(50 * (xs1[i] + (float)ImGui::GetTime() / 10));
    }
    static double xs2[20], ys2[20], zs2[20];
    for (int i = 0; i < 20; i++) {
        xs2[i] = i * 1 / 19.0f;
        ys2[i] = xs2[i] * xs2[i];
        zs2[i] = xs2[i] * ys2[i];
    }
    if (ImPlot3D::BeginPlot("Line Plots")) {
        ImPlot3D::SetupAxes("x", "y", "z");
        ImPlot3D::PlotLine("f(x)", xs1, ys1, zs1, 1001);
        ImPlot3D::SetNextMarkerStyle(ImPlot3DMarker_Circle);
        ImPlot3D::PlotLine("g(x)", xs2, ys2, zs2, 20, ImPlot3DLineFlags_Segments);
        ImPlot3D::EndPlot();
    }
}

void DemoScatterPlots() {
    srand(0);
    static float xs1[100], ys1[100], zs1[100];
    for (int i = 0; i < 100; i++) {
        xs1[i] = i * 0.01f;
        ys1[i] = xs1[i] + 0.1f * ((float)rand() / (float)RAND_MAX);
        zs1[i] = xs1[i] + 0.1f * ((float)rand() / (float)RAND_MAX);
    }
    static float xs2[50], ys2[50], zs2[50];
    for (int i = 0; i < 50; i++) {
        xs2[i] = 0.25f + 0.2f * ((float)rand() / (float)RAND_MAX);
        ys2[i] = 0.50f + 0.2f * ((float)rand() / (float)RAND_MAX);
        zs2[i] = 0.75f + 0.2f * ((float)rand() / (float)RAND_MAX);
    }

    if (ImPlot3D::BeginPlot("Scatter Plots")) {
        ImPlot3D::PlotScatter("Data 1", xs1, ys1, zs1, 100);
        ImPlot3D::PushStyleVar(ImPlot3DStyleVar_FillAlpha, 0.25f);
        ImPlot3D::SetNextMarkerStyle(ImPlot3DMarker_Square, 6, ImPlot3D::GetColormapColor(1), IMPLOT3D_AUTO, ImPlot3D::GetColormapColor(1));
        ImPlot3D::PlotScatter("Data 2", xs2, ys2, zs1, 50);
        ImPlot3D::PopStyleVar();
        ImPlot3D::EndPlot();
    }
}

void DemoTrianglePlots() {
    // Pyramid coordinates
    // Apex
    float ax = 0.0f, ay = 0.0f, az = 1.0f;
    // Square base corners
    float cx[4] = {-0.5f, 0.5f, 0.5f, -0.5f};
    float cy[4] = {-0.5f, -0.5f, 0.5f, 0.5f};
    float cz[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    // We have 6 triangles (18 vertices) total:
    // Sides:
    // T1: apex, corner0, corner1
    // T2: apex, corner1, corner2
    // T3: apex, corner2, corner3
    // T4: apex, corner3, corner0
    // Base (two triangles form a square):
    // T5: corner0, corner1, corner2
    // T6: corner0, corner2, corner3

    static float xs[18], ys[18], zs[18];
    int i = 0;

    // Helper lambda to append a vertex
    auto AddVertex = [&](float X, float Y, float Z) {
        xs[i] = X;
        ys[i] = Y;
        zs[i] = Z;
        i++;
    };

    // Triangle 1
    AddVertex(ax, ay, az);
    AddVertex(cx[0], cy[0], cz[0]);
    AddVertex(cx[1], cy[1], cz[1]);

    // Triangle 2
    AddVertex(ax, ay, az);
    AddVertex(cx[1], cy[1], cz[1]);
    AddVertex(cx[2], cy[2], cz[2]);

    // Triangle 3
    AddVertex(ax, ay, az);
    AddVertex(cx[2], cy[2], cz[2]);
    AddVertex(cx[3], cy[3], cz[3]);

    // Triangle 4
    AddVertex(ax, ay, az);
    AddVertex(cx[3], cy[3], cz[3]);
    AddVertex(cx[0], cy[0], cz[0]);

    // Triangle 5 (base)
    AddVertex(cx[0], cy[0], cz[0]);
    AddVertex(cx[1], cy[1], cz[1]);
    AddVertex(cx[2], cy[2], cz[2]);

    // Triangle 6 (base)
    AddVertex(cx[0], cy[0], cz[0]);
    AddVertex(cx[2], cy[2], cz[2]);
    AddVertex(cx[3], cy[3], cz[3]);

    // Now we have 18 vertices in xs, ys, zs forming the pyramid

    if (ImPlot3D::BeginPlot("Triangle Plots")) {
        ImPlot3D::SetupAxesLimits(-1, 1, -1, 1, -0.5, 1.5);

        // Setup pyramid colors
        ImPlot3D::SetNextFillStyle(ImPlot3D::GetColormapColor(0));
        ImPlot3D::SetNextLineStyle(ImPlot3D::GetColormapColor(1), 2);
        ImPlot3D::SetNextMarkerStyle(ImPlot3DMarker_Square, 3, ImPlot3D::GetColormapColor(2), IMPLOT3D_AUTO, ImPlot3D::GetColormapColor(2));

        // Plot pyramid
        ImPlot3D::PlotTriangle("Pyramid", xs, ys, zs, 6 * 3); // 6 triangles, 3 vertices each = 18
        ImPlot3D::EndPlot();
    }
}

void DemoQuadPlots() {
    static float xs[6 * 4], ys[6 * 4], zs[6 * 4];

    // clang-format off
    // Initialize the cube vertices for +x and -x faces
    // +x face
    xs[0] = 1; ys[0] = -1; zs[0] = -1;
    xs[1] = 1; ys[1] =  1; zs[1] = -1;
    xs[2] = 1; ys[2] =  1; zs[2] =  1;
    xs[3] = 1; ys[3] = -1; zs[3] =  1;

    // -x face
    xs[4] = -1; ys[4] = -1; zs[4] = -1;
    xs[5] = -1; ys[5] =  1; zs[5] = -1;
    xs[6] = -1; ys[6] =  1; zs[6] =  1;
    xs[7] = -1; ys[7] = -1; zs[7] =  1;

    // Initialize the cube vertices for +y and -y faces
    // +y face
    xs[8] = -1; ys[8] = 1; zs[8] = -1;
    xs[9] =  1; ys[9] = 1; zs[9] = -1;
    xs[10] =  1; ys[10] = 1; zs[10] =  1;
    xs[11] = -1; ys[11] = 1; zs[11] =  1;

    // -y face
    xs[12] = -1; ys[12] = -1; zs[12] = -1;
    xs[13] =  1; ys[13] = -1; zs[13] = -1;
    xs[14] =  1; ys[14] = -1; zs[14] =  1;
    xs[15] = -1; ys[15] = -1; zs[15] =  1;

    // Initialize the cube vertices for +z and -z faces
    // +z face
    xs[16] = -1; ys[16] = -1; zs[16] = 1;
    xs[17] =  1; ys[17] = -1; zs[17] = 1;
    xs[18] =  1; ys[18] =  1; zs[18] = 1;
    xs[19] = -1; ys[19] =  1; zs[19] = 1;

    // -z face
    xs[20] = -1; ys[20] = -1; zs[20] = -1;
    xs[21] =  1; ys[21] = -1; zs[21] = -1;
    xs[22] =  1; ys[22] =  1; zs[22] = -1;
    xs[23] = -1; ys[23] =  1; zs[23] = -1;
    // clang-format on

    if (ImPlot3D::BeginPlot("Quad Plots")) {
        ImPlot3D::SetupAxesLimits(-1.5f, 1.5f, -1.5f, 1.5f, -1.5f, 1.5f);

        // Render +x and -x faces
        static ImVec4 colorX(0.8f, 0.2f, 0.2f, 0.8f); // Red
        ImPlot3D::SetNextFillStyle(colorX);
        ImPlot3D::SetNextLineStyle(colorX, 2);
        ImPlot3D::SetNextMarkerStyle(ImPlot3DMarker_Square, 3, colorX, IMPLOT3D_AUTO, colorX);
        ImPlot3D::PlotQuad("X", &xs[0], &ys[0], &zs[0], 8);

        // Render +y and -y faces
        static ImVec4 colorY(0.2f, 0.8f, 0.2f, 0.8f); // Green
        ImPlot3D::SetNextFillStyle(colorY);
        ImPlot3D::SetNextLineStyle(colorY, 2);
        ImPlot3D::SetNextMarkerStyle(ImPlot3DMarker_Square, 3, colorY, IMPLOT3D_AUTO, colorY);
        ImPlot3D::PlotQuad("Y", &xs[8], &ys[8], &zs[8], 8);

        // Render +z and -z faces
        static ImVec4 colorZ(0.2f, 0.2f, 0.8f, 0.8f); // Blue
        ImPlot3D::SetNextFillStyle(colorZ);
        ImPlot3D::SetNextLineStyle(colorZ, 2);
        ImPlot3D::SetNextMarkerStyle(ImPlot3DMarker_Square, 3, colorZ, IMPLOT3D_AUTO, colorZ);
        ImPlot3D::PlotQuad("Z", &xs[16], &ys[16], &zs[16], 8);

        ImPlot3D::EndPlot();
    }
}

void DemoSurfacePlots() {
    constexpr int N = 20;
    static float xs[N * N], ys[N * N], zs[N * N];

    // Define the range for X and Y
    constexpr float range_min = -5.0f;
    constexpr float range_max = 5.0f;
    constexpr float step = (range_max - range_min) / (N - 1);

    // Populate the xs, ys, and zs arrays
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            int idx = i * N + j;
            xs[idx] = range_min + j * step;                                   // X values are constant along rows
            ys[idx] = range_min + i * step;                                   // Y values are constant along columns
            zs[idx] = sinf(sqrt(xs[idx] * xs[idx] + ys[idx] * ys[idx])); // Z = sin(sqrt(X^2 + Y^2))
        }
    }

    // Begin the plot
    ImPlot3D::PushColormap("Hot");
    if (ImPlot3D::BeginPlot("Surface Plots")) {
        // Set styles
        ImPlot3D::PushStyleVar(ImPlot3DStyleVar_FillAlpha, 0.8f);
        ImPlot3D::SetNextLineStyle(ImPlot3D::GetColormapColor(1));

        // Plot the surface
        ImPlot3D::PlotSurface("Wave Surface", xs, ys, zs, N, N);

        // End the plot
        ImPlot3D::PopStyleVar();
        ImPlot3D::EndPlot();
    }
    ImPlot3D::PopColormap();
}

void DemoMeshPlots() {
    static int mesh_id = 0;
    ImGui::Combo("Mesh", &mesh_id, "Duck\0Sphere\0Cube\0\0");

    // Choose fill color
    static bool set_fill_color = true;
    static ImVec4 fill_color = ImVec4(0.8f, 0.8f, 0.2f, 0.6f);
    ImGui::Checkbox("Fill Color", &set_fill_color);
    if (set_fill_color) {
        ImGui::SameLine();
        ImGui::ColorEdit4("##MeshFillColor", (float*)&fill_color);
    }

    // Choose line color
    static bool set_line_color = true;
    static ImVec4 line_color = ImVec4(0.2f, 0.2f, 0.2f, 0.8f);
    ImGui::Checkbox("Line Color", &set_line_color);
    if (set_line_color) {
        ImGui::SameLine();
        ImGui::ColorEdit4("##MeshLineColor", (float*)&line_color);
    }

    // Choose marker color
    static bool set_marker_color = false;
    static ImVec4 marker_color = ImVec4(0.2f, 0.2f, 0.2f, 0.8f);
    ImGui::Checkbox("Marker Color", &set_marker_color);
    if (set_marker_color) {
        ImGui::SameLine();
        ImGui::ColorEdit4("##MeshMarkerColor", (float*)&marker_color);
    }

    if (ImPlot3D::BeginPlot("Mesh Plots")) {
        ImPlot3D::SetupAxesLimits(-1, 1, -1, 1, -1, 1);

        // Set colors
        if (set_fill_color)
            ImPlot3D::SetNextFillStyle(fill_color);
        else {
            // If not set as transparent, the fill color will be determined by the colormap
            ImPlot3D::SetNextFillStyle(ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        }
        if (set_line_color)
            ImPlot3D::SetNextLineStyle(line_color);
        if (set_marker_color)
            ImPlot3D::SetNextMarkerStyle(ImPlot3DMarker_Square, 3, marker_color, IMPLOT3D_AUTO, marker_color);

        // Plot mesh
        if (mesh_id == 0)
            ImPlot3D::PlotMesh("Duck", duck_vtx, duck_idx, DUCK_VTX_COUNT, DUCK_IDX_COUNT);
        else if (mesh_id == 1)
            ImPlot3D::PlotMesh("Sphere", sphere_vtx, sphere_idx, SPHERE_VTX_COUNT, SPHERE_IDX_COUNT);
        else if (mesh_id == 2)
            ImPlot3D::PlotMesh("Cube", cube_vtx, cube_idx, CUBE_VTX_COUNT, CUBE_IDX_COUNT);

        ImPlot3D::EndPlot();
    }
}

void DemoRealtimePlots() {
    ImGui::BulletText("Move your mouse to change the data!");
    static ScrollingBuffer sdata1, sdata2, sdata3;
    static ImPlot3DAxisFlags flags = ImPlot3DAxisFlags_NoTickLabels;
    static float t = 0.0f;
    static float last_t = -1.0f;

    if (ImPlot3D::BeginPlot("Scrolling Plot", ImVec2(-1, 400))) {
        // Pool mouse data every 10 ms
        t += ImGui::GetIO().DeltaTime;
        if (t - last_t > 0.01f) {
            last_t = t;
            ImVec2 mouse = ImGui::GetMousePos();
            if (ImAbs(mouse.x) < 1e4f && ImAbs(mouse.y) < 1e4f) {
                ImVec2 plot_center = ImPlot3D::GetFramePos();
                plot_center.x += ImPlot3D::GetFrameSize().x / 2;
                plot_center.y += ImPlot3D::GetFrameSize().y / 2;
                sdata1.AddPoint(t);
                sdata2.AddPoint(mouse.x - plot_center.x);
                sdata3.AddPoint(mouse.y - plot_center.y);
            }
        }

        ImPlot3D::SetupAxes("Time", "Mouse X", "Mouse Y", flags, flags, flags);
        ImPlot3D::SetupAxisLimits(ImAxis3D_X, t - 10.0f, t, ImPlot3DCond_Always);
        ImPlot3D::SetupAxisLimits(ImAxis3D_Y, -400, 400, ImPlot3DCond_Once);
        ImPlot3D::SetupAxisLimits(ImAxis3D_Z, -400, 400, ImPlot3DCond_Once);
        ImPlot3D::PlotLine("Mouse", &sdata1.Data[0], &sdata2.Data[0], &sdata3.Data[0], sdata1.Data.size(), 0, sdata1.Offset, sizeof(float));
        ImPlot3D::EndPlot();
    }
}

void DemoMarkersAndText() {
    static float mk_size = ImPlot3D::GetStyle().MarkerSize;
    static float mk_weight = ImPlot3D::GetStyle().MarkerWeight;
    ImGui::DragFloat("Marker Size", &mk_size, 0.1f, 2.0f, 10.0f, "%.2f px");
    ImGui::DragFloat("Marker Weight", &mk_weight, 0.05f, 0.5f, 3.0f, "%.2f px");

    if (ImPlot3D::BeginPlot("##MarkerStyles", ImVec2(-1, 0), ImPlot3DFlags_CanvasOnly)) {

        ImPlot3D::SetupAxes(nullptr, nullptr, nullptr, ImPlot3DAxisFlags_NoDecorations, ImPlot3DAxisFlags_NoDecorations, ImPlot3DAxisFlags_NoDecorations);
        ImPlot3D::SetupAxesLimits(-0.5, 1.5, -0.5, 1.5, 0, ImPlot3DMarker_COUNT + 1);

        float xs[2] = {0, 0};
        float ys[2] = {0, 0};
        float zs[2] = {ImPlot3DMarker_COUNT, ImPlot3DMarker_COUNT + 1};

        // Filled markers
        for (int m = 0; m < ImPlot3DMarker_COUNT; ++m) {
            xs[1] = xs[0] + ImCos(zs[0] / float(ImPlot3DMarker_COUNT) * 2 * IM_PI) * 0.5;
            ys[1] = ys[0] + ImSin(zs[0] / float(ImPlot3DMarker_COUNT) * 2 * IM_PI) * 0.5;

            ImGui::PushID(m);
            ImPlot3D::SetNextMarkerStyle(m, mk_size, IMPLOT3D_AUTO_COL, mk_weight);
            ImPlot3D::PlotLine("##Filled", xs, ys, zs, 2);
            ImGui::PopID();
            zs[0]--;
            zs[1]--;
        }

        xs[0] = 1;
        ys[0] = 1;
        zs[0] = ImPlot3DMarker_COUNT;
        zs[1] = zs[0] + 1;

        // Open markers
        for (int m = 0; m < ImPlot3DMarker_COUNT; ++m) {
            xs[1] = xs[0] + ImCos(zs[0] / float(ImPlot3DMarker_COUNT) * 2 * IM_PI) * 0.5;
            ys[1] = ys[0] - ImSin(zs[0] / float(ImPlot3DMarker_COUNT) * 2 * IM_PI) * 0.5;

            ImGui::PushID(m);
            ImPlot3D::SetNextMarkerStyle(m, mk_size, ImVec4(0, 0, 0, 0), mk_weight);
            ImPlot3D::PlotLine("##Open", xs, ys, zs, 2);
            ImGui::PopID();
            zs[0]--;
            zs[1]--;
        }

        ImPlot3D::PlotText("Filled Markers", 0.0f, 0.0f, 6.0f);
        ImPlot3D::PlotText("Open Markers", 1.0f, 1.0f, 6.0f);

        ImPlot3D::PushStyleColor(ImPlot3DCol_InlayText, ImVec4(1, 0, 1, 1));
        ImPlot3D::PlotText("Rotated Text", 0.5f, 0.5f, 6.0f, IM_PI / 4, ImVec2(0, 0));
        ImPlot3D::PopStyleColor();

        ImPlot3D::EndPlot();
    }
}

void DemoNaNValues() {
    static bool include_nan = true;
    static ImPlot3DLineFlags flags = 0;

    float data1[5] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
    float data2[5] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
    float data3[5] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};

    if (include_nan)
        data1[2] = NAN;

    ImGui::Checkbox("Include NaN", &include_nan);
    ImGui::SameLine();
    ImGui::CheckboxFlags("Skip NaN", (unsigned int*)&flags, ImPlot3DLineFlags_SkipNaN);

    if (ImPlot3D::BeginPlot("##NaNValues")) {
        ImPlot3D::SetNextMarkerStyle(ImPlot3DMarker_Square);
        ImPlot3D::PlotLine("Line", data1, data2, data3, 5, flags);
        ImPlot3D::EndPlot();
    }
}

//-----------------------------------------------------------------------------
// [SECTION] Custom
//-----------------------------------------------------------------------------

void DemoCustomStyles() {
    ImPlot3D::PushColormap(ImPlot3DColormap_Deep);
    // normally you wouldn't change the entire style each frame
    ImPlot3DStyle backup = ImPlot3D::GetStyle();
    MyImPlot3D::StyleSeaborn();
    if (ImPlot3D::BeginPlot("Seaborn Style")) {
        ImPlot3D::SetupAxes("X-axis", "Y-axis", "Z-axis");
        ImPlot3D::SetupAxesLimits(-0.5f, 9.5f, -0.5f, 0.5f, 0, 10);
        unsigned int xs[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        unsigned int ys[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        unsigned int lin[10] = {8, 8, 9, 7, 8, 8, 8, 9, 7, 8};
        unsigned int dot[10] = {7, 6, 6, 7, 8, 5, 6, 5, 8, 7};
        ImPlot3D::NextColormapColor(); // Skip blue
        ImPlot3D::PlotLine("Line", xs, ys, lin, 10);
        ImPlot3D::NextColormapColor(); // Skip green
        ImPlot3D::PlotScatter("Scatter", xs, ys, dot, 10);
        ImPlot3D::EndPlot();
    }
    ImPlot3D::GetStyle() = backup;
    ImPlot3D::PopColormap();
}

void DemoCustomRendering() {
    if (ImPlot3D::BeginPlot("##CustomRend")) {
        ImPlot3D::SetupAxesLimits(-0.1f, 1.1f, -0.1f, 1.1f, -0.1f, 1.1f);

        // Draw circle
        ImVec2 cntr = ImPlot3D::PlotToPixels(ImPlot3DPoint(0.5f, 0.5f, 0.5f));
        ImPlot3D::GetPlotDrawList()->AddCircleFilled(cntr, 20, IM_COL32(255, 255, 0, 255), 20);

        // Draw box
        ImPlot3DPoint corners[8] = {
            ImPlot3DPoint(0, 0, 0),
            ImPlot3DPoint(1, 0, 0),
            ImPlot3DPoint(1, 1, 0),
            ImPlot3DPoint(0, 1, 0),
            ImPlot3DPoint(0, 0, 1),
            ImPlot3DPoint(1, 0, 1),
            ImPlot3DPoint(1, 1, 1),
            ImPlot3DPoint(0, 1, 1),
        };
        ImVec2 corners_px[8];
        for (int i = 0; i < 8; i++)
            corners_px[i] = ImPlot3D::PlotToPixels(corners[i]);

        ImU32 col = IM_COL32(128, 0, 255, 255);
        for (int i = 0; i < 4; i++) {
            ImPlot3D::GetPlotDrawList()->AddLine(corners_px[i], corners_px[(i + 1) % 4], col);
            ImPlot3D::GetPlotDrawList()->AddLine(corners_px[i + 4], corners_px[(i + 1) % 4 + 4], col);
            ImPlot3D::GetPlotDrawList()->AddLine(corners_px[i], corners_px[i + 4], col);
        }
        ImPlot3D::EndPlot();
    }
}

//-----------------------------------------------------------------------------
// [SECTION] Demo Window
//-----------------------------------------------------------------------------

void DemoHelp() {
    ImGui::SeparatorText("ABOUT THIS DEMO:");
    ImGui::BulletText("The other tabs are demonstrating many aspects of the library.");

    ImGui::SeparatorText("PROGRAMMER GUIDE:");
    ImGui::BulletText("See the ShowDemoWindow() code in implot3d_demo.cpp. <- you are here!");
    ImGui::BulletText("See comments in implot3d_demo.cpp.");
    ImGui::BulletText("See example application in example/ folder.");

    ImGui::SeparatorText("USER GUIDE:");
    ImGui::BulletText("Translation");
    {
        ImGui::Indent();
        ImGui::BulletText("Left-click drag to translate.");
        ImGui::BulletText("If over axis, only that axis will translate.");
        ImGui::BulletText("If over plane, only that plane will translate.");
        ImGui::BulletText("If outside plot area, translate in the view plane.");
        ImGui::Unindent();
    }

    ImGui::BulletText("Zoom");
    {
        ImGui::Indent();
        ImGui::BulletText("Scroll or middle-click drag to zoom.");
        ImGui::BulletText("If over axis, only that axis will zoom.");
        ImGui::BulletText("If over plane, only that plane will zoom.");
        ImGui::BulletText("If outside plot area, zoom the entire plot.");
        ImGui::Unindent();
    }

    ImGui::BulletText("Rotation");
    {
        ImGui::Indent();
        ImGui::BulletText("Right-click drag to rotate.");
        ImGui::BulletText("To reset rotation, double right-click outside plot area.");
        ImGui::BulletText("To rotate to plane, double right-click when over the plane.");
        ImGui::Unindent();
    }

    ImGui::BulletText("Fit data");
    {
        ImGui::Indent();
        ImGui::BulletText("Double left-click to fit.");
        ImGui::BulletText("If over axis, fit data to axis.");
        ImGui::BulletText("If over plane, fit data to plane.");
        ImGui::BulletText("If outside plot area, fit data to plot.");
        ImGui::Unindent();
    }

    ImGui::BulletText("Context Menus");
    {
        ImGui::Indent();
        ImGui::BulletText("Right-click outside plot area to show full context menu.");
        ImGui::BulletText("Right-click over legend to show legend context menu.");
        ImGui::BulletText("Right-click over axis to show axis context menu.");
        ImGui::BulletText("Right-click over plane to show plane context menu.");
        ImGui::Unindent();
    }

    ImGui::BulletText("Click legend label icons to show/hide plot items.");
}

void DemoHeader(const char* label, void (*demo)()) {
    if (ImGui::TreeNodeEx(label)) {
        demo();
        ImGui::TreePop();
    }
}

void ShowDemoWindow(bool* p_open) {
    static bool show_implot3d_style_editor = false;
    static bool show_imgui_metrics = false;
    static bool show_imgui_style_editor = false;
    static bool show_imgui_demo = false;

    if (show_implot3d_style_editor) {
        ImGui::Begin("Style Editor (ImPlot3D)", &show_implot3d_style_editor);
        ImPlot3D::ShowStyleEditor();
        ImGui::End();
    }

    if (show_imgui_style_editor) {
        ImGui::Begin("Style Editor (ImGui)", &show_imgui_style_editor);
        ImGui::ShowStyleEditor();
        ImGui::End();
    }
    if (show_imgui_metrics)
        ImGui::ShowMetricsWindow(&show_imgui_metrics);
    if (show_imgui_demo)
        ImGui::ShowDemoWindow(&show_imgui_demo);


    ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(600, 750), ImGuiCond_FirstUseEver);
    ImGui::Begin("ImPlot3D Demo", p_open, ImGuiWindowFlags_MenuBar);
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("Tools")) {
            ImGui::MenuItem("Style Editor", nullptr, &show_implot3d_style_editor);
            ImGui::Separator();
            ImGui::MenuItem("ImGui Metrics", nullptr, &show_imgui_metrics);
            ImGui::MenuItem("ImGui Style Editor", nullptr, &show_imgui_style_editor);
            ImGui::MenuItem("ImGui Demo", nullptr, &show_imgui_demo);
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    ImGui::Text("ImPlot3D says olá! (%s)", IMPLOT3D_VERSION);

    ImGui::Spacing();

    if (ImGui::BeginTabBar("ImPlot3DDemoTabs")) {
        if (ImGui::BeginTabItem("Plots")) {
            DemoHeader("Line Plots", DemoLinePlots);
            DemoHeader("Scatter Plots", DemoScatterPlots);
            DemoHeader("Triangle Plots", DemoTrianglePlots);
            DemoHeader("Quad Plots", DemoQuadPlots);
            DemoHeader("Surface Plots", DemoSurfacePlots);
            DemoHeader("Mesh Plots", DemoMeshPlots);
            DemoHeader("Realtime Plots", DemoRealtimePlots);
            DemoHeader("Markers and Text", DemoMarkersAndText);
            DemoHeader("NaN Values", DemoNaNValues);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Custom")) {
            DemoHeader("Custom Styles", DemoCustomStyles);
            DemoHeader("Custom Rendering", DemoCustomRendering);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Help")) {
            DemoHelp();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}

//-----------------------------------------------------------------------------
// [SECTION] Style Editor
//-----------------------------------------------------------------------------

bool ShowStyleSelector(const char* label) {
    static int style_idx = -1;
    if (ImGui::Combo(label, &style_idx, "Auto\0Classic\0Dark\0Light\0")) {
        switch (style_idx) {
            case 0: StyleColorsAuto(); break;
            case 1: StyleColorsClassic(); break;
            case 2: StyleColorsDark(); break;
            case 3: StyleColorsLight(); break;
        }
        return true;
    }
    return false;
}

void RenderColorBar(const ImU32* colors, int size, ImDrawList& DrawList, const ImRect& bounds, bool vert, bool reversed, bool continuous) {
    const int n = continuous ? size - 1 : size;
    ImU32 col1, col2;
    if (vert) {
        const float step = bounds.GetHeight() / n;
        ImRect rect(bounds.Min.x, bounds.Min.y, bounds.Max.x, bounds.Min.y + step);
        for (int i = 0; i < n; ++i) {
            if (reversed) {
                col1 = colors[size - i - 1];
                col2 = continuous ? colors[size - i - 2] : col1;
            } else {
                col1 = colors[i];
                col2 = continuous ? colors[i + 1] : col1;
            }
            DrawList.AddRectFilledMultiColor(rect.Min, rect.Max, col1, col1, col2, col2);
            rect.TranslateY(step);
        }
    } else {
        const float step = bounds.GetWidth() / n;
        ImRect rect(bounds.Min.x, bounds.Min.y, bounds.Min.x + step, bounds.Max.y);
        for (int i = 0; i < n; ++i) {
            if (reversed) {
                col1 = colors[size - i - 1];
                col2 = continuous ? colors[size - i - 2] : col1;
            } else {
                col1 = colors[i];
                col2 = continuous ? colors[i + 1] : col1;
            }
            DrawList.AddRectFilledMultiColor(rect.Min, rect.Max, col1, col2, col2, col1);
            rect.TranslateX(step);
        }
    }
}

static inline ImU32 CalcTextColor(const ImVec4& bg) { return (bg.x * 0.299f + bg.y * 0.587f + bg.z * 0.114f) > 0.5f ? IM_COL32_BLACK : IM_COL32_WHITE; }
static inline ImU32 CalcTextColor(ImU32 bg) { return CalcTextColor(ImGui::ColorConvertU32ToFloat4(bg)); }

bool ColormapButton(const char* label, const ImVec2& size_arg, ImPlot3DColormap cmap) {
    ImGuiContext& G = *GImGui;
    const ImGuiStyle& style = G.Style;
    ImGuiWindow* Window = G.CurrentWindow;
    if (Window->SkipItems)
        return false;
    ImPlot3DContext& gp = *GImPlot3D;
    cmap = cmap == IMPLOT3D_AUTO ? gp.Style.Colormap : cmap;
    IM_ASSERT_USER_ERROR(cmap >= 0 && cmap < gp.ColormapData.Count, "Invalid colormap index!");
    const ImU32* keys = gp.ColormapData.GetKeys(cmap);
    const int count = gp.ColormapData.GetKeyCount(cmap);
    const bool qual = gp.ColormapData.IsQual(cmap);
    const ImVec2 pos = ImGui::GetCurrentWindow()->DC.CursorPos;
    const ImVec2 label_size = ImGui::CalcTextSize(label, nullptr, true);
    ImVec2 size = ImGui::CalcItemSize(size_arg, label_size.x + style.FramePadding.x * 2.0f, label_size.y + style.FramePadding.y * 2.0f);
    const ImRect rect = ImRect(pos.x, pos.y, pos.x + size.x, pos.y + size.y);
    RenderColorBar(keys, count, *ImGui::GetWindowDrawList(), rect, false, false, !qual);
    const ImU32 text = CalcTextColor(gp.ColormapData.LerpTable(cmap, G.Style.ButtonTextAlign.x));
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32_BLACK_TRANS);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.1f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 1, 1, 0.2f));
    ImGui::PushStyleColor(ImGuiCol_Text, text);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);
    const bool pressed = ImGui::Button(label, size);
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(1);
    return pressed;
}

void ShowStyleEditor(ImPlot3DStyle* ref) {
    ImPlot3DContext& gp = *GImPlot3D;

    // Handle style internal storage
    ImPlot3DStyle& style = GetStyle();
    static ImPlot3DStyle ref_saved_style;
    static bool init = true;
    if (init && ref == nullptr)
        ref_saved_style = style;
    init = false;
    if (ref == nullptr)
        ref = &ref_saved_style;

    // Handle flash style color
    static float flash_color_time = 0.5f;
    static ImPlot3DCol flash_color_idx = ImPlot3DCol_COUNT;
    static ImVec4 flash_color_backup = ImVec4(0, 0, 0, 0);
    if (flash_color_idx != ImPlot3DCol_COUNT) {
        // Flash color
        ImVec4& color = style.Colors[flash_color_idx];
        ImGui::ColorConvertHSVtoRGB(ImCos(flash_color_time * 6.0f) * 0.5f + 0.5f, 0.5f, 0.5f, color.x, color.y, color.z);
        color.w = 1.0f;

        // Decrease timer until zero
        if ((flash_color_time -= ImGui::GetIO().DeltaTime) <= 0.0f) {
            // When timer reaches zero, restore the backup color
            style.Colors[flash_color_idx] = flash_color_backup;
            flash_color_idx = ImPlot3DCol_COUNT;
            flash_color_time = 0.5f;
        }
    }

    // Style selector
    if (ImPlot3D::ShowStyleSelector("Colors##Selector"))
        ref_saved_style = style;

    // Save/Revert button
    if (ImGui::Button("Save Ref"))
        *ref = ref_saved_style = style;
    ImGui::SameLine();
    if (ImGui::Button("Revert Ref"))
        style = *ref;
    ImGui::SameLine();
    HelpMarker(
        "Save/Revert in local non-persistent storage. Default Colors definition are not affected. "
        "Use \"Export\" below to save them somewhere.");

    ImGui::Separator();

    if (ImGui::BeginTabBar("##Tabs", ImGuiTabBarFlags_None)) {
        if (ImGui::BeginTabItem("Variables")) {
            ImGui::Text("Item Styling");
            ImGui::SliderFloat("LineWeight", &style.LineWeight, 0.0f, 5.0f, "%.1f");
            ImGui::SliderFloat("MarkerSize", &style.MarkerSize, 2.0f, 10.0f, "%.1f");
            ImGui::SliderFloat("MarkerWeight", &style.MarkerWeight, 0.0f, 5.0f, "%.1f");
            ImGui::SliderFloat("FillAlpha", &style.FillAlpha, 0.0f, 1.0f, "%.2f");
            ImGui::Text("Plot Styling");
            ImGui::SliderFloat2("PlotDefaultSize", (float*)&style.PlotDefaultSize, 0.0f, 1000, "%.0f");
            ImGui::SliderFloat2("PlotMinSize", (float*)&style.PlotMinSize, 0.0f, 300, "%.0f");
            ImGui::SliderFloat2("PlotPadding", (float*)&style.PlotPadding, 0.0f, 20.0f, "%.0f");
            ImGui::SliderFloat2("LabelPadding", (float*)&style.LabelPadding, 0.0f, 20.0f, "%.0f");
            ImGui::Text("Legend Styling");
            ImGui::SliderFloat2("LegendPadding", (float*)&style.LegendPadding, 0.0f, 20.0f, "%.0f");
            ImGui::SliderFloat2("LegendInnerPadding", (float*)&style.LegendInnerPadding, 0.0f, 10.0f, "%.0f");
            ImGui::SliderFloat2("LegendSpacing", (float*)&style.LegendSpacing, 0.0f, 5.0f, "%.0f");
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Colors")) {
            static int output_dest = 0;
            static bool output_only_modified = true;
            if (ImGui::Button("Export")) {
                if (output_dest == 0)
                    ImGui::LogToClipboard();
                else
                    ImGui::LogToTTY();
                ImGui::LogText("ImVec4* colors = ImPlot3D::GetStyle().Colors;\n");
                for (int i = 0; i < ImPlot3DCol_COUNT; i++) {
                    const ImVec4& col = style.Colors[i];
                    const char* name = ImPlot3D::GetStyleColorName(i);
                    if (!output_only_modified || memcmp(&col, &ref->Colors[i], sizeof(ImVec4)) != 0)
                        ImGui::LogText("colors[ImPlot3DCol_%s]%*s= ImVec4(%.2ff, %.2ff, %.2ff, %.2ff);\n",
                                       name, 15 - (int)strlen(name), "", col.x, col.y, col.z, col.w);
                }
                ImGui::LogFinish();
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(120);
            ImGui::Combo("##output_type", &output_dest, "To Clipboard\0To TTY\0");
            ImGui::SameLine();
            ImGui::Checkbox("Only Modified Colors", &output_only_modified);

            static ImGuiTextFilter filter;
            filter.Draw("Filter colors", ImGui::GetFontSize() * 16);

            static ImGuiColorEditFlags alpha_flags = ImGuiColorEditFlags_AlphaPreviewHalf;
            if (ImGui::RadioButton("Opaque", alpha_flags == ImGuiColorEditFlags_None))
                alpha_flags = ImGuiColorEditFlags_None;
            ImGui::SameLine();
            if (ImGui::RadioButton("Alpha", alpha_flags == ImGuiColorEditFlags_AlphaPreview))
                alpha_flags = ImGuiColorEditFlags_AlphaPreview;
            ImGui::SameLine();
            if (ImGui::RadioButton("Both", alpha_flags == ImGuiColorEditFlags_AlphaPreviewHalf))
                alpha_flags = ImGuiColorEditFlags_AlphaPreviewHalf;
            ImGui::SameLine();
            HelpMarker(
                "In the color list:\n"
                "Left-click on color square to open color picker,\n"
                "Right-click to open edit options menu.");

            ImGui::Separator();

            for (int i = 0; i < ImPlot3DCol_COUNT; i++) {
                const char* name = ImPlot3D::GetStyleColorName(i);
                if (!filter.PassFilter(name))
                    continue;
                ImGui::PushID(i);

                // Flash color
                if (ImGui::Button("?")) {
                    if (flash_color_idx != ImPlot3DCol_COUNT)
                        style.Colors[flash_color_idx] = flash_color_backup;
                    flash_color_time = 0.5f;
                    flash_color_idx = (ImPlot3DCol)i;
                    flash_color_backup = style.Colors[i];
                }
                ImGui::SetItemTooltip("Flash given color to identify places where it is used.");
                ImGui::SameLine();

                // Handle auto color selection
                const bool is_auto = IsColorAuto(style.Colors[i]);
                if (is_auto)
                    ImGui::BeginDisabled();
                if (ImGui::Button("Auto"))
                    style.Colors[i] = IMPLOT3D_AUTO_COL;
                if (is_auto)
                    ImGui::EndDisabled();

                // Color selection
                ImGui::SameLine();
                if (ImGui::ColorEdit4("##Color", (float*)&style.Colors[i], ImGuiColorEditFlags_NoInputs | alpha_flags)) {
                    if (style.Colors[i].w == -1)
                        style.Colors[i].w = 1;
                }

                // Save/Revert buttons if color changed
                if (memcmp(&style.Colors[i], &ref->Colors[i], sizeof(ImVec4)) != 0) {
                    ImGui::SameLine();
                    if (ImGui::Button("Save"))
                        ref->Colors[i] = style.Colors[i];
                    ImGui::SameLine();
                    if (ImGui::Button("Revert"))
                        style.Colors[i] = ref->Colors[i];
                }
                ImGui::SameLine();
                ImGui::TextUnformatted(name);
                ImGui::PopID();
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Colormaps")) {
            static int output_dest = 0;
            if (ImGui::Button("Export", ImVec2(75, 0))) {
                if (output_dest == 0)
                    ImGui::LogToClipboard();
                else
                    ImGui::LogToTTY();
                int size = GetColormapSize();
                const char* name = GetColormapName(gp.Style.Colormap);
                ImGui::LogText("static const ImU32 %s_Data[%d] = {\n", name, size);
                for (int i = 0; i < size; ++i) {
                    ImU32 col = GetColormapColorU32(i, gp.Style.Colormap);
                    ImGui::LogText("    %u%s\n", col, i == size - 1 ? "" : ",");
                }
                ImGui::LogText("};\nImPlotColormap %s = ImPlot::AddColormap(\"%s\", %s_Data, %d);", name, name, name, size);
                ImGui::LogFinish();
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(120);
            ImGui::Combo("##output_type", &output_dest, "To Clipboard\0To TTY\0");
            ImGui::SameLine();
            static bool edit = false;
            ImGui::Checkbox("Edit Mode", &edit);

            // built-in/added
            ImGui::Separator();
            for (int i = 0; i < gp.ColormapData.Count; ++i) {
                ImGui::PushID(i);
                int size = gp.ColormapData.GetKeyCount(i);
                bool selected = i == gp.Style.Colormap;

                const char* name = GetColormapName(i);
                if (!selected)
                    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.25f);
                if (ImGui::Button(name, ImVec2(100, 0))) {
                    gp.Style.Colormap = i;
                    BustItemCache();
                }
                if (!selected)
                    ImGui::PopStyleVar();
                ImGui::SameLine();
                ImGui::BeginGroup();
                if (edit) {
                    for (int c = 0; c < size; ++c) {
                        ImGui::PushID(c);
                        ImVec4 col4 = ImGui::ColorConvertU32ToFloat4(gp.ColormapData.GetKeyColor(i, c));
                        if (ImGui::ColorEdit4("", &col4.x, ImGuiColorEditFlags_NoInputs)) {
                            ImU32 col32 = ImGui::ColorConvertFloat4ToU32(col4);
                            gp.ColormapData.SetKeyColor(i, c, col32);
                            BustItemCache();
                        }
                        if ((c + 1) % 12 != 0 && c != size - 1)
                            ImGui::SameLine();
                        ImGui::PopID();
                    }
                } else {
                    if (ColormapButton("##", ImVec2(-1, 0), i))
                        edit = true;
                }
                ImGui::EndGroup();
                ImGui::PopID();
            }

            static ImVector<ImVec4> custom;
            if (custom.Size == 0) {
                custom.push_back(ImVec4(1, 0, 0, 1));
                custom.push_back(ImVec4(0, 1, 0, 1));
                custom.push_back(ImVec4(0, 0, 1, 1));
            }
            ImGui::Separator();
            ImGui::BeginGroup();
            static char name[16] = "MyColormap";

            if (ImGui::Button("+", ImVec2((100 - ImGui::GetStyle().ItemSpacing.x) / 2, 0)))
                custom.push_back(ImVec4(0, 0, 0, 1));
            ImGui::SameLine();
            if (ImGui::Button("-", ImVec2((100 - ImGui::GetStyle().ItemSpacing.x) / 2, 0)) && custom.Size > 2)
                custom.pop_back();
            ImGui::SetNextItemWidth(100);
            ImGui::InputText("##Name", name, 16, ImGuiInputTextFlags_CharsNoBlank);
            static bool qual = true;
            ImGui::Checkbox("Qualitative", &qual);
            if (ImGui::Button("Add", ImVec2(100, 0)) && gp.ColormapData.GetIndex(name) == -1)
                AddColormap(name, custom.Data, custom.Size, qual);

            ImGui::EndGroup();
            ImGui::SameLine();
            ImGui::BeginGroup();
            for (int c = 0; c < custom.Size; ++c) {
                ImGui::PushID(c);
                if (ImGui::ColorEdit4("##Col1", &custom[c].x, ImGuiColorEditFlags_NoInputs)) {
                }
                if ((c + 1) % 12 != 0)
                    ImGui::SameLine();
                ImGui::PopID();
            }
            ImGui::EndGroup();

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
}

} // namespace ImPlot3D

//-----------------------------------------------------------------------------
// [SECTION] User Namespace Implementation
//-----------------------------------------------------------------------------

namespace MyImPlot3D {

void StyleSeaborn() {

    ImPlot3DStyle& style = ImPlot3D::GetStyle();

    ImVec4* colors = style.Colors;
    colors[ImPlot3DCol_Line] = IMPLOT3D_AUTO_COL;
    colors[ImPlot3DCol_Fill] = IMPLOT3D_AUTO_COL;
    colors[ImPlot3DCol_MarkerOutline] = IMPLOT3D_AUTO_COL;
    colors[ImPlot3DCol_MarkerFill] = IMPLOT3D_AUTO_COL;
    colors[ImPlot3DCol_FrameBg] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImPlot3DCol_PlotBg] = ImVec4(0.92f, 0.92f, 0.95f, 1.00f);
    colors[ImPlot3DCol_PlotBorder] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImPlot3DCol_LegendBg] = ImVec4(0.92f, 0.92f, 0.95f, 1.00f);
    colors[ImPlot3DCol_LegendBorder] = ImVec4(0.80f, 0.81f, 0.85f, 1.00f);
    colors[ImPlot3DCol_LegendText] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    colors[ImPlot3DCol_TitleText] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    colors[ImPlot3DCol_InlayText] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    colors[ImPlot3DCol_AxisText] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    colors[ImPlot3DCol_AxisGrid] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);

    style.LineWeight = 1.5;
    style.Marker = ImPlot3DMarker_None;
    style.MarkerSize = 4;
    style.MarkerWeight = 1;
    style.FillAlpha = 1.0f;
    style.PlotPadding = ImVec2(12, 12);
    style.LabelPadding = ImVec2(5, 5);
    style.LegendPadding = ImVec2(5, 5);
    style.PlotMinSize = ImVec2(300, 225);
}

} // namespace MyImPlot3D
