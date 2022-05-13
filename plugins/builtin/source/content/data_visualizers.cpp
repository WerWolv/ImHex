#include <hex/api/content_registry.hpp>
#include <hex/providers/provider.hpp>

#include <imgui.h>
#include <hex/ui/imgui_imhex_extensions.h>

namespace hex::plugin::builtin {

    class DataVisualizerDefault : public hex::ContentRegistry::HexEditor::DataVisualizer {
    public:
        DataVisualizerDefault() : DataVisualizer(1, 2) { }

        void draw(u64 address, const u8 *data, size_t size, bool upperCase) override {
            hex::unused(address);

            if (size == 1)
                ImGui::TextFormatted(upperCase ? "{:02X}" : "{:02x}", data[0]);
            else
                ImGui::TextFormatted("  ");
        }

        bool drawEditing(u64 address, u8 *data, size_t size, bool upperCase) override {
            hex::unused(address);

            if (size == 1)
                return ImGui::InputScalar("##hex_input", ImGuiDataType_U8, data, nullptr, nullptr, upperCase ? "%02X" : "%02x", DataVisualizer::TextInputFlags);
            else
                return false;
        }
    };

    void registerDataVisualizers() {
        ContentRegistry::HexEditor::addDataVisualizer<DataVisualizerDefault>("Byte Visualizer");
    }

}