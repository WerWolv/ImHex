#include "content/views/view_data_inspector.hpp"

#include <algorithm>
#include <hex/api/achievement_manager.hpp>
#include <hex/api/content_registry/settings.hpp>
#include <hex/providers/provider.hpp>
#include <hex/helpers/logger.hpp>
#include <hex/helpers/default_paths.hpp>
#include <hex/api/events/events_interaction.hpp>

#include <fonts/vscode_icons.hpp>
#include <ui/pattern_drawer.hpp>
#include <ui/visualizer_drawer.hpp>

#include <hex/ui/imgui_imhex_extensions.h>
#include <imgui_internal.h>

#include <pl/pattern_language.hpp>
#include <pl/patterns/pattern.hpp>

#include <wolv/utils/string.hpp>

#include <ranges>
#include <fonts/tabler_icons.hpp>
#include <ui/widgets.hpp>

namespace hex::plugin::builtin {

    using NumberDisplayStyle = ContentRegistry::DataInspector::NumberDisplayStyle;

    ViewDataInspector::ViewDataInspector() : View::Window("hex.builtin.view.data_inspector.name", ICON_VS_INSPECT) {
        // Handle region selection
        EventRegionSelected::subscribe(this, [this](const auto &region) {
            // Save current selection
            if (!ImHexApi::Provider::isValid() || region == Region::Invalid()) {
                m_validBytes = 0;
                m_selectedProvider = nullptr;
            } else {
                m_validBytes   = u64((region.getProvider()->getBaseAddress() + region.getProvider()->getActualSize()) - region.address);
                m_startAddress = region.address;
                m_selectedProvider = region.getProvider();
            }

            // Invalidate inspector rows
            m_shouldInvalidate = true;
        });

        EventDataChanged::subscribe(this, [this](const auto &provider) {
            if (provider == m_selectedProvider)
                m_shouldInvalidate = true;
        });

        EventProviderClosed::subscribe(this, [this](const auto*) {
            m_selectedProvider = nullptr;
        });

        ContentRegistry::Settings::onChange("hex.builtin.setting.data_inspector", "hex.builtin.setting.data_inspector.hidden_rows", [this](const ContentRegistry::Settings::SettingsValue &value) {
            auto filterValues = value.get<std::vector<std::string>>({});
            m_hiddenValues = std::set(filterValues.begin(), filterValues.end());
        });

        ShortcutManager::addShortcut(this, CTRLCMD + Keys::E, "hex.builtin.view.data_inspector.toggle_endianness", [this] {
            if (m_endian == std::endian::little) m_endian = std::endian::big;
            else m_endian = std::endian::little;
            m_shouldInvalidate = true;
        });
    }

    ViewDataInspector::~ViewDataInspector() {
        EventRegionSelected::unsubscribe(this);
        EventProviderClosed::unsubscribe(this);
    }

    void ViewDataInspector::updateInspectorRows() {
        m_updateTask = TaskManager::createBackgroundTask("hex.builtin.task.updating_inspector", [this](auto &) {
            this->updateInspectorRowsTask();
        });
    }

    static u8 reverseBits(u8 byte) {
        byte = (byte & 0xF0) >> 4 | (byte & 0x0F) << 4;
        byte = (byte & 0xCC) >> 2 | (byte & 0x33) << 2;
        byte = (byte & 0xAA) >> 1 | (byte & 0x55) << 1;
        return byte;
    }

    void ViewDataInspector::preprocessBytes(std::span<u8> data) {
        // Handle invert setting
        if (m_invert) {
            for (auto &byte : data)
                byte ^= 0xFF;
        }

        // Handle reverse setting
        if (m_reverse) {
            for (auto &byte : data)
                byte = reverseBits(byte);
        }
    }

    void ViewDataInspector::updateInspectorRowsTask() {
        m_workData.clear();

        if (m_selectedProvider == nullptr)
            return;

        // Decode bytes using registered inspectors
        for (auto &entry : ContentRegistry::DataInspector::impl::getEntries()) {
            if (m_validBytes < entry.requiredSize)
                continue;

            // Try to read as many bytes as requested and possible
            std::vector<u8> buffer(m_validBytes > entry.maxSize ? entry.maxSize : m_validBytes);
            m_selectedProvider->read(m_startAddress, buffer.data(), buffer.size());

            preprocessBytes(buffer);

            // Insert processed data into the inspector list
            m_workData.emplace_back(
                entry.unlocalizedName,
                entry.generatorFunction(buffer, m_endian, m_numberDisplayStyle),
                entry.editingFunction,
                false,
                entry.requiredSize,
                entry.unlocalizedName
            );
        }

        // Execute custom inspectors
        this->executeInspectors();

        m_dataValid = true;
    }

