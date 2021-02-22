#pragma once

#include <functional>

#include <imgui.h>

namespace ImGui {

    bool Hyperlink(const char* label, const ImVec2& size_arg = ImVec2(0, 0), ImGuiButtonFlags flags = 0);
    bool BulletHyperlink(const char* label, const ImVec2& size_arg = ImVec2(0, 0), ImGuiButtonFlags flags = 0);
    bool DescriptionButton(const char* label, const char* description, const ImVec2& size_arg = ImVec2(0, 0), ImGuiButtonFlags flags = 0);

    void UnderlinedText(const char* label, ImColor color, const ImVec2& size_arg = ImVec2(0, 0));

    void Disabled(std::function<void()> widgets, bool disabled);
    void TextSpinner(const char* label);
}