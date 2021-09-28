#include <imgui_imhex_extensions.h>

#include <imgui.h>
#include <imgui_freetype.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>
#undef IMGUI_DEFINE_MATH_OPERATORS

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <string>

#include <imgui_impl_opengl3_loader.h>

namespace ImGui {

    bool IconHyperlink(const char *icon, const char* label, const ImVec2& size_arg, ImGuiButtonFlags flags) {
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems)
            return false;

        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        const ImGuiID id = window->GetID(label);
        ImVec2 label_size = CalcTextSize(icon, NULL, false);
        label_size.x += CalcTextSize(" ", NULL, false).x + CalcTextSize(label, NULL, false).x;

        ImVec2 pos = window->DC.CursorPos;
        ImVec2 size = CalcItemSize(size_arg, label_size.x, label_size.y);

        const ImRect bb(pos, pos + size);
        if (!ItemAdd(bb, id))
            return false;

        if (g.LastItemData.InFlags & ImGuiItemFlags_ButtonRepeat)
            flags |= ImGuiButtonFlags_Repeat;
        bool hovered, held;
        bool pressed = ButtonBehavior(bb, id, &hovered, &held, flags);

        // Render
        const ImU32 col = hovered ? GetColorU32(ImGuiCol_ButtonHovered) : GetColorU32(ImGuiCol_ButtonActive);
        PushStyleColor(ImGuiCol_Text, ImU32(col));

        Text("%s %s", icon, label);
        GetWindowDrawList()->AddLine(ImVec2(pos.x, pos.y + size.y), pos + size, ImU32(col));
        PopStyleColor();