    void ViewDataInspector::inspectorReadFunction(u64 offset, u8 *buffer, size_t size) {
        m_selectedProvider->read(offset, buffer, size);

        preprocessBytes({ buffer, size });
    }

    void ViewDataInspector::executeInspectors() {
        // Decode bytes using custom inspectors defined using the pattern language
        const std::map<std::string, pl::core::Token::Literal> inVariables = {
                { "numberDisplayStyle", u128(u64(m_numberDisplayStyle)) }
        };

        // Setup a new pattern language runtime
        ContentRegistry::PatternLanguage::configureRuntime(m_runtime, m_selectedProvider);

        // Setup the runtime to read from the selected provider
        m_runtime.setDataSource(m_selectedProvider->getBaseAddress(), m_selectedProvider->getActualSize(), [this](u64 offset, u8 *buffer, size_t size) {
            this->inspectorReadFunction(offset, buffer, size);
        });

        // Prevent dangerous function calls
        m_runtime.setDangerousFunctionCallHandler([] { return false; });

        // Set the default endianness based on the endian setting
        m_runtime.setDefaultEndian(m_endian);

        // Set start address to the selected address
        m_runtime.setStartAddress(m_startAddress);

        // Loop over all files in the inspectors folder and execute them
        for (const auto &folderPath : paths::Inspectors.read()) {
            for (const auto &entry: std::fs::recursive_directory_iterator(folderPath)) {
                const auto &filePath = entry.path();
                // Skip non-files and files that don't end with .hexpat
                if (!entry.exists() || !entry.is_regular_file() || filePath.extension() != ".hexpat")
                    continue;

                // Read the inspector file
                wolv::io::File file(filePath, wolv::io::File::Mode::Read);

                if (!file.isValid()) continue;
                auto inspectorCode = file.readString();

                // Execute the inspector file
                if (inspectorCode.empty()) continue;
                this->executeInspector(inspectorCode, filePath, inVariables);
            }
        }
    }

    void ViewDataInspector::executeInspector(const std::string& code, const std::fs::path& path, const std::map<std::string, pl::core::Token::Literal>& inVariables) {
        if (m_runtime.executeString(code, pl::api::Source::DefaultSource, {}, inVariables, true) == 0) {

            auto displayFunction = createPatternErrorDisplayFunction();

            // Insert the inspector containing the error message into the list
            m_workData.emplace_back(
                wolv::util::toUTF8String(path.filename()),
                std::move(displayFunction),
                std::nullopt,
                false,
                0,
                wolv::util::toUTF8String(path)
            );

            return;
        }

        // Loop over patterns produced by the runtime
        const auto &patterns = m_runtime.getPatterns();
        for (const auto &pattern: patterns) {
            // Skip hidden patterns
            if (pattern->getVisibility() == pl::ptrn::Visibility::Hidden)
                continue;
            if (pattern->getVisibility() == pl::ptrn::Visibility::TreeHidden)
                continue;

            // Set up the editing function if a write formatter is available
            std::optional<ContentRegistry::DataInspector::impl::EditingFunction> editingFunction;
            if (!pattern->getWriteFormatterFunction().empty()) {
                editingFunction = ContentRegistry::DataInspector::EditWidget::TextInput([&pattern](const std::string &value, std::endian) -> std::vector<u8> {
                    try {
                        pattern->setValue(value);
                    } catch (const pl::core::err::EvaluatorError::Exception &error) {
                        log::error("Failed to set value of pattern '{}' to '{}': {}",
                                   pattern->getDisplayName(), value, error.what());
                    }

                    return {};
                });
            }

            try {
                // Set up the display function using the pattern's formatter
                auto displayFunction = [pattern,value = pattern->getFormattedValue()] {
                    auto drawer = ui::VisualizerDrawer();
                    if (const auto &inlineVisualizeArgs = pattern->getAttributeArguments("hex::inline_visualize"); !inlineVisualizeArgs.empty()) {
                        drawer.drawVisualizer(ContentRegistry::PatternLanguage::impl::getInlineVisualizers(), inlineVisualizeArgs, *pattern, true);
                    } else {
                        ImGui::TextUnformatted(value.c_str());
                    }
                    return value;
                };

                // Insert the inspector into the list
                m_workData.emplace_back(
                    pattern->getDisplayName(),
                    displayFunction,
                    editingFunction,
                    false,
                    pattern->getSize(),
                    wolv::util::toUTF8String(path) + ":" + pattern->getVariableName()
                );

                AchievementManager::unlockAchievement("hex.builtin.achievement.patterns",
                                                      "hex.builtin.achievement.patterns.data_inspector.name");
            } catch (const pl::core::err::EvaluatorError::Exception &) {
                auto displayFunction = createPatternErrorDisplayFunction();

                // Insert the inspector containing the error message into the list
                m_workData.emplace_back(
                    wolv::util::toUTF8String(path.filename()),
                    std::move(displayFunction),
                    std::nullopt,
                    false,
                    0,
                    wolv::util::toUTF8String(path)
                );
            }
        }
    }

