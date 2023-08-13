#pragma once

#include <hex.hpp>

#include <functional>
#include <string>

#include <imgui.h>
#include <imgui_internal.h>

#include <hex/helpers/fmt.hpp>

#include <wolv/utils/string.hpp>

enum ImGuiCustomCol {
    ImGuiCustomCol_DescButton,
    ImGuiCustomCol_DescButtonHovered,
    ImGuiCustomCol_DescButtonActive,

    ImGuiCustomCol_ToolbarGray,
    ImGuiCustomCol_ToolbarRed,
    ImGuiCustomCol_ToolbarYellow,
    ImGuiCustomCol_ToolbarGreen,
    ImGuiCustomCol_ToolbarBlue,
    ImGuiCustomCol_ToolbarPurple,
    ImGuiCustomCol_ToolbarBrown,

    ImGuiCustomCol_Highlight,

    ImGuiCustomCol_IEEEToolSign,
    ImGuiCustomCol_IEEEToolExp,
    ImGuiCustomCol_IEEEToolMantissa,

    ImGuiCustomCol_BlurBackground,

    ImGuiCustomCol_COUNT
};

enum ImGuiCustomStyle {
    ImGuiCustomStyle_WindowBlur,

    ImGuiCustomStyle_COUNT
};

namespace ImGui {

    class Texture {
    public:
        Texture() = default;
        Texture(const ImU8 *buffer, int size, int width = 0, int height = 0);
        explicit Texture(const char *path);
        Texture(unsigned int texture, int width, int height);
        Texture(const Texture&) = delete;
        Texture(Texture&& other) noexcept;

        ~Texture();

        Texture& operator=(const Texture&) = delete;
        Texture& operator=(Texture&& other) noexcept;

        [[nodiscard]] constexpr bool isValid() const noexcept {
            return this->m_textureId != nullptr;
        }

        [[nodiscard]] constexpr operator ImTextureID() const noexcept {
            return this->m_textureId;
        }

        [[nodiscard]] auto getSize() const noexcept {
            return ImVec2(this->m_width, this->m_height);
        }

        [[nodiscard]] constexpr auto getAspectRatio() const noexcept {
            if (this->m_height == 0) return 1.0F;

            return float(this->m_width) / float(this->m_height);
        }

    private:
        ImTextureID m_textureId = nullptr;
        int m_width = 0, m_height = 0;
    };

    int UpdateStringSizeCallback(ImGuiInputTextCallbackData *data);

    bool IconHyperlink(const char *icon, const char *label, const ImVec2 &size_arg = ImVec2(0, 0), ImGuiButtonFlags flags = 0);
    bool Hyperlink(const char *label, const ImVec2 &size_arg = ImVec2(0, 0), ImGuiButtonFlags flags = 0);
    bool BulletHyperlink(const char *label, const ImVec2 &size_arg = ImVec2(0, 0), ImGuiButtonFlags flags = 0);
    bool DescriptionButton(const char *label, const char *description, const ImVec2 &size_arg = ImVec2(0, 0), ImGuiButtonFlags flags = 0);

    void HelpHover(const char *text);

    void UnderlinedText(const char *label, ImColor color = ImGui::GetStyleColorVec4(ImGuiCol_Text), const ImVec2 &size_arg = ImVec2(0, 0));

    void TextSpinner(const char *label);

    void Header(const char *label, bool firstEntry = false);
    void HeaderColored(const char *label, ImColor color, bool firstEntry);

    bool InfoTooltip(const char *text = "");

    bool TitleBarButton(const char *label, ImVec2 size_arg);
    bool ToolBarButton(const char *symbol, ImVec4 color);
    bool IconButton(const char *symbol, ImVec4 color, ImVec2 size_arg = ImVec2(0, 0));

    bool InputIntegerPrefix(const char* label, const char *prefix, void *value, ImGuiDataType type, const char *format, ImGuiInputTextFlags flags = ImGuiInputTextFlags_None);
    bool InputHexadecimal(const char* label, u32 *value, ImGuiInputTextFlags flags = ImGuiInputTextFlags_None);
    bool InputHexadecimal(const char* label, u64 *value, ImGuiInputTextFlags flags = ImGuiInputTextFlags_None);

    inline bool HasSecondPassed() {
        return static_cast<ImU32>(ImGui::GetTime() * 100) % 100 <= static_cast<ImU32>(ImGui::GetIO().DeltaTime * 100);
    }

    void OpenPopupInWindow(const char *window_name, const char *popup_name);

    struct ImHexCustomData {
        ImVec4 Colors[ImGuiCustomCol_COUNT];

        struct Styles {
            float WindowBlur = 0.0F;
        } styles;
    };

    ImU32 GetCustomColorU32(ImGuiCustomCol idx, float alpha_mul = 1.0F);
    ImVec4 GetCustomColorVec4(ImGuiCustomCol idx, float alpha_mul = 1.0F);

    inline ImHexCustomData::Styles& GetCustomStyle() {
        auto &customData = *static_cast<ImHexCustomData *>(GImGui->IO.UserData);

        return customData.styles;
    }

