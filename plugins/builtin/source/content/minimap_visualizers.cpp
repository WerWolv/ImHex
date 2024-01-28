#include <hex/api/content_registry.hpp>

#include <imgui.h>
#include <hex/ui/imgui_imhex_extensions.h>

#include <hex/helpers/utils.hpp>

#include <wolv/utils/string.hpp>


namespace hex::plugin::builtin {

    namespace {

        ImColor entropyMiniMapVisualizer(const std::vector<u8> &data) {
            std::array<u32, 256> frequencies = { 0 };
            for (u8 byte : data)
                frequencies[byte]++;

            double entropy = 0.0;
            for (u32 frequency : frequencies) {
                if (frequency == 0)
                    continue;

                double probability = static_cast<double>(frequency) / data.size();
                entropy -= probability * std::log2(probability);
            }

            // Calculate color
            ImColor color = ImColor::HSV(0.0F, 0.0F, 1.0F);

            if (entropy > 0.0) {
                double hue = std::clamp(entropy / 8.0, 0.0, 1.0);
                color = ImColor::HSV(static_cast<float>(hue) / 0.75F, 0.8F, 1.0F);
            }

            return color;
        }

        ImColor zerosMiniMapVisualizer(const std::vector<u8> &data) {
            u32 zerosCount = 0;
            for (u8 byte : data) {
                if (byte == 0x00)
                    zerosCount++;
            }

            return ImColor::HSV(0.0F, 0.0F, 1.0F - (double(zerosCount) / data.size()));
        }

    }

    void registerMiniMapVisualizers() {
        ContentRegistry::HexEditor::addMiniMapVisualizer("hex.builtin.minimap_visualizer.entropy", entropyMiniMapVisualizer);
        ContentRegistry::HexEditor::addMiniMapVisualizer("hex.builtin.minimap_visualizer.zeros", zerosMiniMapVisualizer);
    }

}