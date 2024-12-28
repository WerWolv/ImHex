#include "content/views/view_hashes.hpp"

#include <hex/api/project_file_manager.hpp>
#include <hex/api/achievement_manager.hpp>
#include <hex/providers/memory_provider.hpp>

#include <hex/helpers/crypto.hpp>

#include <hex/ui/popup.hpp>
#include <fonts/vscode_icons.hpp>

#include <vector>

namespace hex::plugin::hashes {

    class PopupTextHash : public Popup<PopupTextHash> {
    public:
        explicit PopupTextHash(const ContentRegistry::Hashes::Hash::Function &hash)
                : hex::Popup<PopupTextHash>(hash.getName(), true, false),
                  m_hash(hash) { }

        void drawContent() override {
            ImGuiExt::Header(this->getUnlocalizedName(), true);

            ImGui::PushItemWidth(-1);
            if (ImGui::InputTextMultiline("##input", m_input)) {
                prv::MemoryProvider provider({ m_input.begin(), m_input.end() });

                m_hash.reset();
                auto bytes = m_hash.get(Region { 0x00, provider.getActualSize() }, &provider);

                m_result = crypt::encode16(bytes);
            }

            ImGui::NewLine();
            ImGui::InputText("##result", m_result, ImGuiInputTextFlags_ReadOnly);
            ImGui::PopItemWidth();

            if (ImGui::IsKeyPressed(ImGuiKey_Escape))
                this->close();
        }

        [[nodiscard]] ImGuiWindowFlags getFlags() const override {
            return ImGuiWindowFlags_AlwaysAutoResize;
        }

        ImVec2 getMinSize() const override {
            return scaled({ 400, 200 });
        }

        ImVec2 getMaxSize() const override { return this->getMinSize(); }

    private:
        std::string m_input;
        std::string m_result;
        ContentRegistry::Hashes::Hash::Function m_hash;
    };