    void ViewDataInspector::drawContent() {
        if (m_dataValid && !m_updateTask.isRunning()) {
            m_dataValid = false;
            m_cachedData = std::move(m_workData);
        }

        if (m_shouldInvalidate && !m_updateTask.isRunning()) {
            m_shouldInvalidate = false;

            this->updateInspectorRows();
        }

        const auto selection = ImHexApi::HexEditor::getSelection();
        const auto selectedEntryIt = std::ranges::find_if(m_cachedData, [this](const InspectorCacheEntry &entry) {
            return entry.unlocalizedName == m_selectedEntryName;
        });

        u64 requiredSize = selectedEntryIt == m_cachedData.end() ? 0x00 : selectedEntryIt->requiredSize;

        bool noData = m_selectedProvider == nullptr || !m_selectedProvider->isReadable() || m_validBytes <= 0;

        ImGui::BeginDisabled(noData || !selection.has_value() || !m_selectedEntryName.has_value());
        {
            const auto buttonSizeSmall = ImVec2(ImGui::GetTextLineHeightWithSpacing() * 1.5F, 0);
            const auto buttonSize = ImVec2((ImGui::GetContentRegionAvail().x / 2) - buttonSizeSmall.x - ImGui::GetStyle().FramePadding.x * 3, 0);
            const auto baseAddress = noData ? 0x00 : m_selectedProvider->getBaseAddress();
            const auto providerSize = noData ? 0x00 : m_selectedProvider->getActualSize();
            const auto providerEndAddress = baseAddress + providerSize;

            ImGui::BeginDisabled(!selection.has_value() || providerSize < requiredSize || selection->getStartAddress() < baseAddress + requiredSize);
            if (ImGuiExt::DimmedIconButton(ICON_TA_CHEVRON_LEFT_PIPE, ImGui::GetStyleColorVec4(ImGuiCol_Text), buttonSizeSmall)) {
                ImHexApi::HexEditor::setSelection(Region { .address=selection->getStartAddress() % requiredSize, .size=requiredSize });
            }
            ImGui::SameLine();
            if (ImGuiExt::DimmedIconButton(ICON_TA_CHEVRON_LEFT, ImGui::GetStyleColorVec4(ImGuiCol_Text), buttonSize)) {
                ImHexApi::HexEditor::setSelection(Region { .address=selection->getStartAddress() - requiredSize, .size=requiredSize });
            }
            ImGui::EndDisabled();

            ImGui::SameLine();

            ImGui::BeginDisabled(!selection.has_value() || providerSize < requiredSize || selection->getEndAddress() >= providerEndAddress - requiredSize);
            if (ImGuiExt::DimmedIconButton(ICON_TA_CHEVRON_RIGHT, ImGui::GetStyleColorVec4(ImGuiCol_Text), buttonSize)) {
                ImHexApi::HexEditor::setSelection(Region { .address=selection->getStartAddress() + requiredSize, .size=requiredSize });
            }
            ImGui::SameLine();
            if (ImGuiExt::DimmedIconButton(ICON_TA_CHEVRON_RIGHT_PIPE, ImGui::GetStyleColorVec4(ImGuiCol_Text), buttonSizeSmall)) {
                ImHexApi::HexEditor::setSelection(Region { .address=providerEndAddress - selection->getStartAddress() % requiredSize - requiredSize, .size=requiredSize });
            }
            ImGui::EndDisabled();
        }
        ImGui::EndDisabled();

        static bool hideSettings = true;

        if (ImGui::BeginTable("##datainspector", noData ? 1 : (m_tableEditingModeEnabled ? 3 : 2),
                              ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
                              ImVec2(0, ImGui::GetContentRegionAvail().y - ImGui::GetTextLineHeightWithSpacing() * (hideSettings ? 1.25 : 7.25)))) {
            if (noData) {
                ImGuiExt::TextOverlay("hex.builtin.view.data_inspector.no_data"_lang, ImGui::GetWindowPos() + ImGui::GetWindowSize() / 2, ImGui::GetWindowWidth() * 0.7);
            } else {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn("hex.builtin.view.data_inspector.table.name"_lang,
                                        ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("hex.builtin.view.data_inspector.table.value"_lang,
                                        ImGuiTableColumnFlags_WidthStretch);

                if (m_tableEditingModeEnabled)
                    ImGui::TableSetupColumn("##favorite", ImGuiTableColumnFlags_WidthFixed, ImGui::GetTextLineHeight());

                ImGui::TableHeadersRow();

                this->drawInspectorRows();
                if (m_tableEditingModeEnabled) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TableNextColumn();
                    ImGuiExt::HelpHover("hex.builtin.view.data_inspector.custom_row.hint"_lang, ICON_VS_INFO);
                    ImGui::SameLine();
                    ImGui::TextUnformatted("hex.builtin.view.data_inspector.custom_row.title"_lang);
                }
            }

            ImGui::EndTable();
        }

        // Draw inspector settings
        const auto width = ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(ICON_VS_EDIT).x - ImGui::GetStyle().ItemSpacing.x * 2;
        if (ImGuiExt::BeginSubWindow("hex.ui.common.settings"_lang, &hideSettings, hideSettings ? ImVec2(width, 1) : ImVec2(0, 0))) {
            ImGui::BeginDisabled(noData);
            ImGuiExt::DimmedButtonToggle(fmt::format("{}  {}", ICON_VS_EDIT, "hex.ui.common.edit"_lang).c_str(), &m_tableEditingModeEnabled, ImVec2(-1, 0));
            ImGui::EndDisabled();

            ImGui::Separator();

            ImGui::PushItemWidth(-1);
            {
                // Draw endian setting
                this->drawEndianSetting();

                // Draw radix setting
                this->drawRadixSetting();

                // Draw invert and reverse setting
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x / 2 - ImGui::GetStyle().ItemSpacing.x / 2);
                this->drawInvertSetting();
                ImGui::SameLine();
                this->drawReverseSetting();
                ImGui::PopItemWidth();
            }
            ImGui::PopItemWidth();
        }
        ImGuiExt::EndSubWindow();

