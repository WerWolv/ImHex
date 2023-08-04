#include "content/views/view_hashes.hpp"

#include "content/providers/memory_file_provider.hpp"

#include <hex/api/project_file_manager.hpp>
#include <hex/api/achievement_manager.hpp>

#include <hex/ui/popup.hpp>
#include <hex/helpers/crypto.hpp>

#include <vector>

namespace hex::plugin::builtin {

    class PopupTextHash : public Popup<PopupTextHash> {
    public:
        explicit PopupTextHash(ContentRegistry::Hashes::Hash::Function hash)
                : hex::Popup<PopupTextHash>(hash.getName(), true, false),
                  m_hash(hash) { }

        void drawContent() override {
            ImGui::Header(this->getUnlocalizedName().c_str(), true);

            ImGui::PushItemWidth(-1);
            if (ImGui::InputTextMultiline("##input", this->m_input)) {
                auto provider = std::make_unique<MemoryFileProvider>();
                provider->resize(this->m_input.size());
                provider->writeRaw(0x00, this->m_input.data(), this->m_input.size());

                this->m_hash.reset();
                auto bytes = this->m_hash.get(Region { 0x00, provider->getActualSize() }, provider.get());

                this->m_result = crypt::encode16(bytes);
            }

            ImGui::NewLine();
            ImGui::InputText("##result", this->m_result, ImGuiInputTextFlags_ReadOnly);
            ImGui::PopItemWidth();
        }

        [[nodiscard]] ImGuiWindowFlags getFlags() const override {
            return ImGuiWindowFlags_AlwaysAutoResize;
        }

        [[nodiscard]] ImVec2 getMinSize() const override {
            return scaled({ 400, 100 });
        }

        [[nodiscard]] ImVec2 getMaxSize() const override {
            return scaled({ 600, 300 });
        }

    private:
        std::string m_input;
        std::string m_result;
        ContentRegistry::Hashes::Hash::Function m_hash;
    };

