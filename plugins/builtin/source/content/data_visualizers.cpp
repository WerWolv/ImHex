#include <hex/api/content_registry.hpp>

#include <imgui.h>
#include <hex/ui/imgui_imhex_extensions.h>
#include <nlohmann/json.hpp>

namespace hex::plugin::builtin {

    static bool s_upperCaseHex = true;

    class DataVisualizerDefault : public hex::ContentRegistry::HexEditor::DataVisualizer {
    public:
        DataVisualizerDefault() : DataVisualizer(1, 2) { }

        void draw(u64 address, const u8 *data, size_t size) override {
            hex::unused(address);

            if (size == 1)
                ImGui::TextFormatted(s_upperCaseHex ? "{:02X}" : "{:02x}", data[0]);
            else
                ImGui::TextFormatted("  ");
        }
    };

    void registerDataVisualizers() {
        EventManager::subscribe<EventSettingsChanged>([] {
            auto upperCaseHex = ContentRegistry::Settings::getSetting("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.uppercase_hex");

            if (upperCaseHex.is_number())
                s_upperCaseHex = static_cast<int>(upperCaseHex);
        });

        ContentRegistry::HexEditor::addDataVisualizer<DataVisualizerDefault>("Byte Visualizer");
    }

}