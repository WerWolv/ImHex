#pragma once

#include <functional>

#include <imgui.h>

namespace ImGui {

    bool Hyperlink(const char* label, const ImVec2& size_arg = ImVec2(0, 0), ImGuiButtonFlags flags = 0);
    bool BulletHyperlink(const char* label, const ImVec2& size_arg = ImVec2(0, 0), ImGuiButtonFlags flags = 0);
    bool DescriptionButton(const char* label, const char* description, const ImVec2& size_arg = ImVec2(0, 0), ImGuiButtonFlags flags = 0);

    void UnderlinedText(const char* label, ImColor color, const ImVec2& size_arg = ImVec2(0, 0));

    void Disabled(const std::function<void()> &widgets, bool disabled);
    void TextSpinner(const char* label);

    void Header(const char *label, bool firstEntry = false);

    inline bool HasSecondPassed() {
        return static_cast<ImU32>(ImGui::GetTime() * 100) % 100 <= static_cast<ImU32>(ImGui::GetIO().DeltaTime * 100);
    }

    std::tuple<ImTextureID, int, int> LoadImageFromPath(const char *path);
    void UnloadImage(ImTextureID texture);

    enum ImGuiCustomCol {
        ImGuiCustomCol_DescButton,
        ImGuiCustomCol_DescButtonHovered,
        ImGuiCustomCol_DescButtonActive,
        ImGuiCustomCol_COUNT
    };

    struct ImHexCustomData {
        ImVec4 Colors[ImGuiCustomCol_COUNT];
    };

    ImU32 GetCustomColorU32(ImGuiCustomCol idx, float alpha_mul = 1.0F);

    void StyleCustomColorsDark();
    void StyleCustomColorsLight();
    void StyleCustomColorsClassic();
}