        IMGUI_TEST_ENGINE_ITEM_INFO(id, label, window->DC.LastItemStatusFlags);
        return pressed;
    }

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

        if (g.LastItemData.InFlags & ImGuiItemFlags_ButtonRepeat)
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

        if (g.LastItemData.InFlags & ImGuiItemFlags_ButtonRepeat)
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

        if (g.LastItemData.InFlags & ImGuiItemFlags_ButtonRepeat)
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
            BeginDisabled();
            widgets();
            EndDisabled();
        } else {
            widgets();
        }

    }

    void TextSpinner(const char* label) {
        ImGui::Text("[%c] %s", "|/-\\"[ImU32(ImGui::GetTime() * 20) % 4], label);
    }

    void Header(const char *label, bool firstEntry) {
        if (!firstEntry)
            ImGui::NewLine();
        ImGui::TextUnformatted(label);
        ImGui::Separator();
    }

    void HeaderColored(const char *label, ImColor color, bool firstEntry) {
        if (!firstEntry)
            ImGui::NewLine();
        ImGui::TextColored(color, "%s", label);
        ImGui::Separator();
    }

    void InfoTooltip(const char *text) {
        static double lastMoveTime;
        static ImGuiID lastHoveredID;

        double currTime = ImGui::GetTime();
        ImGuiID hoveredID = ImGui::GetHoveredID();

        if (IsItemHovered() && (currTime - lastMoveTime) >= 0.5 && hoveredID == lastHoveredID) {
            BeginTooltip();
            TextUnformatted(text);
            EndTooltip();
        }

        if (hoveredID != lastHoveredID) {
            lastMoveTime = currTime;
        }
        lastHoveredID = hoveredID;
    }

    ImU32 GetCustomColorU32(ImGuiCustomCol idx, float alpha_mul) {
        auto& customData = *static_cast<ImHexCustomData*>(GImGui->IO.UserData);
        ImVec4 c = customData.Colors[idx];
        c.w *= GImGui->Style.Alpha * alpha_mul;
        return ColorConvertFloat4ToU32(c);
    }

    ImVec4 GetCustomColorVec4(ImGuiCustomCol idx, float alpha_mul) {
        auto& customData = *static_cast<ImHexCustomData*>(GImGui->IO.UserData);
        ImVec4 c = customData.Colors[idx];
        c.w *= GImGui->Style.Alpha * alpha_mul;
        return c;
    }

    void StyleCustomColorsDark() {
        auto &colors = static_cast<ImHexCustomData*>(GImGui->IO.UserData)->Colors;

        colors[ImGuiCustomCol_DescButton]           = ImColor(20, 20, 20);
        colors[ImGuiCustomCol_DescButtonHovered]    = ImColor(40, 40, 40);
        colors[ImGuiCustomCol_DescButtonActive]     = ImColor(60, 60, 60);

        colors[ImGuiCustomCol_ToolbarGray]          = ImColor(230, 230, 230);
        colors[ImGuiCustomCol_ToolbarRed]           = ImColor(231, 76, 60);
        colors[ImGuiCustomCol_ToolbarYellow]        = ImColor(241, 196, 15);
        colors[ImGuiCustomCol_ToolbarGreen]         = ImColor(56, 139, 66);
        colors[ImGuiCustomCol_ToolbarBlue]          = ImColor(6, 83, 155);
        colors[ImGuiCustomCol_ToolbarPurple]        = ImColor(103, 42, 120);
        colors[ImGuiCustomCol_ToolbarBrown]         = ImColor(219, 179, 119);

        colors[ImGuiCustomCol_Highlight]            = ImColor(77, 198, 155);
    }

    void StyleCustomColorsLight() {
        auto &colors = static_cast<ImHexCustomData*>(GImGui->IO.UserData)->Colors;

        colors[ImGuiCustomCol_DescButton]           = ImColor(230, 230, 230);
        colors[ImGuiCustomCol_DescButtonHovered]    = ImColor(210, 210, 210);
        colors[ImGuiCustomCol_DescButtonActive]     = ImColor(190, 190, 190);

        colors[ImGuiCustomCol_ToolbarGray]          = ImColor(25, 25, 25);
        colors[ImGuiCustomCol_ToolbarRed]           = ImColor(231, 76, 60);
        colors[ImGuiCustomCol_ToolbarYellow]        = ImColor(241, 196, 15);
        colors[ImGuiCustomCol_ToolbarGreen]         = ImColor(56, 139, 66);
        colors[ImGuiCustomCol_ToolbarBlue]          = ImColor(6, 83, 155);
        colors[ImGuiCustomCol_ToolbarPurple]        = ImColor(103, 42, 120);
        colors[ImGuiCustomCol_ToolbarBrown]         = ImColor(219, 179, 119);

        colors[ImGuiCustomCol_Highlight]            = ImColor(41, 151, 112);
    }

    void StyleCustomColorsClassic() {
        auto &colors = static_cast<ImHexCustomData*>(GImGui->IO.UserData)->Colors;

        colors[ImGuiCustomCol_DescButton]           = ImColor(40, 40, 80);
        colors[ImGuiCustomCol_DescButtonHovered]    = ImColor(60, 60, 100);
        colors[ImGuiCustomCol_DescButtonActive]     = ImColor(80, 80, 120);

        colors[ImGuiCustomCol_ToolbarGray]          = ImColor(230, 230, 230);
        colors[ImGuiCustomCol_ToolbarRed]           = ImColor(231, 76, 60);
        colors[ImGuiCustomCol_ToolbarYellow]        = ImColor(241, 196, 15);
        colors[ImGuiCustomCol_ToolbarGreen]         = ImColor(56, 139, 66);
        colors[ImGuiCustomCol_ToolbarBlue]          = ImColor(6, 83, 155);
        colors[ImGuiCustomCol_ToolbarPurple]        = ImColor(103, 42, 120);
        colors[ImGuiCustomCol_ToolbarBrown]         = ImColor(219, 179, 119);

        colors[ImGuiCustomCol_Highlight]            = ImColor(77, 198, 155);
    }

    Texture LoadImageFromPath(const char *path) {
        int imageWidth = 0;
        int imageHeight = 0;

        unsigned char* imageData = stbi_load(path, &imageWidth, &imageHeight, nullptr, 4);
        if (imageData == nullptr)
            return { nullptr, -1, -1 };


        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        #if defined(GL_UNPACK_ROW_LENGTH)
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        #endif
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, imageWidth, imageHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, imageData);
        stbi_image_free(imageData);

        return { reinterpret_cast<ImTextureID>(static_cast<intptr_t>(texture)), imageWidth, imageHeight };
    }

    Texture LoadImageFromMemory(ImU8 *buffer, int size) {
        int imageWidth = 0;
        int imageHeight = 0;


        unsigned char* imageData = stbi_load_from_memory(buffer, size, &imageWidth, &imageHeight, nullptr, 4);
        if (imageData == nullptr)
            return { nullptr, -1, -1 };

        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        #if defined(GL_UNPACK_ROW_LENGTH)
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        #endif
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, imageWidth, imageHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, imageData);
        stbi_image_free(imageData);

        return { reinterpret_cast<ImTextureID>(static_cast<intptr_t>(texture)), imageWidth, imageHeight };
    }

    void UnloadImage(Texture &texture) {
        if (texture.textureId == nullptr)
            return;

        auto glTextureId = static_cast<GLuint>(reinterpret_cast<intptr_t>(texture.textureId));
        glDeleteTextures(1, &glTextureId);

        texture = { nullptr, 0, 0 };
    }

    void OpenPopupInWindow(const char *window_name, const char *popup_name) {
        if (ImGui::Begin(window_name)) {
            ImGui::OpenPopup(popup_name);
        }
        ImGui::End();
    }


    bool TitleBarButton(const char* label, ImVec2 size_arg) {
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems)
            return false;

        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        const ImGuiID id = window->GetID(label);
        const ImVec2 label_size = CalcTextSize(label, NULL, true);

        ImVec2 pos = window->DC.CursorPos;

        ImVec2 size = CalcItemSize(size_arg, label_size.x + style.FramePadding.x * 2.0f, label_size.y + style.FramePadding.y * 2.0f);

        const ImRect bb(pos, pos + size);
        ItemSize(size, style.FramePadding.y);
        if (!ItemAdd(bb, id))
            return false;

        bool hovered, held;
        bool pressed = ButtonBehavior(bb, id, &hovered, &held);

        // Render
        const ImU32 col = GetColorU32((held && hovered) ? ImGuiCol_ButtonActive : hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
        RenderNavHighlight(bb, id);
        RenderFrame(bb.Min, bb.Max, col, true, style.FrameRounding);
        RenderTextClipped(bb.Min + style.FramePadding * ImVec2(1, 2), bb.Max - style.FramePadding, label, NULL, &label_size, style.ButtonTextAlign, &bb);

        // Automatically close popups
        //if (pressed && !(flags & ImGuiButtonFlags_DontClosePopups) && (window->Flags & ImGuiWindowFlags_Popup))
        //    CloseCurrentPopup();

        IMGUI_TEST_ENGINE_ITEM_INFO(id, label, window->DC.LastItemStatusFlags);
        return pressed;
    }

    bool ToolBarButton(const char* symbol, ImVec4 color, ImVec2 size_arg) {
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems)
            return false;

        color.w = 1.0F;

        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        const ImGuiID id = window->GetID(symbol);
        const ImVec2 label_size = CalcTextSize(symbol, NULL, true);

        ImVec2 pos = window->DC.CursorPos;

        ImVec2 size = CalcItemSize(size_arg, label_size.x + style.FramePadding.x * 2.0f, label_size.y + style.FramePadding.y * 2.0f);

        const ImRect bb(pos, pos + size);
        ItemSize(size, style.FramePadding.y);
        if (!ItemAdd(bb, id))
            return false;

        bool hovered, held;
        bool pressed = ButtonBehavior(bb, id, &hovered, &held);

        PushStyleColor(ImGuiCol_Text, color);

        // Render
        const ImU32 col = GetColorU32((held && hovered) ? ImGuiCol_ScrollbarGrabActive : hovered ? ImGuiCol_ScrollbarGrabHovered : ImGuiCol_MenuBarBg);
        RenderNavHighlight(bb, id);
        RenderFrame(bb.Min, bb.Max, col, false, style.FrameRounding);
        RenderTextClipped(bb.Min + style.FramePadding * ImVec2(1, 2), bb.Max - style.FramePadding, symbol, NULL, &label_size, style.ButtonTextAlign, &bb);

        PopStyleColor();

        // Automatically close popups
        //if (pressed && !(flags & ImGuiButtonFlags_DontClosePopups) && (window->Flags & ImGuiWindowFlags_Popup))
        //    CloseCurrentPopup();

        IMGUI_TEST_ENGINE_ITEM_INFO(id, label, window->DC.LastItemStatusFlags);
        return pressed;
    }

}