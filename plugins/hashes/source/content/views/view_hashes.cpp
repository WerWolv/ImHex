#include "content/views/view_hashes.hpp"

#include <hex/api/project_file_manager.hpp>
#include <hex/api/achievement_manager.hpp>
#include <hex/api/events/events_interaction.hpp>
#include <hex/api/content_registry/hashes.hpp>
#include <hex/providers/memory_provider.hpp>

#include <hex/helpers/crypto.hpp>

#include <hex/ui/popup.hpp>
#include <fonts/vscode_icons.hpp>

#include <imgui_internal.h>
#include <nlohmann/json.hpp>

#include <vector>

namespace hex::plugin::hashes {

    class PopupTextHash : public Popup<PopupTextHash> {
    public:
        explicit PopupTextHash(const ViewHashes::Function &hash)
                : hex::Popup<PopupTextHash>(hash.getFunction().getName(), true, false),
                  m_hash(hash) { }

        void drawContent() override {
            ImGuiExt::Header(this->getUnlocalizedName(), true);

            ImGui::PushItemWidth(-1);
            if (ImGui::InputTextMultiline("##input", m_input)) {
                m_hash.update({ m_input.begin(), m_input.end() });
                m_result.reset();
            }


            ImGui::NewLine();
            if (m_hash.isCalculating()) {
                ImGuiExt::TextSpinner("");
            } else {
                if (!m_result.has_value()) {
                    const auto data = m_hash.get();
                    if (!data.empty())
                        m_result = crypt::encode16(data);
                }

                auto result = m_result.value_or("???");
                ImGui::InputText("##result", result, ImGuiInputTextFlags_ReadOnly);
            }

            ImGui::PopItemWidth();

            if (ImGui::IsKeyPressed(ImGuiKey_Escape))
                this->close();
        }

        [[nodiscard]] ImGuiWindowFlags getFlags() const override {
            return ImGuiWindowFlags_AlwaysAutoResize;
        }

        ImVec2 getMinSize() const override {
            return scaled({ 400, 230 });
        }

        ImVec2 getMaxSize() const override { return this->getMinSize(); }

    private:
        std::string m_input;
        std::optional<std::string> m_result;
        ViewHashes::Function m_hash;
    };

