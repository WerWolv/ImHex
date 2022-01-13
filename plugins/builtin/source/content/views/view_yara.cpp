#include "content/views/view_yara.hpp"

#include <hex/providers/provider.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/helpers/file.hpp>
#include <hex/helpers/paths.hpp>
#include <hex/helpers/logger.hpp>

#include <yara.h>
#include <filesystem>
#include <thread>

#include <hex/helpers/paths.hpp>

namespace hex::plugin::builtin {

    ViewYara::ViewYara() : View("hex.builtin.view.yara.name") {
        yr_initialize();

        this->reloadRules();

        ContentRegistry::FileHandler::add({ ".yar" }, [](const auto &path) {
            for (auto &destPath : hex::getPath(ImHexPath::Yara)) {
                std::error_code error;
                if (fs::copy_file(path, destPath / path.filename(), fs::copy_options::overwrite_existing, error)) {
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

            if (!this->m_matching && !this->m_errorMessage.empty()) {
                View::showErrorPopup("hex.builtin.view.yara.error"_lang + this->m_errorMessage.data());
                this->m_errorMessage.clear();
            }

            ImGui::TextUnformatted("hex.builtin.view.yara.header.rules"_lang);
            ImGui::Separator();

            if (this->m_rules.empty()) {
                ImGui::TextColored(ImVec4(0.92F, 0.25F, 0.2F, 1.0F), "%s", static_cast<const char*>("hex.builtin.view.yara.no_rules"_lang));

                if (ImGui::Button("hex.builtin.view.yara.reload"_lang)) this->reloadRules();
            } else {
                ImGui::Disabled([this]{
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
                }, this->m_matching);

                if (this->m_matching) {
                    ImGui::SameLine();
                    ImGui::TextSpinner("hex.builtin.view.yara.matching"_lang);
                }
            }

            ImGui::NewLine();
            ImGui::TextUnformatted("hex.builtin.view.yara.header.matches"_lang);
            ImGui::Separator();

            if (ImGui::BeginTable("matches", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn("hex.builtin.view.yara.matches.identifier"_lang);
                ImGui::TableSetupColumn("hex.builtin.view.yara.matches.variable"_lang);
                ImGui::TableSetupColumn("hex.common.address"_lang);
                ImGui::TableSetupColumn("hex.common.size"_lang);

                ImGui::TableHeadersRow();

                ImGuiListClipper clipper;
                clipper.Begin(this->m_matches.size());

                while (clipper.Step()) {
                    for (u32 i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                        auto &[identifier, variableName, address, size, wholeDataMatch] = this->m_matches[i];
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::PushID(i);
                        if (ImGui::Selectable("match", false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap)) {
                            EventManager::post<RequestSelectionChange>(Region { u64(address), size_t(size) });
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
                            ImGui::TextColored(ImVec4(0.92F, 0.25F, 0.2F, 1.0F), "%s", static_cast<const char*>("hex.builtin.view.yara.whole_data"_lang));
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted("");
                        }
                    }
                }

                clipper.End();

                ImGui::EndTable();
            }

        }
        ImGui::End();
    }

    void ViewYara::drawMenu() {

    }

    void ViewYara::reloadRules() {
        this->m_rules.clear();

        for (auto path : hex::getPath(ImHexPath::Yara)) {
            if (!fs::exists(path))
                continue;

            for (const auto &entry : fs::recursive_directory_iterator(path)) {
                if (entry.is_regular_file() && entry.path().extension() == ".yar") {
                    this->m_rules.push_back({ fs::relative(entry.path(), fs::path(path)).string(), entry.path().string() });
                }
            }
        }
    }

    void ViewYara::applyRules() {
        this->m_matches.clear();
        this->m_errorMessage.clear();
        this->m_matching = true;

        std::thread([this] {
            if (!ImHexApi::Provider::isValid()) return;

            auto provider = ImHexApi::Provider::get();
            auto task = ImHexApi::Tasks::createTask("hex.builtin.view.yara.matching", provider->getActualSize());

            YR_COMPILER *compiler = nullptr;
            yr_compiler_create(&compiler);
            ON_SCOPE_EXIT {
                yr_compiler_destroy(compiler);
                this->m_matching = false;
            };

            yr_compiler_set_include_callback(
                    compiler,
                    [](const char *includeName, const char *callingRuleFileName, const char *callingRuleNamespace, void *userData) -> const char * {
                        auto currFilePath = static_cast<const char*>(userData);

                        File file((fs::path(currFilePath).parent_path() / includeName).string(), File::Mode::Read);
                        if (!file.isValid())
                            return nullptr;

                        auto size = file.getSize();
                        char *buffer = new char[size + 1];
                        file.readBuffer(reinterpret_cast<u8*>(buffer), size);
                        buffer[size] = 0x00;

                        return buffer;
                    },
                    [](const char *ptr, void *userData) {
                        delete[] ptr;
                    },
                    this->m_rules[this->m_selectedRule].second.data());


            File file(this->m_rules[this->m_selectedRule].second, File::Mode::Read);
            if (!file.isValid()) return;

            if (yr_compiler_add_file(compiler, file.getHandle(), nullptr, nullptr) != 0) {
                this->m_errorMessage.resize(0xFFFF);
                yr_compiler_get_error_message(compiler, this->m_errorMessage.data(), this->m_errorMessage.size());
                return;
            }

            YR_RULES *rules;
            yr_compiler_get_rules(compiler, &rules);
            ON_SCOPE_EXIT { yr_rules_destroy(rules); };

            std::vector<YaraMatch> newMatches;

            YR_MEMORY_BLOCK_ITERATOR iterator;

            struct ScanContext {
                Task *task;
                std::vector<u8> buffer;
                YR_MEMORY_BLOCK currBlock;
            };

            ScanContext context;
            context.task = &task;
            context.currBlock.base = 0;
            context.currBlock.fetch_data = [](auto *block) -> const u8* {
                auto &context = *static_cast<ScanContext*>(block->context);

                auto provider = ImHexApi::Provider::get();

                context.buffer.resize(context.currBlock.size);

                if (context.buffer.empty()) return nullptr;

                block->size = context.currBlock.size;

                provider->read(context.currBlock.base + provider->getBaseAddress(), context.buffer.data(), context.buffer.size());

                return context.buffer.data();
            };
            iterator.file_size = [](auto *iterator) -> u64 {
                return ImHexApi::Provider::get()->getActualSize();
            };

            iterator.context = &context;
            iterator.first = [](YR_MEMORY_BLOCK_ITERATOR* iterator) -> YR_MEMORY_BLOCK* {
                auto &context = *static_cast<ScanContext*>(iterator->context);

                context.currBlock.base = 0;
                context.currBlock.size = 0;
                context.buffer.clear();
                iterator->last_error = ERROR_SUCCESS;

                return iterator->next(iterator);
            };
            iterator.next = [](YR_MEMORY_BLOCK_ITERATOR* iterator) -> YR_MEMORY_BLOCK* {
                auto &context = *static_cast<ScanContext*>(iterator->context);

                u64 address = context.currBlock.base + context.currBlock.size;

                iterator->last_error = ERROR_SUCCESS;
                context.currBlock.base = address;
                context.currBlock.size = ImHexApi::Provider::get()->getActualSize() - address;
                context.currBlock.context = &context;
                context.task->update(address);

                if (context.currBlock.size == 0) return nullptr;

                return &context.currBlock;
            };


            yr_rules_scan_mem_blocks(rules, &iterator, 0, [](YR_SCAN_CONTEXT* context, int message, void *data, void *userData) -> int {
                if (message == CALLBACK_MSG_RULE_MATCHING) {
                    auto &newMatches = *static_cast<std::vector<YaraMatch>*>(userData);
                    auto rule  = static_cast<YR_RULE*>(data);

                    YR_STRING *string;
                    YR_MATCH *match;

                    if (rule->strings != nullptr) {
                        yr_rule_strings_foreach(rule, string) {
                            yr_string_matches_foreach(context, string, match) {
                                newMatches.push_back({ rule->identifier, string->identifier, match->offset, match->match_length, false });
                            }
                        }
                    } else {
                        newMatches.push_back({ rule->identifier, "", 0, 0, true });
                    }

                }

                return CALLBACK_CONTINUE;
            }, &newMatches, 0);

            std::copy(newMatches.begin(), newMatches.end(), std::back_inserter(this->m_matches));
        }).detach();

    }

}