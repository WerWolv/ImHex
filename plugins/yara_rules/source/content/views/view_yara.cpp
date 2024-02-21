#include "content/views/view_yara.hpp"

#include <hex/api/content_registry.hpp>
#include <hex/api/project_file_manager.hpp>

#include <hex/helpers/fs.hpp>

#include <toasts/toast_notification.hpp>
#include <popups/popup_file_chooser.hpp>

#include <filesystem>

#include <wolv/io/fs.hpp>
#include <wolv/literals.hpp>

namespace hex::plugin::yara {

    using namespace wolv::literals;

    ViewYara::ViewYara() : View::Window("hex.yara_rules.view.yara.name", ICON_VS_BUG) {
        YaraRule::init();

        ContentRegistry::FileHandler::add({ ".yar", ".yara" }, [](const auto &path) {
            for (const auto &destPath : fs::getDefaultPaths(fs::ImHexPath::Yara)) {
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

                m_matches.get(provider).clear();

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
    }

    ViewYara::~ViewYara() {
        YaraRule::cleanup();
    }

    void ViewYara::drawContent() {
        ImGuiExt::Header("hex.yara_rules.view.yara.header.rules"_lang, true);

        if (ImGui::BeginListBox("##rules", ImVec2(-FLT_MIN, ImGui::GetTextLineHeightWithSpacing() * 5))) {
            for (u32 i = 0; i < m_rulePaths->size(); i++) {
                const bool selected = (*m_selectedRule == i);
                if (ImGui::Selectable(wolv::util::toUTF8String((*m_rulePaths)[i].first).c_str(), selected)) {
                    *m_selectedRule = i;
                }
            }
            ImGui::EndListBox();
        }

        if (ImGuiExt::IconButton(ICON_VS_ADD, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
            const auto basePaths = fs::getDefaultPaths(fs::ImHexPath::Yara);
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

        ImGui::SameLine();
        if (ImGuiExt::IconButton(ICON_VS_REMOVE, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
            if (*m_selectedRule < m_rulePaths->size()) {
                m_rulePaths->erase(m_rulePaths->begin() + *m_selectedRule);
                m_selectedRule = std::min(*m_selectedRule, u32(m_rulePaths->size() - 1));
            }
        }

        ImGui::NewLine();
        if (ImGui::Button("hex.yara_rules.view.yara.match"_lang)) this->applyRules();
        ImGui::SameLine();

        if (m_matcherTask.isRunning()) {
            ImGui::SameLine();
            ImGuiExt::TextSpinner("hex.yara_rules.view.yara.matching"_lang);
        }

        ImGuiExt::Header("hex.yara_rules.view.yara.header.matches"_lang);

        auto matchesTableSize = ImGui::GetContentRegionAvail();
        matchesTableSize.y *= 3.75 / 5.0;
        matchesTableSize.y -= ImGui::GetTextLineHeightWithSpacing();

        if (ImGui::BeginTable("matches", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, matchesTableSize)) {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("hex.yara_rules.view.yara.matches.identifier"_lang, ImGuiTableColumnFlags_PreferSortAscending, 0, ImGui::GetID("identifier"));
            ImGui::TableSetupColumn("hex.yara_rules.view.yara.matches.variable"_lang, ImGuiTableColumnFlags_PreferSortAscending, 0, ImGui::GetID("variable"));
            ImGui::TableSetupColumn("hex.ui.common.address"_lang, ImGuiTableColumnFlags_PreferSortAscending, 0, ImGui::GetID("address"));
            ImGui::TableSetupColumn("hex.ui.common.size"_lang, ImGuiTableColumnFlags_PreferSortAscending, 0, ImGui::GetID("size"));

            ImGui::TableHeadersRow();

            auto sortSpecs = ImGui::TableGetSortSpecs();
            if (!m_matches->empty() && (sortSpecs->SpecsDirty || m_sortedMatches->empty())) {
                m_sortedMatches->clear();
                std::transform(m_matches->begin(), m_matches->end(), std::back_inserter(*m_sortedMatches), [](auto &match) {
                    return &match;
                });

                std::sort(m_sortedMatches->begin(), m_sortedMatches->end(), [&sortSpecs](const YaraMatch *left, const YaraMatch *right) -> bool {
                    if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("identifier"))
                        return left->match.identifier < right->match.identifier;
                    else if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("variable"))
                        return left->match.variable < right->match.variable;
                    else if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("address"))
                        return left->match.region.getStartAddress() < right->match.region.getStartAddress();
                    else if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("size"))
                        return left->match.region.getSize() < right->match.region.getSize();
                    else
                        return false;
                });

                if (sortSpecs->Specs->SortDirection == ImGuiSortDirection_Descending)
                    std::reverse(m_sortedMatches->begin(), m_sortedMatches->end());

                sortSpecs->SpecsDirty = false;
            }

            if (!m_matcherTask.isRunning()) {
                ImGuiListClipper clipper;
                clipper.Begin(m_sortedMatches->size());

                while (clipper.Step()) {
                    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                        auto &[match, highlightId, tooltipId] = *(*m_sortedMatches)[i];
                        auto &[identifier, variableName, region, wholeDataMatch] = match;
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::PushID(i);
                        if (ImGui::Selectable("match", false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap)) {
                            ImHexApi::HexEditor::setSelection(region.getStartAddress(), region.getSize());
                        }
                        ImGui::PopID();
                        ImGui::SameLine();
                        ImGui::TextUnformatted(identifier.c_str());
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(variableName.c_str());

                        if (!wholeDataMatch) {
                            ImGui::TableNextColumn();
                            ImGuiExt::TextFormatted("0x{0:X} : 0x{1:X}", region.getStartAddress(), region.getEndAddress());
                            ImGui::TableNextColumn();
                            ImGuiExt::TextFormatted("0x{0:X}", region.getSize());
                        } else {
                            ImGui::TableNextColumn();
                            ImGuiExt::TextFormattedColored(ImVec4(0.92F, 0.25F, 0.2F, 1.0F), "{}", "hex.yara_rules.view.yara.whole_data"_lang);
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted("");
                        }
                    }
                }

                clipper.End();
            }

            ImGui::EndTable();
        }

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
        for (const auto &match : *m_matches) {
            ImHexApi::HexEditor::removeBackgroundHighlight(match.highlightId);
            ImHexApi::HexEditor::removeTooltip(match.tooltipId);
        }

        m_matches->clear();
        m_consoleMessages->clear();
    }

    void ViewYara::applyRules() {
        this->clearResult();

        auto provider = ImHexApi::Provider::get();
        if (provider == nullptr)
            return;

        m_matcherTask = TaskManager::createTask("hex.yara_rules.view.yara.matching", 0, [this, provider](auto &task) {
            u32 progress = 0;

            std::vector<YaraRule::Result> results;
            for (const auto &[fileName, filePath] : *m_rulePaths) {
                YaraRule rule(filePath);

                task.setInterruptCallback([&rule] {
                    rule.interrupt();
                });

                auto result = rule.match(provider, provider->getBaseAddress(), provider->getSize());
                if (!result.has_value()) {
                    TaskManager::doLater([this, error = result.error()] {
                        m_consoleMessages->emplace_back(error.message);
                    });

                    break;
                }

                results.emplace_back(result.value());

                task.update(progress);
                progress += 1;
            }

            TaskManager::doLater([this, results = std::move(results)] {
                for (const auto &match : *m_matches) {
                    ImHexApi::HexEditor::removeBackgroundHighlight(match.highlightId);
                    ImHexApi::HexEditor::removeTooltip(match.tooltipId);
                }

                for (auto &result : results) {
                    for (auto &match : result.matches) {
                        m_matches->emplace_back(YaraMatch { std::move(match), 0, 0 });
                    }

                    m_consoleMessages->append_range(result.consoleMessages);
                }

                auto uniques = std::set(m_matches->begin(), m_matches->end(), [](const auto &leftMatch, const auto &rightMatch) {
                    const auto &l = leftMatch.match;
                    const auto &r = rightMatch.match;
                    return std::tie(l.region.address, l.wholeDataMatch, l.identifier, l.variable) <
                           std::tie(r.region.address, r.wholeDataMatch, r.identifier, r.variable);
                });

                m_matches->clear();
                std::move(uniques.begin(), uniques.end(), std::back_inserter(*m_matches));

                constexpr static color_t YaraColor = 0x70B4771F;
                for (const YaraMatch &yaraMatch : uniques) {
                    yaraMatch.highlightId = ImHexApi::HexEditor::addBackgroundHighlight(yaraMatch.match.region, YaraColor);
                    yaraMatch.tooltipId = ImHexApi::HexEditor::addTooltip(yaraMatch.match.region, hex::format("{0} [{1}]", yaraMatch.match.identifier, yaraMatch.match.variable), YaraColor);
                }
            });
        });
    }

}