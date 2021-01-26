#include <imgui_imhex_extensions.h>

#include <imgui.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>
#undef IMGUI_DEFINE_MATH_OPERATORS

namespace ImGui {

    bool Hyperlink(const char* label, const ImVec2& size_arg, ImGuiButtonFlags flags)
    {
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
        const ImColor col = (hovered && !pressed) ? ImColor(0.5F, 0.5F, 0.9F, 1.0F) : (pressed ? ImColor(0.6F, 0.6F, 1.0F, 1.0F) : ImColor(0.4F, 0.4F, 0.8F, 1.0F));
        PushStyleColor(ImGuiCol_Text, ImU32(col));
        TextEx(label, NULL, ImGuiTextFlags_NoWidthForLargeClippedText); // Skip formatting
        GetOverlayDrawList()->AddLine(ImVec2(pos.x, pos.y + size.y), pos + size, ImU32(col));
        PopStyleColor();

        IMGUI_TEST_ENGINE_ITEM_INFO(id, label, window->DC.LastItemStatusFlags);
        return pressed;
    }

}