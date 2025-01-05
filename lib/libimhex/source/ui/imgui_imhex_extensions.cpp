#include <hex/ui/imgui_imhex_extensions.h>

#include <imgui.h>
#include <imgui_internal.h>
#include <implot.h>
#include <implot_internal.h>
#include <cimgui.h>
#include <opengl_support.h>

#undef IMGUI_DEFINE_MATH_OPERATORS

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <lunasvg.h>

#include <set>
#include <string>
#include <algorithm>

#include <hex/api/imhex_api.hpp>

#include <hex/api/task_manager.hpp>
#include <hex/api/theme_manager.hpp>
#include <hex/helpers/logger.hpp>
#include <hex/helpers/utils_macos.hpp>


namespace ImGuiExt {

    using namespace ImGui;

    namespace {

        bool isOpenGLExtensionSupported(const char *name) {
            static std::set<std::string> extensions;

            if (extensions.empty()) {
                GLint extensionCount = 0;
                glGetIntegerv(GL_NUM_EXTENSIONS, &extensionCount);

                for (GLint i = 0; i < extensionCount; i++) {
                    std::string extension = reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, i));
                    extensions.emplace(std::move(extension));
                }
            }

            return extensions.contains(name);
        }

        bool isOpenGLVersionAtLeast(u8 major, u8 minor) {
            static GLint actualMajor = 0, actualMinor = 0;
            if (actualMajor == 0 || actualMinor == 0) {
                glGetIntegerv(GL_MAJOR_VERSION, &actualMajor);
                glGetIntegerv(GL_MINOR_VERSION, &actualMinor);
            }

            return actualMajor > major || (actualMajor == major && actualMinor >= minor);
        }

        constexpr auto getGLFilter(Texture::Filter filter) {
            switch (filter) {
                using enum Texture::Filter;
                case Nearest:
                    return GL_NEAREST;
                case Linear:
                    return GL_LINEAR;
            }

            return GL_NEAREST;
        }

        [[maybe_unused]] GLint getMaxSamples(GLenum target, GLenum format) {
            GLint maxSamples;

            glGetInternalformativ(target, format, GL_SAMPLES, 1, &maxSamples);
            return maxSamples;
        }

        GLuint createTextureFromRGBA8Array(const ImU8 *buffer, int width, int height, Texture::Filter filter) {
            GLuint texture;

            // Generate texture
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_2D, texture);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, getGLFilter(filter));
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, getGLFilter(filter));

            #if defined(GL_UNPACK_ROW_LENGTH)
                glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
            #endif

            // Allocate storage for the texture
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, buffer);

            return texture;
        }

        GLuint createMultisampleTextureFromRGBA8Array(const ImU8 *buffer, int width, int height, Texture::Filter filter) {
            // Create a regular texture from the RGBA8 array
            GLuint texture = createTextureFromRGBA8Array(buffer, width, height, filter);

            if (filter == Texture::Filter::Nearest) {
                return texture;
            }

            if (!isOpenGLVersionAtLeast(3,2)) {
                return texture;
            }

            if (!isOpenGLExtensionSupported("GL_ARB_texture_multisample")) {
                return texture;
            }

            #if defined(GL_TEXTURE_2D_MULTISAMPLE)
                static const auto sampleCount = std::min(static_cast<GLint>(8), getMaxSamples(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8));

                // Generate renderbuffer
                GLuint renderbuffer;
                glGenRenderbuffers(1, &renderbuffer);
                glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
                glRenderbufferStorageMultisample(GL_RENDERBUFFER, sampleCount, GL_DEPTH24_STENCIL8, width, height);

                // Generate framebuffer
                GLuint framebuffer;
                glGenFramebuffers(1, &framebuffer);
                glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

                // Unbind framebuffer on exit
                ON_SCOPE_EXIT {
                    glBindFramebuffer(GL_FRAMEBUFFER, 0);
                };

                // Attach texture to color attachment 0
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, texture, 0);

                // Attach renderbuffer to depth-stencil attachment
                glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, renderbuffer);

                // Check framebuffer status
                if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
                    hex::log::error("Driver claims to support texture multisampling but it's not working");
                    return texture;
                }

            #endif

            return texture;
        }

    }
    
    Texture Texture::fromImage(const ImU8 *buffer, int size, Filter filter) {
        if (size == 0)
            return {};

        unsigned char *imageData = nullptr;

        Texture result;
        imageData = stbi_load_from_memory(buffer, size, &result.m_width, &result.m_height, nullptr, 4);
        if (imageData == nullptr)
            return {};

        GLuint texture = createMultisampleTextureFromRGBA8Array(imageData, result.m_width, result.m_height, filter);

        STBI_FREE(imageData);

        result.m_textureId = texture;

        return result;
    }

    Texture Texture::fromImage(std::span<const std::byte> buffer, Filter filter) {
        return Texture::fromImage(reinterpret_cast<const ImU8*>(buffer.data()), buffer.size(), filter);
    }


    Texture Texture::fromImage(const std::fs::path &path, Filter filter) {
        return Texture::fromImage(wolv::util::toUTF8String(path).c_str(), filter);
    }

    Texture Texture::fromImage(const char *path, Filter filter) {
        Texture result;
        unsigned char *imageData = stbi_load(path, &result.m_width, &result.m_height, nullptr, 4);
        if (imageData == nullptr)
            return {};

        GLuint texture = createMultisampleTextureFromRGBA8Array(imageData, result.m_width, result.m_height, filter);

        STBI_FREE(imageData);

        result.m_textureId = texture;

        return result;
    }

    Texture Texture::fromGLTexture(unsigned int glTexture, int width, int height) {
        Texture texture;
        texture.m_textureId = glTexture;
        texture.m_width = width;
        texture.m_height = height;

        return texture;
    }

    Texture Texture::fromBitmap(std::span<const std::byte> buffer, int width, int height, Filter filter) {
        return Texture::fromBitmap(reinterpret_cast<const ImU8*>(buffer.data()), buffer.size(), width, height, filter);
    }

    Texture Texture::fromBitmap(const ImU8 *buffer, int size, int width, int height, Filter filter) {
        if (width * height * 4 > size)
            return {};

        GLuint texture = createMultisampleTextureFromRGBA8Array(buffer, width, height, filter);

        Texture result;
        result.m_width = width;
        result.m_height = height;
        result.m_textureId = texture;

        return result;
    }

    Texture Texture::fromSVG(const char *path, int width, int height, Filter filter) {
        const auto scaleFactor = hex::ImHexApi::System::getBackingScaleFactor();

        auto document = lunasvg::Document::loadFromFile(path);
        auto bitmap = document->renderToBitmap(width * scaleFactor, height * scaleFactor);

        auto texture = createMultisampleTextureFromRGBA8Array(bitmap.data(), bitmap.width(), bitmap.height(), filter);

        Texture result;
        result.m_width = bitmap.width() / scaleFactor;
        result.m_height = bitmap.height() / scaleFactor;
        result.m_textureId = texture;

        return result;
    }

    Texture Texture::fromSVG(const std::fs::path &path, int width, int height, Filter filter) {
        return Texture::fromSVG(wolv::util::toUTF8String(path).c_str(), width, height, filter);
    }

    Texture Texture::fromSVG(std::span<const std::byte> buffer, int width, int height, Filter filter) {
        const auto scaleFactor = hex::ImHexApi::System::getBackingScaleFactor();

        auto document = lunasvg::Document::loadFromData(reinterpret_cast<const char*>(buffer.data()), buffer.size());
        auto bitmap = document->renderToBitmap(width * scaleFactor, height * scaleFactor);
        bitmap.convertToRGBA();

        auto texture = createMultisampleTextureFromRGBA8Array(bitmap.data(), bitmap.width(), bitmap.height(), filter);

        Texture result;
        result.m_width = bitmap.width() / scaleFactor;
        result.m_height = bitmap.height() / scaleFactor;
        result.m_textureId = texture;

        return result;
    }

    Texture::Texture(Texture&& other) noexcept {
        if (m_textureId != 0)
            glDeleteTextures(1, reinterpret_cast<GLuint*>(&m_textureId));

        m_textureId = other.m_textureId;
        m_width = other.m_width;
        m_height = other.m_height;

        other.m_textureId = 0;
    }

    Texture& Texture::operator=(Texture&& other) noexcept {
        if (m_textureId != 0)
            glDeleteTextures(1, reinterpret_cast<GLuint*>(&m_textureId));

        m_textureId = other.m_textureId;
        m_width = other.m_width;
        m_height = other.m_height;

        other.m_textureId = 0;
        
        return *this;
    }

    Texture::~Texture() {
        if (m_textureId == 0)
            return;

        glDeleteTextures(1, reinterpret_cast<GLuint*>(&m_textureId));
    }

    float GetTextWrapPos() {
        return GImGui->CurrentWindow->DC.TextWrapPos;
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

        bool hovered, held;
        bool pressed = ButtonBehavior(bb, id, &hovered, &held, flags);

        // Render
        const ImU32 col = hovered ? GetColorU32(ImGuiCol_ButtonHovered) : GetColorU32(ImGuiCol_ButtonActive);
        PushStyleColor(ImGuiCol_Text, ImU32(col));

        Text("%s %s", icon, label);

        if (hovered)
            GetWindowDrawList()->AddLine(ImVec2(pos.x, pos.y + size.y), pos + size, ImU32(col));

        PopStyleColor();

        IMGUI_TEST_ENGINE_ITEM_INFO(id, label, g.LastItemData.StatusFlags);
        return pressed;
    }

    bool Hyperlink(const char *label, const ImVec2 &size_arg, ImGuiButtonFlags flags) {
        ImGuiWindow *window = GetCurrentWindow();

        ImGuiContext &g         = *GImGui;
        const ImGuiID id        = window->GetID(label);
        const ImVec2 label_size = CalcTextSize(label, nullptr, true);

        ImVec2 pos  = window->DC.CursorPos;
        ImVec2 size = CalcItemSize(size_arg, label_size.x, label_size.y);

        const ImRect bb(pos, pos + size);
        ItemAdd(bb, id);

        bool hovered, held;
        bool pressed = ButtonBehavior(bb, id, &hovered, &held, flags);

        // Render
        const ImU32 col = hovered ? GetColorU32(ImGuiCol_ButtonHovered) : GetColorU32(ImGuiCol_ButtonActive);
        PushStyleColor(ImGuiCol_Text, ImU32(col));
        TextEx(label, nullptr, ImGuiTextFlags_NoWidthForLargeClippedText);    // Skip formatting

        if (hovered)
            GetWindowDrawList()->AddLine(ImVec2(pos.x, pos.y + size.y), pos + size, ImU32(col));

        PopStyleColor();

        IMGUI_TEST_ENGINE_ITEM_INFO(id, label, g.LastItemData.StatusFlags);
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
        ImVec2 size = CalcItemSize(size_arg, label_size.x, label_size.y) + ImVec2(g.FontSize + style.FramePadding.x * 2, 0.0F);

        const ImRect bb(pos, pos + size);
        ItemSize(size, 0);
        if (!ItemAdd(bb, id))
            return false;

        bool hovered, held;
        bool pressed = ButtonBehavior(bb, id, &hovered, &held, flags);

        // Render
        const ImU32 col = hovered ? GetColorU32(ImGuiCol_ButtonHovered) : GetColorU32(ImGuiCol_ButtonActive);
        PushStyleColor(ImGuiCol_Text, ImU32(col));
        RenderBullet(window->DrawList, bb.Min + ImVec2(style.FramePadding.x, g.FontSize * 0.5F), col);
        RenderText(bb.Min + ImVec2(g.FontSize * 0.5 + style.FramePadding.x, 0.0F), label, nullptr, false);
        GetWindowDrawList()->AddLine(bb.Min + ImVec2(g.FontSize * 0.5 + style.FramePadding.x, size.y), pos + size - ImVec2(g.FontSize * 0.5 + style.FramePadding.x, 0), ImU32(col));
        PopStyleColor();

        IMGUI_TEST_ENGINE_ITEM_INFO(id, label, g.LastItemData.StatusFlags);
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
        ImVec2 size = CalcItemSize(size_arg, text_size.x + style.FramePadding.x * 4.0F, text_size.y + style.FramePadding.y * 4.0F);

        const ImRect bb(pos, pos + size);
        ItemSize(size, style.FramePadding.y);
        if (!ItemAdd(bb, id))
            return false;

        bool hovered, held;
        bool pressed = ButtonBehavior(bb, id, &hovered, &held, flags);

        PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0, 0.5));
        PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);

        // Render
        const ImU32 col = GetCustomColorU32((held && hovered) ? ImGuiCustomCol_DescButtonActive : hovered ? ImGuiCustomCol_DescButtonHovered
                                                                                                          : ImGuiCustomCol_DescButton);
        RenderNavCursor(bb, id);
        RenderFrame(bb.Min, bb.Max, col, true, style.FrameRounding);
        PushStyleColor(ImGuiCol_Text, GetColorU32(ImGuiCol_ButtonActive));
        RenderTextClipped(bb.Min + style.FramePadding * 2, bb.Max - style.FramePadding, label, nullptr, nullptr);
        PopStyleColor();
        PushStyleColor(ImGuiCol_Text, GetColorU32(ImGuiCol_Text));
        auto clipBb = bb;
        clipBb.Max.x -= style.FramePadding.x;
        RenderTextClipped(bb.Min + style.FramePadding * 2 + ImVec2(style.FramePadding.x * 2, label_size.y), bb.Max - style.FramePadding, description, nullptr, &text_size, style.ButtonTextAlign, &clipBb);
        PopStyleColor();

        PopStyleVar(2);

        // Automatically close popups
        // if (pressed && !(flags & ImGuiButtonFlags_DontClosePopups) && (window->Flags & ImGuiWindowFlags_Popup))
        //    CloseCurrentPopup();

        IMGUI_TEST_ENGINE_ITEM_INFO(id, label, g.LastItemData.StatusFlags);
        return pressed;
    }

    bool DescriptionButtonProgress(const char *label, const char *description, float fraction, const ImVec2 &size_arg, ImGuiButtonFlags flags) {
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
        ImVec2 size = CalcItemSize(size_arg, text_size.x + style.FramePadding.x * 4.0F, text_size.y + style.FramePadding.y * 6.0F);

        const ImRect bb(pos, pos + size);
        ItemSize(size, style.FramePadding.y);
        if (!ItemAdd(bb, id))
            return false;

        bool hovered, held;
        bool pressed = ButtonBehavior(bb, id, &hovered, &held, flags);

        PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0, 0.5));
        PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);

        // Render
        const ImU32 col = GetCustomColorU32((held && hovered) ? ImGuiCustomCol_DescButtonActive : hovered ? ImGuiCustomCol_DescButtonHovered
                                                                                                          : ImGuiCustomCol_DescButton);
        RenderNavCursor(bb, id);
        RenderFrame(bb.Min, bb.Max, col, false, style.FrameRounding);
        PushStyleColor(ImGuiCol_Text, GetColorU32(ImGuiCol_ButtonActive));
        RenderTextClipped(bb.Min + style.FramePadding * 2, bb.Max - style.FramePadding, label, nullptr, nullptr);
        PopStyleColor();
        PushStyleColor(ImGuiCol_Text, GetColorU32(ImGuiCol_Text));
        auto clipBb = bb;
        clipBb.Max.x -= style.FramePadding.x;
        RenderTextClipped(bb.Min + style.FramePadding * 2 + ImVec2(style.FramePadding.x * 2, label_size.y), bb.Max - style.FramePadding, description, nullptr, &text_size, style.ButtonTextAlign, &clipBb);
        PopStyleColor();

        RenderFrame(ImVec2(bb.Min.x, bb.Max.y - 5 * hex::ImHexApi::System::getGlobalScale()), bb.Max, GetColorU32(ImGuiCol_ScrollbarBg), false, style.FrameRounding);
        RenderFrame(ImVec2(bb.Min.x, bb.Max.y - 5 * hex::ImHexApi::System::getGlobalScale()), ImVec2(bb.Min.x + fraction * bb.GetSize().x, bb.Max.y), GetColorU32(ImGuiCol_Button), false, style.FrameRounding);
        RenderFrame(bb.Min, bb.Max, 0x00, true, style.FrameRounding);

        PopStyleVar(2);

        // Automatically close popups
        // if (pressed && !(flags & ImGuiButtonFlags_DontClosePopups) && (window->Flags & ImGuiWindowFlags_Popup))
        //    CloseCurrentPopup();

        IMGUI_TEST_ENGINE_ITEM_INFO(id, label, g.LastItemData.StatusFlags);
        return pressed;
    }

    void HelpHover(const char *text, const char *icon, ImU32 iconColor) {
        PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
        PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
        PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
        PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0F);

        PushStyleColor(ImGuiCol_Text, iconColor);
        ImGui::PushID(text);
        Button(icon);
        ImGui::PopID();
        PopStyleColor();

        if (IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            SetNextWindowSizeConstraints(ImVec2(GetTextLineHeight() * 25, 0), ImVec2(GetTextLineHeight() * 35, FLT_MAX));
            BeginTooltip();
            TextFormattedWrapped("{}", text);
            EndTooltip();
        }

        PopStyleVar(2);
        PopStyleColor(3);
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

    void UnderwavedText(const char *label, ImColor textColor, ImColor lineColor, const ImVec2 &size_arg) {
        ImGuiWindow *window = GetCurrentWindow();
        std::string labelStr(label);
        for (char letter : labelStr) {
            std::string letterStr(1, letter);
            const ImVec2 label_size = CalcTextSize(letterStr.c_str(), nullptr, true);
            ImVec2 size = CalcItemSize(size_arg, label_size.x, label_size.y);
            ImVec2 pos = window->DC.CursorPos;
            float lineWidth = size.x / 3.0f;
            float halfLineW = lineWidth / 2.0f;
            float lineY = pos.y + size.y;
            ImVec2 initial = ImVec2(pos.x, lineY);
            ImVec2 pos1 = ImVec2(pos.x + lineWidth, lineY - 2.0f);
            ImVec2 pos2 = ImVec2(pos.x + lineWidth + halfLineW, lineY);
            ImVec2 pos3 = ImVec2(pos.x + lineWidth * 2 + halfLineW, lineY - 2.0f);
            ImVec2 pos4 = ImVec2(pos.x + lineWidth * 3, lineY - 1.0f);

            PushStyleColor(ImGuiCol_Text, ImU32(textColor));
            TextEx(letterStr.c_str(), nullptr, ImGuiTextFlags_NoWidthForLargeClippedText);    // Skip formatting
            GetWindowDrawList()->AddLine(initial, pos1, ImU32(lineColor),0.4f);
            GetWindowDrawList()->AddLine(pos1, pos2, ImU32(lineColor),0.3f);
            GetWindowDrawList()->AddLine(pos2, pos3, ImU32(lineColor),0.4f);
            GetWindowDrawList()->AddLine(pos3, pos4, ImU32(lineColor),0.3f);
            PopStyleColor();
            window->DC.CursorPos = ImVec2(pos.x + size.x, pos.y);
        }
    }

    void TextSpinner(const char *label) {
        Text("[%c] %s", "|/-\\"[ImU32(GetTime() * 20) % 4], label);
    }

    void Header(const char *label, bool firstEntry) {
        if (!firstEntry)
            NewLine();
        SeparatorText(label);
    }

    void HeaderColored(const char *label, ImColor color, bool firstEntry) {
        if (!firstEntry)
            NewLine();
        TextFormattedColored(color, "{}", label);
        Separator();
    }

    bool InfoTooltip(const char *text, bool isSeparator) {
        static double lastMoveTime;
        static ImGuiID lastHoveredID;

        double currTime   = GetTime();
        ImGuiID hoveredID = GetHoveredID();

        bool result = false;
        if (IsItemHovered(ImGuiHoveredFlags_DelayNormal) && (currTime - lastMoveTime) >= 0.5 && hoveredID == lastHoveredID) {
            if (!std::string_view(text).empty()) {
                const auto textWidth = CalcTextSize(text).x;

                const auto maxWidth = 300 * hex::ImHexApi::System::getGlobalScale();
                const bool wrapping = textWidth > maxWidth;

                if (wrapping)
                    ImGui::SetNextWindowSizeConstraints(ImVec2(maxWidth, 0), ImVec2(maxWidth, FLT_MAX));
                else
                    ImGui::SetNextWindowSize(ImVec2(textWidth + GetStyle().WindowPadding.x * 2, 0));

                if (BeginTooltip()) {
                    if (isSeparator)
                        SeparatorText(text);
                    else {
                        if (wrapping)
                            TextFormattedWrapped("{}", text);
                        else
                            TextFormatted("{}", text);
                    }

                    EndTooltip();
                }
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
                return 0.0F;
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
        if (Begin(window_name)) {
            OpenPopup(popup_name);
        }
        End();
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

        ImVec2 size = CalcItemSize(size_arg, label_size.x + style.FramePadding.x * 2.0F, label_size.y + style.FramePadding.y * 2.0F);

        const ImRect bb(pos, pos + size);
        ItemSize(size, style.FramePadding.y);
        if (!ItemAdd(bb, id))
            return false;

        bool hovered, held;
        bool pressed = ButtonBehavior(bb, id, &hovered, &held);

        // Render
        const ImU32 col = GetColorU32((held && hovered) ? ImGuiCol_ButtonActive : hovered ? ImGuiCol_ButtonHovered
                                                                                          : ImGuiCol_Button);
        RenderNavCursor(bb, id);
        RenderFrame(bb.Min, bb.Max, col, false, style.FrameRounding);
        RenderTextClipped(bb.Min + style.FramePadding, bb.Max - style.FramePadding, label, nullptr, &label_size, style.ButtonTextAlign, &bb);

        // Automatically close popups
        // if (pressed && !(flags & ImGuiButtonFlags_DontClosePopups) && (window->Flags & ImGuiWindowFlags_Popup))
        //    CloseCurrentPopup();

        IMGUI_TEST_ENGINE_ITEM_INFO(id, label, g.LastItemData.StatusFlags);
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

        ImVec2 size = CalcItemSize(ImVec2(1, 1) * GetCurrentWindow()->MenuBarHeight, label_size.x + style.FramePadding.x * 2.0F, label_size.y + style.FramePadding.y * 2.0F);

        ImVec2 padding = (size - label_size) / 2;

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
        RenderNavCursor(bb, id);
        RenderFrame(bb.Min, bb.Max, col, false, style.FrameRounding);
        RenderTextClipped(bb.Min + padding, bb.Max - padding, symbol, nullptr, &size, style.ButtonTextAlign, &bb);

        PopStyleColor();

        // Automatically close popups
        // if (pressed && !(flags & ImGuiButtonFlags_DontClosePopups) && (window->Flags & ImGuiWindowFlags_Popup))
        //    CloseCurrentPopup();

        IMGUI_TEST_ENGINE_ITEM_INFO(id, symbol, g.LastItemData.StatusFlags);
        return pressed;
    }

    bool IconButton(const char *symbol, ImVec4 color, ImVec2 size_arg, ImVec2 iconOffset) {
        ImGuiWindow *window = GetCurrentWindow();
        if (window->SkipItems)
            return false;

        color.w = 1.0F;

        ImGuiContext &g         = *GImGui;
        const ImGuiStyle &style = g.Style;
        const ImGuiID id        = window->GetID(symbol);
        const ImVec2 label_size = CalcTextSize(symbol, nullptr, true);

        ImVec2 pos = window->DC.CursorPos;

        ImVec2 size = CalcItemSize(size_arg, label_size.x + style.FramePadding.x * 2.0F, label_size.y + style.FramePadding.y * 2.0F);

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
        RenderNavCursor(bb, id);
        RenderFrame(bb.Min, bb.Max, col, true, style.FrameRounding);
        RenderTextClipped(bb.Min + style.FramePadding * ImVec2(1.3, 1) + iconOffset, bb.Max - style.FramePadding, symbol, nullptr, &label_size, style.ButtonTextAlign, &bb);

        PopStyleColor();

        // Automatically close popups
        // if (pressed && !(flags & ImGuiButtonFlags_DontClosePopups) && (window->Flags & ImGuiWindowFlags_Popup))
        //    CloseCurrentPopup();

        IMGUI_TEST_ENGINE_ITEM_INFO(id, symbol, g.LastItemData.StatusFlags);
        return pressed;
    }

    bool InputIntegerPrefix(const char *label, const char *prefix, void *value, ImGuiDataType type, const char *format, ImGuiInputTextFlags flags) {
        auto window             = GetCurrentWindow();
        const ImGuiID id        = window->GetID(label);
        const ImGuiStyle &style = GImGui->Style;


        const ImVec2 label_size = CalcTextSize(label, nullptr, true);
        const ImVec2 frame_size = CalcItemSize(ImVec2(0, 0), CalcTextSize(prefix).x, label_size.y + style.FramePadding.y * 2.0F);
        const ImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + ImVec2(CalcItemWidth(), frame_size.y));

        SetCursorPosX(GetCursorPosX() + frame_size.x);

        char buf[64];
        DataTypeFormatString(buf, IM_ARRAYSIZE(buf), type, value, format);

        RenderNavCursor(frame_bb, id);
        RenderFrame(frame_bb.Min, frame_bb.Max, GetColorU32(ImGuiCol_FrameBg), true, style.FrameRounding);

        PushStyleVar(ImGuiStyleVar_Alpha, 0.6F);
        RenderText(ImVec2(frame_bb.Min.x + style.FramePadding.x, frame_bb.Min.y + style.FramePadding.y), prefix);
        PopStyleVar();

        bool value_changed = false;
        PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0);
        PushStyleColor(ImGuiCol_FrameBg, 0x00000000);
        PushStyleColor(ImGuiCol_FrameBgHovered, 0x00000000);
        PushStyleColor(ImGuiCol_FrameBgActive, 0x00000000);
        if (InputTextEx(label, nullptr, buf, IM_ARRAYSIZE(buf), ImVec2(CalcItemWidth() - frame_size.x, label_size.y + style.FramePadding.y * 2.0F), flags))
            value_changed = DataTypeApplyFromText(buf, type, value, format);
        PopStyleColor(3);
        PopStyleVar();

        if (value_changed)
            MarkItemEdited(GImGui->LastItemData.ID);

        return value_changed;
    }

    bool InputHexadecimal(const char *label, u32 *value, ImGuiInputTextFlags flags) {
        return InputIntegerPrefix(label, "0x", value, ImGuiDataType_U32, "%X", flags | ImGuiInputTextFlags_CharsHexadecimal);
    }

    bool InputHexadecimal(const char *label, u64 *value, ImGuiInputTextFlags flags) {
        return InputIntegerPrefix(label, "0x", value, ImGuiDataType_U64, "%llX", flags | ImGuiInputTextFlags_CharsHexadecimal);
    }

    bool SliderBytes(const char *label, u64 *value, u64 min, u64 max, u64 stepSize, ImGuiSliderFlags flags) {
        std::string format;

        if (*value < 1024) {
            format = hex::format("{} Bytes", *value);
        } else if (*value < 1024 * 1024) {
            format = hex::format("{:.2f} KB", *value / 1024.0);
        } else if (*value < 1024 * 1024 * 1024) {
            format = hex::format("{:.2f} MB", *value / (1024.0 * 1024.0));
        } else {
            format = hex::format("{:.2f} GB", *value / (1024.0 * 1024.0 * 1024.0));
        }

        *value /= stepSize;
        min /= stepSize;
        max /= stepSize;

        auto result = ImGui::SliderScalar(label, ImGuiDataType_U64, value, &min, &max, format.c_str(), flags | ImGuiSliderFlags_Logarithmic);

        *value *= stepSize;

        return result;
    }

    void SmallProgressBar(float fraction, float yOffset) {
        ImGuiWindow *window = GetCurrentWindow();
        if (window->SkipItems)
            return;

        ImGuiContext &g         = *GImGui;
        const ImGuiStyle &style = g.Style;

        ImVec2 pos  = window->DC.CursorPos + ImVec2(0, yOffset);
        ImVec2 size = CalcItemSize(ImVec2(100, 5) * hex::ImHexApi::System::getGlobalScale(), 100, g.FontSize + style.FramePadding.y * 2.0F);
        ImRect bb(pos, pos + size);
        ItemSize(size, 0);
        if (!ItemAdd(bb, 0))
            return;

        // Render
        bool no_progress = fraction < 0;
        fraction = ImSaturate(fraction);
        RenderFrame(bb.Min, bb.Max, GetColorU32(ImGuiCol_FrameBg), true, style.FrameRounding);
        bb.Expand(ImVec2(-style.FrameBorderSize, -style.FrameBorderSize));

        if (no_progress) {
            auto time = (fmod(ImGui::GetTime() * 2, 1.8) - 0.4);
            RenderRectFilledRangeH(window->DrawList, bb, GetColorU32(ImGuiCol_PlotHistogram), ImSaturate(time), ImSaturate(time + 0.2), style.FrameRounding);
        } else {
            RenderRectFilledRangeH(window->DrawList, bb, GetColorU32(ImGuiCol_PlotHistogram), 0.0F, fraction, style.FrameRounding);
        }
    }

    void TextUnformattedCentered(const char *text) {
        auto availableSpace = ImGui::GetContentRegionAvail();

        std::string drawString;
        auto textEnd = text + strlen(text);
        for (auto wrapPos = text; wrapPos != textEnd;) {
            wrapPos = ImGui::GetFont()->CalcWordWrapPositionA(1, wrapPos, textEnd, availableSpace.x * 0.8F);
            drawString += std::string(text, wrapPos) + "\n";
            text = wrapPos;
        }

        drawString.pop_back();

        auto textSize = ImGui::CalcTextSize(drawString.c_str());

        ImPlot::AddTextCentered(ImGui::GetWindowDrawList(), ImGui::GetCursorScreenPos() + availableSpace / 2 - ImVec2(0, textSize.y / 2), ImGui::GetColorU32(ImGuiCol_Text), drawString.c_str());
    }
    
    bool InputTextIcon(const char *label, const char *icon, std::string &buffer, ImGuiInputTextFlags flags) {
        return InputTextIconHint(label, icon, nullptr, buffer, flags);
    }

    bool InputTextIconHint(const char* label, const char *icon, const char *hint, std::string &buffer, ImGuiInputTextFlags flags) {
        auto window             = GetCurrentWindow();
        const ImGuiID id        = window->GetID(label);
        const ImGuiStyle &style = GImGui->Style;

        const ImVec2 label_size = CalcTextSize(label, nullptr, true);
        const ImVec2 icon_frame_size = CalcTextSize(icon) + style.FramePadding * 2.0F;
        const ImVec2 frame_size = CalcItemSize(ImVec2(0, 0), icon_frame_size.x, label_size.y + style.FramePadding.y * 2.0F);
        const ImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + frame_size);

        SetCursorPosX(GetCursorPosX() + frame_size.x);

        float width_adjustment = window->DC.ItemWidth < 0 ? 0 : icon_frame_size.x;

        bool value_changed = InputTextEx(label, hint, buffer.data(), buffer.size() + 1, ImVec2(CalcItemWidth() - width_adjustment, label_size.y + style.FramePadding.y * 2.0F), ImGuiInputTextFlags_CallbackResize | flags, UpdateStringSizeCallback, &buffer);

        if (value_changed)
            MarkItemEdited(GImGui->LastItemData.ID);

        RenderNavCursor(frame_bb, id);
        RenderFrame(frame_bb.Min, frame_bb.Max, GetColorU32(ImGuiCol_FrameBg), true, style.FrameRounding);

        RenderFrame(frame_bb.Min, frame_bb.Min + icon_frame_size, GetColorU32(ImGuiCol_TableBorderStrong), true, style.FrameRounding);
        RenderText(ImVec2(frame_bb.Min.x + style.FramePadding.x, frame_bb.Min.y + style.FramePadding.y), icon);

        return value_changed;
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

        if (ImGui::InputText(label, buf, IM_ARRAYSIZE(buf), flags, callback, user_data))
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
        RenderNavCursor(total_bb, id);
        RenderFrame(check_bb.Min, check_bb.Max, GetColorU32((held && hovered) ? ImGuiCol_FrameBgActive : hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg), true, style.FrameRounding);

        RenderText(check_bb.Min + style.FramePadding, *v ? "1" : "0");

        ImVec2 label_pos = ImVec2(check_bb.Max.x + style.ItemInnerSpacing.x, check_bb.Min.y + style.FramePadding.y);
        if (label_size.x > 0.0F)
            RenderText(label_pos, label);

        IMGUI_TEST_ENGINE_ITEM_INFO(id, label, g.LastItemData.StatusFlags | ImGuiItemStatusFlags_Checkable | (*v ? ImGuiItemStatusFlags_Checked : 0));
        return pressed;
    }

    bool DimmedButton(const char* label, ImVec2 size){
        PushStyleColor(ImGuiCol_ButtonHovered, GetCustomColorU32(ImGuiCustomCol_DescButtonHovered));
        PushStyleColor(ImGuiCol_Button, GetCustomColorU32(ImGuiCustomCol_DescButton));
        PushStyleColor(ImGuiCol_Text, GetColorU32(ImGuiCol_ButtonActive));
        PushStyleColor(ImGuiCol_ButtonActive, GetCustomColorU32(ImGuiCustomCol_DescButtonActive));
        PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);

        bool res = Button(label, size);

        PopStyleColor(4);
        PopStyleVar(1);

        return res;
    }

    bool DimmedIconButton(const char *symbol, ImVec4 color, ImVec2 size, ImVec2 iconOffset) {
        PushStyleColor(ImGuiCol_ButtonHovered, GetCustomColorU32(ImGuiCustomCol_DescButtonHovered));
        PushStyleColor(ImGuiCol_Button, GetCustomColorU32(ImGuiCustomCol_DescButton));
        PushStyleColor(ImGuiCol_Text, GetColorU32(ImGuiCol_ButtonActive));
        PushStyleColor(ImGuiCol_ButtonActive, GetCustomColorU32(ImGuiCustomCol_DescButtonActive));
        PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);

        bool res = IconButton(symbol, color, size, iconOffset);

        PopStyleColor(4);
        PopStyleVar(1);

        return res;
    }

    bool DimmedButtonToggle(const char *icon, bool *v, ImVec2 size, ImVec2 iconOffset) {
        bool pushed = false;
        bool toggled = false;

        if (*v) {
            PushStyleColor(ImGuiCol_Border, GetStyleColorVec4(ImGuiCol_ButtonActive));
            pushed = true;
        }

        if (DimmedIconButton(icon, GetStyleColorVec4(ImGuiCol_Text), size, iconOffset)) {
            *v = !*v;
            toggled = true;
        }

        if (pushed)
            PopStyleColor();

        return toggled;
    }

    bool DimmedIconToggle(const char *icon, bool *v) {
        bool pushed = false;
        bool toggled = false;

        if (*v) {
            PushStyleColor(ImGuiCol_Border, GetStyleColorVec4(ImGuiCol_ButtonActive));
            pushed = true;
        }

        if (DimmedIconButton(icon, GetStyleColorVec4(ImGuiCol_Text))) {
            *v = !*v;
            toggled = true;
        }

        if (pushed)
            PopStyleColor();

        return toggled;
    }

    bool DimmedIconToggle(const char *iconOn, const char *iconOff, bool *v) {
        bool pushed = false;
        bool toggled = false;

        if (*v) {
            PushStyleColor(ImGuiCol_Border, GetStyleColorVec4(ImGuiCol_ButtonActive));
            pushed = true;
        }

        if (DimmedIconButton(*v ? iconOn : iconOff, GetStyleColorVec4(ImGuiCol_Text))) {
            *v = !*v;
            toggled = true;
        }

        if (pushed)
            PopStyleColor();

        return toggled;
    }

    void TextOverlay(const char *text, ImVec2 pos, float maxWidth) {
        const auto textSize = CalcTextSize(text, nullptr, false, maxWidth);
        const auto textPos  = pos - textSize / 2;
        const auto margin   = GetStyle().FramePadding * 2;
        const auto textRect = ImRect(textPos - margin, textPos + textSize + margin);

        auto drawList = GetWindowDrawList();

        drawList->AddDrawCmd();
        drawList->AddRectFilled(textRect.Min, textRect.Max, GetColorU32(ImGuiCol_WindowBg) | 0xFF000000);
        drawList->AddRect(textRect.Min, textRect.Max, GetColorU32(ImGuiCol_Border));
        drawList->AddDrawCmd();
        drawList->AddText(nullptr, 0.0F, textPos, GetColorU32(ImGuiCol_Text), text, nullptr, maxWidth);
    }

    bool BeginBox() {
        PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(5, 5));
        auto result = BeginTable("##box", 1, ImGuiTableFlags_BordersOuter | ImGuiTableFlags_SizingStretchSame);
        TableNextRow();
        TableNextColumn();

        return result;
    }

    void EndBox() {
        EndTable();
        PopStyleVar();
    }

    bool BeginSubWindow(const char *label, bool *collapsed, ImVec2 size, ImGuiChildFlags flags) {
        const bool hasMenuBar = !std::string_view(label).empty();

        bool result = false;
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0F);
        if (ImGui::BeginChild(hex::format("{}##SubWindow", label).c_str(), size, ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY | flags, hasMenuBar ? ImGuiWindowFlags_MenuBar : ImGuiWindowFlags_None)) {
            result = true;

            if (hasMenuBar && ImGui::BeginMenuBar()) {
                if (collapsed == nullptr)
                    ImGui::TextUnformatted(label);
                else {
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, ImGui::GetStyle().FramePadding.y));
                    ImGui::PushStyleColor(ImGuiCol_Button, 0x00);
                    if (ImGui::Button(label))
                        *collapsed = !*collapsed;
                    ImGui::PopStyleColor();
                    ImGui::PopStyleVar();
                }
                ImGui::EndMenuBar();
            }

            if (collapsed != nullptr && *collapsed) {
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() - (ImGui::GetStyle().FramePadding.y * 2));
                ImGuiExt::TextFormattedDisabled("...");
                result = false;
            }
        }
        ImGui::PopStyleVar();

        return result;
    }

    void EndSubWindow() {
        ImGui::EndChild();
    }

    bool VSliderAngle(const char* label, const ImVec2& size, float* v_rad, float v_degrees_min, float v_degrees_max, const char* format, ImGuiSliderFlags flags) {
        if (format == nullptr)
            format = "%.0f deg";
        float v_deg = (*v_rad) * 360.0F / (2 * IM_PI);
        bool value_changed = ImGui::VSliderFloat(label, size, &v_deg, v_degrees_min, v_degrees_max, format, flags);
        *v_rad = v_deg * (2 * IM_PI) / 360.0F;
        return value_changed;
    }

    bool InputFilePicker(const char *label, std::fs::path &path, const std::vector<hex::fs::ItemFilter> &validExtensions) {
        bool picked = false;

        ImGui::PushID(label);

        const auto buttonSize = ImGui::CalcTextSize("...") + ImGui::GetStyle().FramePadding * 2;
        ImGui::PushItemWidth(ImGui::CalcItemWidth() - buttonSize.x - ImGui::GetStyle().FramePadding.x);
        std::string string = wolv::util::toUTF8String(path);
        if (ImGui::InputText("##pathInput", string, ImGuiInputTextFlags_AutoSelectAll)) {
            path = std::u8string(string.begin(), string.end());
            picked = true;
        }
        ImGui::PopItemWidth();

        ImGui::SameLine();

        if (ImGui::Button("...", buttonSize)) {
            hex::fs::openFileBrowser(hex::fs::DialogMode::Open, validExtensions, [&](const std::fs::path &pickedPath) {
                path = pickedPath;
                picked = true;
            });
        }

        ImGui::SameLine();

        ImGui::TextUnformatted(label);

        ImGui::PopID();

        return picked;
    }

    bool ToggleSwitch(const char *label, bool *v) {
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems)
            return false;

        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        const ImGuiID id = window->GetID(label);
        const ImVec2 label_size = CalcTextSize(label, nullptr, true);

        const ImVec2 size = ImVec2(GetFrameHeight() * 2.0F, GetFrameHeight());
        const ImVec2 pos = window->DC.CursorPos;
        const ImRect total_bb(pos, pos + ImVec2(size.x + (label_size.x > 0.0F ? style.ItemInnerSpacing.x + label_size.x : 0.0F), label_size.y + style.FramePadding.y * 2.0F));
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

        const ImRect knob_bb(pos, pos + size);

        window->DrawList->AddRectFilled(knob_bb.Min, knob_bb.Max, GetColorU32(held ? ImGuiCol_ButtonActive : hovered ? ImGuiCol_ButtonHovered : *v ? ImGuiCol_ButtonActive : ImGuiCol_Button), size.y / 2);

        if (*v)
            window->DrawList->AddCircleFilled(knob_bb.Max - ImVec2(size.y / 2, size.y / 2), (size.y - style.ItemInnerSpacing.y) / 2, GetColorU32(ImGuiCol_ScrollbarGrabActive), 16);
        else
            window->DrawList->AddCircleFilled(knob_bb.Min + ImVec2(size.y / 2, size.y / 2), (size.y - style.ItemInnerSpacing.y) / 2, GetColorU32(ImGuiCol_ScrollbarGrabActive), 16);

        ImVec2 label_pos = ImVec2(knob_bb.Max.x + style.ItemInnerSpacing.x, knob_bb.Min.y + style.FramePadding.y);
        if (g.LogEnabled)
            LogRenderedText(&label_pos, *v ? "((*)  )" : "(  (*))");
        if (label_size.x > 0.0F)
            RenderText(label_pos, label);

        IMGUI_TEST_ENGINE_ITEM_INFO(id, label, g.LastItemData.StatusFlags | ImGuiItemStatusFlags_Checkable | (*v ? ImGuiItemStatusFlags_Checked : 0));
        return pressed;
    }

    bool ToggleSwitch(const char *label, bool v) {
        return ToggleSwitch(label, &v);
    }

    bool PopupTitleBarButton(const char* label, bool p_enabled)
    {
        ImGuiContext& g = *GImGui;
        ImGuiWindow* window = g.CurrentWindow;
        const ImGuiID id = window->GetID(label);
        const ImRect title_rect = window->TitleBarRect();
        const ImVec2 size(g.FontSize, g.FontSize); // Button size matches font size for aesthetic consistency.
        const ImVec2 pos = window->DC.CursorPos;
        const ImVec2 max_pos = pos + size;
        const ImRect bb(pos.x, title_rect.Min.y, max_pos.x, title_rect.Max.y);

        ImGui::PushClipRect(title_rect.Min, title_rect.Max, false);

        // Check for item addition (similar to how clipping is handled in the original button functions).
        bool is_clipped = !ItemAdd(bb, id);
        bool hovered, held;
        bool pressed = ButtonBehavior(bb, id, &hovered, &held, ImGuiButtonFlags_None);
        if (is_clipped)
        {
            ImGui::PopClipRect();
            return pressed;
        }

        // const ImU32 bg_col = GetColorU32((held && hovered) ? ImGuiCol_ButtonActive : hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
        // window->DrawList->AddCircleFilled(bb.GetCenter(), ImMax(2.0f, g.FontSize * 0.5f + 1.0f), bg_col);

        // Draw the label in the center
        ImU32 text_col = GetColorU32(p_enabled || hovered ? ImGuiCol_Text : ImGuiCol_TextDisabled);
        window->DrawList->AddText(bb.GetCenter() - ImVec2(g.FontSize * 0.45F, g.FontSize * 0.5F), text_col, label);

        // Return the button press state
        ImGui::PopClipRect();
        return pressed;
    }

    void PopupTitleBarText(const char* text) {
        ImGuiContext& g = *GImGui;
        ImGuiWindow* window = g.CurrentWindow;
        const ImRect title_rect = window->TitleBarRect();
        const ImVec2 size(g.FontSize, g.FontSize); // Button size matches font size for aesthetic consistency.
        const ImVec2 pos = window->DC.CursorPos;
        const ImVec2 max_pos = pos + size;
        const ImRect bb(pos.x, title_rect.Min.y, max_pos.x, title_rect.Max.y);

        ImGui::PushClipRect(title_rect.Min, title_rect.Max, false);

        // Draw the label in the center
        ImU32 text_col = GetColorU32(ImGuiCol_Text);
        window->DrawList->AddText(bb.GetCenter() - ImVec2(g.FontSize * 0.45F, g.FontSize * 0.5F), text_col, text);

        // Return the button press state
        ImGui::PopClipRect();
    }
}

