#include <imgui.h>

extern "C" {
void __attribute__((visibility("default"))) plugin_example_drawText(ImGuiContext *ctx) {
    ImGui::SetCurrentContext(ctx);

    ImGui::Text("Hello from Plugin");
}
}