        if (hideSettings) {
            ImGui::SameLine();
            ImGui::BeginDisabled(noData);
            ImGuiExt::DimmedButtonToggle(ICON_VS_EDIT, &m_tableEditingModeEnabled);
            ImGui::EndDisabled();
            ImGui::SetItemTooltip("%s", "hex.ui.common.edit"_lang.get());
        }
    }

    void ViewDataInspector::drawInspectorRows() {
        int inspectorRowId = 1;
        for (auto &entry : m_cachedData) {
            ON_SCOPE_EXIT {
                ImGui::PopID();
                inspectorRowId++;
            };

            ImGui::PushID(inspectorRowId);

            bool grayedOut = m_hiddenValues.contains(entry.filterValue);
            if (!m_tableEditingModeEnabled && grayedOut)
                continue;

            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            ImGui::BeginDisabled(grayedOut);

            this->drawInspectorRow(entry);

            ImGui::EndDisabled();

            if (!m_tableEditingModeEnabled) {
                continue;
            }

            ImGui::TableNextColumn();

            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_Text));

            bool hidden = m_hiddenValues.contains(entry.filterValue);
            if (ImGuiExt::DimmedButton(hidden ? ICON_VS_EYE : ICON_VS_EYE_CLOSED)) {
                if (hidden)
                    m_hiddenValues.erase(entry.filterValue);
                else
                    m_hiddenValues.insert(entry.filterValue);

                std::vector filterValues(m_hiddenValues.begin(), m_hiddenValues.end());

                ContentRegistry::Settings::write<std::vector<std::string>>(
                        "hex.builtin.setting.data_inspector",
                        "hex.builtin.setting.data_inspector.hidden_rows", filterValues);
            }

            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
        }
    }

    void ViewDataInspector::drawInspectorRow(InspectorCacheEntry& entry) {
        // Render inspector row name
        ImGui::TextUnformatted(Lang(entry.unlocalizedName));
        ImGui::TableNextColumn();

        if (!entry.editing) {
            // Handle regular display case

            if (ImGui::BeginPopup("##DataInspectorRowContextMenu")) {
                ImGuiExt::TextFormattedDisabled("{} bits", entry.requiredSize * 8);
                ImGui::Separator();
                ImGui::EndPopup();
            }

            // Render inspector row value
            const auto &copyValue = entry.displayFunction();

            ImGui::SameLine();

            // Handle copying the value to the clipboard when clicking the row
            if (ImGui::Selectable("##InspectorLine", m_selectedEntryName == entry.unlocalizedName, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap | ImGuiSelectableFlags_AllowDoubleClick)) {
                m_selectedEntryName = entry.unlocalizedName;
                if (auto selection = ImHexApi::HexEditor::getSelection(); selection.has_value()) {
                    ImHexApi::HexEditor::setSelection(Region { .address=selection->getStartAddress(), .size=entry.requiredSize });
                }
            }

            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                m_selectedEntryName.reset();
            }

            // Enter editing mode when double-clicking the row
            const bool editable = entry.editingFunction.has_value() && m_selectedProvider->isWritable();
            if (ImGui::IsItemHovered()) {
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && editable) {
                    entry.editing = true;
                    m_editingValue = copyValue;
                    m_selectedEntryName.reset();
                }
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                    ImGui::OpenPopup("##DataInspectorRowContextMenu");
                }
            }

            if (ImGui::BeginPopup("##DataInspectorRowContextMenu")) {
                if (ImGui::MenuItemEx("hex.builtin.view.data_inspector.menu.copy"_lang, ICON_VS_COPY)) {
                    ImGui::SetClipboardText(copyValue.c_str());
                }
                if (ImGui::MenuItemEx("hex.builtin.view.data_inspector.menu.edit"_lang, ICON_VS_EDIT, nullptr, false, editable)) {
                    entry.editing = true;
                    m_editingValue = copyValue;
                    m_selectedEntryName.reset();
                }
                ImGui::EndPopup();
            }
        } else {
            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                entry.editing = false;
            }

            // Handle editing mode
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
            ImGui::SetNextItemWidth(-1);
            ImGui::SetKeyboardFocusHere();

            // Draw editing widget and capture edited value
            auto bytes = (*entry.editingFunction)(m_editingValue, m_endian, {});
            if (bytes.has_value()) {
                preprocessBytes(*bytes);

                // Write those bytes to the selected provider at the current address
                m_selectedProvider->write(m_startAddress, bytes->data(), bytes->size());

                // Disable editing mode
                m_editingValue.clear();
                entry.editing = false;

                // Reload all inspector rows
                m_shouldInvalidate = true;
            }

            ImGui::PopStyleVar();

            // Disable editing mode when clicking outside the input text box
            if (!ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                m_editingValue.clear();
                entry.editing = false;
            }
        }

    }

    void ViewDataInspector::drawEndianSetting() {
        if (ui::endiannessSlider(m_endian)) {
            m_shouldInvalidate = true;
        }
    }

    void ViewDataInspector::drawRadixSetting() {
        int selection = [this] {
            switch (m_numberDisplayStyle) {
                default:
                case NumberDisplayStyle::Decimal:
                    return 0;
                case NumberDisplayStyle::Hexadecimal:
                    return 1;
                case NumberDisplayStyle::Octal:
                    return 2;
            }
        }();

        std::array options = {
            fmt::format("{}:  {}", "hex.ui.common.number_format"_lang, "hex.ui.common.decimal"_lang),
            fmt::format("{}:  {}", "hex.ui.common.number_format"_lang, "hex.ui.common.hexadecimal"_lang),
            fmt::format("{}:  {}", "hex.ui.common.number_format"_lang, "hex.ui.common.octal"_lang)
        };

        if (ImGui::SliderInt("##format", &selection, 0, options.size() - 1, options[selection].c_str(), ImGuiSliderFlags_NoInput)) {
            m_shouldInvalidate = true;

            switch (selection) {
                default:
                case 0:
                    m_numberDisplayStyle = NumberDisplayStyle::Decimal;
                    break;
                case 1:
                    m_numberDisplayStyle = NumberDisplayStyle::Hexadecimal;
                    break;
                case 2:
                    m_numberDisplayStyle = NumberDisplayStyle::Octal;
                    break;
            }
        }
    }

    void ViewDataInspector::drawInvertSetting() {
        int selection = m_invert ? 1 : 0;

        std::array options = {
            fmt::format("{}:  {}", "hex.builtin.view.data_inspector.invert"_lang, "hex.ui.common.no"_lang),
            fmt::format("{}:  {}", "hex.builtin.view.data_inspector.invert"_lang, "hex.ui.common.yes"_lang)
        };

        if (ImGui::SliderInt("##invert", &selection, 0, options.size() - 1, options[selection].c_str(), ImGuiSliderFlags_NoInput)) {
            m_shouldInvalidate = true;

            m_invert = selection == 1;
        }
    }

    void ViewDataInspector::drawReverseSetting() {
        int selection = m_reverse ? 1 : 0;

        std::array options = {
            fmt::format("{}:  {}", "hex.builtin.view.data_inspector.reverse"_lang, "hex.ui.common.no"_lang),
            fmt::format("{}:  {}", "hex.builtin.view.data_inspector.reverse"_lang, "hex.ui.common.yes"_lang)
        };

        if (ImGui::SliderInt("##reverse", &selection, 0, options.size() - 1, options[selection].c_str(), ImGuiSliderFlags_NoInput)) {
            m_shouldInvalidate = true;

            m_reverse = selection == 1;
        }
    }

    ContentRegistry::DataInspector::impl::DisplayFunction ViewDataInspector::createPatternErrorDisplayFunction() {
        // Generate error message
        std::string errorMessage;
        if (const auto &compileErrors = m_runtime.getCompileErrors(); !compileErrors.empty()) {
            for (const auto &error : compileErrors) {
                errorMessage += fmt::format("{}\n", error.format());
            }
        } else if (const auto &evalError = m_runtime.getEvalError(); evalError.has_value()) {
            errorMessage += fmt::format("{}:{}  {}\n", evalError->line, evalError->column, evalError->message);
        }

        // Create a dummy display function that displays the error message
        auto displayFunction = [errorMessage = std::move(errorMessage)] {
            ImGuiExt::HelpHover(
                errorMessage.c_str(),
                "hex.builtin.view.data_inspector.execution_error"_lang,
                ImGuiExt::GetCustomColorU32(ImGuiCustomCol_LoggerError)
            );

            return errorMessage;
        };

        return displayFunction;
    }

    void ViewDataInspector::drawHelpText() {
        ImGuiExt::TextFormattedWrapped("This view decodes bytes, starting from the currently selected address in the Hex Editor View, as various different data types.");
        ImGui::NewLine();
        ImGuiExt::TextFormattedWrapped("The decoding here may or may not make sense depending on the actual data at the selected address but it can give a rough idea of what kind of data is present. If certain types make no sense, they can be hidden by entering the editing mode (pencil icon) and clicking the eye icon next to the corresponding row.");
        ImGui::NewLine();
        ImGuiExt::TextFormattedWrapped("By clicking on a row, the corresponding bytes will be selected in the Hex Editor View and you can use the navigation buttons at the top to move to the next or previous value, assuming you're dealing with a list of such values.");
        ImGuiExt::TextFormattedWrapped("Double-clicking a row (if editable) will allow you to change the value and write it back to the underlying data. Some types may also have additional options available in the context menu (right-click on a row).");
    }

}
