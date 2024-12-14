#include <hex/api/content_registry.hpp>

#include <imgui.h>
#include <hex/ui/imgui_imhex_extensions.h>

#include <hex/helpers/utils.hpp>

#include <wolv/utils/string.hpp>


namespace hex::plugin::builtin {

    template<std::integral T>
    class DataVisualizerHexadecimal : public hex::ContentRegistry::HexEditor::DataVisualizer {
    public:
        explicit DataVisualizerHexadecimal(const std::string &name) : DataVisualizer(name, ByteCount, CharCount) { }

        void draw(u64 address, const u8 *data, size_t size, bool upperCase) override {
            std::ignore = address;

            if (size == ByteCount)
                ImGuiExt::TextFormatted(upperCase ? "{:0{}X}" : "{:0{}x}", *reinterpret_cast<const T*>(data), sizeof(T) * 2);
            else
                ImGuiExt::TextFormatted("{: {}s}", CharCount);
        }

        bool drawEditing(u64 address, u8 *data, size_t size, bool upperCase, bool startedEditing) override {
            std::ignore = address;
            std::ignore = startedEditing;

            if (size == ByteCount) {
                return drawDefaultScalarEditingTextBox(address, getFormatString(upperCase), ImGuiExt::getImGuiDataType<T>(), data, ImGuiInputTextFlags_CharsHexadecimal);
            } else {
                return false;
            }
        }

    private:
        constexpr static auto ByteCount = sizeof(T);
        constexpr static auto CharCount = ByteCount * 2;

        const static inline auto FormattingUpperCase = hex::format("%0{}{}X", CharCount, ImGuiExt::getFormatLengthSpecifier<T>());
        const static inline auto FormattingLowerCase = hex::format("%0{}{}x", CharCount, ImGuiExt::getFormatLengthSpecifier<T>());

        const char *getFormatString(bool upperCase) {
            if (upperCase)
                return FormattingUpperCase.c_str();
            else
                return FormattingLowerCase.c_str();
        }
    };

    class DataVisualizerHexii : public hex::ContentRegistry::HexEditor::DataVisualizer {
    public:
        DataVisualizerHexii() : DataVisualizer("hex.builtin.visualizer.hexii", ByteCount, CharCount) { }

        void draw(u64 address, const u8 *data, size_t size, bool upperCase) override {
            std::ignore = address;

            if (size == ByteCount) {
                const u8 c = data[0];
                switch (c) {
                    case 0x00:
                        ImGui::TextUnformatted("  ");
                        break;
                    case 0xFF:
                        ImGuiExt::TextFormattedDisabled("##");
                        break;
                    default:
                        if (c >= ' ' && c <= '~')
                            ImGuiExt::TextFormatted(".{:c}", char(c));
                        else
                            ImGui::Text(getFormatString(upperCase), c);
                        break;
                }
            } else {
                ImGuiExt::TextFormatted("{: {}s}", CharCount);
            }
        }

        bool drawEditing(u64 address, u8 *data, size_t size, bool upperCase, bool startedEditing) override {
            std::ignore = address;
            std::ignore = startedEditing;

            if (size == ByteCount) {
                return drawDefaultScalarEditingTextBox(address, getFormatString(upperCase), ImGuiExt::getImGuiDataType<u8>(), data, ImGuiInputTextFlags_CharsHexadecimal);
            } else {
                return false;
            }
        }

    private:
        constexpr static inline auto ByteCount = 1;
        constexpr static inline auto CharCount = ByteCount * 2;

        const static inline auto FormattingUpperCase = hex::format("%0{}{}X", CharCount, ImGuiExt::getFormatLengthSpecifier<u8>());
        const static inline auto FormattingLowerCase = hex::format("%0{}{}x", CharCount, ImGuiExt::getFormatLengthSpecifier<u8>());

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
        explicit DataVisualizerDecimal(const std::string &name) : DataVisualizer(name, ByteCount, CharCount) { }

        void draw(u64 address, const u8 *data, size_t size, bool upperCase) override {
            std::ignore = address;
            std::ignore = upperCase;

            if (size == ByteCount) {
                if (std::is_signed_v<T>)
                    ImGui::Text(getFormatString(), static_cast<i64>(*reinterpret_cast<const T*>(data)));
                else
                    ImGui::Text(getFormatString(), static_cast<u64>(*reinterpret_cast<const T*>(data)));
            } else {
                ImGuiExt::TextFormatted("{: {}s}", CharCount);
            }
        }

        bool drawEditing(u64 address, u8 *data, size_t size, bool upperCase, bool startedEditing) override {
            std::ignore = address;
            std::ignore = upperCase;
            std::ignore = startedEditing;

            if (size == ByteCount) {
                return drawDefaultScalarEditingTextBox(address, FormatString.c_str(), ImGuiExt::getImGuiDataType<T>(), data, ImGuiInputTextFlags_None);
            } else {
                return false;
            }
        }

    private:
        constexpr static auto ByteCount = sizeof(T);
        constexpr static auto CharCount = std::numeric_limits<T>::digits10 + 2;

        const static inline auto FormatString = hex::format("%{}{}{}", CharCount, ImGuiExt::getFormatLengthSpecifier<T>(), std::is_signed_v<T> ? "d" : "u");

        const char *getFormatString() {
            return FormatString.c_str();
        }
    };

    enum class Float16 : u16 {};

