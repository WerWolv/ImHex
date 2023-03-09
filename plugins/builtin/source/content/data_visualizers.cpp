#include <hex/api/content_registry.hpp>

#include <imgui.h>
#include <hex/ui/imgui_imhex_extensions.h>
#include <hex/helpers/logger.hpp>
#include <hex/helpers/utils.hpp>

namespace hex::plugin::builtin {

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

    template<std::integral T>
    class DataVisualizerHexadecimal : public hex::ContentRegistry::HexEditor::DataVisualizer {
    public:
        DataVisualizerHexadecimal() : DataVisualizer(ByteCount, CharCount) { }

        void draw(u64 address, const u8 *data, size_t size, bool upperCase) override {
            hex::unused(address);

            if (size == ByteCount)
                ImGui::Text(getFormatString(upperCase), *reinterpret_cast<const T*>(data));
            else
                ImGui::TextFormatted("{: {}s}", CharCount);
        }

        bool drawEditing(u64 address, u8 *data, size_t size, bool upperCase, bool startedEditing) override {
            hex::unused(address, startedEditing);

            if (size == ByteCount) {
                return drawDefaultScalarEditingTextBox(address, getFormatString(upperCase), getImGuiDataType<T>(), data, ImGuiInputTextFlags_CharsHexadecimal);
            }
            else
                return false;
        }

    private:
        constexpr static inline auto ByteCount = sizeof(T);
        constexpr static inline auto CharCount = ByteCount * 2;

        const static inline auto FormattingUpperCase = hex::format("%0{}{}X", CharCount, getFormatLengthSpecifier<T>());
        const static inline auto FormattingLowerCase = hex::format("%0{}{}x", CharCount, getFormatLengthSpecifier<T>());

        const char *getFormatString(bool upperCase) {
            if (upperCase)
                return FormattingUpperCase.c_str();
            else
                return FormattingLowerCase.c_str();
        }
    };

    class DataVisualizerHexii : public hex::ContentRegistry::HexEditor::DataVisualizer {
    public:
        DataVisualizerHexii() : DataVisualizer(ByteCount, CharCount) { }

        void draw(u64 address, const u8 *data, size_t size, bool upperCase) override {
            hex::unused(address);

            if (size == ByteCount) {
                const u8 c = data[0];
                switch (c) {
                    case 0x00:
                        ImGui::Text("  ");
                        break;
                    case 0xFF:
                        ImGui::TextDisabled("##");
                        break;
                    default:
                        if (c >= ' ' && c <= '~')
                            ImGui::Text(".%c", c);
                        else
                            ImGui::Text(getFormatString(upperCase), c);
                        break;
                }
            }
            else
                ImGui::TextFormatted("{: {}s}", CharCount);
        }

        bool drawEditing(u64 address, u8 *data, size_t size, bool upperCase, bool startedEditing) override {
            hex::unused(address, startedEditing);

            if (size == ByteCount) {
                return drawDefaultScalarEditingTextBox(address, getFormatString(upperCase), getImGuiDataType<u8>(), data, ImGuiInputTextFlags_CharsHexadecimal);
            }
            else
                return false;
        }

    private:
        constexpr static inline auto ByteCount = 1;
        constexpr static inline auto CharCount = ByteCount * 2;

        const static inline auto FormattingUpperCase = hex::format("%0{}{}X", CharCount, getFormatLengthSpecifier<u8>());
        const static inline auto FormattingLowerCase = hex::format("%0{}{}x", CharCount, getFormatLengthSpecifier<u8>());

        static const char *getFormatString(bool upperCase) {
            if (upperCase)
                return FormattingUpperCase.c_str();
            else
                return FormattingLowerCase.c_str();
        }
    };

    template<std::integral T>
    class DataVisualizerDecimal : public hex::ContentRegistry::HexEditor::DataVisualizer {
    public:
        DataVisualizerDecimal() : DataVisualizer(ByteCount, CharCount) { }

        void draw(u64 address, const u8 *data, size_t size, bool upperCase) override {
            hex::unused(address, upperCase);

            if (size == ByteCount) {
                if (std::is_signed<T>::value)
                    ImGui::Text(getFormatString(), static_cast<i64>(*reinterpret_cast<const T*>(data)));
                else
                    ImGui::Text(getFormatString(), static_cast<u64>(*reinterpret_cast<const T*>(data)));
            }
            else
                ImGui::TextFormatted("{: {}s}", CharCount);
        }

        bool drawEditing(u64 address, u8 *data, size_t size, bool upperCase, bool startedEditing) override {
            hex::unused(address, upperCase, startedEditing);

            if (size == ByteCount) {
                return ImGui::InputScalar(
                           "##hex_input",
                           getImGuiDataType<T>(),
                           data,
                           nullptr,
                           nullptr,
                           nullptr,
                           DataVisualizer::TextInputFlags);
            }
            else
                return false;
        }