    ViewHashes::ViewHashes() : View::Window("hex.hashes.view.hashes.name", ICON_VS_KEY) {
        EventRegionSelected::subscribe(this, [this](const auto &providerRegion) {
            if (providerRegion.getProvider() != nullptr)
                for (auto &function : m_hashFunctions.get(providerRegion.getProvider()))
                    function.update(providerRegion.getRegion(), providerRegion.getProvider());
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

                                ImGui::TableNextRow();
                                ImGui::TableNextColumn();
                                ImGuiExt::TextFormatted("{}", function.getFunction().getName());

                                ImGui::TableNextColumn();
                                ImGuiExt::TextFormatted("    ");

                                ImGui::TableNextColumn();
                                ImGuiExt::TextFormatted("{}", crypt::encode16(function.get()));
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


    void ViewHashes::drawAddHashPopup() {
        const auto &hashes = ContentRegistry::Hashes::impl::getHashes();

        if (m_selectedHash == nullptr && !hashes.empty()) {
            m_selectedHash = hashes.front().get();
        }

        if (ImGui::BeginPopup("##CreateHash")) {
            {
                const auto text = "hex.hashes.view.hashes.hash_name"_lang;
                ImGui::PushItemWidth(-ImGui::CalcTextSize(text).x - ImGui::GetStyle().FramePadding.x * 2);
                ImGuiExt::InputTextIcon(text, ICON_VS_SYMBOL_KEY, m_newHashName);
                ImGui::PopItemWidth();
            }

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
                m_newHashName = fmt::format("{} {}", Lang(m_selectedHash->getUnlocalizedName()), static_cast<const char *>("hex.hashes.view.hashes.hash"_lang));

            if (ImGuiExt::BeginSubWindow("hex.ui.common.settings"_lang, nullptr, scaled({ 0, 100 }))) {
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
                    m_hashFunctions->emplace_back(m_selectedHash->create(m_newHashName));
                    AchievementManager::unlockAchievement("hex.builtin.achievement.misc", "hex.hashes.achievement.misc.create_hash.name");
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::EndDisabled();

            ImGui::EndPopup();
        }
    }


    void ViewHashes::drawContent() {
        if (ImGui::BeginTable("##hashes", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY)) {
            ImGui::TableSetupColumn("hex.hashes.view.hashes.table.name"_lang);
            ImGui::TableSetupColumn("hex.hashes.view.hashes.table.type"_lang);
            ImGui::TableSetupColumn("hex.hashes.view.hashes.table.result"_lang, ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("##buttons", ImGuiTableColumnFlags_WidthFixed, ImGui::GetTextLineHeight() * 2);

            ImGui::TableHeadersRow();

            auto provider  = ImHexApi::Provider::get();
            auto selection = ImHexApi::HexEditor::getSelection();

            u32 i = 0;
            auto itToRemove = m_hashFunctions->end();
            for (auto it = m_hashFunctions->begin(); it != m_hashFunctions->end(); ++it) {
                auto &function = *it;
                ImGui::PushID(i + 1);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                ImGui::PushStyleColor(ImGuiCol_Header, 0x00);
                ImGui::PushStyleColor(ImGuiCol_HeaderActive, 0x00);
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, 0x00);
                ImGui::Selectable(function.getFunction().getName().c_str(), false);
                ImGui::PopStyleColor(3);

                ImGui::TableNextColumn();
                ImGuiExt::TextFormatted("{}", Lang(function.getFunction().getType()->getUnlocalizedName()));

                ImGui::TableNextColumn();
                std::string result;
                if (provider != nullptr && selection.has_value()) {
                    try {
                        result = crypt::encode16(function.get());
                    } catch (const std::exception &e) {
                        result = e.what();
                    }
                } else {
                    result = "???";
                }

                if (!function.isCalculating())
                    ImGuiExt::TextFormattedSelectable("{}", result);
                else
                    ImGuiExt::TextSpinner("");

                ImGui::TableNextColumn();

                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                if (ImGuiExt::DimmedIconButton(ICON_VS_OPEN_PREVIEW, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
                    PopupTextHash::open(function);
                }
                ImGui::SameLine(0, 3_scaled);
                if (ImGuiExt::DimmedIconButton(ICON_VS_CHROME_CLOSE, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
                    itToRemove = it;
                }
                ImGui::PopStyleVar();

                ImGui::PopID();

                i += 1;
            }

            if (itToRemove != m_hashFunctions->end()) {
                m_hashFunctions->erase(itToRemove);
            }

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            const auto startPos = ImGui::GetCursorScreenPos();
            ImGui::TableNextColumn();
            ImGui::TableNextColumn();
            ImGui::Selectable("##add_hash", false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap | ImGuiSelectableFlags_NoAutoClosePopups);

            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                ImGui::OpenPopup("##CreateHash");

            ImGui::SetNextWindowPos(startPos + ImVec2(-ImGui::GetStyle().CellPadding.x, ImGui::GetTextLineHeight()), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(ImGui::GetWindowSize().x, 0), ImGuiCond_Always);
            this->drawAddHashPopup();

            ImGui::SameLine();

            {
                ImGui::PushClipRect(ImGui::GetWindowPos(), ImGui::GetWindowPos() + ImGui::GetWindowSize(), false);
                const auto text = "hex.hashes.view.hashes.table_add"_lang;
                const auto textSize = ImGui::CalcTextSize(text);
                ImGui::SetCursorPosX((ImGui::GetWindowSize().x - textSize.x) / 2);
                ImGuiExt::TextFormattedDisabled(text);
                ImGui::PopClipRect();
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

                    m_hashFunctions.get(provider).emplace_back(std::move(newFunction));
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
            const auto &function = hashFunction.getFunction();
            json["hashes"][index] = {
                    { "name", function.getName() },
                    { "type", function.getType()->getUnlocalizedName() },
                    { "settings", function.getType()->store() }
            };
            index++;
        }

        return true;
    }

    void ViewHashes::drawHelpText() {
        ImGuiExt::TextFormattedWrapped("This view allows you to compute various hashes (MD5, SHA1, etc.) on selected data regions.");
        ImGui::NewLine();
        ImGuiExt::TextFormattedWrapped("Add a new hash function by double clicking on the last row in the Hashes table and configure it to your needs. You can add multiple hash functions and see their results in real-time as you select different regions of data in the hex editor. "
                                       "Hold SHIFT while selecting data to see hash results in the tooltip.");
    }
}
