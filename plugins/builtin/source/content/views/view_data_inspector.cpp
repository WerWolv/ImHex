#include "content/views/view_data_inspector.hpp"

#include <hex/ui/imgui_imhex_extensions.h>

#include <hex/providers/provider.hpp>

#include <cstring>

#include <hex/helpers/logger.hpp>
#include <pl/pattern_language.hpp>
#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>

namespace hex::plugin::builtin {

    using NumberDisplayStyle = ContentRegistry::DataInspector::NumberDisplayStyle;

    ViewDataInspector::ViewDataInspector() : View("hex.builtin.view.data_inspector.name") {
        EventManager::subscribe<EventRegionSelected>(this, [this](Region region) {
            auto provider = ImHexApi::Provider::get();

            if (!ImHexApi::Provider::isValid() || region.address == (size_t)-1) {
                this->m_validBytes = 0;
            } else {
                this->m_validBytes   = u64(provider->getActualSize() - region.address);
                this->m_startAddress = region.address;
            }

            this->m_shouldInvalidate = true;
        });
    }

    ViewDataInspector::~ViewDataInspector() {
        EventManager::unsubscribe<EventRegionSelected>(this);
    }

    void ViewDataInspector::drawContent() {
        if (this->m_shouldInvalidate) {
            this->m_shouldInvalidate = false;
            this->m_cachedData.clear();

            auto provider = ImHexApi::Provider::get();

            // Decode bytes using registered inspectors
            for (auto &entry : ContentRegistry::DataInspector::getEntries()) {
                if (this->m_validBytes < entry.requiredSize)
                    continue;

                std::vector<u8> buffer(this->m_validBytes > entry.maxSize ? entry.maxSize : this->m_validBytes);
                provider->read(this->m_startAddress, buffer.data(), buffer.size());

                if (this->m_invert) {
                    for (auto &byte : buffer)
                        byte ^= 0xFF;
                }

                InspectorCacheEntry cacheEntry = {
                        entry.unlocalizedName,
                        entry.generatorFunction(buffer, this->m_endian, this->m_numberDisplayStyle),
                        entry.editingFunction,
                        false
                };

                this->m_cachedData.push_back(cacheEntry);
            }

            // Decode bytes using custom inspectors defined using the pattern language
            pl::PatternLanguage runtime;
            ContentRegistry::PatternLanguage::configureRuntime(runtime, nullptr);

            runtime.setDataSource([this, provider](u64 offset, u8 *buffer, size_t size) {
                provider->read(offset, buffer, size);

                if (this->m_invert) {
                    for (size_t i = 0; i < size; i++)
                        buffer[i] ^= 0xFF;
                }
            }, provider->getBaseAddress(), provider->getActualSize());

            runtime.setDangerousFunctionCallHandler([]{ return false; });
            runtime.setDefaultEndian(this->m_endian);
            runtime.setStartAddress(this->m_startAddress);

            std::map<std::string, pl::core::Token::Literal> inVariables = {
                { "numberDisplayStyle", u128(this->m_numberDisplayStyle) }
            };

            bool ranSuccessfully = false;
            for (const auto &path : fs::getDefaultPaths(fs::ImHexPath::Config)) {
                auto inspectorFilePath = path / "inspectors.hexpat";

                if (fs::exists(inspectorFilePath) && fs::isRegularFile(inspectorFilePath)) {
                    ranSuccessfully = runtime.executeFile(inspectorFilePath, {}, inVariables, true);
                    break;
                }
            }

            if (ranSuccessfully) {
                const auto &patterns = runtime.getAllPatterns();

                for (const auto &pattern : patterns) {
                    InspectorCacheEntry cacheEntry = {
                            pattern->getDisplayName(),
                            [value = pattern->getFormattedValue()]() {
                                ImGui::TextUnformatted(value.c_str());
                                return value;
                            },
                            std::nullopt,
                            false
                    };

                    this->m_cachedData.push_back(cacheEntry);
                }
            } else {
                const auto& error = runtime.getError();

                if (error.has_value()) {
                    log::error("Failed to execute inspectors.hexpat:\n {}", error.value().what());
                } else {
                    log::error("Failed to execute inspectors.hexpat");
                }
            }
        }

        if (ImGui::Begin(View::toWindowName("hex.builtin.view.data_inspector.name").c_str(), &this->getWindowOpenState(), ImGuiWindowFlags_NoCollapse)) {
            auto provider = ImHexApi::Provider::get();

            if (ImHexApi::Provider::isValid() && provider->isReadable() && this->m_validBytes > 0) {
                if (ImGui::BeginTable("##datainspector", 2, ImGuiTableFlags_ScrollY | ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg, ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * (this->m_cachedData.size() + 1)))) {
                    ImGui::TableSetupScrollFreeze(0, 1);
                    ImGui::TableSetupColumn("hex.builtin.view.data_inspector.table.name"_lang);
                    ImGui::TableSetupColumn("hex.builtin.view.data_inspector.table.value"_lang);

                    ImGui::TableHeadersRow();

                    u32 i = 0;
                    for (auto &[unlocalizedName, displayFunction, editingFunction, editing] : this->m_cachedData) {
                        ImGui::PushID(i);
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(LangEntry(unlocalizedName));
                        ImGui::TableNextColumn();

                        if (!editing) {
                            const auto &copyValue = displayFunction();
                            ImGui::SameLine();

                            if (ImGui::Selectable("##InspectorLine", false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap)) {
                                ImGui::SetClipboardText(copyValue.c_str());
                            }

                            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && editingFunction.has_value()) {
                                editing              = true;
                                this->m_editingValue = copyValue;
                            }

                        } else {
                            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                            ImGui::SetKeyboardFocusHere();
                            if (ImGui::InputText("##InspectorLineEditing", this->m_editingValue, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
                                auto bytes = (*editingFunction)(this->m_editingValue, this->m_endian);

                                provider->write(this->m_startAddress, bytes.data(), bytes.size());
                                this->m_editingValue.clear();
                                editing                  = false;
                                this->m_shouldInvalidate = true;
                            }
                            ImGui::PopStyleVar();

                            if (!ImGui::IsItemHovered() && ImGui::IsAnyMouseDown()) {
                                this->m_editingValue.clear();
                                editing = false;
                            }
                        }

                        ImGui::PopID();
                        i++;
                    }

                    ImGui::EndTable();
                }

                ImGui::NewLine();
                ImGui::Separator();
                ImGui::NewLine();

                {
                    int selection = [this] {
                       switch (this->m_endian) {
                           default:
                           case std::endian::little:    return 0;
                           case std::endian::big:       return 1;
                       }
                    }();

                    std::array options = { "hex.builtin.common.little"_lang, "hex.builtin.common.big"_lang };

                    if (ImGui::SliderInt("hex.builtin.common.endian"_lang, &selection, 0, options.size() - 1, options[selection], ImGuiSliderFlags_NoInput)) {
                        this->m_shouldInvalidate = true;

                        switch (selection) {
                            default:
                            case 0: this->m_endian = std::endian::little;   break;
                            case 1: this->m_endian = std::endian::big;      break;
                        }
                    }
                }

                {
                    int selection = [this] {
                        switch (this->m_numberDisplayStyle) {
                            default:
                            case NumberDisplayStyle::Decimal:       return 0;
                            case NumberDisplayStyle::Hexadecimal:   return 1;
                            case NumberDisplayStyle::Octal:         return 2;
                        }
                    }();
                    std::array options = { "hex.builtin.common.decimal"_lang, "hex.builtin.common.hexadecimal"_lang, "hex.builtin.common.octal"_lang };

                    if (ImGui::SliderInt("hex.builtin.common.number_format"_lang, &selection, 0, options.size() - 1, options[selection], ImGuiSliderFlags_NoInput)) {
                        this->m_shouldInvalidate = true;

                        switch (selection) {
                            default:
                            case 0: this->m_numberDisplayStyle =  NumberDisplayStyle::Decimal;     break;
                            case 1: this->m_numberDisplayStyle =  NumberDisplayStyle::Hexadecimal; break;
                            case 2: this->m_numberDisplayStyle =  NumberDisplayStyle::Octal;       break;
                        }
                    }
                }

                {
                    int selection = this->m_invert ? 1 : 0;
                    std::array options = { "hex.builtin.common.no"_lang, "hex.builtin.common.yes"_lang };

                    if (ImGui::SliderInt("hex.builtin.view.data_inspector.invert"_lang, &selection, 0, options.size() - 1, options[selection], ImGuiSliderFlags_NoInput)) {
                        this->m_shouldInvalidate = true;

                        this->m_invert = selection == 1;
                    }
                }
            } else {
                std::string text    = "hex.builtin.view.data_inspector.no_data"_lang;
                auto textSize       = ImGui::CalcTextSize(text.c_str());
                auto availableSpace = ImGui::GetContentRegionAvail();

                ImGui::SetCursorPos((availableSpace - textSize) / 2.0F);
                ImGui::TextUnformatted(text.c_str());
            }
        }
        ImGui::End();
    }

}