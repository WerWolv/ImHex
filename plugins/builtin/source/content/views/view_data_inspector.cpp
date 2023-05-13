#include "content/views/view_data_inspector.hpp"

#include <hex/ui/imgui_imhex_extensions.h>

#include <hex/providers/provider.hpp>

#include <cstring>

#include <hex/helpers/logger.hpp>

#include <pl/pattern_language.hpp>
#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>

#include <wolv/io/file.hpp>

namespace hex::plugin::builtin {

    using NumberDisplayStyle = ContentRegistry::DataInspector::NumberDisplayStyle;

    ViewDataInspector::ViewDataInspector() : View("hex.builtin.view.data_inspector.name") {
        EventManager::subscribe<EventRegionSelected>(this, [this](const auto &region) {
            if (!ImHexApi::Provider::isValid() || region == Region::Invalid()) {
                this->m_validBytes = 0;
                this->m_selectedProvider = nullptr;
            } else {
                this->m_validBytes   = u64(region.getProvider()->getActualSize() - region.address);
                this->m_startAddress = region.address;
                this->m_selectedProvider = region.getProvider();
            }

            this->m_shouldInvalidate = true;
        });

        EventManager::subscribe<EventProviderClosed>(this, [this](const auto*) {
            this->m_selectedProvider = nullptr;
        });
    }

    ViewDataInspector::~ViewDataInspector() {
        EventManager::unsubscribe<EventRegionSelected>(this);
        EventManager::unsubscribe<EventProviderClosed>(this);
    }

    void ViewDataInspector::drawContent() {
        if (this->m_dataValid && !this->m_updateTask.isRunning()) {
            this->m_dataValid = false;
            this->m_cachedData = std::move(this->m_workData);
        }

        if (this->m_shouldInvalidate && !this->m_updateTask.isRunning()) {
            this->m_shouldInvalidate = false;

            this->m_updateTask = TaskManager::createBackgroundTask("Update Inspector",
               [this, validBytes = this->m_validBytes, startAddress = this->m_startAddress, endian = this->m_endian, invert = this->m_invert, numberDisplayStyle = this->m_numberDisplayStyle](auto &) {
                this->m_workData.clear();

                if (this->m_selectedProvider == nullptr)
                    return;

                // Decode bytes using registered inspectors
                for (auto &entry : ContentRegistry::DataInspector::impl::getEntries()) {
                    if (validBytes < entry.requiredSize)
                        continue;

                    std::vector<u8> buffer(validBytes > entry.maxSize ? entry.maxSize : validBytes);
                    this->m_selectedProvider->read(startAddress, buffer.data(), buffer.size());

                    if (invert) {
                        for (auto &byte : buffer)
                            byte ^= 0xFF;
                    }

                    this->m_workData.push_back({
                        entry.unlocalizedName,
                        entry.generatorFunction(buffer, endian, numberDisplayStyle),
                        entry.editingFunction,
                        false
                    });
                }


                // Decode bytes using custom inspectors defined using the pattern language
                const std::map<std::string, pl::core::Token::Literal> inVariables = {
                        { "numberDisplayStyle", u128(numberDisplayStyle) }
                };

                ContentRegistry::PatternLanguage::configureRuntime(this->m_runtime, this->m_selectedProvider);

                this->m_runtime.setDataSource(this->m_selectedProvider->getBaseAddress(), this->m_selectedProvider->getActualSize(),
                [this, invert](u64 offset, u8 *buffer, size_t size) {
                    this->m_selectedProvider->read(offset, buffer, size);

                    if (invert) {
                        for (size_t i = 0; i < size; i++)
                            buffer[i] ^= 0xFF;
                    }
                });

                this->m_runtime.setDangerousFunctionCallHandler([]{ return false; });
                this->m_runtime.setDefaultEndian(endian);
                this->m_runtime.setStartAddress(startAddress);

                for (const auto &folderPath : fs::getDefaultPaths(fs::ImHexPath::Inspectors)) {
                    for (const auto &filePath : std::fs::recursive_directory_iterator(folderPath)) {
                        if (!filePath.exists() || !filePath.is_regular_file() || filePath.path().extension() != ".hexpat")
                            continue;

                        wolv::io::File file(filePath, wolv::io::File::Mode::Read);
                        if (file.isValid()) {
                            auto inspectorCode = file.readString();

                            if (!inspectorCode.empty()) {
                                if (this->m_runtime.executeString(inspectorCode, {}, inVariables, true)) {
                                    const auto &patterns = this->m_runtime.getPatterns();

                                    for (const auto &pattern : patterns) {
                                        if (pattern->getVisibility() == pl::ptrn::Visibility::Hidden)
                                            continue;

                                        auto formatWriteFunction = pattern->getWriteFormatterFunction();
                                        std::optional<ContentRegistry::DataInspector::impl::EditingFunction> editingFunction;
                                        if (!formatWriteFunction.empty()) {
                                            editingFunction = [formatWriteFunction, &pattern](const std::string &value, std::endian) -> std::vector<u8> {
                                                try {
                                                    pattern->setValue(value);
                                                } catch (const pl::core::err::EvaluatorError::Exception &error) {
                                                    log::error("Failed to set value of pattern '{}' to '{}': {}", pattern->getDisplayName(), value, error.what());
                                                    return { };
                                                }

                                                return { };
                                            };
                                        }

                                        try {
                                            this->m_workData.push_back({
                                                pattern->getDisplayName(),
                                                [value = pattern->getFormattedValue()]() {
                                                    ImGui::TextUnformatted(value.c_str());
                                                    return value;
                                                },
                                                editingFunction,
                                                false
                                            });
                                        } catch (const pl::core::err::EvaluatorError::Exception &error) {
                                            log::error("Failed to get value of pattern '{}': {}", pattern->getDisplayName(), error.what());
                                        }
                                    }
                                } else {
                                    const auto& error = this->m_runtime.getError();

                                    log::error("Failed to execute custom inspector file '{}'!", wolv::util::toUTF8String(filePath.path()));
                                    if (error.has_value())
                                        log::error("{}", error.value().what());
                                }
                            }
                        }
                    }
                }

                this->m_dataValid = true;

            });
        }

        if (ImGui::Begin(View::toWindowName("hex.builtin.view.data_inspector.name").c_str(), &this->getWindowOpenState(), ImGuiWindowFlags_NoCollapse)) {
            if (this->m_selectedProvider != nullptr && this->m_selectedProvider->isReadable() && this->m_validBytes > 0) {
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
                            ImGui::SetNextItemWidth(ImGui::GetColumnWidth());
                            ImGui::SetKeyboardFocusHere();
                            if (ImGui::InputText("##InspectorLineEditing", this->m_editingValue, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
                                auto bytes = (*editingFunction)(this->m_editingValue, this->m_endian);

                                this->m_selectedProvider->write(this->m_startAddress, bytes.data(), bytes.size());
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