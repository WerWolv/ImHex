#include "content/views/view_data_inspector.hpp"

#include <hex/ui/imgui_imhex_extensions.h>

#include <hex/providers/provider.hpp>

#include <hex/api/achievement_manager.hpp>
#include <hex/helpers/logger.hpp>

#include <pl/pattern_language.hpp>
#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>

#include <wolv/io/file.hpp>
#include <wolv/utils/string.hpp>

namespace hex::plugin::builtin {

    using NumberDisplayStyle = ContentRegistry::DataInspector::NumberDisplayStyle;

    ViewDataInspector::ViewDataInspector() : View("hex.builtin.view.data_inspector.name") {
        // Handle region selection
        EventManager::subscribe<EventRegionSelected>(this, [this](const auto &region) {

            // Save current selection
            if (!ImHexApi::Provider::isValid() || region == Region::Invalid()) {
                this->m_validBytes = 0;
                this->m_selectedProvider = nullptr;
            } else {
                this->m_validBytes   = u64(region.getProvider()->getActualSize() - region.address);
                this->m_startAddress = region.address;
                this->m_selectedProvider = region.getProvider();
            }

            // Invalidate inspector rows
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


    void ViewDataInspector::updateInspectorRows() {
        this->m_updateTask = TaskManager::createBackgroundTask("Update Inspector", [this, validBytes = this->m_validBytes, startAddress = this->m_startAddress, endian = this->m_endian, invert = this->m_invert, numberDisplayStyle = this->m_numberDisplayStyle](auto &) {
            this->m_workData.clear();

            if (this->m_selectedProvider == nullptr)
               return;

            // Decode bytes using registered inspectors
            for (auto &entry : ContentRegistry::DataInspector::impl::getEntries()) {
               if (validBytes < entry.requiredSize)
                   continue;

               // Try to read as many bytes as requested and possible
               std::vector<u8> buffer(validBytes > entry.maxSize ? entry.maxSize : validBytes);
               this->m_selectedProvider->read(startAddress, buffer.data(), buffer.size());

               // Handle invert setting
               if (invert) {
                   for (auto &byte : buffer)
                       byte ^= 0xFF;
               }

               // Insert processed data into the inspector list
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

            // Setup a new pattern language runtime
            ContentRegistry::PatternLanguage::configureRuntime(this->m_runtime, this->m_selectedProvider);

            // Setup the runtime to read from the selected provider
            this->m_runtime.setDataSource(this->m_selectedProvider->getBaseAddress(), this->m_selectedProvider->getActualSize(),
                                         [this, invert](u64 offset, u8 *buffer, size_t size) {
                                             // Read bytes from the selected provider
                                             this->m_selectedProvider->read(offset, buffer, size);

                                             // Handle invert setting
                                             if (invert) {
                                                 for (auto &byte : std::span(buffer, size))
                                                     byte ^= 0xFF;
                                             }
                                         });

            // Prevent dangerous function calls
            this->m_runtime.setDangerousFunctionCallHandler([] { return false; });

            // Set the default endianness based on the endian setting
            this->m_runtime.setDefaultEndian(endian);

            // Set start address to the selected address
            this->m_runtime.setStartAddress(startAddress);

            // Loop over all files in the inspectors folder and execute them
            for (const auto &folderPath : fs::getDefaultPaths(fs::ImHexPath::Inspectors)) {
               for (const auto &filePath : std::fs::recursive_directory_iterator(folderPath)) {

                   // Skip non-files and files that don't end with .hexpat
                   if (!filePath.exists() || !filePath.is_regular_file() || filePath.path().extension() != ".hexpat")
                       continue;

                   // Read the inspector file
                   wolv::io::File file(filePath, wolv::io::File::Mode::Read);
                   if (file.isValid()) {
                       auto inspectorCode = file.readString();

                       // Execute the inspector file
                       if (!inspectorCode.empty()) {
                           if (this->m_runtime.executeString(inspectorCode, {}, inVariables, true)) {

                               // Loop over patterns produced by the runtime
                               const auto &patterns = this->m_runtime.getPatterns();
                               for (const auto &pattern : patterns) {
                                   // Skip hidden patterns
                                   if (pattern->getVisibility() == pl::ptrn::Visibility::Hidden)
                                       continue;

                                   // Set up the editing function if a write formatter is available
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
                                       // Set up the display function using the pattern's formatter
                                       auto displayFunction = [value = pattern->getFormattedValue()]() {
                                           ImGui::TextUnformatted(value.c_str());
                                           return value;
                                       };

                                       // Insert the inspector into the list
                                       this->m_workData.push_back({
                                            pattern->getDisplayName(),
                                            displayFunction,
                                            editingFunction,
                                            false
                                        });

                                       AchievementManager::unlockAchievement("hex.builtin.achievement.patterns", "hex.builtin.achievement.patterns.data_inspector.name");
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

    void ViewDataInspector::drawContent() {
        if (this->m_dataValid && !this->m_updateTask.isRunning()) {
            this->m_dataValid = false;
            this->m_cachedData = std::move(this->m_workData);
        }

        if (this->m_shouldInvalidate && !this->m_updateTask.isRunning()) {
            this->m_shouldInvalidate = false;

            this->updateInspectorRows();
        }

        if (ImGui::Begin(View::toWindowName("hex.builtin.view.data_inspector.name").c_str(), &this->getWindowOpenState(), ImGuiWindowFlags_NoCollapse)) {
            if (this->m_selectedProvider != nullptr && this->m_selectedProvider->isReadable() && this->m_validBytes > 0) {
                if (ImGui::BeginTable("##datainspector", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg, ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * (this->m_cachedData.size() + 1)))) {
                    ImGui::TableSetupScrollFreeze(0, 1);
                    ImGui::TableSetupColumn("hex.builtin.view.data_inspector.table.name"_lang);
                    ImGui::TableSetupColumn("hex.builtin.view.data_inspector.table.value"_lang);

                    ImGui::TableHeadersRow();

                    int inspectorRowId = 1;
                    for (auto &[unlocalizedName, displayFunction, editingFunction, editing] : this->m_cachedData) {
                        ImGui::PushID(inspectorRowId);
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();

                        // Render inspector row name
                        ImGui::TextUnformatted(LangEntry(unlocalizedName));
                        ImGui::TableNextColumn();

                        if (!editing) {
                            // Handle regular display case

                            // Render inspector row value
                            const auto &copyValue = displayFunction();

                            ImGui::SameLine();

                            // Handle copying the value to the clipboard when clicking the row
                            if (ImGui::Selectable("##InspectorLine", false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap)) {
                                ImGui::SetClipboardText(copyValue.c_str());
                            }

                            // Enter editing mode when double-clicking the row
                            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && editingFunction.has_value()) {
                                editing              = true;
                                this->m_editingValue = copyValue;
                            }

                        } else {
                            // Handle editing mode

                            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                            ImGui::SetNextItemWidth(-1);
                            ImGui::SetKeyboardFocusHere();

                            // Draw input text box
                            if (ImGui::InputText("##InspectorLineEditing", this->m_editingValue, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
                                // Turn the entered value into bytes
                                auto bytes = editingFunction.value()(this->m_editingValue, this->m_endian);

                                // Write those bytes to the selected provider at the current address
                                this->m_selectedProvider->write(this->m_startAddress, bytes.data(), bytes.size());

                                // Disable editing mode
                                this->m_editingValue.clear();
                                editing                  = false;

                                // Reload all inspector rows
                                this->m_shouldInvalidate = true;
                            }
                            ImGui::PopStyleVar();

                            // Disable editing mode when clicking outside the input text box
                            if (!ImGui::IsItemHovered() && ImGui::IsAnyMouseDown()) {
                                this->m_editingValue.clear();
                                editing = false;
                            }
                        }

                        ImGui::PopID();
                        inspectorRowId++;
                    }

                    ImGui::EndTable();
                }

                ImGui::NewLine();
                ImGui::Separator();
                ImGui::NewLine();

                // Draw inspector settings

                // Draw endian setting
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

                // Draw radix setting
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

                // Draw invert setting
                {
                    int selection = this->m_invert ? 1 : 0;
                    std::array options = { "hex.builtin.common.no"_lang, "hex.builtin.common.yes"_lang };

                    if (ImGui::SliderInt("hex.builtin.view.data_inspector.invert"_lang, &selection, 0, options.size() - 1, options[selection], ImGuiSliderFlags_NoInput)) {
                        this->m_shouldInvalidate = true;

                        this->m_invert = selection == 1;
                    }
                }
            } else {
                // Draw a message when no bytes are selected
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