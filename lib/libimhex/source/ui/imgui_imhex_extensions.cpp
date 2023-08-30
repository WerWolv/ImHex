#include <hex/ui/imgui_imhex_extensions.h>

#include <imgui.h>
#include <imgui_internal.h>
#undef IMGUI_DEFINE_MATH_OPERATORS

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <string>

#include <imgui_impl_opengl3_loader.h>

#include <hex/api/imhex_api.hpp>

#include <fonts/codicons_font.h>

namespace ImGui {

    Texture::Texture(const ImU8 *buffer, int size, int width, int height) {
        unsigned char *imageData = stbi_load_from_memory(buffer, size, &this->m_width, &this->m_height, nullptr, 4);
        if (imageData == nullptr) {
            if (width * height * 4 > size)
                return;

            imageData = (unsigned char*) STBI_MALLOC(size);
            std::memcpy(imageData, buffer, size);
            this->m_width = width;
            this->m_height = height;
        }
        if (imageData == nullptr)
            return;

        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        #if defined(GL_UNPACK_ROW_LENGTH)
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        #endif

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, this->m_width, this->m_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, imageData);
        stbi_image_free(imageData);

        this->m_textureId = reinterpret_cast<ImTextureID>(static_cast<intptr_t>(texture));
    }

    Texture::Texture(std::span<const std::byte> bytes, int width, int height) : Texture(reinterpret_cast<const ImU8*>(bytes.data()), bytes.size(), width, height) { }

    Texture::Texture(const char *path) {
        unsigned char *imageData = stbi_load(path, &this->m_width, &this->m_height, nullptr, 4);
        if (imageData == nullptr)
            return;

        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        #if defined(GL_UNPACK_ROW_LENGTH)
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        #endif

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, this->m_width, this->m_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, imageData);
        stbi_image_free(imageData);

        this->m_textureId = reinterpret_cast<ImTextureID>(static_cast<intptr_t>(texture));
    }

    Texture::Texture(unsigned int texture, int width, int height) : m_textureId(reinterpret_cast<ImTextureID>(static_cast<intptr_t>(texture))), m_width(width), m_height(height) {

    }

    Texture::Texture(Texture&& other) noexcept {
        glDeleteTextures(1, reinterpret_cast<GLuint*>(&this->m_textureId));
        this->m_textureId = other.m_textureId;
        this->m_width = other.m_width;
        this->m_height = other.m_height;

        other.m_textureId = nullptr;
    }

    Texture& Texture::operator=(Texture&& other) noexcept {
        glDeleteTextures(1, reinterpret_cast<GLuint*>(&this->m_textureId));
        this->m_textureId = other.m_textureId;
        this->m_width = other.m_width;
        this->m_height = other.m_height;

        other.m_textureId = nullptr;
        
        return *this;
    }

    Texture::~Texture() {
        if (this->m_textureId == nullptr)
            return;

        auto glTextureId = static_cast<GLuint>(reinterpret_cast<intptr_t>(this->m_textureId));
        glDeleteTextures(1, &glTextureId);
    }

    int UpdateStringSizeCallback(ImGuiInputTextCallbackData *data) {
        if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
            auto &string = *static_cast<std::string *>(data->UserData);

            string.resize(data->BufTextLen);
            data->Buf = string.data();
        }

        return 0;
    }

    bool IconHyperlink(const char *icon, const char *label, const ImVec2 &size_arg, ImGuiButtonFlags flags) {
        ImGuiWindow *window = GetCurrentWindow();
        if (window->SkipItems)
            return false;

        ImGuiContext &g         = *GImGui;
        const ImGuiID id        = window->GetID(label);
        ImVec2 label_size       = CalcTextSize(icon, nullptr, false);
        label_size.x += CalcTextSize(" ", nullptr, false).x + CalcTextSize(label, nullptr, false).x;

        ImVec2 pos  = window->DC.CursorPos;
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

    bool Hyperlink(const char *label, const ImVec2 &size_arg, ImGuiButtonFlags flags) {
        ImGuiWindow *window = GetCurrentWindow();
        if (window->SkipItems)
            return false;

        ImGuiContext &g         = *GImGui;
        const ImGuiID id        = window->GetID(label);
        const ImVec2 label_size = CalcTextSize(label, nullptr, true);

        ImVec2 pos  = window->DC.CursorPos;
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
        TextEx(label, nullptr, ImGuiTextFlags_NoWidthForLargeClippedText);    // Skip formatting
        GetWindowDrawList()->AddLine(ImVec2(pos.x, pos.y + size.y), pos + size, ImU32(col));
        PopStyleColor();

        IMGUI_TEST_ENGINE_ITEM_INFO(id, label, window->DC.LastItemStatusFlags);
        return pressed;
    }

    bool BulletHyperlink(const char *label, const ImVec2 &size_arg, ImGuiButtonFlags flags) {
        ImGuiWindow *window = GetCurrentWindow();
        if (window->SkipItems)
            return false;

        ImGuiContext &g         = *GImGui;
        const ImGuiStyle &style = g.Style;
        const ImGuiID id        = window->GetID(label);
        const ImVec2 label_size = CalcTextSize(label, nullptr, true);

        ImVec2 pos  = window->DC.CursorPos;
        ImVec2 size = CalcItemSize(size_arg, label_size.x, label_size.y) + ImVec2(g.FontSize + style.FramePadding.x * 2, 0.0f);

        const ImRect bb(pos, pos + size);
        ItemSize(size, 0);
        if (!ItemAdd(bb, id))
            return false;

        if (g.LastItemData.InFlags & ImGuiItemFlags_ButtonRepeat)
            flags |= ImGuiButtonFlags_Repeat;
        bool hovered, held;
        bool pressed = ButtonBehavior(bb, id, &hovered, &held, flags);

        // Render
        const ImU32 col = hovered ? GetColorU32(ImGuiCol_ButtonHovered) : GetColorU32(ImGuiCol_ButtonActive);
        PushStyleColor(ImGuiCol_Text, ImU32(col));
        RenderBullet(window->DrawList, bb.Min + ImVec2(style.FramePadding.x, g.FontSize * 0.5f), col);
        RenderText(bb.Min + ImVec2(g.FontSize * 0.5 + style.FramePadding.x, 0.0f), label, nullptr, false);
        GetWindowDrawList()->AddLine(bb.Min + ImVec2(g.FontSize * 0.5 + style.FramePadding.x, size.y), pos + size - ImVec2(g.FontSize * 0.5 + style.FramePadding.x, 0), ImU32(col));
        PopStyleColor();

        IMGUI_TEST_ENGINE_ITEM_INFO(id, label, window->DC.LastItemStatusFlags);
        return pressed;
    }

    bool DescriptionButton(const char *label, const char *description, const ImVec2 &size_arg, ImGuiButtonFlags flags) {
        ImGuiWindow *window = GetCurrentWindow();
        if (window->SkipItems)
            return false;

        ImGuiContext &g         = *GImGui;
        const ImGuiStyle &style = g.Style;
        const ImGuiID id        = window->GetID(label);
        const ImVec2 text_size  = CalcTextSize((std::string(label) + "\n  " + std::string(description)).c_str(), nullptr, true);
        const ImVec2 label_size = CalcTextSize(label, nullptr, true);

        ImVec2 pos = window->DC.CursorPos;
        if ((flags & ImGuiButtonFlags_AlignTextBaseLine) && style.FramePadding.y < window->DC.CurrLineTextBaseOffset)    // Try to vertically align buttons that are smaller/have no padding so that text baseline matches (bit hacky, since it shouldn't be a flag)
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
        const ImU32 col = GetCustomColorU32((held && hovered) ? ImGuiCustomCol_DescButtonActive : hovered ? ImGuiCustomCol_DescButtonHovered
                                                                                                          : ImGuiCustomCol_DescButton);
        RenderNavHighlight(bb, id);
        RenderFrame(bb.Min, bb.Max, col, true, style.FrameRounding);
        PushStyleColor(ImGuiCol_Text, GetColorU32(ImGuiCol_ButtonActive));
        RenderTextWrapped(bb.Min + style.FramePadding * 2, label, nullptr, CalcWrapWidthForPos(window->DC.CursorPos, window->DC.TextWrapPos));
        PopStyleColor();
        PushStyleColor(ImGuiCol_Text, GetColorU32(ImGuiCol_Text));
        RenderTextClipped(bb.Min + style.FramePadding * 2 + ImVec2(style.FramePadding.x * 2, label_size.y), bb.Max - style.FramePadding, description, nullptr, &text_size, style.ButtonTextAlign, &bb);
        PopStyleColor();

        ImGui::PopStyleVar();

        // Automatically close popups
        // if (pressed && !(flags & ImGuiButtonFlags_DontClosePopups) && (window->Flags & ImGuiWindowFlags_Popup))
        //    CloseCurrentPopup();

        IMGUI_TEST_ENGINE_ITEM_INFO(id, label, window->DC.LastItemStatusFlags);
        return pressed;
    }

    void HelpHover(const char *text) {
        const auto iconColor = ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive);

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, ImGui::GetStyle().FramePadding.y));

        ImGui::PushStyleColor(ImGuiCol_Text, iconColor);
        ImGui::Button(ICON_VS_INFO);
        ImGui::PopStyleColor();

        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetNextWindowSizeConstraints(ImVec2(ImGui::GetTextLineHeight() * 25, 0), ImVec2(ImGui::GetTextLineHeight() * 25, FLT_MAX));
            ImGui::BeginTooltip();
            ImGui::TextFormattedWrapped("{}", text);
            ImGui::EndTooltip();
        }

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);
    }

    void UnderlinedText(const char *label, ImColor color, const ImVec2 &size_arg) {
        ImGuiWindow *window = GetCurrentWindow();

        const ImVec2 label_size = CalcTextSize(label, nullptr, true);

        ImVec2 pos  = window->DC.CursorPos;
        ImVec2 size = CalcItemSize(size_arg, label_size.x, label_size.y);

        PushStyleColor(ImGuiCol_Text, ImU32(color));
        TextEx(label, nullptr, ImGuiTextFlags_NoWidthForLargeClippedText);    // Skip formatting
        GetWindowDrawList()->AddLine(ImVec2(pos.x, pos.y + size.y), pos + size, ImU32(color));
        PopStyleColor();
    }

    void TextSpinner(const char *label) {
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
        ImGui::TextFormattedColored(color, "{}", label);
        ImGui::Separator();
    }

    bool InfoTooltip(const char *text) {
        static double lastMoveTime;
        static ImGuiID lastHoveredID;

        double currTime   = ImGui::GetTime();
        ImGuiID hoveredID = ImGui::GetHoveredID();

        bool result = false;
        if (IsItemHovered() && (currTime - lastMoveTime) >= 0.5 && hoveredID == lastHoveredID) {
            if (!std::string_view(text).empty()) {
                BeginTooltip();
                TextUnformatted(text);
                EndTooltip();
            }

            result = true;
        }

        if (hoveredID != lastHoveredID) {
            lastMoveTime = currTime;
        }
        lastHoveredID = hoveredID;

        return result;
    }

    ImU32 GetCustomColorU32(ImGuiCustomCol idx, float alpha_mul) {
        auto &customData = *static_cast<ImHexCustomData *>(GImGui->IO.UserData);
        ImVec4 c         = customData.Colors[idx];
        c.w *= GImGui->Style.Alpha * alpha_mul;
        return ColorConvertFloat4ToU32(c);
    }

    ImVec4 GetCustomColorVec4(ImGuiCustomCol idx, float alpha_mul) {
        auto &customData = *static_cast<ImHexCustomData *>(GImGui->IO.UserData);
        ImVec4 c         = customData.Colors[idx];
        c.w *= GImGui->Style.Alpha * alpha_mul;
        return c;
    }

    float GetCustomStyleFloat(ImGuiCustomStyle idx) {
        auto &customData = *static_cast<ImHexCustomData *>(GImGui->IO.UserData);

        switch (idx) {
            case ImGuiCustomStyle_WindowBlur:
                return customData.styles.WindowBlur;
            default:
                return 0.0f;
        }
    }

    ImVec2 GetCustomStyleVec2(ImGuiCustomStyle idx) {
        switch (idx) {
            default:
                return { };
        }
    }

    void StyleCustomColorsDark() {
        auto &colors = static_cast<ImHexCustomData *>(GImGui->IO.UserData)->Colors;

        colors[ImGuiCustomCol_DescButton]        = ImColor(20, 20, 20);
        colors[ImGuiCustomCol_DescButtonHovered] = ImColor(40, 40, 40);
        colors[ImGuiCustomCol_DescButtonActive]  = ImColor(60, 60, 60);

        colors[ImGuiCustomCol_ToolbarGray]   = ImColor(230, 230, 230);
        colors[ImGuiCustomCol_ToolbarRed]    = ImColor(231, 76, 60);
        colors[ImGuiCustomCol_ToolbarYellow] = ImColor(241, 196, 15);
        colors[ImGuiCustomCol_ToolbarGreen]  = ImColor(56, 139, 66);
        colors[ImGuiCustomCol_ToolbarBlue]   = ImColor(6, 83, 155);
        colors[ImGuiCustomCol_ToolbarPurple] = ImColor(103, 42, 120);
        colors[ImGuiCustomCol_ToolbarBrown]  = ImColor(219, 179, 119);

        colors[ImGuiCustomCol_Highlight] = ImColor(77, 198, 155);

        colors[ImGuiCustomCol_IEEEToolSign] = ImColor(93, 93, 127);
        colors[ImGuiCustomCol_IEEEToolExp]  = ImColor(93, 127,  93);
        colors[ImGuiCustomCol_IEEEToolMantissa]  = ImColor(127, 93,  93);
    }

    void StyleCustomColorsLight() {
        auto &colors = static_cast<ImHexCustomData *>(GImGui->IO.UserData)->Colors;

        colors[ImGuiCustomCol_DescButton]        = ImColor(230, 230, 230);
        colors[ImGuiCustomCol_DescButtonHovered] = ImColor(210, 210, 210);
        colors[ImGuiCustomCol_DescButtonActive]  = ImColor(190, 190, 190);

        colors[ImGuiCustomCol_ToolbarGray]   = ImColor(25, 25, 25);
        colors[ImGuiCustomCol_ToolbarRed]    = ImColor(231, 76, 60);
        colors[ImGuiCustomCol_ToolbarYellow] = ImColor(241, 196, 15);
        colors[ImGuiCustomCol_ToolbarGreen]  = ImColor(56, 139, 66);
        colors[ImGuiCustomCol_ToolbarBlue]   = ImColor(6, 83, 155);
        colors[ImGuiCustomCol_ToolbarPurple] = ImColor(103, 42, 120);
        colors[ImGuiCustomCol_ToolbarBrown]  = ImColor(219, 179, 119);

        colors[ImGuiCustomCol_Highlight] = ImColor(41, 151, 112);

        colors[ImGuiCustomCol_IEEEToolSign] = ImColor(187, 187, 255);
        colors[ImGuiCustomCol_IEEEToolExp]  = ImColor(187, 255,  187);
        colors[ImGuiCustomCol_IEEEToolMantissa]  = ImColor(255, 187,187);
    }

    void StyleCustomColorsClassic() {
        auto &colors = static_cast<ImHexCustomData *>(GImGui->IO.UserData)->Colors;

        colors[ImGuiCustomCol_DescButton]        = ImColor(40, 40, 80);
        colors[ImGuiCustomCol_DescButtonHovered] = ImColor(60, 60, 100);
        colors[ImGuiCustomCol_DescButtonActive]  = ImColor(80, 80, 120);

        colors[ImGuiCustomCol_ToolbarGray]   = ImColor(230, 230, 230);
        colors[ImGuiCustomCol_ToolbarRed]    = ImColor(231, 76, 60);
        colors[ImGuiCustomCol_ToolbarYellow] = ImColor(241, 196, 15);
        colors[ImGuiCustomCol_ToolbarGreen]  = ImColor(56, 139, 66);
        colors[ImGuiCustomCol_ToolbarBlue]   = ImColor(6, 83, 155);
        colors[ImGuiCustomCol_ToolbarPurple] = ImColor(103, 42, 120);
        colors[ImGuiCustomCol_ToolbarBrown]  = ImColor(219, 179, 119);

        colors[ImGuiCustomCol_Highlight] = ImColor(77, 198, 155);
        colors[ImGuiCustomCol_IEEEToolSign] = ImColor(93, 93, 127);
        colors[ImGuiCustomCol_IEEEToolExp]  = ImColor(93, 127,  93);
        colors[ImGuiCustomCol_IEEEToolMantissa]  = ImColor(127, 93,  93);
    }

    void OpenPopupInWindow(const char *window_name, const char *popup_name) {
        if (ImGui::Begin(window_name)) {
            ImGui::OpenPopup(popup_name);
        }
        ImGui::End();
    }


    bool TitleBarButton(const char *label, ImVec2 size_arg) {
        ImGuiWindow *window = GetCurrentWindow();
        if (window->SkipItems)
            return false;

        ImGuiContext &g         = *GImGui;
        const ImGuiStyle &style = g.Style;
        const ImGuiID id        = window->GetID(label);
        const ImVec2 label_size = CalcTextSize(label, nullptr, true);

        ImVec2 pos = window->DC.CursorPos;

        ImVec2 size = CalcItemSize(size_arg, label_size.x + style.FramePadding.x * 2.0f, label_size.y + style.FramePadding.y * 2.0f);

        const ImRect bb(pos, pos + size);
        ItemSize(size, style.FramePadding.y);
        if (!ItemAdd(bb, id))
            return false;

        bool hovered, held;
        bool pressed = ButtonBehavior(bb, id, &hovered, &held);

        // Render
        const ImU32 col = GetColorU32((held && hovered) ? ImGuiCol_ButtonActive : hovered ? ImGuiCol_ButtonHovered
                                                                                          : ImGuiCol_Button);
        RenderNavHighlight(bb, id);
        RenderFrame(bb.Min, bb.Max, col, true, style.FrameRounding);
        RenderTextClipped(bb.Min + style.FramePadding, bb.Max - style.FramePadding, label, nullptr, &label_size, style.ButtonTextAlign, &bb);

        // Automatically close popups
        // if (pressed && !(flags & ImGuiButtonFlags_DontClosePopups) && (window->Flags & ImGuiWindowFlags_Popup))
        //    CloseCurrentPopup();

        IMGUI_TEST_ENGINE_ITEM_INFO(id, label, window->DC.LastItemStatusFlags);
        return pressed;
    }

    bool ToolBarButton(const char *symbol, ImVec4 color) {
        ImGuiWindow *window = GetCurrentWindow();
        if (window->SkipItems)
            return false;

        color.w = 1.0F;

        ImGuiContext &g         = *GImGui;
        const ImGuiStyle &style = g.Style;
        const ImGuiID id        = window->GetID(symbol);
        const ImVec2 label_size = CalcTextSize(symbol, nullptr, true);

        ImVec2 pos = window->DC.CursorPos;

        ImVec2 size = CalcItemSize(ImVec2(1, 1) * ImGui::GetCurrentWindow()->MenuBarHeight(), label_size.x + style.FramePadding.x * 2.0f, label_size.y + style.FramePadding.y * 2.0f);

        const ImRect bb(pos, pos + size);
        ItemSize(size, style.FramePadding.y);
        if (!ItemAdd(bb, id))
            return false;

        bool hovered, held;
        bool pressed = ButtonBehavior(bb, id, &hovered, &held);

        PushStyleColor(ImGuiCol_Text, color);

        // Render
        const ImU32 col = GetColorU32((held && hovered) ? ImGuiCol_ScrollbarGrabActive : hovered ? ImGuiCol_ScrollbarGrabHovered
                                                                                                 : ImGuiCol_MenuBarBg);
        RenderNavHighlight(bb, id);
        RenderFrame(bb.Min, bb.Max, col, false, style.FrameRounding);
        RenderTextClipped(bb.Min + style.FramePadding, bb.Max - style.FramePadding, symbol, nullptr, &label_size, style.ButtonTextAlign, &bb);

        PopStyleColor();

        // Automatically close popups
        // if (pressed && !(flags & ImGuiButtonFlags_DontClosePopups) && (window->Flags & ImGuiWindowFlags_Popup))
        //    CloseCurrentPopup();

        IMGUI_TEST_ENGINE_ITEM_INFO(id, label, window->DC.LastItemStatusFlags);
        return pressed;
    }

    bool IconButton(const char *symbol, ImVec4 color, ImVec2 size_arg) {
        ImGuiWindow *window = GetCurrentWindow();
        if (window->SkipItems)
            return false;

        color.w = 1.0F;

        ImGuiContext &g         = *GImGui;
        const ImGuiStyle &style = g.Style;
        const ImGuiID id        = window->GetID(symbol);
        const ImVec2 label_size = CalcTextSize(symbol, nullptr, true);

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
        const ImU32 col = GetColorU32((held && hovered) ? ImGuiCol_ButtonActive : hovered ? ImGuiCol_ButtonHovered
                                                                                          : ImGuiCol_Button);
        RenderNavHighlight(bb, id);
        RenderFrame(bb.Min, bb.Max, col, true, style.FrameRounding);
        RenderTextClipped(bb.Min + style.FramePadding, bb.Max - style.FramePadding, symbol, nullptr, &label_size, style.ButtonTextAlign, &bb);

        PopStyleColor();

        // Automatically close popups
        // if (pressed && !(flags & ImGuiButtonFlags_DontClosePopups) && (window->Flags & ImGuiWindowFlags_Popup))
        //    CloseCurrentPopup();

        IMGUI_TEST_ENGINE_ITEM_INFO(id, label, window->DC.LastItemStatusFlags);
        return pressed;
    }

    bool InputIntegerPrefix(const char *label, const char *prefix, void *value, ImGuiDataType type, const char *format, ImGuiInputTextFlags flags) {
        auto window             = ImGui::GetCurrentWindow();
        const ImGuiID id        = window->GetID(label);
        const ImGuiStyle &style = GImGui->Style;


        const ImVec2 label_size = CalcTextSize(label, nullptr, true);
        const ImVec2 frame_size = CalcItemSize(ImVec2(0, 0), CalcTextSize(prefix).x, label_size.y + style.FramePadding.y * 2.0f);
        const ImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + frame_size);

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + frame_size.x);

        char buf[64];
        DataTypeFormatString(buf, IM_ARRAYSIZE(buf), type, value, format);

        bool value_changed = false;
        if (InputTextEx(label, nullptr, buf, IM_ARRAYSIZE(buf), ImVec2(CalcItemWidth() - frame_size.x, label_size.y + style.FramePadding.y * 2.0f), flags))
            value_changed = DataTypeApplyFromText(buf, type, value, format);

        if (value_changed)
            MarkItemEdited(GImGui->LastItemData.ID);

        RenderNavHighlight(frame_bb, id);
        RenderFrame(frame_bb.Min, frame_bb.Max, GetColorU32(ImGuiCol_FrameBg), true, style.FrameRounding);

        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.6F);
        RenderText(ImVec2(frame_bb.Min.x + style.FramePadding.x, frame_bb.Min.y + style.FramePadding.y), prefix);
        ImGui::PopStyleVar();

        return value_changed;
    }

    bool InputHexadecimal(const char *label, u32 *value, ImGuiInputTextFlags flags) {
        return InputIntegerPrefix(label, "0x", value, ImGuiDataType_U32, "%lX", flags | ImGuiInputTextFlags_CharsHexadecimal);
    }

    bool InputHexadecimal(const char *label, u64 *value, ImGuiInputTextFlags flags) {
        return InputIntegerPrefix(label, "0x", value, ImGuiDataType_U64, "%llX", flags | ImGuiInputTextFlags_CharsHexadecimal);
    }

    void SmallProgressBar(float fraction, float yOffset) {
        ImGuiWindow *window = GetCurrentWindow();
        if (window->SkipItems)
            return;

        ImGuiContext &g         = *GImGui;
        const ImGuiStyle &style = g.Style;

        ImVec2 pos  = window->DC.CursorPos + ImVec2(0, yOffset);
        ImVec2 size = CalcItemSize(ImVec2(100, 5) * hex::ImHexApi::System::getGlobalScale(), 100, g.FontSize + style.FramePadding.y * 2.0f);
        ImRect bb(pos, pos + size);
        ItemSize(size, 0);
        if (!ItemAdd(bb, 0))
            return;

        // Render
        fraction = ImSaturate(fraction);
        RenderFrame(bb.Min, bb.Max, GetColorU32(ImGuiCol_FrameBg), true, style.FrameRounding);
        bb.Expand(ImVec2(-style.FrameBorderSize, -style.FrameBorderSize));
        RenderRectFilledRangeH(window->DrawList, bb, GetColorU32(ImGuiCol_PlotHistogram), 0.0f, fraction, style.FrameRounding);
    }

    bool InputText(const char *label, std::string &buffer, ImGuiInputTextFlags flags) {
        return ImGui::InputText(label, buffer.data(), buffer.size() + 1, ImGuiInputTextFlags_CallbackResize | flags, ImGui::UpdateStringSizeCallback, &buffer);
    }

    bool InputTextIcon(const char *label, const char *icon, std::string &buffer, ImGuiInputTextFlags flags) {
        auto window             = ImGui::GetCurrentWindow();
        const ImGuiID id        = window->GetID(label);
        const ImGuiStyle &style = GImGui->Style;


        const ImVec2 label_size = CalcTextSize(label, nullptr, true);
        const ImVec2 icon_frame_size = CalcTextSize(icon) + style.FramePadding * 2.0f;
        const ImVec2 frame_size = CalcItemSize(ImVec2(0, 0), icon_frame_size.x, label_size.y + style.FramePadding.y * 2.0f);
        const ImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + frame_size);

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + frame_size.x);

        bool value_changed = ImGui::InputTextEx(label, nullptr, buffer.data(), buffer.size() + 1, ImVec2(CalcItemWidth(), label_size.y + style.FramePadding.y * 2.0f), ImGuiInputTextFlags_CallbackResize | flags, ImGui::UpdateStringSizeCallback, &buffer);

        if (value_changed)
            MarkItemEdited(GImGui->LastItemData.ID);

        RenderNavHighlight(frame_bb, id);
        RenderFrame(frame_bb.Min, frame_bb.Max, GetColorU32(ImGuiCol_FrameBg), true, style.FrameRounding);

        RenderFrame(frame_bb.Min, frame_bb.Min + icon_frame_size, GetColorU32(ImGuiCol_TableBorderStrong), true, style.FrameRounding);
        RenderText(ImVec2(frame_bb.Min.x + style.FramePadding.x, frame_bb.Min.y + style.FramePadding.y), icon);

        return value_changed;
    }

    bool InputTextWithHint(const char *label, const char *hint, std::string &buffer, ImGuiInputTextFlags flags) {
        return ImGui::InputTextWithHint(label, hint, buffer.data(), buffer.size() + 1, ImGuiInputTextFlags_CallbackResize | flags, ImGui::UpdateStringSizeCallback, &buffer);
    }

    bool InputText(const char *label, std::u8string &buffer, ImGuiInputTextFlags flags) {
        return ImGui::InputText(label, reinterpret_cast<char *>(buffer.data()), buffer.size() + 1, ImGuiInputTextFlags_CallbackResize | flags, ImGui::UpdateStringSizeCallback, &buffer);
    }

    bool InputTextMultiline(const char *label, std::string &buffer, const ImVec2 &size, ImGuiInputTextFlags flags) {
        return ImGui::InputTextMultiline(label, buffer.data(), buffer.size() + 1, size, ImGuiInputTextFlags_CallbackResize | flags, ImGui::UpdateStringSizeCallback, &buffer);
    }

    bool InputScalarCallback(const char* label, ImGuiDataType data_type, void* p_data, const char* format, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data) {
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems)
            return false;

        ImGuiContext& g = *GImGui;

        if (format == nullptr)
            format = DataTypeGetInfo(data_type)->PrintFmt;

        char buf[64];
        DataTypeFormatString(buf, IM_ARRAYSIZE(buf), data_type, p_data, format);

        bool value_changed = false;
        if ((flags & (ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsScientific)) == 0)
            flags |= ImGuiInputTextFlags_CharsDecimal;
        flags |= ImGuiInputTextFlags_AutoSelectAll;
        flags |= ImGuiInputTextFlags_NoMarkEdited;  // We call MarkItemEdited() ourselves by comparing the actual data rather than the string.

        if (InputText(label, buf, IM_ARRAYSIZE(buf), flags, callback, user_data))
            value_changed = DataTypeApplyFromText(buf, data_type, p_data, format);

        if (value_changed)
            MarkItemEdited(g.LastItemData.ID);

        return value_changed;
    }

    void HideTooltip() {
        char window_name[16];
        ImFormatString(window_name, IM_ARRAYSIZE(window_name), "##Tooltip_%02d", GImGui->TooltipOverrideCount);
        if (ImGuiWindow* window = FindWindowByName(window_name); window != nullptr) {
            if (window->Active)
                window->Hidden = true;
        }
    }


    bool BitCheckbox(const char* label, bool* v) {
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems)
            return false;

        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        const ImGuiID id = window->GetID(label);
        const ImVec2 label_size = CalcTextSize(label, nullptr, true);

        const ImVec2 size = ImVec2(CalcTextSize("0").x + style.FramePadding.x * 2, GetFrameHeight());
        const ImVec2 pos = window->DC.CursorPos;
        const ImRect total_bb(pos, pos + size);
        ItemSize(total_bb, style.FramePadding.y);
        if (!ItemAdd(total_bb, id))
        {
            IMGUI_TEST_ENGINE_ITEM_INFO(id, label, g.LastItemData.StatusFlags | ImGuiItemStatusFlags_Checkable | (*v ? ImGuiItemStatusFlags_Checked : 0));
            return false;
        }

        bool hovered, held;
        bool pressed = ButtonBehavior(total_bb, id, &hovered, &held);
        if (pressed)
        {
            *v = !(*v);
            MarkItemEdited(id);
        }

        const ImRect check_bb(pos, pos + size);
        RenderNavHighlight(total_bb, id);
        RenderFrame(check_bb.Min, check_bb.Max, GetColorU32((held && hovered) ? ImGuiCol_FrameBgActive : hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg), true, style.FrameRounding);

        RenderText(check_bb.Min + style.FramePadding, *v ? "1" : "0");

        ImVec2 label_pos = ImVec2(check_bb.Max.x + style.ItemInnerSpacing.x, check_bb.Min.y + style.FramePadding.y);
        if (label_size.x > 0.0f)
            RenderText(label_pos, label);

        IMGUI_TEST_ENGINE_ITEM_INFO(id, label, g.LastItemData.StatusFlags | ImGuiItemStatusFlags_Checkable | (*v ? ImGuiItemStatusFlags_Checked : 0));
        return pressed;
    }

    bool DimmedButton(const char* label){
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetCustomColorU32(ImGuiCustomCol_DescButtonHovered));
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetCustomColorU32(ImGuiCustomCol_DescButton));
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_ButtonActive));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetCustomColorU32(ImGuiCustomCol_DescButtonActive));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);

        bool res = ImGui::Button(label);

        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar(1);

        return res;
    }

    bool DimmedIconButton(const char *symbol, ImVec4 color, ImVec2 size_arg){
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetCustomColorU32(ImGuiCustomCol_DescButtonHovered));
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetCustomColorU32(ImGuiCustomCol_DescButton));
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_ButtonActive));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetCustomColorU32(ImGuiCustomCol_DescButtonActive));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);

        bool res = IconButton(symbol, color, size_arg);

        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar(1);

        return res;
    }

    bool DimmedIconToggle(const char *icon, bool *v) {
        bool pushed = false;
        bool toggled = false;

        if (*v) {
            ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            pushed = true;
        }

        if (ImGui::DimmedIconButton(icon, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
            *v = !*v;
            toggled = true;
        }

        if (pushed)
            ImGui::PopStyleColor();

        return toggled;
    }

    void TextOverlay(const char *text, ImVec2 pos) {
        const auto textSize = ImGui::CalcTextSize(text);
        const auto textPos  = pos - textSize / 2;
        const auto margin   = ImGui::GetStyle().FramePadding * 2;
        const auto textRect = ImRect(textPos - margin, textPos + textSize + margin);

        auto drawList = ImGui::GetForegroundDrawList();

        drawList->AddRectFilled(textRect.Min, textRect.Max, ImGui::GetColorU32(ImGuiCol_WindowBg) | 0xFF000000);
        drawList->AddRect(textRect.Min, textRect.Max, ImGui::GetColorU32(ImGuiCol_Border));
        drawList->AddText(textPos, ImGui::GetColorU32(ImGuiCol_Text), text);
    }

}