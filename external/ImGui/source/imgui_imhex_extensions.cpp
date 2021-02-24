#include <imgui_imhex_extensions.h>

#include <imgui.h>
#include <imgui_freetype.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>
#undef IMGUI_DEFINE_MATH_OPERATORS

#include <string>

namespace ImGui {

    bool Hyperlink(const char* label, const ImVec2& size_arg, ImGuiButtonFlags flags) {
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems)
            return false;

        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        const ImGuiID id = window->GetID(label);
        const ImVec2 label_size = CalcTextSize(label, NULL, true);

        ImVec2 pos = window->DC.CursorPos;
        ImVec2 size = CalcItemSize(size_arg, label_size.x, label_size.y);

        const ImRect bb(pos, pos + size);
        if (!ItemAdd(bb, id))
            return false;

        if (window->DC.ItemFlags & ImGuiItemFlags_ButtonRepeat)
            flags |= ImGuiButtonFlags_Repeat;
        bool hovered, held;
        bool pressed = ButtonBehavior(bb, id, &hovered, &held, flags);

        // Render
        const ImU32 col = hovered ? GetColorU32(ImGuiCol_ButtonHovered) : GetColorU32(ImGuiCol_ButtonActive);
        PushStyleColor(ImGuiCol_Text, ImU32(col));
        TextEx(label, NULL, ImGuiTextFlags_NoWidthForLargeClippedText); // Skip formatting
        GetWindowDrawList()->AddLine(ImVec2(pos.x, pos.y + size.y), pos + size, ImU32(col));
        PopStyleColor();

