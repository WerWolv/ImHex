#include "content/views/view_data_inspector.hpp"

#include <hex/api/achievement_manager.hpp>
#include <hex/providers/provider.hpp>
#include <hex/helpers/logger.hpp>
#include <hex/helpers/default_paths.hpp>

#include <fonts/vscode_icons.hpp>
#include <hex/ui/imgui_imhex_extensions.h>
#include <ui/pattern_drawer.hpp>
#include <ui/visualizer_drawer.hpp>

#include <pl/pattern_language.hpp>
#include <pl/patterns/pattern.hpp>

#include <wolv/utils/string.hpp>

#include <ranges>

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
    }

    ViewDataInspector::~ViewDataInspector() {
        EventRegionSelected::unsubscribe(this);
        EventProviderClosed::unsubscribe(this);
    }

    void ViewDataInspector::updateInspectorRows() {
        m_updateTask = TaskManager::createBackgroundTask("hex.builtin.task.updating_inspector"_lang, [this](auto &) {
            this->updateInspectorRowsTask();
        });
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

            // Handle invert setting
            if (m_invert) {
                for (auto &byte : buffer)
                    byte ^= 0xFF;
            }

            // Insert processed data into the inspector list
            m_workData.emplace_back(
                entry.unlocalizedName,
                entry.generatorFunction(buffer, m_endian, m_numberDisplayStyle),
                entry.editingFunction,
                false,
                entry.unlocalizedName
            );
        }

        // Execute custom inspectors
        this->executeInspectors();

        m_dataValid = true;
    }

    void ViewDataInspector::inspectorReadFunction(u64 offset, u8 *buffer, size_t size) {
        m_selectedProvider->read(offset, buffer, size);

        // Handle invert setting
        if (m_invert) {
            for (auto &byte : std::span(buffer, size))
                byte ^= 0xFF;
        }
    }

    void ViewDataInspector::executeInspectors() {
        // Decode bytes using custom inspectors defined using the pattern language
        const std::map<std::string, pl::core::Token::Literal> inVariables = {
                { "numberDisplayStyle", u128(m_numberDisplayStyle) }
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
        if (!m_runtime.executeString(code, pl::api::Source::DefaultSource, {}, inVariables, true)) {

            auto displayFunction = createPatternErrorDisplayFunction();

            // Insert the inspector into the list
            m_workData.emplace_back(
                wolv::util::toUTF8String(path.filename()),
                std::move(displayFunction),
                std::nullopt,
                false,
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
                editingFunction = [&pattern](const std::string &value,
                                                                  std::endian) -> std::vector<u8> {
                    try {
                        pattern->setValue(value);
                    } catch (const pl::core::err::EvaluatorError::Exception &error) {
                        log::error("Failed to set value of pattern '{}' to '{}': {}",
                                   pattern->getDisplayName(), value, error.what());
                    }

                    return {};
                };
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
                    wolv::util::toUTF8String(path) + ":" + pattern->getVariableName()
                );

                AchievementManager::unlockAchievement("hex.builtin.achievement.patterns",
                                                      "hex.builtin.achievement.patterns.data_inspector.name");
            } catch (const pl::core::err::EvaluatorError::Exception &) {
                auto displayFunction = createPatternErrorDisplayFunction();

                // Insert the inspector into the list
                m_workData.emplace_back(
                    wolv::util::toUTF8String(path.filename()),
                    std::move(displayFunction),
                    std::nullopt,
                    false,
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

        if (m_selectedProvider == nullptr || !m_selectedProvider->isReadable() || m_validBytes <= 0) {
            // Draw a message when no bytes are selected
            std::string text = "hex.builtin.view.data_inspector.no_data"_lang;
            auto textSize = ImGui::CalcTextSize(text.c_str());
            auto availableSpace = ImGui::GetContentRegionAvail();

            ImGui::SetCursorPos((availableSpace - textSize) / 2.0F);
            ImGui::TextUnformatted(text.c_str());

            return;
        }

        u32 validLineCount = m_cachedData.size();
        if (!m_tableEditingModeEnabled) {
            validLineCount = std::count_if(m_cachedData.begin(), m_cachedData.end(), [this](const auto &entry) {
                return !m_hiddenValues.contains(entry.filterValue);
            });
        }

        if (ImGui::BeginTable("##datainspector", m_tableEditingModeEnabled ? 3 : 2,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg,
                              ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * (validLineCount + 1)))) {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("hex.builtin.view.data_inspector.table.name"_lang,
                                    ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("hex.builtin.view.data_inspector.table.value"_lang,
                                    ImGuiTableColumnFlags_WidthStretch);

            if (m_tableEditingModeEnabled)
                ImGui::TableSetupColumn("##favorite", ImGuiTableColumnFlags_WidthFixed, ImGui::GetTextLineHeight());

            ImGui::TableHeadersRow();

            this->drawInspectorRows();

            ImGui::EndTable();
        }

        ImGuiExt::DimmedButtonToggle("hex.ui.common.edit"_lang, &m_tableEditingModeEnabled,
                                     ImVec2(ImGui::GetContentRegionAvail().x, 0));

        ImGui::NewLine();
        ImGui::Separator();
        ImGui::NewLine();

        // Draw inspector settings

        // Draw endian setting
        this->drawEndianSetting();

        // Draw radix setting
        this->drawRadixSetting();

        // Draw invert setting
        this->drawInvertSetting();
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

            // Render inspector row value
            const auto &copyValue = entry.displayFunction();

            ImGui::SameLine();

            // Handle copying the value to the clipboard when clicking the row
            if (ImGui::Selectable("##InspectorLine", false, ImGuiSelectableFlags_SpanAllColumns |
                                                            ImGuiSelectableFlags_AllowOverlap)) {
                ImGui::SetClipboardText(copyValue.c_str());
            }

            // Enter editing mode when double-clicking the row
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) &&
                entry.editingFunction.has_value() && m_selectedProvider->isWritable()) {
                entry.editing = true;
                m_editingValue = copyValue;
            }

            return;
        }

        // Handle editing mode
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
        ImGui::SetNextItemWidth(-1);
        ImGui::SetKeyboardFocusHere();

        // Draw input text box
        if (ImGui::InputText("##InspectorLineEditing", m_editingValue,
                             ImGuiInputTextFlags_EnterReturnsTrue |
                             ImGuiInputTextFlags_AutoSelectAll)) {
            // Turn the entered value into bytes
            auto bytes = entry.editingFunction.value()(m_editingValue, m_endian);

            if (m_invert)
                std::ranges::transform(bytes, bytes.begin(), [](auto byte) { return byte ^ 0xFF; });

            // Write those bytes to the selected provider at the current address
            m_selectedProvider->write(m_startAddress, bytes.data(), bytes.size());

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

    void ViewDataInspector::drawEndianSetting() {
        int selection = [this] {
            switch (m_endian) {
                default:
                case std::endian::little:
                    return 0;
                case std::endian::big:
                    return 1;
            }
        }();

        std::array options = {"hex.ui.common.little"_lang, "hex.ui.common.big"_lang};

        if (ImGui::SliderInt("hex.ui.common.endian"_lang, &selection, 0, options.size() - 1, options[selection],
                             ImGuiSliderFlags_NoInput)) {
            m_shouldInvalidate = true;

            switch (selection) {
                default:
                case 0:
                    m_endian = std::endian::little;
                    break;
                case 1:
                    m_endian = std::endian::big;
                    break;
            }
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
        std::array options = {"hex.ui.common.decimal"_lang, "hex.ui.common.hexadecimal"_lang,
                              "hex.ui.common.octal"_lang};

        if (ImGui::SliderInt("hex.ui.common.number_format"_lang, &selection, 0, options.size() - 1,
                             options[selection], ImGuiSliderFlags_NoInput)) {
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
        std::array options = {"hex.ui.common.no"_lang, "hex.ui.common.yes"_lang};

        if (ImGui::SliderInt("hex.builtin.view.data_inspector.invert"_lang, &selection, 0, options.size() - 1,
                             options[selection], ImGuiSliderFlags_NoInput)) {
            m_shouldInvalidate = true;

            m_invert = selection == 1;
        }
    }

    ContentRegistry::DataInspector::impl::DisplayFunction ViewDataInspector::createPatternErrorDisplayFunction() {
        // Generate error message
        std::string errorMessage;
        if (const auto &compileErrors = m_runtime.getCompileErrors(); !compileErrors.empty()) {
            for (const auto &error : compileErrors) {
                errorMessage += hex::format("{}\n", error.format());
            }
        } else if (const auto &evalError = m_runtime.getEvalError(); evalError.has_value()) {
            errorMessage += hex::format("{}:{}  {}\n", evalError->line, evalError->column, evalError->message);
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


}