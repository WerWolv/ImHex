#pragma once

#include <hex.hpp>

#include <cstddef>
#include <string>
#include <span>

#include <imgui.h>

#include <hex/helpers/fmt.hpp>
#include <hex/helpers/concepts.hpp>
#include <hex/helpers/fs.hpp>

#include <wolv/utils/string.hpp>

enum ImGuiCustomCol : int {
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

    ImGuiCustomCol_LoggerDebug,
    ImGuiCustomCol_LoggerInfo,
    ImGuiCustomCol_LoggerWarning,
    ImGuiCustomCol_LoggerError,
    ImGuiCustomCol_LoggerFatal,

    ImGuiCustomCol_AchievementUnlocked,

    ImGuiCustomCol_FindHighlight,

    ImGuiCustomCol_DiffAdded,
    ImGuiCustomCol_DiffRemoved,
    ImGuiCustomCol_DiffChanged,

    ImGuiCustomCol_AdvancedEncodingASCII,
    ImGuiCustomCol_AdvancedEncodingSingleChar,
    ImGuiCustomCol_AdvancedEncodingMultiChar,
    ImGuiCustomCol_AdvancedEncodingUnknown,

    ImGuiCustomCol_Highlight,

    ImGuiCustomCol_Patches,
    ImGuiCustomCol_PatternSelected,

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

namespace ImGuiExt {

    class Texture {
    public:
        enum class Filter {
            Linear,
            Nearest
        };

        Texture() = default;
        Texture(const ImU8 *buffer, int size, Filter filter = Filter::Nearest, int width = 0, int height = 0);
        Texture(std::span<const std::byte> bytes, Filter filter = Filter::Nearest, int width = 0, int height = 0);
        explicit Texture(const char *path, Filter filter = Filter::Nearest);
        explicit Texture(const std::fs::path &path, Filter filter = Filter::Nearest);
        Texture(unsigned int texture, int width, int height);
        Texture(const Texture&) = delete;
        Texture(Texture&& other) noexcept;

        ~Texture();

        Texture& operator=(const Texture&) = delete;
        Texture& operator=(Texture&& other) noexcept;

        [[nodiscard]] constexpr bool isValid() const noexcept {
            return m_textureId != nullptr;
        }

        [[nodiscard]] operator ImTextureID() const noexcept {
            return m_textureId;
        }

        [[nodiscard]] operator intptr_t() const noexcept {
            return reinterpret_cast<intptr_t>(m_textureId);
        }

        [[nodiscard]] auto getSize() const noexcept {
            return ImVec2(m_width, m_height);
        }