        IMGUI_TEST_ENGINE_ITEM_INFO(id, label, window->DC.LastItemStatusFlags);
        return pressed;
    }

    bool BulletHyperlink(const char* label, const ImVec2& size_arg, ImGuiButtonFlags flags) {
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems)
            return false;

        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        const ImGuiID id = window->GetID(label);
        const ImVec2 label_size = CalcTextSize(label, NULL, true);

        ImVec2 pos = window->DC.CursorPos;
        ImVec2 size = CalcItemSize(size_arg, label_size.x, label_size.y) + ImVec2(g.FontSize + style.FramePadding.x * 2, 0.0f);

        const ImRect bb(pos, pos + size);
        if (!ItemAdd(bb, id))
            return false;

        if (window->DC.ItemFlags & ImGuiItemFlags_ButtonRepeat)
            flags |= ImGuiButtonFlags_Repeat;
        bool hovered, held;
        bool pressed = ButtonBehavior(bb, id, &hovered, &held, flags);

        // Render
        const ImU32 col = hovered ? GetColorU32(ImGuiCol_ButtonHovered) : GetColorU32(ImGuiCol_ButtonActive);
        PushStyleColor(ImGuiCol_Text, ImU32(col));
        RenderBullet(window->DrawList, bb.Min + ImVec2(style.FramePadding.x + g.FontSize * 0.5f, g.FontSize * 0.5f), col);
        RenderText(bb.Min + ImVec2(g.FontSize + style.FramePadding.x * 2, 0.0f), label, nullptr, false);
        GetWindowDrawList()->AddLine(bb.Min + ImVec2(style.FramePadding.x, size.y), pos + size, ImU32(col));
        ImGui::NewLine();
        PopStyleColor();

        IMGUI_TEST_ENGINE_ITEM_INFO(id, label, window->DC.LastItemStatusFlags);
        return pressed;
    }

    bool DescriptionButton(const char* label, const char* description, const ImVec2& size_arg, ImGuiButtonFlags flags) {
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems)
            return false;

        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        const ImGuiID id = window->GetID(label);
        const ImVec2 text_size = CalcTextSize((std::string(label) + "\n  " + std::string(description)).c_str(), NULL, true);
        const ImVec2 label_size = CalcTextSize(label, NULL, true);

        ImVec2 pos = window->DC.CursorPos;
        if ((flags & ImGuiButtonFlags_AlignTextBaseLine) && style.FramePadding.y < window->DC.CurrLineTextBaseOffset) // Try to vertically align buttons that are smaller/have no padding so that text baseline matches (bit hacky, since it shouldn't be a flag)
            pos.y += window->DC.CurrLineTextBaseOffset - style.FramePadding.y;
        ImVec2 size = CalcItemSize(size_arg, text_size.x + style.FramePadding.x * 4.0f, text_size.y + style.FramePadding.y * 4.0f);

        const ImRect bb(pos, pos + size);
        ItemSize(size, style.FramePadding.y);
        if (!ItemAdd(bb, id))
            return false;

        if (window->DC.ItemFlags & ImGuiItemFlags_ButtonRepeat)
            flags |= ImGuiButtonFlags_Repeat;
        bool hovered, held;
        bool pressed = ButtonBehavior(bb, id, &hovered, &held, flags);

        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0, 0.5));

        // Render
        const ImU32 col = GetCustomColorU32((held && hovered) ? ImGuiCustomCol_DescButtonActive : hovered ? ImGuiCustomCol_DescButtonHovered : ImGuiCustomCol_DescButton);
        RenderNavHighlight(bb, id);
        RenderFrame(bb.Min, bb.Max, col, true, style.FrameRounding);
        PushStyleColor(ImGuiCol_Text, GetColorU32(ImGuiCol_ButtonActive));
        RenderTextWrapped(bb.Min + style.FramePadding * 2, label, nullptr, CalcWrapWidthForPos(window->DC.CursorPos, window->DC.TextWrapPos));
        PopStyleColor();
        PushStyleColor(ImGuiCol_Text, GetColorU32(ImGuiCol_Text));
        RenderTextClipped(bb.Min + style.FramePadding * 2 + ImVec2(style.FramePadding.x * 2, label_size.y), bb.Max - style.FramePadding, description, NULL, &text_size, style.ButtonTextAlign, &bb);
        PopStyleColor();

        ImGui::PopStyleVar();

        // Automatically close popups
        //if (pressed && !(flags & ImGuiButtonFlags_DontClosePopups) && (window->Flags & ImGuiWindowFlags_Popup))
        //    CloseCurrentPopup();

        IMGUI_TEST_ENGINE_ITEM_INFO(id, label, window->DC.LastItemStatusFlags);
        return pressed;
    }

    void UnderlinedText(const char* label, ImColor color, const ImVec2& size_arg) {
        ImGuiWindow* window = GetCurrentWindow();

        const ImVec2 label_size = CalcTextSize(label, NULL, true);

        ImVec2 pos = window->DC.CursorPos;
        ImVec2 size = CalcItemSize(size_arg, label_size.x, label_size.y);

        PushStyleColor(ImGuiCol_Text, ImU32(color));
        TextEx(label, NULL, ImGuiTextFlags_NoWidthForLargeClippedText); // Skip formatting
        GetWindowDrawList()->AddLine(ImVec2(pos.x, pos.y + size.y), pos + size, ImU32(color));
        PopStyleColor();
    }

    void Disabled(const std::function<void()> &widgets, bool disabled) {
        if (disabled) {
            ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5F);
            widgets();
            ImGui::PopStyleVar();
            ImGui::PopItemFlag();
        } else {
            widgets();
        }
    }

    void TextSpinner(const char* label) {
        ImGui::Text("[%c] %s", "|/-\\"[ImU32(ImGui::GetTime() * 20) % 4], label);
    }

    ImU32 GetCustomColorU32(ImGuiCustomCol idx, float alpha_mul) {
        auto& customData = *static_cast<ImHexCustomData*>(GImGui->IO.UserData);
        ImVec4 c = customData.Colors[idx];
        c.w *= GImGui->Style.Alpha * alpha_mul;
        return ColorConvertFloat4ToU32(c);
    }

    void StyleCustomColorsDark() {
        auto &colors = static_cast<ImHexCustomData*>(GImGui->IO.UserData)->Colors;

        colors[ImGuiCustomCol_DescButton]           = ImColor(20, 20, 20);
        colors[ImGuiCustomCol_DescButtonHovered]    = ImColor(40, 40, 40);
        colors[ImGuiCustomCol_DescButtonActive]     = ImColor(60, 60, 60);
    }

    void StyleCustomColorsLight() {
        auto &colors = static_cast<ImHexCustomData*>(GImGui->IO.UserData)->Colors;

        colors[ImGuiCustomCol_DescButton]           = ImColor(230, 230, 230);
        colors[ImGuiCustomCol_DescButtonHovered]    = ImColor(210, 210, 210);
        colors[ImGuiCustomCol_DescButtonActive]     = ImColor(190, 190, 190);
    }

    void StyleCustomColorsClassic() {
        auto &colors = static_cast<ImHexCustomData*>(GImGui->IO.UserData)->Colors;

        colors[ImGuiCustomCol_DescButton]           = ImColor(40, 40, 80);
        colors[ImGuiCustomCol_DescButtonHovered]    = ImColor(60, 60, 100);
        colors[ImGuiCustomCol_DescButtonActive]     = ImColor(80, 80, 120);
    }

}