    template<typename T>
    class DataVisualizerFloatingPoint : public hex::ContentRegistry::HexEditor::DataVisualizer {
    public:
        explicit DataVisualizerFloatingPoint(const std::string &name) : DataVisualizer(name, ByteCount, CharCount) { }

        void draw(u64 address, const u8 *data, size_t size, bool upperCase) override {
            std::ignore = address;

            if (size == ByteCount)
                ImGui::Text(getFormatString(upperCase), *reinterpret_cast<const T*>(data));
            else
                ImGuiExt::TextFormatted("{: {}s}", CharCount);
        }

        bool drawEditing(u64 address, u8 *data, size_t size, bool upperCase, bool startedEditing) override {
            std::ignore = address;
            std::ignore = upperCase;
            std::ignore = startedEditing;

            if (size == ByteCount) {
                return drawDefaultScalarEditingTextBox(address, getFormatString(upperCase), ImGuiExt::getImGuiDataType<T>(), data, ImGuiInputTextFlags_CharsScientific);
            } else {
                return false;
            }
        }

    private:
        constexpr static inline auto ByteCount = sizeof(T);
        constexpr static inline auto CharCount = 14;

        const static inline auto FormatStringUpperCase = hex::format("%{}G", CharCount);
        const static inline auto FormatStringLowerCase = hex::format("%{}g", CharCount);

        const char *getFormatString(bool upperCase) const {
            if (upperCase)
                return FormatStringUpperCase.c_str();
            else
                return FormatStringLowerCase.c_str();
        }
    };

    template<>
    class DataVisualizerFloatingPoint<Float16> : public hex::ContentRegistry::HexEditor::DataVisualizer {
    public:
        explicit DataVisualizerFloatingPoint(const std::string &name) : DataVisualizer(name, ByteCount, CharCount) { }

        void draw(u64 address, const u8 *data, size_t size, bool upperCase) override {
            std::ignore = address;

            if (size == ByteCount)
                ImGui::Text(getFormatString(upperCase), hex::float16ToFloat32(*reinterpret_cast<const u16*>(data)));
            else
                ImGuiExt::TextFormatted("{: {}s}", CharCount);
        }

        bool drawEditing(u64 address, u8 *data, size_t size, bool upperCase, bool startedEditing) override {
            std::ignore = startedEditing;

            this->draw(address, data, size, upperCase);
            return false;
        }

    private:
        constexpr static auto ByteCount = sizeof(Float16);
        constexpr static auto CharCount = 14;

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
        DataVisualizerRGBA8() : DataVisualizer("hex.builtin.visualizer.rgba8", 4, 2) { }

        void draw(u64 address, const u8 *data, size_t size, bool upperCase) override {
            std::ignore = address;
            std::ignore = upperCase;

            if (size == 4)
                ImGui::ColorButton("##color", ImColor(data[0], data[1], data[2], data[3]), ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoDragDrop, ImVec2(ImGui::GetColumnWidth(), ImGui::GetTextLineHeight()));
            else
                ImGui::ColorButton("##color", ImColor(0, 0, 0, 0xFF), ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoDragDrop, ImVec2(ImGui::GetColumnWidth(), ImGui::GetTextLineHeight()));
        }

        bool drawEditing(u64 address, u8 *data, size_t size, bool upperCase, bool startedEditing) override {
            std::ignore = address;
            std::ignore = data;
            std::ignore = size;
            std::ignore = upperCase;

            m_currColor = { float(data[0]) / 0xFF, float(data[1]) / 0xFF, float(data[2]) / 0xFF, float(data[3]) / 0xFF };
            if (startedEditing) {
                ImGui::OpenPopup("##color_popup");
            }

            ImGui::ColorButton("##color", ImColor(m_currColor[0], m_currColor[1], m_currColor[2], m_currColor[3]), ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoDragDrop, ImVec2(ImGui::GetColumnWidth(), ImGui::GetTextLineHeight()));

            if (ImGui::BeginPopup("##color_popup")) {
                if (ImGui::ColorPicker4("##picker", m_currColor.data(), ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_InputRGB)) {
                    for (u8 i = 0; i < 4; i++)
                        data[i] = m_currColor[i] * 0xFF;
                }
                ImGui::EndPopup();
            } else {
                return true;
            }

            return false;
        }

    private:
        std::array<float, 4> m_currColor = { };
    };

    class DataVisualizerBinary : public hex::ContentRegistry::HexEditor::DataVisualizer {
    public:
        DataVisualizerBinary() : DataVisualizer("hex.builtin.visualizer.binary", 1, 8) { }

        void draw(u64 address, const u8 *data, size_t size, bool) override {
            std::ignore = address;

            if (size == 1)
                ImGuiExt::TextFormatted("{:08b}", *data);
        }

        bool drawEditing(u64 address, u8 *data, size_t, bool, bool startedEditing) override {
            std::ignore = address;
            std::ignore = startedEditing;

            if (startedEditing) {
                m_inputBuffer = hex::format("{:08b}", *data);
            }

            if (drawDefaultTextEditingTextBox(address, m_inputBuffer, ImGuiInputTextFlags_None)) {
                if (auto result = hex::parseBinaryString(wolv::util::trim(m_inputBuffer)); result.has_value()) {
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

        ContentRegistry::HexEditor::addDataVisualizer<DataVisualizerRGBA8>();
        ContentRegistry::HexEditor::addDataVisualizer<DataVisualizerHexii>();

        ContentRegistry::HexEditor::addDataVisualizer<DataVisualizerBinary>();
    }

}