    float GetCustomStyleFloat(ImGuiCustomStyle idx);
    ImVec2 GetCustomStyleVec2(ImGuiCustomStyle idx);

    void StyleCustomColorsDark();
    void StyleCustomColorsLight();
    void StyleCustomColorsClassic();

    void SmallProgressBar(float fraction, float yOffset = 0.0F);

    inline void TextFormatted(const std::string &fmt, auto &&...args) {
        ImGui::TextUnformatted(hex::format(fmt, std::forward<decltype(args)>(args)...).c_str());
    }

    inline void TextFormattedSelectable(const std::string &fmt, auto &&...args) {
        auto text = hex::format(fmt, std::forward<decltype(args)>(args)...);

        ImGui::PushID(text.c_str());

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2());
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4());

        ImGui::PushItemWidth(-FLT_MIN);
        ImGui::InputText("##", const_cast<char *>(text.c_str()), text.size(), ImGuiInputTextFlags_ReadOnly);
        ImGui::PopItemWidth();

        ImGui::PopStyleColor();
        ImGui::PopStyleVar();

        ImGui::PopID();
    }

    inline void TextFormattedColored(ImColor color, const std::string &fmt, auto &&...args) {
        ImGui::TextColored(color, "%s", hex::format(fmt, std::forward<decltype(args)>(args)...).c_str());
    }

    inline void TextFormattedDisabled(const std::string &fmt, auto &&...args) {
        ImGui::TextDisabled("%s", hex::format(fmt, std::forward<decltype(args)>(args)...).c_str());
    }

    inline void TextFormattedWrapped(const std::string &fmt, auto &&...args) {
        ImGui::TextWrapped("%s", hex::format(fmt, std::forward<decltype(args)>(args)...).c_str());
    }

    inline void TextFormattedWrappedSelectable(const std::string &fmt, auto &&...args) {
        //Manually wrap text, using the letter M (generally the widest character in non-monospaced fonts) to calculate the character width to use.
        auto text = wolv::util::wrapMonospacedString(
                hex::format(fmt, std::forward<decltype(args)>(args)...),
                ImGui::CalcTextSize("M").x,
                ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ScrollbarSize - ImGui::GetStyle().FrameBorderSize
        );

        ImGui::PushID(text.c_str());

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2());
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4());

        ImGui::PushItemWidth(-FLT_MIN);
        ImGui::InputTextMultiline(
                "##",
                const_cast<char *>(text.c_str()),
                text.size(),
                ImVec2(0, -FLT_MIN),
                ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_NoHorizontalScroll
        );
        ImGui::PopItemWidth();

        ImGui::PopStyleColor();
        ImGui::PopStyleVar();

        ImGui::PopID();
    }

    inline void TextFormattedCentered(const std::string &fmt, auto &&...args) {
        auto text = hex::format(fmt, std::forward<decltype(args)>(args)...);
        auto availableSpace = ImGui::GetContentRegionAvail();
        auto textSize = ImGui::CalcTextSize(text.c_str(), nullptr, false, availableSpace.x * 0.75F);

        ImGui::SetCursorPos(((availableSpace - textSize) / 2.0F));

        ImGui::PushTextWrapPos(availableSpace.x * 0.75F);
        ImGui::TextFormattedWrapped("{}", text);
        ImGui::PopTextWrapPos();
    }

    inline void TextFormattedCenteredHorizontal(const std::string &fmt, auto &&...args) {
        auto text = hex::format(fmt, std::forward<decltype(args)>(args)...);
        auto availableSpace = ImGui::GetContentRegionAvail();
        auto textSize = ImGui::CalcTextSize(text.c_str(), nullptr, false, availableSpace.x * 0.75F);

        ImGui::SetCursorPosX(((availableSpace - textSize) / 2.0F).x);

        ImGui::PushTextWrapPos(availableSpace.x * 0.75F);
        ImGui::TextFormattedWrapped("{}", text);
        ImGui::PopTextWrapPos();
    }

    bool InputText(const char* label, std::string &buffer, ImGuiInputTextFlags flags = ImGuiInputTextFlags_None);
    bool InputTextIcon(const char* label, const char *icon, std::string &buffer, ImGuiInputTextFlags flags = ImGuiInputTextFlags_None);
    bool InputText(const char *label, std::u8string &buffer, ImGuiInputTextFlags flags = ImGuiInputTextFlags_None);
    bool InputTextMultiline(const char* label, std::string &buffer, const ImVec2& size = ImVec2(0, 0), ImGuiInputTextFlags flags = ImGuiInputTextFlags_None);
    bool InputTextWithHint(const char *label, const char *hint, std::string &buffer, ImGuiInputTextFlags flags = ImGuiInputTextFlags_None);

    bool InputScalarCallback(const char* label, ImGuiDataType data_type, void* p_data, const char* format, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data);

    void HideTooltip();

    bool BitCheckbox(const char* label, bool* v);

    bool DimmedButton(const char* label);
    bool DimmedIconButton(const char *symbol, ImVec4 color, ImVec2 size_arg = ImVec2(0, 0));
    bool DimmedIconToggle(const char *icon, bool *v);

}