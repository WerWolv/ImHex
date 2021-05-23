#include "views/view_yara.hpp"

#include <hex/providers/provider.hpp>

#include <yara.h>
#include <filesystem>
#include <thread>

#include <imgui_imhex_extensions.h>

namespace hex {

    ViewYara::ViewYara() : View("hex.view.yara.name") {
        yr_initialize();

        this->reloadRules();
    }

    ViewYara::~ViewYara() {
        yr_finalize();
    }

    void ViewYara::drawContent() {
        if (ImGui::Begin(View::toWindowName("hex.view.yara.name").c_str(), &this->getWindowOpenState(), ImGuiWindowFlags_NoCollapse)) {

            if (!this->m_matching && !this->m_errorMessage.empty()) {
                View::showErrorPopup("hex.view.yara.error"_lang + this->m_errorMessage.data());
                this->m_errorMessage.clear();
            }

            ImGui::TextUnformatted("hex.view.yara.header.rules"_lang);
            ImGui::Separator();

            if (this->m_rules.empty()) {
                ImGui::TextColored(ImVec4(0.92F, 0.25F, 0.2F, 1.0F), "%s", static_cast<const char*>("hex.view.yara.no_rules"_lang));

                if (ImGui::Button("hex.view.yara.reload"_lang)) this->reloadRules();
            } else {
                ImGui::Disabled([this]{
                    if (ImGui::BeginCombo("hex.view.yara.header.rules"_lang, this->m_rules[this->m_selectedRule].c_str())) {
                        for (u32 i = 0; i < this->m_rules.size(); i++) {
                            const bool selected = (this->m_selectedRule == i);
                            if (ImGui::Selectable(this->m_rules[i].c_str(), selected))
                                this->m_selectedRule = i;

                            if (selected)
                                ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("hex.view.yara.reload"_lang)) this->reloadRules();
                    if (ImGui::Button("hex.view.yara.match"_lang)) this->applyRules();
                }, this->m_matching);

                if (this->m_matching) {
                    ImGui::SameLine();
                    ImGui::TextSpinner("hex.view.yara.matching"_lang);
                }
            }

            ImGui::NewLine();
            ImGui::TextUnformatted("hex.view.yara.header.matches"_lang);
            ImGui::Separator();

            if (ImGui::BeginTable("matches", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn("hex.view.yara.matches.identifier"_lang);
                ImGui::TableSetupColumn("hex.common.address"_lang);
                ImGui::TableSetupColumn("hex.common.size"_lang);

                ImGui::TableHeadersRow();

                ImGuiListClipper clipper;
                clipper.Begin(this->m_matches.size());

                while (clipper.Step()) {
                    for (u32 i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                        auto &[identifier, address, size, wholeDataMatch] = this->m_matches[i];
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::PushID(i);
                        if (ImGui::Selectable("match", false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap)) {
                            EventManager::post<RequestSelectionChange>(Region { u64(address), size_t(size) });
                        }
                        ImGui::PopID();
                        ImGui::SameLine();
                        ImGui::TextUnformatted(identifier.c_str());

                        if (!wholeDataMatch) {
                            ImGui::TableNextColumn();
                            ImGui::Text("0x%llX : 0x%llX", address, address + size - 1);
                            ImGui::TableNextColumn();
                            ImGui::Text("0x%lX", size);
                        } else {
                            ImGui::TableNextColumn();
                            ImGui::TextColored(ImVec4(0.92F, 0.25F, 0.2F, 1.0F), "%s", static_cast<const char*>("hex.view.yara.whole_data"_lang));
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

        if (!std::filesystem::exists("./yara"))
            return;

        for (const auto &entry : std::filesystem::directory_iterator("yara")) {
            if (entry.is_regular_file())
                this->m_rules.push_back(entry.path().string());
        }
    }

    void ViewYara::applyRules() {
        this->m_matches.clear();
        this->m_errorMessage.clear();
        this->m_matching = true;

        std::thread([this] {

            YR_COMPILER *compiler = nullptr;
            yr_compiler_create(&compiler);

            FILE *file = fopen(this->m_rules[this->m_selectedRule].c_str(), "r");
            if (file == nullptr) return;
            ON_SCOPE_EXIT { fclose(file); };

            if (yr_compiler_add_file(compiler, file, nullptr, nullptr) != 0) {
                this->m_errorMessage.resize(0xFFFF);
                yr_compiler_get_error_message(compiler, this->m_errorMessage.data(), this->m_errorMessage.size());
                this->m_matching = false;
                return;
            }

            YR_RULES *rules;
            yr_compiler_get_rules(compiler, &rules);

            auto &provider = SharedData::currentProvider;

            std::vector<YaraMatch> newMatches;

            YR_MEMORY_BLOCK_ITERATOR iterator;

            struct ScanContext {
                std::vector<u8> buffer;
                YR_MEMORY_BLOCK currBlock;
            };

            ScanContext context;
            context.currBlock.base = 0;
            context.currBlock.fetch_data = [](auto *block) -> const u8* {
                auto &context = *static_cast<ScanContext*>(block->context);

                auto &provider = SharedData::currentProvider;

                context.buffer.resize(std::min<u64>(0xF'FFFF, provider->getSize() - context.currBlock.base));

                if (context.buffer.empty()) return nullptr;

                provider->readRelative(context.currBlock.base, context.buffer.data(), context.buffer.size());

                return context.buffer.data();
            };
            iterator.file_size = [](auto *iterator) -> u64 {
                return SharedData::currentProvider->getSize();
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
                context.currBlock.size = std::min<u64>(0xF'FFFF, SharedData::currentProvider->getSize() - address);
                context.currBlock.context = &context;

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
                                newMatches.push_back({ rule->identifier, match->offset, match->match_length, false });
                            }
                        }
                    } else {
                        newMatches.push_back({ rule->identifier, 0, 0, true });
                    }

                }

                return CALLBACK_CONTINUE;
            }, &newMatches, 0);

            std::copy(newMatches.begin(), newMatches.end(), std::back_inserter(this->m_matches));

            yr_compiler_destroy(compiler);

            this->m_matching = false;
        }).detach();

    }

}