        [[nodiscard]] constexpr auto getAspectRatio() const noexcept {
            if (m_height == 0) return 1.0F;

            return float(m_width) / float(m_height);
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
    bool DescriptionButtonProgress(const char *label, const char *description, float fraction, const ImVec2 &size_arg = ImVec2(0, 0), ImGuiButtonFlags flags = 0);

    void HelpHover(const char *text);

    void UnderlinedText(const char *label, ImColor color = ImGui::GetStyleColorVec4(ImGuiCol_Text), const ImVec2 &size_arg = ImVec2(0, 0));

    void TextSpinner(const char *label);

    void Header(const char *label, bool firstEntry = false);
    void HeaderColored(const char *label, ImColor color, bool firstEntry);

    bool InfoTooltip(const char *text = "",bool = false);

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
        auto &customData = *static_cast<ImHexCustomData *>(ImGui::GetIO().UserData);

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
        // Manually wrap text, using the letter M (generally the widest character in non-monospaced fonts) to calculate the character width to use.
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

    void TextUnformattedCentered(const char *text);
    inline void TextFormattedCentered(const std::string &fmt, auto &&...args) {
        auto text = hex::format(fmt, std::forward<decltype(args)>(args)...);
        TextUnformattedCentered(text.c_str());
    }


    inline void TextFormattedCenteredHorizontal(const std::string &fmt, auto &&...args) {
        auto text = hex::format(fmt, std::forward<decltype(args)>(args)...);
        auto availableSpace = ImGui::GetContentRegionAvail();
        auto textSize = ImGui::CalcTextSize(text.c_str(), nullptr, false, availableSpace.x * 0.75F);

        ImGui::SetCursorPosX(((availableSpace - textSize) / 2.0F).x);

        ImGui::PushTextWrapPos(availableSpace.x * 0.75F);
        ImGuiExt::TextFormattedWrapped("{}", text);
        ImGui::PopTextWrapPos();
    }

    bool InputTextIcon(const char* label, const char *icon, std::string &buffer, ImGuiInputTextFlags flags = ImGuiInputTextFlags_None);

    bool InputScalarCallback(const char* label, ImGuiDataType data_type, void* p_data, const char* format, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data);

    void HideTooltip();

    bool BitCheckbox(const char* label, bool* v);

    bool DimmedButton(const char* label, ImVec2 size = ImVec2(0, 0));
    bool DimmedIconButton(const char *symbol, ImVec4 color, ImVec2 size = ImVec2(0, 0));
    bool DimmedButtonToggle(const char *icon, bool *v, ImVec2 size);
    bool DimmedIconToggle(const char *icon, bool *v);
    bool DimmedIconToggle(const char *iconOn, const char *iconOff, bool *v);

    void TextOverlay(const char *text, ImVec2 pos);

    bool BeginBox();
    void EndBox();

    void BeginSubWindow(const char *label, ImVec2 size = ImVec2(0, 0), ImGuiChildFlags flags = ImGuiChildFlags_None);
    void EndSubWindow();

    void ConfirmButtons(const char *textLeft, const char *textRight, const auto &leftButtonCallback, const auto &rightButtonCallback) {
        auto width = ImGui::GetWindowWidth();
        ImGui::SetCursorPosX(width / 9);
        if (ImGui::Button(textLeft, ImVec2(width / 3, 0)))
            leftButtonCallback();
        ImGui::SameLine();
        ImGui::SetCursorPosX(width / 9 * 5);
        if (ImGui::Button(textRight, ImVec2(width / 3, 0)))
            rightButtonCallback();
    }

    bool VSliderAngle(const char* label, ImVec2& size, float* v_rad, float v_degrees_min, float v_degrees_max, const char* format, ImGuiSliderFlags flags);

    bool InputFilePicker(const char *label, std::fs::path &path, const std::vector<hex::fs::ItemFilter> &validExtensions);

    bool ToggleSwitch(const char *label, bool *v);
    bool ToggleSwitch(const char *label, bool v);

    template<typename T>
    constexpr ImGuiDataType getImGuiDataType() {
        if constexpr      (std::same_as<T, u8>)     return ImGuiDataType_U8;
        else if constexpr (std::same_as<T, u16>)    return ImGuiDataType_U16;
        else if constexpr (std::same_as<T, u32>)    return ImGuiDataType_U32;
        else if constexpr (std::same_as<T, u64>)    return ImGuiDataType_U64;
        else if constexpr (std::same_as<T, i8>)     return ImGuiDataType_S8;
        else if constexpr (std::same_as<T, i16>)    return ImGuiDataType_S16;
        else if constexpr (std::same_as<T, i32>)    return ImGuiDataType_S32;
        else if constexpr (std::same_as<T, i64>)    return ImGuiDataType_S64;
        else if constexpr (std::same_as<T, float>)  return ImGuiDataType_Float;
        else if constexpr (std::same_as<T, double>) return ImGuiDataType_Double;
        else static_assert(hex::always_false<T>::value, "Invalid data type!");
    }

    template<typename T>
    constexpr const char *getFormatLengthSpecifier() {
        if constexpr      (std::same_as<T, u8>)     return "hh";
        else if constexpr (std::same_as<T, u16>)    return "h";
        else if constexpr (std::same_as<T, u32>)    return "l";
        else if constexpr (std::same_as<T, u64>)    return "ll";
        else if constexpr (std::same_as<T, i8>)     return "hh";
        else if constexpr (std::same_as<T, i16>)    return "h";
        else if constexpr (std::same_as<T, i32>)    return "l";
        else if constexpr (std::same_as<T, i64>)    return "ll";
        else static_assert(hex::always_false<T>::value, "Invalid data type!");
    }

}

// these functions are exception because they just allow conversion from string to char*, they do not really add anything
namespace ImGui {
    bool InputText(const char* label, std::string &buffer, ImGuiInputTextFlags flags = ImGuiInputTextFlags_None);
    bool InputText(const char *label, std::u8string &buffer, ImGuiInputTextFlags flags = ImGuiInputTextFlags_None);
    bool InputTextMultiline(const char* label, std::string &buffer, const ImVec2& size = ImVec2(0, 0), ImGuiInputTextFlags flags = ImGuiInputTextFlags_None);
    bool InputTextWithHint(const char *label, const char *hint, std::string &buffer, ImGuiInputTextFlags flags = ImGuiInputTextFlags_None);

}