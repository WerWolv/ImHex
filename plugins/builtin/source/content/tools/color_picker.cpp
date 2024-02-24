#include <hex/helpers/utils.hpp>
#include <hex/helpers/fmt.hpp>

#include <hex/api/localization_manager.hpp>

#include <imgui.h>

#include <hex/ui/imgui_imhex_extensions.h>
#include <nlohmann/json.hpp>
#include <romfs/romfs.hpp>

namespace hex::plugin::builtin {

    void drawColorPicker() {
        static std::array<float, 4> pickedColor = { 0 };
        static std::string rgba8;

        struct BitValue {
            int         bits;
            float       color;
            float       saturationMultiplier;
            const char *name;
            u8          index;
        };

        static std::array bitValues = {
            BitValue{ 8, 0.00F, 1.0F, "R", 0 },
            BitValue{ 8, 0.33F, 1.0F, "G", 1 },
            BitValue{ 8, 0.66F, 1.0F, "B", 2 },
            BitValue{ 8, 0.00F, 0.0F, "A", 3 }
        };

        if (ImGui::BeginTable("##color_picker_table", 3, ImGuiTableFlags_BordersInnerV)) {
            ImGui::TableSetupColumn(hex::format(" {}", "hex.builtin.tools.color"_lang).c_str(), ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, 300_scaled);
            ImGui::TableSetupColumn(hex::format(" {}", "hex.builtin.tools.color.components"_lang).c_str(), ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, 105_scaled);
            ImGui::TableSetupColumn(hex::format(" {}", "hex.builtin.tools.color.formats"_lang).c_str(), ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoResize);

            ImGui::TableHeadersRow();

            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            // Draw main color picker widget
            {
                ImGui::PushItemWidth(-1);
                ImGui::ColorPicker4("hex.builtin.tools.color"_lang, pickedColor.data(), ImGuiColorEditFlags_Uint8 | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_DisplayHSV | ImGuiColorEditFlags_DisplayHex);
                ImGui::ColorButton("##color_button", ImColor(pickedColor[0], pickedColor[1], pickedColor[2], pickedColor[3]), ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_AlphaPreviewHalf, ImVec2(300_scaled, 0));
                ImGui::PopItemWidth();
            }

            ImGui::TableNextColumn();

            const auto colorFormatName = hex::format("{}{}{}{}",
                bitValues[0].bits > 0 ? bitValues[0].name : "",
                bitValues[1].bits > 0 ? bitValues[1].name : "",
                bitValues[2].bits > 0 ? bitValues[2].name : "",
                bitValues[3].bits > 0 ? bitValues[3].name : ""
            );

            // Draw color bit count sliders
            {
                ImGui::Indent();

                static auto drawBitsSlider = [&](BitValue *bitValue) {
                    // Change slider color
                    ImGui::PushStyleColor(ImGuiCol_FrameBg,         ImColor::HSV(bitValue->color, 0.5F * bitValue->saturationMultiplier, 0.5F).Value);
                    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,  ImColor::HSV(bitValue->color, 0.6F * bitValue->saturationMultiplier, 0.5F).Value);
                    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,   ImColor::HSV(bitValue->color, 0.7F * bitValue->saturationMultiplier, 0.5F).Value);
                    ImGui::PushStyleColor(ImGuiCol_SliderGrab,      ImColor::HSV(bitValue->color, 0.9F * bitValue->saturationMultiplier, 0.9F).Value);

                    // Draw slider
                    ImGui::PushID(&bitValue->bits);
                    auto format = hex::format("%d\n{}", bitValue->name);
                    ImGui::VSliderInt("##slider", ImVec2(18_scaled, 350_scaled), &bitValue->bits, 0, 16, format.c_str(), ImGuiSliderFlags_AlwaysClamp);
                    ImGui::PopID();

                    ImGui::PopStyleColor(4);
                };

                // Force sliders closer together
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));

                // Draw a slider for each color component
                for (u32 index = 0; auto &bitValue : bitValues) {
                    // Draw slider
                    drawBitsSlider(&bitValue);

                    // Configure drag and drop source and target
                    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoPreviewTooltip)) {
                        // Set the current slider index as the payload
                        ImGui::SetDragDropPayload("BIT_VALUE", &index, sizeof(u32));

                        // Draw a color button to show the color being dragged
                        ImGui::ColorButton("##color_button", ImColor::HSV(bitValue.color, 0.5F * bitValue.saturationMultiplier, 0.5F).Value);

                        ImGui::EndDragDropSource();
                    }
                    if (ImGui::BeginDragDropTarget()) {
                        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("BIT_VALUE"); payload != nullptr) {
                            auto otherIndex = *static_cast<const u32 *>(payload->Data);

                            // Swap the currently hovered slider with the one being dragged
                            std::swap(bitValues[index], bitValues[otherIndex]);
                        }
                        ImGui::EndDragDropTarget();
                    }

                    ImGui::SameLine();

                    index += 1;
                }

                ImGui::NewLine();

                // Draw color name below sliders
                ImGuiExt::TextFormatted("{}", colorFormatName);

                ImGui::PopStyleVar();

                ImGui::Unindent();
            }

            ImGui::TableNextColumn();

            // Draw encoded color values
            {
                // Calculate int and float representations of the selected color
                std::array<u32, 4> intColor = {};
                std::array<float, 4> floatColor = {};
                for (u32 index = 0; auto &bitValue : bitValues) {
                    intColor[index] = u64(std::round(static_cast<long double>(pickedColor[bitValue.index]) * std::numeric_limits<u32>::max())) >> (32 - bitValue.bits);
                    floatColor[index] = pickedColor[bitValue.index];

                    index += 1;
                }

                // Draw a table with the color values
                if (ImGui::BeginTable("##value_table", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoHostExtendX , ImVec2(230_scaled, 0))) {
                    ImGui::TableSetupColumn("name",  ImGuiTableColumnFlags_WidthFixed);
                    ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);

                    const static auto drawValue = [](const char *name, auto formatter) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();

                        // Draw name of the formatting
                        ImGui::TextUnformatted(name);

                        ImGui::TableNextColumn();

                        // Draw value
                        ImGui::PushID(name);
                        ImGuiExt::TextFormattedSelectable("{}", formatter());
                        ImGui::PopID();
                    };

                    const u32 bitCount = bitValues[0].bits + bitValues[1].bits + bitValues[2].bits + bitValues[3].bits;

                    // Draw the different representations

                    drawValue("hex.builtin.tools.color.formats.hex"_lang, [&] {
                        u64 hexValue = 0;
                        for (u32 index = 0; auto &bitValue : bitValues) {
                            hexValue <<= bitValue.bits;
                            hexValue |= u64(intColor[index]) & hex::bitmask(bitValue.bits);
                            index += 1;
                        }

                        return hex::format("#{0:0{1}X}", hexValue, bitCount / 4);
                    });

                    drawValue(colorFormatName.c_str(), [&] {
                        return hex::format("{}({}, {}, {}, {})", colorFormatName, intColor[0], intColor[1], intColor[2], intColor[3]);
                    });

                    drawValue("hex.builtin.tools.color.formats.vec4"_lang, [&] {
                        return hex::format("{{ {:.2}F, {:.2}F, {:.2}F, {:.2}F }}", floatColor[0], floatColor[1], floatColor[2], floatColor[3]);
                    });

                    drawValue("hex.builtin.tools.color.formats.percent"_lang, [&] {
                        return hex::format("{{ {}%, {}%, {}%, {}% }}", u32(floatColor[0] * 100), u32(floatColor[1] * 100), u32(floatColor[2] * 100), u32(floatColor[3] * 100));
                    });

                    drawValue("hex.builtin.tools.color.formats.color_name"_lang, [&] -> std::string {
                        const static auto ColorTable = [] {
                            auto colorMap = nlohmann::json::parse(romfs::get("assets/common/color_names.json").string()).get<std::map<std::string, std::string>>();

                            std::map<u8, std::map<u8, std::map<u8, std::string>>> result;
                            for (const auto &[colorValue, colorName] : colorMap) {
                                result
                                [hex::parseHexString(colorValue.substr(0, 2))[0]]
                                [hex::parseHexString(colorValue.substr(2, 2))[0]]
                                [hex::parseHexString(colorValue.substr(4, 2))[0]] = colorName;
                            }

                            return result;
                        }();

                        const auto r = pickedColor[0] * 0xFF;
                        const auto g = pickedColor[1] * 0xFF;
                        const auto b = pickedColor[2] * 0xFF;

                        auto gTable = ColorTable.lower_bound(r);
                        if (gTable == ColorTable.end())
                            return "???";

                        auto bTable = gTable->second.lower_bound(g);
                        if (bTable == gTable->second.end())
                            return "???";

                        auto name = bTable->second.lower_bound(b);
                        if (name == bTable->second.end())
                            return "???";

                        return name->second;
                    });

                    ImGui::EndTable();
                }
            }

            ImGui::EndTable();
        }
    }

}