    private:
        constexpr static inline auto ByteCount = sizeof(T);
        constexpr static inline auto CharCount = std::numeric_limits<T>::digits10 + 2;

        const static inline auto FormatString = hex::format("%{}{}{}", CharCount, getFormatLengthSpecifier<T>(), std::is_signed<T>::value ? "d" : "u");

        const char *getFormatString() {
            return FormatString.c_str();
        }
    };

    enum class Float16 : u16 {};

    template<typename T>
    class DataVisualizerFloatingPoint : public hex::ContentRegistry::HexEditor::DataVisualizer {
    public:
        DataVisualizerFloatingPoint() : DataVisualizer(ByteCount, CharCount) { }

        void draw(u64 address, const u8 *data, size_t size, bool upperCase) override {
            hex::unused(address);

            if (size == ByteCount)
                ImGui::Text(getFormatString(upperCase), *reinterpret_cast<const T*>(data));
            else
                ImGui::TextFormatted("{: {}s}", CharCount);
        }

        bool drawEditing(u64 address, u8 *data, size_t size, bool upperCase, bool startedEditing) override {
            hex::unused(address, upperCase, startedEditing);

            if (size == ByteCount) {
                return ImGui::InputScalar(
                           "##hex_input",
                           getImGuiDataType<T>(),
                           data,
                           nullptr,
                           nullptr,
                           nullptr,
                           DataVisualizer::TextInputFlags | ImGuiInputTextFlags_CharsScientific);
            }
            else
                return false;
        }

    private:
        constexpr static inline auto ByteCount = sizeof(T);
        constexpr static inline auto CharCount = 14;

        const static inline auto FormatStringUpperCase = hex::format("%{}G", CharCount);
        const static inline auto FormatStringLowerCase = hex::format("%{}g", CharCount);

        const char *getFormatString(bool upperCase) {
            if (upperCase)
                return FormatStringUpperCase.c_str();
            else
                return FormatStringLowerCase.c_str();
        }
    };

    template<>
    class DataVisualizerFloatingPoint<Float16> : public hex::ContentRegistry::HexEditor::DataVisualizer {
    public:
        DataVisualizerFloatingPoint() : DataVisualizer(ByteCount, CharCount) { }

        void draw(u64 address, const u8 *data, size_t size, bool upperCase) override {
            hex::unused(address);

            if (size == ByteCount)
                ImGui::Text(getFormatString(upperCase), hex::float16ToFloat32(*reinterpret_cast<const u16*>(data)));
            else
                ImGui::TextFormatted("{: {}s}", CharCount);
        }

        bool drawEditing(u64 address, u8 *data, size_t size, bool upperCase, bool startedEditing) override {
            hex::unused(startedEditing);

            this->draw(address, data, size, upperCase);
            return false;
        }

    private:
        constexpr static inline auto ByteCount = sizeof(Float16);
        constexpr static inline auto CharCount = 14;

        const static inline auto FormatStringUpperCase = hex::format("%{}G", CharCount);
        const static inline auto FormatStringLowerCase = hex::format("%{}g", CharCount);

        static const char *getFormatString(bool upperCase) {
            if (upperCase)
                return FormatStringUpperCase.c_str();
            else
                return FormatStringLowerCase.c_str();
        }
    };

    class DataVisualizerRGBA8 : public hex::ContentRegistry::HexEditor::DataVisualizer {
    public:
        DataVisualizerRGBA8() : DataVisualizer(4, 2) { }

        void draw(u64 address, const u8 *data, size_t size, bool upperCase) override {
            hex::unused(address, upperCase);

            if (size == 4)
                ImGui::ColorButton("##color", ImColor(data[0], data[1], data[2], data[3]), ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoDragDrop, ImVec2(ImGui::GetColumnWidth(), ImGui::GetTextLineHeight()));
            else
                ImGui::ColorButton("##color", ImColor(0, 0, 0, 0xFF), ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoDragDrop, ImVec2(ImGui::GetColumnWidth(), ImGui::GetTextLineHeight()));
        }