    ViewHashes::ViewHashes() : View("hex.builtin.view.hashes.name") {
        EventManager::subscribe<EventRegionSelected>(this, [this](const auto &providerRegion) {
            for (auto &function : this->m_hashFunctions.get(providerRegion.getProvider()))
                function.reset();
        });

        ImHexApi::HexEditor::addTooltipProvider([this](u64 address, const u8 *data, size_t size) {
            hex::unused(data);

            auto selection = ImHexApi::HexEditor::getSelection();

            if (selection.has_value() && ImGui::GetIO().KeyShift) {
                auto &hashFunctions = this->m_hashFunctions.get(selection->getProvider());
                if (!hashFunctions.empty() && selection.has_value() && selection->overlaps(Region { address, size })) {
                    ImGui::BeginTooltip();

                    if (ImGui::BeginTable("##tooltips", 1, ImGuiTableFlags_NoHostExtendX | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoClip)) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();

                        ImGui::TextUnformatted("hex.builtin.view.hashes.name"_lang);
                        ImGui::Separator();

                        ImGui::Indent();
                        if (ImGui::BeginTable("##hashes_tooltip", 3, ImGuiTableFlags_NoHostExtendX | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
                            auto provider  = ImHexApi::Provider::get();
                            for (auto &function : hashFunctions) {
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

        ProjectFile::registerPerProviderHandler({
            .basePath = "hashes.json",
            .required = false,
            .load = [this](prv::Provider *provider, const std::fs::path &basePath, Tar &tar) -> bool {
                auto fileContent = tar.readString(basePath);
                if (fileContent.empty())
                    return true;

                auto data = nlohmann::json::parse(fileContent.begin(), fileContent.end());
                this->m_hashFunctions->clear();

                return this->importHashes(provider, data);
            },
            .store = [this](prv::Provider *provider, const std::fs::path &basePath, Tar &tar) -> bool {
                nlohmann::json data;

                bool result = this->exportHashes(provider, data);
                tar.writeString(basePath, data.dump(4));

                return result;
            }
        });
    }

    ViewHashes::~ViewHashes() {
        EventManager::unsubscribe<EventRegionSelected>(this);
    }


    void ViewHashes::drawContent() {
        const auto &hashes = ContentRegistry::Hashes::impl::getHashes();

        if (this->m_selectedHash == nullptr && !hashes.empty()) {
            this->m_selectedHash = hashes.front().get();
        }

        if (ImGui::Begin(View::toWindowName("hex.builtin.view.hashes.name").c_str(), &this->getWindowOpenState(), ImGuiWindowFlags_NoCollapse)) {
            if (ImGui::BeginCombo("hex.builtin.view.hashes.function"_lang, this->m_selectedHash != nullptr ? LangEntry(this->m_selectedHash->getUnlocalizedName()) : "")) {

                for (const auto &hash : hashes) {
                    if (ImGui::Selectable(LangEntry(hash->getUnlocalizedName()), this->m_selectedHash == hash.get())) {
                        this->m_selectedHash = hash.get();
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


            ImGui::InputTextIcon("##hash_name", ICON_VS_SYMBOL_KEY, this->m_newHashName);
            ImGui::SameLine();

            ImGui::BeginDisabled(this->m_newHashName.empty() || this->m_selectedHash == nullptr);
            if (ImGui::IconButton(ICON_VS_ADD, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
                if (this->m_selectedHash != nullptr) {
                    this->m_hashFunctions->push_back(this->m_selectedHash->create(this->m_newHashName));
                    AchievementManager::unlockAchievement("hex.builtin.achievement.misc", "hex.builtin.achievement.misc.create_hash.name");
                }
            }
            ImGui::EndDisabled();

            ImGui::SameLine();
            ImGui::HelpHover("hex.builtin.view.hashes.hover_info"_lang);

            if (ImGui::BeginTable("##hashes", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY)) {
                ImGui::TableSetupColumn("hex.builtin.view.hashes.table.name"_lang);
                ImGui::TableSetupColumn("hex.builtin.view.hashes.table.type"_lang);
                ImGui::TableSetupColumn("hex.builtin.view.hashes.table.result"_lang, ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("##buttons", ImGuiTableColumnFlags_WidthFixed, 50_scaled);

                ImGui::TableHeadersRow();

                auto provider  = ImHexApi::Provider::get();
                auto selection = ImHexApi::HexEditor::getSelection();

                std::optional<u32> indexToRemove;
                for (u32 i = 0; i < this->m_hashFunctions->size(); i++) {
                    auto &function = (*this->m_hashFunctions)[i];

                    ImGui::PushID(i);

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();

                    ImGui::PushStyleColor(ImGuiCol_Header, 0x00);
                    ImGui::PushStyleColor(ImGuiCol_HeaderActive, 0x00);
                    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, 0x00);
                    ImGui::Selectable(function.getName().c_str(), false);
                    ImGui::PopStyleColor(3);

                    ImGui::TableNextColumn();
                    ImGui::TextFormatted("{}", LangEntry(function.getType()->getUnlocalizedName()));

                    ImGui::TableNextColumn();
                    std::string result;
                    if (provider != nullptr && selection.has_value())
                        result = crypt::encode16(function.get(*selection, provider));
                    else
                        result = "???";

                    ImGui::PushItemWidth(-1);
                    ImGui::InputText("##result", result, ImGuiInputTextFlags_ReadOnly);
                    ImGui::PopItemWidth();

                    ImGui::TableNextColumn();

                    if (ImGui::IconButton(ICON_VS_OPEN_PREVIEW, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
                        PopupTextHash::open(function);
                    }
                    ImGui::SameLine();
                    if (ImGui::IconButton(ICON_VS_X, ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarRed))) {
                        indexToRemove = i;
                    }

                    ImGui::PopID();
                }

                if (indexToRemove.has_value()) {
                    this->m_hashFunctions->erase(this->m_hashFunctions->begin() + indexToRemove.value());
                }

                ImGui::EndTable();
            }
        }
        ImGui::End();
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

                    this->m_hashFunctions.get(provider).push_back(std::move(newFunction));
                    break;
                }
            }
        }

        return true;
    }

    bool ViewHashes::exportHashes(prv::Provider *provider, nlohmann::json &json) {
        json["hashes"] = nlohmann::json::array();
        size_t index = 0;
        for (const auto &hashFunction : this->m_hashFunctions.get(provider)) {
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
