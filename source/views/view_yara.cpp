#include "views/view_yara.hpp"

#include <hex/providers/provider.hpp>

#include <yara.h>
#include <filesystem>
#include <thread>

namespace hex {

    ViewYara::ViewYara() : View("Yara") {
        yr_initialize();

        this->reloadRules();
    }

    ViewYara::~ViewYara() {
        yr_finalize();
    }

    void ViewYara::drawContent() {
        if (ImGui::Begin("Yara", &this->getWindowOpenState(), ImGuiWindowFlags_NoCollapse)) {

            ImGui::TextUnformatted("Rules");
            ImGui::Separator();

            if (this->m_rules.empty()) {
                ImGui::TextColored(ImVec4(0.92F, 0.25F, 0.2F, 1.0F), "No YARA rules found. Put them in the 'yara' folder next to the ImHex executable");

                if (ImGui::Button("Reload")) this->reloadRules();
            } else {
                if (ImGui::BeginCombo("Rule", this->m_rules[this->m_selectedRule].c_str())) {
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
                if (ImGui::Button("R")) this->reloadRules();
                ImGui::SameLine();
                if (ImGui::Button("Apply")) std::thread([this]{ this->applyRules(); }).detach();
            }

            ImGui::NewLine();
            ImGui::TextUnformatted("Matches");
            ImGui::Separator();

            if (ImGui::BeginTable("matches", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn("Identifier");
                ImGui::TableSetupColumn("Address");
                ImGui::TableSetupColumn("Size");

                ImGui::TableHeadersRow();

                ImGuiListClipper clipper;
                clipper.Begin(this->m_matches.size());

                while (clipper.Step()) {
                    for (u32 i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                        auto &[identifier, address, size] = this->m_matches[i];
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::PushID(i);
                        if (ImGui::Selectable("match", false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap)) {
                            Region selectRegion = { u64(address), size_t(size) };
                            View::postEvent(Events::SelectionChangeRequest, selectRegion);
                        }
                        ImGui::PopID();
                        ImGui::SameLine();
                        ImGui::TextUnformatted(identifier.c_str());
                        ImGui::TableNextColumn();
                        ImGui::Text("0x%llX : 0x%llX", address, address + size - 1);
                        ImGui::TableNextColumn();
                        ImGui::Text("0x%lX", size);
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

        YR_COMPILER *compiler = nullptr;
        yr_compiler_create(&compiler);

        FILE *file = fopen(this->m_rules[this->m_selectedRule].c_str(), "r");
        if (file == nullptr) return;

        yr_compiler_add_file(compiler, file, nullptr, nullptr);
        fclose(file);

        YR_RULES *rules;
        yr_compiler_get_rules(compiler, &rules);

        auto &provider = SharedData::currentProvider;

        std::vector<u8> buffer(0xFFFF, 0x00);
        for (u64 offset = 0; offset < provider->getSize(); offset += buffer.size()) {
            if (provider->getSize() - offset < buffer.size())
                buffer.resize(provider->getSize() - offset);

            SharedData::currentProvider->read(offset, buffer.data(), buffer.size());

            std::vector<YaraMatch> newMatches;

            auto result = yr_rules_scan_mem(rules, buffer.data(), buffer.size(), 0, [](YR_SCAN_CONTEXT* context, int message, void *data, void *userData) -> int {
                if (message == CALLBACK_MSG_RULE_MATCHING) {
                    auto &newMatches = *static_cast<std::vector<YaraMatch>*>(userData);
                    auto rule  = static_cast<YR_RULE*>(data);

                    YR_STRING *string;
                    YR_MATCH *match;
                    yr_rule_strings_foreach(rule, string) {
                        yr_string_matches_foreach(context, string, match) {
                            newMatches.push_back({ rule->identifier, match->offset, match->match_length });
                        }
                    }
                }

                return CALLBACK_CONTINUE;
            }, &newMatches, 0);

            if (result != ERROR_SUCCESS)
                break;

            for (auto &newMatch : newMatches) {
                newMatch.address += offset;
                this->m_matches.push_back(newMatch);
            }
        }

        yr_compiler_destroy(compiler);
    }

}