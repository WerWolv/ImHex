#include <hex/api/content_registry.hpp>

#include <imgui.h>
#include <imgui_internal.h>
#include <hex/api/imhex_api.hpp>
#include <hex/ui/imgui_imhex_extensions.h>

#include <hex/helpers/utils.hpp>
#include <pl/patterns/pattern.hpp>
#include <wolv/utils/lock.hpp>

#include <wolv/utils/string.hpp>


namespace hex::plugin::builtin {

    namespace {

        void entropyMiniMapVisualizer(u64, std::span<const u8> data, std::vector<ImColor> &output) {
            std::array<u8, 256> frequencies = { 0 };
            for (u8 byte : data)
                frequencies[byte] += 1;

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

            output.push_back(color);
        }

        void zerosCountMiniMapVisualizer(u64, std::span<const u8> data, std::vector<ImColor> &output) {
            u32 zerosCount = 0;
            for (u8 byte : data) {
                if (byte == 0x00)
                    zerosCount += 1;
            }

            output.push_back(ImColor::HSV(0.0F, 0.0F, 1.0F - (double(zerosCount) / data.size())));
        }

        void zerosMiniMapVisualizer(u64, std::span<const u8> data, std::vector<ImColor> &output) {
            for (u8 byte : data) {
                if (byte == 0x00)
                    output.push_back(ImColor(1.0F, 1.0F, 1.0F, 1.0F));
                else
                    output.push_back(ImColor(0.0F, 0.0F, 0.0F, 1.0F));
            }
        }

        void byteTypeMiniMapVisualizer(u64, std::span<const u8> data, std::vector<ImColor> &output) {
            for (u8 byte : data) {
                if (std::isalpha(byte))
                    output.emplace_back(1.0F, 0.0F, 0.0F, 1.0F);
                else if (std::isdigit(byte))
                    output.emplace_back(0.0F, 1.0F, 0.0F, 1.0F);
                else if (std::isspace(byte))
                    output.emplace_back(0.0F, 0.0F, 1.0F, 1.0F);
                else if (std::iscntrl(byte))
                    output.emplace_back(0.5F, 0.5F, 0.5F, 1.0F);
                else
                    output.emplace_back(0.0F, 0.0F, 0.0F, 1.0F);
            }
        }

        void asciiCountMiniMapVisualizer(u64, std::span<const u8> data, std::vector<ImColor> &output) {
            u8 asciiCount = 0;
            for (u8 byte : data) {
                if (std::isprint(byte))
                    asciiCount += 1;
            }

            output.push_back(ImColor::HSV(0.5F, 0.5F, (double(asciiCount) / data.size())));
        }

        void byteMagnitudeMiniMapVisualizer(u64, std::span<const u8> data, std::vector<ImColor> &output) {
            for (u8 byte : data) {
                output.push_back(ImColor::HSV(0.0F, 0.0F, static_cast<float>(byte) / 255.0F));
            }
        }

        void highlightsMiniMapVisualizer(u64 address, std::span<const u8> data, std::vector<ImColor> &output) {
            for (size_t i = 0; i < data.size(); i += 1) {
                std::optional<ImColor> result;
                for (const auto &[id, callback] : ImHexApi::HexEditor::impl::getBackgroundHighlightingFunctions()) {
                    if (auto color = callback(address + i, data.data() + i, 1, result.has_value()); color.has_value())
                        result = color;
                }

                if (!result.has_value()) {
                    for (const auto &[id, highlighting] : ImHexApi::HexEditor::impl::getBackgroundHighlights()) {
                        if (highlighting.getRegion().overlaps({ address, 1 })) {
                            result = highlighting.getColor();
                            break;
                        }
                    }
                }

                if (result.has_value()) {
                    result->Value.w = 1.0F;
                } else {
                    if (auto region = ImHexApi::HexEditor::getSelection(); region.has_value()) {
                        if (region->overlaps({ address + i, 1 }))
                            result = 0x60C08080;
                    }
                }

                output.push_back(result.value_or(ImColor()));
            }
        }

    }

    void registerMiniMapVisualizers() {
        ContentRegistry::HexEditor::addMiniMapVisualizer("hex.builtin.minimap_visualizer.highlights",       highlightsMiniMapVisualizer);
        ContentRegistry::HexEditor::addMiniMapVisualizer("hex.builtin.minimap_visualizer.entropy",          entropyMiniMapVisualizer);
        ContentRegistry::HexEditor::addMiniMapVisualizer("hex.builtin.minimap_visualizer.zero_count",       zerosCountMiniMapVisualizer);
        ContentRegistry::HexEditor::addMiniMapVisualizer("hex.builtin.minimap_visualizer.zeros",            zerosMiniMapVisualizer);
        ContentRegistry::HexEditor::addMiniMapVisualizer("hex.builtin.minimap_visualizer.ascii_count",      asciiCountMiniMapVisualizer);
        ContentRegistry::HexEditor::addMiniMapVisualizer("hex.builtin.minimap_visualizer.byte_type",        byteTypeMiniMapVisualizer);
        ContentRegistry::HexEditor::addMiniMapVisualizer("hex.builtin.minimap_visualizer.byte_magnitude",   byteMagnitudeMiniMapVisualizer);
    }

}