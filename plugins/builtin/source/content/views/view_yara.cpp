#include "content/views/view_yara.hpp"

#include <hex/api/content_registry.hpp>

#include <hex/helpers/utils.hpp>
#include <hex/helpers/file.hpp>
#include <hex/helpers/fs.hpp>

#include <yara.h>
#include <filesystem>
#include <thread>

namespace hex::plugin::builtin {

    ViewYara::ViewYara() : View("hex.builtin.view.yara.name") {
        yr_initialize();

        this->reloadRules();

        ContentRegistry::FileHandler::add({ ".yar" }, [](const auto &path) {
            for (const auto &destPath : fs::getDefaultPaths(fs::ImHexPath::Yara)) {
                if (fs::copyFile(path, destPath / path.filename(), std::fs::copy_options::overwrite_existing)) {
                    View::showMessagePopup("hex.builtin.view.yara.rule_added"_lang);
                    return true;
                }
            }

            return false;
        });
    }

    ViewYara::~ViewYara() {
        yr_finalize();
    }

    void ViewYara::drawContent() {
        if (ImGui::Begin(View::toWindowName("hex.builtin.view.yara.name").c_str(), &this->getWindowOpenState(), ImGuiWindowFlags_NoCollapse)) {

            ImGui::TextUnformatted("hex.builtin.view.yara.header.rules"_lang);
            ImGui::Separator();

            if (this->m_rules.empty()) {
                ImGui::TextFormattedColored(ImVec4(0.92F, 0.25F, 0.2F, 1.0F), "{}", "hex.builtin.view.yara.no_rules"_lang);

                if (ImGui::Button("hex.builtin.view.yara.reload"_lang)) this->reloadRules();
            } else {
                ImGui::Disabled([this] {
                    if (ImGui::BeginCombo("hex.builtin.view.yara.header.rules"_lang, this->m_rules[this->m_selectedRule].first.c_str())) {
                        for (u32 i = 0; i < this->m_rules.size(); i++) {
                            const bool selected = (this->m_selectedRule == i);
                            if (ImGui::Selectable(this->m_rules[i].first.c_str(), selected))
                                this->m_selectedRule = i;

                            if (selected)
                                ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("hex.builtin.view.yara.reload"_lang)) this->reloadRules();
                    if (ImGui::Button("hex.builtin.view.yara.match"_lang)) this->applyRules();
                },
                    this->m_matching);

                if (this->m_matching) {
                    ImGui::SameLine();
                    ImGui::TextSpinner("hex.builtin.view.yara.matching"_lang);
                }
            }

            ImGui::NewLine();
            ImGui::TextUnformatted("hex.builtin.view.yara.header.matches"_lang);
            ImGui::Separator();

            auto matchesTableSize = ImGui::GetContentRegionAvail();
            matchesTableSize.y *= 3.75 / 5.0;
            matchesTableSize.y -= ImGui::GetTextLineHeightWithSpacing();

            if (ImGui::BeginTable("matches", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, matchesTableSize)) {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn("hex.builtin.view.yara.matches.identifier"_lang);
                ImGui::TableSetupColumn("hex.builtin.view.yara.matches.variable"_lang);
                ImGui::TableSetupColumn("hex.builtin.common.address"_lang);
                ImGui::TableSetupColumn("hex.builtin.common.size"_lang);

                ImGui::TableHeadersRow();

                if (!this->m_matching) {
                    ImGuiListClipper clipper;
                    clipper.Begin(this->m_matches.size());

                    while (clipper.Step()) {
                        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                            auto &[identifier, variableName, address, size, wholeDataMatch, highlightId, tooltipId] = this->m_matches[i];
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
                ImGuiListClipper clipper(this->m_consoleMessages.size());
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
        for (const auto &match : this->m_matches) {
            ImHexApi::HexEditor::removeBackgroundHighlight(match.highlightId);
            ImHexApi::HexEditor::removeTooltip(match.tooltipId);
        }

        this->m_matches.clear();
        this->m_consoleMessages.clear();
    }

    void ViewYara::reloadRules() {
        this->m_rules.clear();

        for (const auto &path : fs::getDefaultPaths(fs::ImHexPath::Yara)) {
            if (!fs::exists(path))
                continue;

            std::error_code error;
            for (const auto &entry : std::fs::recursive_directory_iterator(path, error)) {
                if (entry.is_regular_file() && entry.path().extension() == ".yar") {
                    this->m_rules.emplace_back(std::fs::relative(entry.path(), std::fs::path(path)).string(), entry.path().string());
                }
            }
        }
    }

    void ViewYara::applyRules() {
        this->clearResult();

        this->m_matching = true;

        std::thread([this] {
            if (!ImHexApi::Provider::isValid()) return;

            auto provider = ImHexApi::Provider::get();
            auto task     = ImHexApi::Tasks::createTask("hex.builtin.view.yara.matching", provider->getActualSize());

            YR_COMPILER *compiler = nullptr;
            yr_compiler_create(&compiler);
            ON_SCOPE_EXIT {
                yr_compiler_destroy(compiler);
                this->m_matching = false;
            };

            yr_compiler_set_include_callback(
                compiler,
                [](const char *includeName, const char *, const char *, void *userData) -> const char * {
                    auto currFilePath = static_cast<const char *>(userData);

                    fs::File file((std::fs::path(currFilePath).parent_path() / includeName).string(), fs::File::Mode::Read);
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
                this->m_rules[this->m_selectedRule].second.data());


            fs::File file(this->m_rules[this->m_selectedRule].second, fs::File::Mode::Read);
            if (!file.isValid()) return;

            if (yr_compiler_add_file(compiler, file.getHandle(), nullptr, nullptr) != 0) {
                std::string errorMessage(0xFFFF, '\x00');
                yr_compiler_get_error_message(compiler, errorMessage.data(), errorMessage.size());
                hex::trim(errorMessage);

                ImHexApi::Tasks::doLater([this, errorMessage] {
                    this->clearResult();

                    this->m_consoleMessages.push_back("Error: " + errorMessage);
                });

                return;
            }

            YR_RULES *rules;
            yr_compiler_get_rules(compiler, &rules);
            ON_SCOPE_EXIT { yr_rules_destroy(rules); };

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

                if (context.buffer.empty()) return nullptr;

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
                context.currBlock.size    = ImHexApi::Provider::get()->getActualSize() - address;
                context.currBlock.context = &context;
                context.task->update(address);

                if (context.currBlock.size == 0) return nullptr;

                return &context.currBlock;
            };


            struct ResultContext {
                std::vector<YaraMatch> newMatches;
                std::vector<std::string> consoleMessages;
            };

            ResultContext resultContext;

            yr_rules_scan_mem_blocks(
                rules, &iterator, 0, [](YR_SCAN_CONTEXT *context, int message, void *data, void *userData) -> int {
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

                    return CALLBACK_CONTINUE;
                },
                &resultContext,
                0);


            ImHexApi::Tasks::doLater([this, resultContext] {
                this->m_matches         = resultContext.newMatches;
                this->m_consoleMessages = resultContext.consoleMessages;

                constexpr static color_t YaraColor = 0x70B4771F;
                for (auto &match : this->m_matches) {
                    match.highlightId = ImHexApi::HexEditor::addBackgroundHighlight({ match.address, match.size }, YaraColor);
                    match.tooltipId = ImHexApi::HexEditor::addTooltip({ match. address, match.size }, hex::format("{0} [{1}]", match.identifier, match.variable), YaraColor);
                }
            });
        }).detach();
    }

}