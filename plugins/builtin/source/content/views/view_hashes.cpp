#include "content/views/view_hashes.hpp"

#include <hex/helpers/crypto.hpp>

#include <vector>

namespace hex::plugin::builtin {

    ViewHashes::ViewHashes() : View("hex.builtin.view.hashes.name") {
        EventManager::subscribe<EventRegionSelected>(this, [this](const auto &) {
            for (auto &function : this->m_hashFunctions)
                function.reset();
        });

        ImHexApi::HexEditor::addTooltipProvider([this](u64 address, const u8 *data, size_t size) {
            hex::unused(data);

            auto selection = ImHexApi::HexEditor::getSelection();

            if (ImGui::GetIO().KeyShift) {
                if (!this->m_hashFunctions.empty() && selection.has_value() && selection->overlaps(Region { address, size })) {
                    ImGui::BeginTooltip();

                    if (ImGui::BeginTable("##tooltips", 1, ImGuiTableFlags_NoHostExtendX | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoClip)) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();

                        ImGui::TextUnformatted("hex.builtin.view.hashes.name"_lang);
                        ImGui::Separator();

                        ImGui::Indent();
                        if (ImGui::BeginTable("##hashes_tooltip", 3, ImGuiTableFlags_NoHostExtendX | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
                            auto provider  = ImHexApi::Provider::get();
                            for (auto &function : this->m_hashFunctions) {
                                ImGui::TableNextRow();
                                ImGui::TableNextColumn();
                                ImGui::TextFormatted("{}", function.getName());

                                ImGui::TableNextColumn();
                                ImGui::TextFormatted("    ");

                                ImGui::TableNextColumn();
                                if (provider != nullptr)
                                    ImGui::TextFormatted("{}", crypt::encode16(function.get(*selection, provider)));
                            }

                            ImGui::EndTable();
                        }
                        ImGui::Unindent();

                        ImGui::EndTable();
                    }

                    ImGui::EndTooltip();
                }
            }
        });
    }

    ViewHashes::~ViewHashes() {
        EventManager::unsubscribe<EventRegionSelected>(this);
    }


    void ViewHashes::drawContent() {
        const auto &hashes = ContentRegistry::Hashes::impl::getHashes();

        if (this->m_selectedHash == nullptr && !hashes.empty()) {
            this->m_selectedHash = hashes.front();
        }

        if (ImGui::Begin(View::toWindowName("hex.builtin.view.hashes.name").c_str(), &this->getWindowOpenState(), ImGuiWindowFlags_NoCollapse)) {
            if (ImGui::BeginCombo("hex.builtin.view.hashes.function"_lang, this->m_selectedHash != nullptr ? LangEntry(this->m_selectedHash->getUnlocalizedName()) : "")) {

                for (const auto hash : hashes) {
                    if (ImGui::Selectable(LangEntry(hash->getUnlocalizedName()), this->m_selectedHash == hash)) {
                        this->m_selectedHash = hash;
                        this->m_newHashName.clear();
                    }
                }

                ImGui::EndCombo();
            }

            if (this->m_newHashName.empty() && this->m_selectedHash != nullptr)
                this->m_newHashName = hex::format("{} {}", LangEntry(this->m_selectedHash->getUnlocalizedName()), static_cast<const char *>("hex.builtin.view.hashes.hash"_lang));

            if (ImGui::BeginChild("##settings", ImVec2(ImGui::GetContentRegionAvail().x, 200_scaled), true)) {
                if (this->m_selectedHash != nullptr) {
                    auto startPos = ImGui::GetCursorPosY();
                    this->m_selectedHash->draw();

                    // Check if no elements have been added
                    if (startPos == ImGui::GetCursorPosY()) {
                        ImGui::TextFormattedCentered("hex.builtin.view.hashes.no_settings"_lang);
                    }
                }
            }
            ImGui::EndChild();

            ImGui::NewLine();

            ImGui::InputTextIcon("##hash_name", ICON_VS_SYMBOL_KEY, this->m_newHashName);
            ImGui::SameLine();

            ImGui::BeginDisabled(this->m_newHashName.empty() || this->m_selectedHash == nullptr);
            if (ImGui::IconButton(ICON_VS_ADD, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
                if (this->m_selectedHash != nullptr)
                    this->m_hashFunctions.push_back(this->m_selectedHash->create(this->m_newHashName));
            }
            ImGui::EndDisabled();

            if (ImGui::BeginTable("##hashes", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Borders, ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetTextLineHeightWithSpacing() * 10))) {
                ImGui::TableSetupColumn("hex.builtin.view.hashes.table.name"_lang);
                ImGui::TableSetupColumn("hex.builtin.view.hashes.table.type"_lang);
                ImGui::TableSetupColumn("hex.builtin.view.hashes.table.result"_lang, ImGuiTableColumnFlags_WidthStretch);

                ImGui::TableHeadersRow();

                auto provider  = ImHexApi::Provider::get();
                auto selection = ImHexApi::HexEditor::getSelection();

                std::optional<u32> indexToRemove;
                for (u32 i = 0; i < this->m_hashFunctions.size(); i++) {
                    auto &function = this->m_hashFunctions[i];

                    ImGui::PushID(i);

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();

                    ImGui::PushStyleColor(ImGuiCol_Header, 0x00);
                    ImGui::PushStyleColor(ImGuiCol_HeaderActive, 0x00);
                    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, 0x00);
                    ImGui::Selectable(function.getName().c_str(), false);
                    ImGui::PopStyleColor(3);

                    {
                        const auto ContextMenuId = hex::format("Context Menu {}", i);

                        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
                            ImGui::OpenPopup(ContextMenuId.c_str());

                        if (ImGui::BeginPopup(ContextMenuId.c_str())) {
                            if (ImGui::MenuItem("hex.builtin.view.hashes.remove"_lang))
                                indexToRemove = i;

                            ImGui::EndPopup();
                        }
                    }

                    ImGui::TableNextColumn();
                    ImGui::TextFormatted("{}", LangEntry(function.getType()->getUnlocalizedName()));

                    ImGui::TableNextColumn();
                    std::string result;
                    if (provider != nullptr && selection.has_value())
                        result = crypt::encode16(function.get(*selection, provider));
                    else
                        result = "???";

                    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
                    ImGui::InputText("##result", result, ImGuiInputTextFlags_ReadOnly);
                    ImGui::PopItemWidth();

                    ImGui::PopID();
                }

                if (indexToRemove.has_value()) {
                    this->m_hashFunctions.erase(this->m_hashFunctions.begin() + indexToRemove.value());
                }

                ImGui::EndTable();
            }

            ImGui::NewLine();
            ImGui::TextWrapped("%s", static_cast<const char *>("hex.builtin.view.hashes.hover_info"_lang));
        }
        ImGui::End();
    }

}