    ViewHashes::ViewHashes() : View::Window("hex.hashes.view.hashes.name", ICON_VS_KEY) {
        EventRegionSelected::subscribe(this, [this](const auto &providerRegion) {
            for (auto &function : m_hashFunctions.get(providerRegion.getProvider()))
                function.reset();
        });

        ImHexApi::HexEditor::addTooltipProvider([this](u64 address, const u8 *data, size_t size) {
            std::ignore = data;

            auto selection = ImHexApi::HexEditor::getSelection();

            if (selection.has_value() && ImGui::GetIO().KeyShift) {
                auto &hashFunctions = m_hashFunctions.get(selection->getProvider());
                if (!hashFunctions.empty() && selection.has_value() && selection->overlaps(Region { address, size })) {
                    ImGui::BeginTooltip();

                    if (ImGui::BeginTable("##tooltips", 1, ImGuiTableFlags_NoHostExtendX | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoClip, ImMax(ImGui::GetContentRegionAvail(), ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 5)))) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();

                        ImGui::TextUnformatted("hex.hashes.view.hashes.name"_lang);
                        ImGui::Separator();

                        ImGui::Indent();
                        if (ImGui::BeginTable("##hashes_tooltip", 3, ImGuiTableFlags_NoHostExtendX | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
                            auto provider  = ImHexApi::Provider::get();
                            for (auto &function : hashFunctions) {
                                if (provider == nullptr)
                                    continue;

                                std::vector<u8> bytes;
                                try {
                                    bytes = function.get(*selection, provider);
                                } catch (const std::exception &) {
                                    continue;
                                }

                                ImGui::TableNextRow();
                                ImGui::TableNextColumn();
                                ImGuiExt::TextFormatted("{}", function.getName());

                                ImGui::TableNextColumn();
                                ImGuiExt::TextFormatted("    ");

                                ImGui::TableNextColumn();
                                ImGuiExt::TextFormatted("{}", crypt::encode16(bytes));
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

        ProjectFile::registerPerProviderHandler({
            .basePath = "hashes.json",
            .required = false,
            .load = [this](prv::Provider *provider, const std::fs::path &basePath, const Tar &tar) -> bool {
                auto fileContent = tar.readString(basePath);
                if (fileContent.empty())
                    return true;

                auto data = nlohmann::json::parse(fileContent.begin(), fileContent.end());
                m_hashFunctions->clear();

                return this->importHashes(provider, data);
            },
            .store = [this](prv::Provider *provider, const std::fs::path &basePath, const Tar &tar) -> bool {
                nlohmann::json data;

                bool result = this->exportHashes(provider, data);
                tar.writeString(basePath, data.dump(4));

                return result;
            }
        });
    }

    ViewHashes::~ViewHashes() {
        EventRegionSelected::unsubscribe(this);
    }


    void ViewHashes::drawContent() {
        const auto &hashes = ContentRegistry::Hashes::impl::getHashes();

        if (m_selectedHash == nullptr && !hashes.empty()) {
            m_selectedHash = hashes.front().get();
        }

        if (ImGuiExt::DimmedButton("hex.hashes.view.hashes.add"_lang)) {
            ImGui::OpenPopup("##CreateHash");
        }

        ImGui::SetNextWindowSize(scaled({ 400, 0 }), ImGuiCond_Always);
        if (ImGui::BeginPopup("##CreateHash")) {
            ImGuiExt::InputTextIcon("hex.hashes.view.hashes.hash_name"_lang, ICON_VS_SYMBOL_KEY, m_newHashName);

            ImGui::NewLine();

            if (ImGui::BeginCombo("hex.hashes.view.hashes.function"_lang, m_selectedHash != nullptr ? Lang(m_selectedHash->getUnlocalizedName()) : "")) {
                for (const auto &hash : hashes) {
                    if (ImGui::Selectable(Lang(hash->getUnlocalizedName()), m_selectedHash == hash.get())) {
                        m_selectedHash = hash.get();
                        m_newHashName.clear();
                    }
                }

                ImGui::EndCombo();
            }

            if (m_newHashName.empty() && m_selectedHash != nullptr)
                m_newHashName = hex::format("{} {}", Lang(m_selectedHash->getUnlocalizedName()), static_cast<const char *>("hex.hashes.view.hashes.hash"_lang));

            if (ImGuiExt::BeginSubWindow("hex.ui.common.settings"_lang, nullptr, scaled({ 0, 250 }))) {
                if (m_selectedHash != nullptr) {
                    auto startPos = ImGui::GetCursorPosY();
                    m_selectedHash->draw();

                    // Check if no elements have been added
                    if (startPos == ImGui::GetCursorPosY()) {
                        ImGuiExt::TextFormattedCentered("hex.hashes.view.hashes.no_settings"_lang);
                    }
                }
            }
            ImGuiExt::EndSubWindow();

            ImGui::BeginDisabled(m_newHashName.empty() || m_selectedHash == nullptr);
            if (ImGuiExt::DimmedButton("hex.hashes.view.hashes.add"_lang, ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                if (m_selectedHash != nullptr) {
                    m_hashFunctions->push_back(m_selectedHash->create(m_newHashName));
                    AchievementManager::unlockAchievement("hex.builtin.achievement.misc", "hex.hashes.achievement.misc.create_hash.name");
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::EndDisabled();

            ImGui::EndPopup();
        }

        ImGui::SameLine(0, 10_scaled);
        ImGuiExt::HelpHover("hex.hashes.view.hashes.hover_info"_lang, ICON_VS_INFO);

        if (ImGui::BeginTable("##hashes", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY)) {
            ImGui::TableSetupColumn("hex.hashes.view.hashes.table.name"_lang);
            ImGui::TableSetupColumn("hex.hashes.view.hashes.table.type"_lang);
            ImGui::TableSetupColumn("hex.hashes.view.hashes.table.result"_lang, ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("##buttons", ImGuiTableColumnFlags_WidthFixed, 50_scaled);

            ImGui::TableHeadersRow();

            auto provider  = ImHexApi::Provider::get();
            auto selection = ImHexApi::HexEditor::getSelection();

            std::optional<u32> indexToRemove;
            for (u32 i = 0; i < m_hashFunctions->size(); i++) {
                auto &function = (*m_hashFunctions)[i];

                ImGui::PushID(i);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                ImGui::PushStyleColor(ImGuiCol_Header, 0x00);
                ImGui::PushStyleColor(ImGuiCol_HeaderActive, 0x00);
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, 0x00);
                ImGui::Selectable(function.getName().c_str(), false);
                ImGui::PopStyleColor(3);

                ImGui::TableNextColumn();
                ImGuiExt::TextFormatted("{}", Lang(function.getType()->getUnlocalizedName()));

                ImGui::TableNextColumn();
                std::string result;
                if (provider != nullptr && selection.has_value()) {
                    try {
                        result = crypt::encode16(function.get(*selection, provider));
                    } catch (const std::exception &e) {
                        result = e.what();
                    }
                }
                else
                    result = "???";

                ImGui::PushItemWidth(-1);
                ImGui::InputText("##result", result, ImGuiInputTextFlags_ReadOnly);
                ImGui::PopItemWidth();

                ImGui::TableNextColumn();

                if (ImGuiExt::IconButton(ICON_VS_OPEN_PREVIEW, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
                    PopupTextHash::open(function);
                }
                ImGui::SameLine();
                if (ImGuiExt::IconButton(ICON_VS_X, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
                    indexToRemove = i;
                }

                ImGui::PopID();
            }

            if (indexToRemove.has_value()) {
                m_hashFunctions->erase(m_hashFunctions->begin() + indexToRemove.value());
            }

            ImGui::EndTable();
        }
    }

    bool ViewHashes::importHashes(prv::Provider *provider, const nlohmann::json &json) {
        if (!json.contains("hashes"))
            return false;

        const auto &hashes = ContentRegistry::Hashes::impl::getHashes();

        for (const auto &hash : json["hashes"]) {
            if (!hash.contains("name") || !hash.contains("type") || !hash.contains("settings"))
                continue;

            for (const auto &newHash : hashes) {
                if (newHash->getUnlocalizedName() == hash["type"].get<std::string>()) {

                    auto newFunction = newHash->create(hash["name"]);
                    newFunction.getType()->load(hash["settings"]);

                    m_hashFunctions.get(provider).push_back(std::move(newFunction));
                    break;
                }
            }
        }

        return true;
    }

    bool ViewHashes::exportHashes(prv::Provider *provider, nlohmann::json &json) {
        json["hashes"] = nlohmann::json::array();
        size_t index = 0;
        for (const auto &hashFunction : m_hashFunctions.get(provider)) {
            json["hashes"][index] = {
                    { "name", hashFunction.getName() },
                    { "type", hashFunction.getType()->getUnlocalizedName() },
                    { "settings", hashFunction.getType()->store() }
            };
            index++;
        }

        return true;
    }

}
