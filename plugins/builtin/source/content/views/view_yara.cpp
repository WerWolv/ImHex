#include "content/views/view_yara.hpp"

#include <hex/api/content_registry.hpp>
#include <hex/api/project_file_manager.hpp>

#include <hex/helpers/utils.hpp>
#include <hex/helpers/fs.hpp>

#include <content/popups/popup_notification.hpp>
#include <content/popups/popup_file_chooser.hpp>

// <yara/types.h>'s RE type has a zero-sized array, which is not allowed in ISO C++.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#include <yara.h>
#pragma GCC diagnostic pop

#include <filesystem>
#include <thread>

#include <wolv/io/file.hpp>
#include <wolv/io/fs.hpp>
#include <wolv/utils/guards.hpp>
#include <wolv/literals.hpp>

namespace hex::plugin::builtin {

    using namespace wolv::literals;

    ViewYara::ViewYara() : View("hex.builtin.view.yara.name") {
        yr_initialize();

        ContentRegistry::FileHandler::add({ ".yar", ".yara" }, [](const auto &path) {
            for (const auto &destPath : fs::getDefaultPaths(fs::ImHexPath::Yara)) {
                if (wolv::io::fs::copyFile(path, destPath / path.filename(), std::fs::copy_options::overwrite_existing)) {
                    PopupInfo::open("hex.builtin.view.yara.rule_added"_lang);
                    return true;
                }
            }

            return false;
        });

        ProjectFile::registerPerProviderHandler({
            .basePath = "yara.json",
            .required = false,
            .load = [this](prv::Provider *provider, const std::fs::path &basePath, Tar &tar) -> bool {
                auto fileContent = tar.readString(basePath);
                if (fileContent.empty())
                    return true;

                auto data = nlohmann::json::parse(fileContent.begin(), fileContent.end());

                if (!data.contains("rules"))
                    return false;

                auto &rules = data["rules"];
                if (!rules.is_array())
                    return false;

                this->m_matches.get(provider).clear();

                for (auto &rule : rules) {
                    if (!rule.contains("name") || !rule.contains("path"))
                        return false;

                    auto &name = rule["name"];
                    auto &path = rule["path"];

                    if (!name.is_string() || !path.is_string())
                        return false;

                    this->m_rules.get(provider).emplace_back(std::fs::path(name.get<std::string>()), std::fs::path(path.get<std::string>()));
                }

                return true;
            },
            .store = [this](prv::Provider *provider, const std::fs::path &basePath, Tar &tar) -> bool {
                nlohmann::json data;

                data["rules"] = nlohmann::json::array();

                for (auto &[name, path] : this->m_rules.get(provider)) {
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
        yr_finalize();
    }

    void ViewYara::drawContent() {
        if (ImGui::Begin(View::toWindowName("hex.builtin.view.yara.name").c_str(), &this->getWindowOpenState(), ImGuiWindowFlags_NoCollapse)) {

            ImGui::TextUnformatted("hex.builtin.view.yara.header.rules"_lang);
            ImGui::Separator();

            if (ImGui::BeginListBox("##rules", ImVec2(-FLT_MIN, ImGui::GetTextLineHeightWithSpacing() * 5))) {
                for (u32 i = 0; i < this->m_rules->size(); i++) {
                    const bool selected = (this->m_selectedRule == i);
                    if (ImGui::Selectable(wolv::util::toUTF8String((*this->m_rules)[i].first).c_str(), selected)) {
                        this->m_selectedRule = i;
                    }
                }
                ImGui::EndListBox();
            }

            if (ImGui::IconButton(ICON_VS_ADD, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
                std::vector<std::fs::path> paths;
                for (const auto &path : fs::getDefaultPaths(fs::ImHexPath::Yara)) {
                    std::error_code error;
                    for (const auto &entry : std::fs::recursive_directory_iterator(path, error)) {
                        if (!entry.is_regular_file()) continue;
                        if (entry.path().extension() != ".yara" && entry.path().extension() != ".yar") continue;

                        paths.push_back(entry);
                    }
                }

                PopupFileChooser::open(paths, std::vector<nfdfilteritem_t>{ { "Yara File", "yara" }, { "Yara File", "yar" } }, true,
                    [&](const auto &path) {
                        this->m_rules->push_back({ path.filename(), path });
                    });
            }

            ImGui::SameLine();
            if (ImGui::IconButton(ICON_VS_REMOVE, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
                if (this->m_selectedRule < this->m_rules->size()) {
                    this->m_rules->erase(this->m_rules->begin() + this->m_selectedRule);
                    this->m_selectedRule = std::min(this->m_selectedRule, (u32)this->m_rules->size() - 1);
                }
            }

            ImGui::NewLine();
            if (ImGui::Button("hex.builtin.view.yara.match"_lang)) this->applyRules();
            ImGui::SameLine();

            if (this->m_matcherTask.isRunning()) {
                ImGui::SameLine();
                ImGui::TextSpinner("hex.builtin.view.yara.matching"_lang);
            }

            ImGui::NewLine();
            ImGui::TextUnformatted("hex.builtin.view.yara.header.matches"_lang);
            ImGui::Separator();

            auto matchesTableSize = ImGui::GetContentRegionAvail();
            matchesTableSize.y *= 3.75 / 5.0;
            matchesTableSize.y -= ImGui::GetTextLineHeightWithSpacing();

            if (ImGui::BeginTable("matches", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, matchesTableSize)) {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn("hex.builtin.view.yara.matches.identifier"_lang, ImGuiTableColumnFlags_PreferSortAscending, 0, ImGui::GetID("identifier"));
                ImGui::TableSetupColumn("hex.builtin.view.yara.matches.variable"_lang, ImGuiTableColumnFlags_PreferSortAscending, 0, ImGui::GetID("variable"));
                ImGui::TableSetupColumn("hex.builtin.common.address"_lang, ImGuiTableColumnFlags_PreferSortAscending, 0, ImGui::GetID("address"));
                ImGui::TableSetupColumn("hex.builtin.common.size"_lang, ImGuiTableColumnFlags_PreferSortAscending, 0, ImGui::GetID("size"));

                ImGui::TableHeadersRow();

                auto sortSpecs = ImGui::TableGetSortSpecs();
                if (!this->m_matches->empty() && (sortSpecs->SpecsDirty || this->m_sortedMatches->empty())) {
                    this->m_sortedMatches->clear();
                    std::transform(this->m_matches->begin(), this->m_matches->end(), std::back_inserter(*this->m_sortedMatches), [](auto &match) {
                        return &match;
                    });

                    std::sort(this->m_sortedMatches->begin(), this->m_sortedMatches->end(), [&sortSpecs](YaraMatch *left, YaraMatch *right) -> bool {
                        if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("identifier"))
                            return left->identifier < right->identifier;
                        else if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("variable"))
                            return left->variable < right->variable;
                        else if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("address"))
                            return left->address < right->address;
                        else if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("size"))
                            return left->size < right->size;
                        else
                            return false;
                    });

                    if (sortSpecs->Specs->SortDirection == ImGuiSortDirection_Descending)
                        std::reverse(this->m_sortedMatches->begin(), this->m_sortedMatches->end());

                    sortSpecs->SpecsDirty = false;
                }

                if (!this->m_matcherTask.isRunning()) {
                    ImGuiListClipper clipper;
                    clipper.Begin(this->m_sortedMatches->size());

                    while (clipper.Step()) {
                        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                            auto &[identifier, variableName, address, size, wholeDataMatch, highlightId, tooltipId] = *(*this->m_sortedMatches)[i];
                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            ImGui::PushID(i);
                            if (ImGui::Selectable("match", false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap)) {
                                ImHexApi::HexEditor::setSelection(address, size);
                            }
                            ImGui::PopID();
                            ImGui::SameLine();
                            ImGui::TextUnformatted(identifier.c_str());
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted(variableName.c_str());

                            if (!wholeDataMatch) {
                                ImGui::TableNextColumn();
                                ImGui::TextFormatted("0x{0:X} : 0x{1:X}", address, address + size - 1);
                                ImGui::TableNextColumn();
                                ImGui::TextFormatted("0x{0:X}", size);
                            } else {
                                ImGui::TableNextColumn();
                                ImGui::TextFormattedColored(ImVec4(0.92F, 0.25F, 0.2F, 1.0F), "{}", "hex.builtin.view.yara.whole_data"_lang);
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

                clipper.Begin(this->m_consoleMessages.size());
                while (clipper.Step())
                    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                        const auto &message = this->m_consoleMessages[i];

                        if (ImGui::Selectable(message.c_str()))
                            ImGui::SetClipboardText(message.c_str());
                    }
            }
            ImGui::EndChild();
        }
        ImGui::End();
    }

    void ViewYara::clearResult() {
        for (const auto &match : *this->m_matches) {
            ImHexApi::HexEditor::removeBackgroundHighlight(match.highlightId);
            ImHexApi::HexEditor::removeTooltip(match.tooltipId);
        }

        this->m_matches->clear();
        this->m_consoleMessages.clear();
    }

    void ViewYara::applyRules() {
        this->clearResult();

        this->m_matcherTask = TaskManager::createTask("hex.builtin.view.yara.matching", 0, [this](auto &task) {
            if (!ImHexApi::Provider::isValid()) return;

            struct ResultContext {
                Task *task = nullptr;
                std::vector<YaraMatch> newMatches;
                std::vector<std::string> consoleMessages;
            };

            ResultContext resultContext;
            resultContext.task = &task;

            for (const auto &[fileName, filePath] : *this->m_rules) {
                YR_COMPILER *compiler = nullptr;
                yr_compiler_create(&compiler);
                ON_SCOPE_EXIT {
                    yr_compiler_destroy(compiler);
                };

                auto currFilePath = wolv::util::toUTF8String(wolv::io::fs::toShortPath(filePath));

                yr_compiler_set_include_callback(
                    compiler,
                    [](const char *includeName, const char *, const char *, void *userData) -> const char * {
                        wolv::io::File file(std::fs::path(static_cast<const char *>(userData)).parent_path() / includeName, wolv::io::File::Mode::Read);
                        if (!file.isValid())
                            return nullptr;

                        auto size    = file.getSize();
                        char *buffer = new char[size + 1];
                        file.readBuffer(reinterpret_cast<u8 *>(buffer), size);
                        buffer[size] = 0x00;

                        return buffer;
                    },
                    [](const char *ptr, void *userData) {
                        hex::unused(userData);

                        delete[] ptr;
                    },
                    currFilePath.data()
                );

                wolv::io::File file((*this->m_rules)[this->m_selectedRule].second, wolv::io::File::Mode::Read);
                if (!file.isValid()) return;

                if (yr_compiler_add_file(compiler, file.getHandle(), nullptr, nullptr) != 0) {
                    std::string errorMessage(0xFFFF, '\x00');
                    yr_compiler_get_error_message(compiler, errorMessage.data(), errorMessage.size());

                    TaskManager::doLater([this, errorMessage = wolv::util::trim(errorMessage)] {
                        this->clearResult();

                        this->m_consoleMessages.push_back("Error: " + errorMessage);
                    });

                    return;
                }

                YR_RULES *yaraRules;
                yr_compiler_get_rules(compiler, &yaraRules);
                ON_SCOPE_EXIT { yr_rules_destroy(yaraRules); };

                YR_MEMORY_BLOCK_ITERATOR iterator;

                struct ScanContext {
                    Task *task = nullptr;
                    std::vector<u8> buffer;
                    YR_MEMORY_BLOCK currBlock = {};
                };

                ScanContext context;
                context.task                 = &task;
                context.currBlock.base       = 0;
                context.currBlock.fetch_data = [](auto *block) -> const u8 * {
                    auto &context = *static_cast<ScanContext *>(block->context);
                    auto provider = ImHexApi::Provider::get();

                    context.buffer.resize(context.currBlock.size);

                    if (context.buffer.empty())
                        return nullptr;

                    block->size = context.currBlock.size;
                    provider->read(context.currBlock.base + provider->getBaseAddress(), context.buffer.data(), context.buffer.size());

                    return context.buffer.data();
                };
                iterator.file_size = [](auto *iterator) -> u64 {
                    hex::unused(iterator);

                    return ImHexApi::Provider::get()->getActualSize();
                };

                iterator.context = &context;
                iterator.first   = [](YR_MEMORY_BLOCK_ITERATOR *iterator) -> YR_MEMORY_BLOCK   *{
                    auto &context = *static_cast<ScanContext *>(iterator->context);

                    context.currBlock.base = 0;
                    context.currBlock.size = 0;
                    context.buffer.clear();
                    iterator->last_error = ERROR_SUCCESS;

                    return iterator->next(iterator);
                };
                iterator.next = [](YR_MEMORY_BLOCK_ITERATOR *iterator) -> YR_MEMORY_BLOCK * {
                    auto &context = *static_cast<ScanContext *>(iterator->context);

                    u64 address = context.currBlock.base + context.currBlock.size;

                    iterator->last_error      = ERROR_SUCCESS;
                    context.currBlock.base    = address;
                    context.currBlock.size    = std::min<size_t>(ImHexApi::Provider::get()->getActualSize() - address, 10_MiB);
                    context.currBlock.context = &context;
                    context.task->update(address);

                    if (context.currBlock.size == 0) return nullptr;

                    return &context.currBlock;
                };

                yr_rules_scan_mem_blocks(
                        yaraRules, &iterator, 0, [](YR_SCAN_CONTEXT *context, int message, void *data, void *userData) -> int {
                            auto &results = *static_cast<ResultContext *>(userData);

                            switch (message) {
                                case CALLBACK_MSG_RULE_MATCHING:
                                {
                                    auto rule = static_cast<YR_RULE *>(data);

                                    YR_STRING *string;
                                    YR_MATCH *match;

                                    if (rule->strings != nullptr) {
                                        yr_rule_strings_foreach(rule, string) {
                                            yr_string_matches_foreach(context, string, match) {
                                                    results.newMatches.push_back({ rule->identifier, string->identifier, u64(match->offset), size_t(match->match_length), false, 0, 0 });
                                                }
                                        }
                                    } else {
                                        results.newMatches.push_back({ rule->identifier, "", 0, 0, true, 0, 0 });
                                    }
                                }
                                    break;
                                case CALLBACK_MSG_CONSOLE_LOG:
                                {
                                    results.consoleMessages.emplace_back(static_cast<const char *>(data));
                                }
                                    break;
                                default:
                                    break;
                            }

                            return results.task->shouldInterrupt() ? CALLBACK_ABORT : CALLBACK_CONTINUE;
                        },
                        &resultContext,
                        0);

            }
            TaskManager::doLater([this, resultContext] {
                for (const auto &match : *this->m_matches) {
                    ImHexApi::HexEditor::removeBackgroundHighlight(match.highlightId);
                    ImHexApi::HexEditor::removeTooltip(match.tooltipId);
                }

                this->m_consoleMessages = resultContext.consoleMessages;

                std::move(resultContext.newMatches.begin(), resultContext.newMatches.end(), std::back_inserter(*this->m_matches));

                auto uniques = std::set(this->m_matches->begin(), this->m_matches->end(), [](const auto &l, const auto &r) {
                    return std::tie(l.address, l.size, l.wholeDataMatch, l.identifier, l.variable) <
                           std::tie(r.address, r.size, r.wholeDataMatch, r.identifier, r.variable);
                });

                this->m_matches->clear();
                std::move(uniques.begin(), uniques.end(), std::back_inserter(*this->m_matches));

                constexpr static color_t YaraColor = 0x70B4771F;
                for (auto &match : uniques) {
                    match.highlightId = ImHexApi::HexEditor::addBackgroundHighlight({ match.address, match.size }, YaraColor);
                    match.tooltipId = ImHexApi::HexEditor::addTooltip({ match. address, match.size }, hex::format("{0} [{1}]", match.identifier, match.variable), YaraColor);
                }
            });
        });
    }

}