namespace ImGui {

    bool InputText(const char *label, std::u8string &buffer, ImGuiInputTextFlags flags) {
        return ImGui::InputText(label, reinterpret_cast<char *>(buffer.data()), buffer.size() + 1, ImGuiInputTextFlags_CallbackResize | flags, ImGuiExt::UpdateStringSizeCallback, &buffer);
    }

    bool InputText(const char *label, std::string &buffer, ImGuiInputTextFlags flags) {
        return ImGui::InputText(label, buffer.data(), buffer.size() + 1, ImGuiInputTextFlags_CallbackResize | flags, ImGuiExt::UpdateStringSizeCallback, &buffer);
    }

    bool InputTextMultiline(const char *label, std::string &buffer, const ImVec2 &size, ImGuiInputTextFlags flags) {
        return ImGui::InputTextMultiline(label, buffer.data(), buffer.size() + 1, size, ImGuiInputTextFlags_CallbackResize | flags, ImGuiExt::UpdateStringSizeCallback, &buffer);
    }

    bool InputTextWithHint(const char *label, const char *hint, std::string &buffer, ImGuiInputTextFlags flags) {
        return ImGui::InputTextWithHint(label, hint, buffer.data(), buffer.size() + 1, ImGuiInputTextFlags_CallbackResize | flags, ImGuiExt::UpdateStringSizeCallback, &buffer);
    }

}