        bool drawEditing(u64 address, u8 *data, size_t size, bool upperCase, bool startedEditing) override {
            hex::unused(address, data, size, upperCase);

            if (startedEditing) {
                this->m_currColor = { float(data[0]) / 0xFF, float(data[1]) / 0xFF, float(data[2]) / 0xFF, float(data[3]) / 0xFF };
                ImGui::OpenPopup("##color_popup");
            }

            ImGui::ColorButton("##color", ImColor(this->m_currColor[0], this->m_currColor[1], this->m_currColor[2], this->m_currColor[3]), ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoDragDrop, ImVec2(ImGui::GetColumnWidth(), ImGui::GetTextLineHeight()));

            if (ImGui::BeginPopup("##color_popup")) {
                if (ImGui::ColorPicker4("##picker", this->m_currColor.data(), ImGuiColorEditFlags_AlphaBar)) {
                    for (u8 i = 0; i < 4; i++)
                        data[i] = this->m_currColor[i] * 0xFF;
                }
                ImGui::EndPopup();
            } else {
                return true;
            }

            return false;
        }

        std::array<float, 4> m_currColor = { 0 };

    };

    class DataVisualizerBinary : public hex::ContentRegistry::HexEditor::DataVisualizer {
    public:
        DataVisualizerBinary() : DataVisualizer(1, 8) { }

        void draw(u64 address, const u8 *data, size_t size, bool) override {
            hex::unused(address);

            if (size == 1)
                ImGui::TextFormatted("{:08b}", *data);
        }

        bool drawEditing(u64 address, u8 *data, size_t, bool, bool startedEditing) override {
            hex::unused(address, startedEditing);

            if (startedEditing) {
                this->m_inputBuffer = hex::format("{:08b}", *data);
            }

            if (drawDefaultTextEditingTextBox(address, this->m_inputBuffer, ImGuiInputTextFlags_None)) {
                hex::trim(this->m_inputBuffer);
                if (auto result = hex::parseBinaryString(this->m_inputBuffer); result.has_value()) {
                    *data = result.value();
                    return true;
                }
            }

            return false;
        }

    private:
        std::string m_inputBuffer;
    };

    void registerDataVisualizers() {
        ContentRegistry::HexEditor::addDataVisualizer<DataVisualizerHexadecimal<u8>>("hex.builtin.visualizer.hexadecimal.8bit");
        ContentRegistry::HexEditor::addDataVisualizer<DataVisualizerHexadecimal<u16>>("hex.builtin.visualizer.hexadecimal.16bit");
        ContentRegistry::HexEditor::addDataVisualizer<DataVisualizerHexadecimal<u32>>("hex.builtin.visualizer.hexadecimal.32bit");
        ContentRegistry::HexEditor::addDataVisualizer<DataVisualizerHexadecimal<u64>>("hex.builtin.visualizer.hexadecimal.64bit");

        ContentRegistry::HexEditor::addDataVisualizer<DataVisualizerDecimal<u8>>("hex.builtin.visualizer.decimal.unsigned.8bit");
        ContentRegistry::HexEditor::addDataVisualizer<DataVisualizerDecimal<u16>>("hex.builtin.visualizer.decimal.unsigned.16bit");
        ContentRegistry::HexEditor::addDataVisualizer<DataVisualizerDecimal<u32>>("hex.builtin.visualizer.decimal.unsigned.32bit");
        ContentRegistry::HexEditor::addDataVisualizer<DataVisualizerDecimal<u64>>("hex.builtin.visualizer.decimal.unsigned.64bit");

        ContentRegistry::HexEditor::addDataVisualizer<DataVisualizerDecimal<i8>>("hex.builtin.visualizer.decimal.signed.8bit");
        ContentRegistry::HexEditor::addDataVisualizer<DataVisualizerDecimal<i16>>("hex.builtin.visualizer.decimal.signed.16bit");
        ContentRegistry::HexEditor::addDataVisualizer<DataVisualizerDecimal<i32>>("hex.builtin.visualizer.decimal.signed.32bit");
        ContentRegistry::HexEditor::addDataVisualizer<DataVisualizerDecimal<i64>>("hex.builtin.visualizer.decimal.signed.64bit");

        ContentRegistry::HexEditor::addDataVisualizer<DataVisualizerFloatingPoint<Float16>>("hex.builtin.visualizer.floating_point.16bit");
        ContentRegistry::HexEditor::addDataVisualizer<DataVisualizerFloatingPoint<float>>("hex.builtin.visualizer.floating_point.32bit");
        ContentRegistry::HexEditor::addDataVisualizer<DataVisualizerFloatingPoint<double>>("hex.builtin.visualizer.floating_point.64bit");

        ContentRegistry::HexEditor::addDataVisualizer<DataVisualizerRGBA8>("hex.builtin.visualizer.rgba8");
        ContentRegistry::HexEditor::addDataVisualizer<DataVisualizerHexii>("hex.builtin.visualizer.hexii");

        ContentRegistry::HexEditor::addDataVisualizer<DataVisualizerBinary>("hex.builtin.visualizer.binary");
    }

}