#pragma once

#include <functional>

#include <imgui.h>

namespace ImGui {

    struct Texture {
        ImTextureID textureId;
        int width, height;

        [[nodiscard]]
        constexpr bool valid() const noexcept {
            return this->textureId != nullptr;
        }

        [[nodiscard]]
        constexpr operator ImTextureID() {
            return this->textureId;
        }

        [[nodiscard]]
        auto size() const noexcept {
            return ImVec2(this->width, this->height);
        }
    };

    bool IconHyperlink(const char *icon, const char* label, const ImVec2& size_arg = ImVec2(0, 0), ImGuiButtonFlags flags = 0);
    bool Hyperlink(const char* label, const ImVec2& size_arg = ImVec2(0, 0), ImGuiButtonFlags flags = 0);
    bool BulletHyperlink(const char* label, const ImVec2& size_arg = ImVec2(0, 0), ImGuiButtonFlags flags = 0);
    bool DescriptionButton(const char* label, const char* description, const ImVec2& size_arg = ImVec2(0, 0), ImGuiButtonFlags flags = 0);

    void UnderlinedText(const char* label, ImColor color, const ImVec2& size_arg = ImVec2(0, 0));

    void Disabled(const std::function<void()> &widgets, bool disabled);
    void TextSpinner(const char* label);

    void Header(const char *label, bool firstEntry = false);

    void InfoTooltip(const char *text);

    inline bool HasSecondPassed() {
        return static_cast<ImU32>(ImGui::GetTime() * 100) % 100 <= static_cast<ImU32>(ImGui::GetIO().DeltaTime * 100);
    }

    Texture LoadImageFromPath(const char *path);
    Texture LoadImageFromMemory(ImU8 *buffer, int size);
    void UnloadImage(Texture &texture);

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