#include "content/views/view_yara.hpp"

#include <hex/api/imhex_api/hex_editor.hpp>
#include <hex/api/content_registry/file_type_handler.hpp>
#include <hex/api/project_file_manager.hpp>

#include <hex/helpers/fs.hpp>
#include <hex/helpers/default_paths.hpp>

#include <toasts/toast_notification.hpp>
#include <popups/popup_file_chooser.hpp>

#include <wolv/io/fs.hpp>
#include <wolv/literals.hpp>
#include <nlohmann/json.hpp>

namespace hex::plugin::yara {

    using namespace wolv::literals;

    ViewYara::ViewYara() : View::Window("hex.yara_rules.view.yara.name", ICON_VS_BUG) {
        YaraRule::init();

        ContentRegistry::FileTypeHandler::add({ ".yar", ".yara" }, [](const auto &path) {
            for (const auto &destPath : paths::Yara.write()) {
                if (wolv::io::fs::copyFile(path, destPath / path.filename(), std::fs::copy_options::overwrite_existing)) {
                    ui::ToastInfo::open("hex.yara_rules.view.yara.rule_added"_lang);
                    return true;
                }
            }

            return false;
        });

        ProjectFile::registerPerProviderHandler({
            .basePath = "yara.json",
            .required = false,
            .load = [this](prv::Provider *provider, const std::fs::path &basePath, const Tar &tar) -> bool {
                auto fileContent = tar.readString(basePath);
                if (fileContent.empty())
                    return true;

                auto data = nlohmann::json::parse(fileContent.begin(), fileContent.end());

                if (!data.contains("rules"))
                    return false;

                auto &rules = data["rules"];
                if (!rules.is_array())
                    return false;

                m_matchedRules.get(provider).clear();

                for (auto &rule : rules) {
                    if (!rule.contains("name") || !rule.contains("path"))
                        return false;

                    auto &name = rule["name"];
                    auto &path = rule["path"];

                    if (!name.is_string() || !path.is_string())
                        return false;

                    m_rulePaths.get(provider).emplace_back(std::fs::path(name.get<std::string>()), std::fs::path(path.get<std::string>()));
                }

                return true;
            },
            .store = [this](prv::Provider *provider, const std::fs::path &basePath, const Tar &tar) -> bool {
                nlohmann::json data;

                data["rules"] = nlohmann::json::array();

                for (auto &[name, path] : m_rulePaths.get(provider)) {
                    data["rules"].push_back({
                        { "name", wolv::util::toUTF8String(name) },
                        { "path", wolv::util::toUTF8String(path) }
                    });
                }

                tar.writeString(basePath, data.dump(4));

                return true;
            }
        });

        ImHexApi::HexEditor::addBackgroundHighlightingProvider([this](u64 address, const u8 *, size_t size, bool) -> std::optional<color_t> {
            auto &highlights = m_highlights.get();
            const auto regions = highlights.overlapping({ address, address + (size - 1) });

            constexpr static color_t YaraColor = 0x70B4771F;
            if (regions.empty())
                return std::nullopt;
            else
                return YaraColor;
        });

        ImHexApi::HexEditor::addTooltipProvider([this](u64 address, const u8 *, size_t size) {
            if (m_matcherTask.isRunning())
                return;

            auto occurrences = m_highlights->overlapping({ address, (address + size - 1) });
            if (occurrences.empty())
                return;

            ImGui::BeginTooltip();

            for (const auto &occurrence : occurrences) {
                ImGui::PushID(&occurrence);
                if (ImGui::BeginTable("##tooltips", 1, ImGuiTableFlags_RowBg | ImGuiTableFlags_NoClip)) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();

                    {
                        const auto &tooltipValue = *occurrence.value;

                        ImGuiExt::TextFormatted("{}", tooltipValue);
                    }

                    ImGui::EndTable();
                }
                ImGui::PopID();
            }

            ImGui::EndTooltip();
        });
    }

    ViewYara::~ViewYara() {
        YaraRule::cleanup();
    }

    void ViewYara::drawContent() {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0, 0 });
        if (ImGuiExt::BeginSubWindow("hex.yara_rules.view.yara.header.rules"_lang, nullptr, ImVec2(0, 150_scaled))) {
            if (ImGui::BeginTable("##rules", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH)) {
                ImGui::TableSetupColumn("##rule", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("##delete", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed, ImGui::GetTextLineHeight());

                std::optional<u32> indexToErase;
                for (u32 i = 0; i < m_rulePaths->size(); i++) {
                    ImGui::PushID(i + 1);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Indent(5_scaled);
                    ImGui::TextUnformatted(wolv::util::toUTF8String((*m_rulePaths)[i].first).c_str());
                    ImGui::Unindent(5_scaled);

                    ImGui::TableNextColumn();
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                    if (ImGuiExt::DimmedIconButton(ICON_VS_TRASH, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
                        indexToErase = i;
                    }
                    ImGui::PopStyleVar();
                    ImGui::PopID();
                }

                if (indexToErase.has_value()) {
                    m_rulePaths->erase(m_rulePaths->begin() + *indexToErase);
                }

                ImGui::EndTable();
            }
        }
        ImGuiExt::EndSubWindow();
        ImGui::PopStyleVar();

        ImGui::BeginDisabled(m_rulePaths->empty());
        if (ImGuiExt::DimmedButton("hex.yara_rules.view.yara.match"_lang))
            this->applyRules();
        ImGui::EndDisabled();

        if (m_matcherTask.isRunning()) {
            ImGui::SameLine(0, 20_scaled);
            ImGuiExt::TextSpinner("hex.yara_rules.view.yara.matching"_lang);
        }

        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetWindowSize().x - ImGui::CalcTextSize(ICON_VS_ADD).x - ImGui::GetStyle().ItemSpacing.x * 2);
        if (ImGuiExt::DimmedIconButton(ICON_VS_ADD, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
            const auto basePaths = paths::Yara.read();
            std::vector<std::fs::path> paths;
            for (const auto &path : basePaths) {
                std::error_code error;
                for (const auto &entry : std::fs::recursive_directory_iterator(path, error)) {
                    if (!entry.is_regular_file()) continue;
                    if (entry.path().extension() != ".yara" && entry.path().extension() != ".yar") continue;

                    paths.push_back(entry);
                }
            }

            ui::PopupFileChooser::open(basePaths, paths, std::vector<hex::fs::ItemFilter>{ { "Yara File", "yara" }, { "Yara File", "yar" } }, true,
                [&](const auto &path) {
                    m_rulePaths->push_back({ path.filename(), path });
            });
        }

        ImGui::NewLine();

        auto matchesTableSize = ImGui::GetContentRegionAvail();
        matchesTableSize.y *= 3.75 / 5.0;
        matchesTableSize.y -= ImGui::GetTextLineHeightWithSpacing();
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0, 0 });
        if (ImGuiExt::BeginSubWindow("hex.yara_rules.view.yara.header.matches"_lang, nullptr, matchesTableSize)) {
            if (ImGui::BeginTable("matches", 3, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn("hex.yara_rules.view.yara.matches.variable"_lang, ImGuiTableColumnFlags_None, 0.5);
                ImGui::TableSetupColumn("hex.ui.common.address"_lang, ImGuiTableColumnFlags_None, 0.25);
                ImGui::TableSetupColumn("hex.ui.common.size"_lang, ImGuiTableColumnFlags_None, 0.25);

                ImGui::TableHeadersRow();

                if (!m_matcherTask.isRunning()) {
                    u32 ruleId = 1;
                    for (const auto &rule : *m_matchedRules) {
                        if (rule.matches.empty()) continue;

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();

                        ImGui::PushID(ruleId);
                        ImGui::PushStyleVarX(ImGuiStyleVar_FramePadding, 0.0F);
                        const bool open = ImGui::TreeNodeEx("##TreeNode", ImGuiTreeNodeFlags_DrawLinesToNodes | ImGuiTreeNodeFlags_SpanLabelWidth | ImGuiTreeNodeFlags_OpenOnArrow);
                        ImGui::PopStyleVar();
                        ImGui::SameLine();
                        ImGui::TextUnformatted(rule.identifier.c_str());
                        if (open) {
                            u32 matchId = 1;
                            for (const auto &match : rule.matches) {
                                ImGui::TableNextRow();
                                ImGui::TableNextColumn();
                                ImGui::PushID(matchId);

                                if (ImGui::Selectable("##match_selectable", false, ImGuiSelectableFlags_SpanAllColumns)) {
                                    ImHexApi::HexEditor::setSelection(match.region);
                                }

                                ImGui::SameLine();
                                ImGui::TextUnformatted(match.variable.c_str());
                                ImGui::TableNextColumn();
                                ImGui::TextUnformatted(fmt::format("0x{0:08X}", match.region.getStartAddress()).c_str());
                                ImGui::TableNextColumn();
                                ImGui::TextUnformatted(fmt::format("0x{0:08X}", match.region.getSize()).c_str());

                                ImGui::PopID();
                                matchId += 1;
                            }
                            ImGui::TreePop();
                        }
                        ImGui::PopID();
                        ruleId += 1;
                    }
                }

                ImGui::EndTable();
            }
        }
        ImGuiExt::EndSubWindow();
        ImGui::PopStyleVar();

        auto consoleSize = ImGui::GetContentRegionAvail();

        if (ImGui::BeginChild("##console", consoleSize, true, ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_HorizontalScrollbar)) {
            ImGuiListClipper clipper;

            clipper.Begin(m_consoleMessages->size());
            while (clipper.Step()) {
                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                    const auto &message = m_consoleMessages->at(i);

                    if (ImGui::Selectable(message.c_str()))
                        ImGui::SetClipboardText(message.c_str());
                }
            }
        }
        ImGui::EndChild();
    }

    void ViewYara::clearResult() {
        m_matchedRules->clear();
        m_consoleMessages->clear();
    }

    void ViewYara::applyRules() {
        this->clearResult();

        auto provider = ImHexApi::Provider::get();
        if (provider == nullptr)
            return;

        m_matcherTask = TaskManager::createTask("hex.yara_rules.view.yara.matching", 0, [this, provider](auto &task) {
            std::vector<YaraRule::Result> results;
            for (const auto &[fileName, filePath] : *m_rulePaths) {
                YaraRule rule(filePath);

                task.setInterruptCallback([&rule] {
                    rule.interrupt();
                });

                auto result = rule.match(provider, { provider->getBaseAddress(), provider->getSize() });
                if (!result.has_value()) {
                    TaskManager::doLater([this, error = result.error()] {
                        m_consoleMessages->emplace_back(error.message);
                    });

                    return;
                }

                results.emplace_back(result.value());

                task.increment();
            }

            TaskManager::doLater([this, results = std::move(results)] {
                this->clearResult();

                for (auto &result : results) {
                    m_matchedRules->insert(m_matchedRules->end(), result.matchedRules.begin(), result.matchedRules.end());
                    m_consoleMessages->insert(m_consoleMessages->end(), result.consoleMessages.begin(), result.consoleMessages.end());
                }

                for (YaraRule::Rule &rule : *m_matchedRules) {
                    for (auto &match : rule.matches) {
                        auto tags = fmt::format("{}", fmt::join(rule.tags, ", "));
                        m_highlights->insert(
                            { match.region.getStartAddress(), match.region.getEndAddress() },
                            fmt::format("rule {0}{1} {{ {2} }}",
                                rule.identifier,
                                tags.empty() ? "" : fmt::format(" : {}", tags),
                                match.variable
                            )
                        );
                    }
                }
            });
        });
    }

    void ViewYara::drawHelpText() {
        ImGuiExt::TextFormattedWrapped("This view allows you to apply YARA rules to the currently opened file and highlights matched regions.");
        ImGui::NewLine();
        ImGuiExt::TextFormattedWrapped(
            "You can add YARA rules by clicking the + button in the top right corner of the view. "
            "This will open a file chooser where you can select one or more YARA files to add."
            "For further information on how to write YARA rules, please refer to its official documentation."
        );
    }
}
