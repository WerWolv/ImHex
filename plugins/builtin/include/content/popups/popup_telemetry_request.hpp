#include <hex/ui/popup.hpp>

#include <hex/api/localization.hpp>

#include <wolv/hash/uuid.hpp>

#include <functional>
#include <string>

namespace hex::plugin::builtin {

    class PopupTelemetryRequest : public Popup<PopupTelemetryRequest> {
    public:
        PopupTelemetryRequest()
                : hex::Popup<PopupTelemetryRequest>("hex.builtin.common.question", false) {
            // Check if there is a telemetry uuid
            this->m_uuid = ContentRegistry::Settings::read("hex.builtin.setting.general", "hex.builtin.setting.general.uuid", "");
            if(this->m_uuid.empty()) {
                // Generate a new uuid
                this->m_uuid = wolv::hash::generateUUID();
                // Save
                ContentRegistry::Settings::write("hex.builtin.setting.general", "hex.builtin.setting.general.uuid", this->m_uuid);
            }
        }

        void drawContent() override {
            static std::string message = "hex.builtin.welcome.server_contact_text"_lang;
            ImGui::TextFormattedWrapped("{}", message.c_str());
            ImGui::NewLine();

            if(ImGui::CollapsingHeader("hex.builtin.welcome.server_contact.data_collected_title"_lang)) {
                if(ImGui::BeginTable("hex.builtin.welcome.server_contact.data_collected_table"_lang, 2,
                                     ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_NoHostExtendY,
                                     ImVec2(ImGui::GetContentRegionAvail().x, 100_scaled))) {
                    ImGui::TableSetupColumn("hex.builtin.welcome.server_contact.data_collected_table.key"_lang);
                    ImGui::TableSetupColumn("hex.builtin.welcome.server_contact.data_collected_table.value"_lang, ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupScrollFreeze(0, 1);

                    ImGui::TableHeadersRow();

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted("hex.builtin.welcome.server_contact.data_collected.uuid"_lang);
                    ImGui::TableNextColumn();
                    ImGui::TextWrapped("%s", this->m_uuid.c_str());

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted("hex.builtin.welcome.server_contact.data_collected.version"_lang);
                    ImGui::TableNextColumn();
                    ImGui::TextFormattedWrapped("{}\n{}@{}\n{}",
                                                IMHEX_VERSION,
                                                #if defined (GIT_COMMIT_HASH_LONG) && defined (GIT_BRANCH)
                                                   GIT_COMMIT_HASH_LONG, GIT_BRANCH,
                                                #else
                                                   "??", "??",
                                                #endif
                                                ImHexApi::System::isPortableVersion() ? "Portable" : "Installed"
                     );

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted("hex.builtin.welcome.server_contact.data_collected.os"_lang);
                    ImGui::TableNextColumn();
                    ImGui::TextFormattedWrapped("{}\n{}\n{}\n{}",
                                                ImHexApi::System::getOSName(),
                                                ImHexApi::System::getOSVersion(),
                                                ImHexApi::System::getArchitecture(),
                                                ImHexApi::System::getGPUVendor());

                    ImGui::EndTable();
                }
            }

            ImGui::NewLine();

            const auto width = ImGui::GetWindowWidth();
            const auto buttonSize = ImVec2(width / 3 - ImGui::GetStyle().FramePadding.x * 3, 0);
            const auto buttonPos = [&](u8 index) { return ImGui::GetStyle().FramePadding.x + (buttonSize.x + ImGui::GetStyle().FramePadding.x * 3) * index; };

            ImGui::SetCursorPosX(buttonPos(0));
            if (ImGui::Button("hex.builtin.common.allow"_lang, buttonSize)) {
                ContentRegistry::Settings::write("hex.builtin.setting.general", "hex.builtin.setting.general.server_contact", 1);
                ContentRegistry::Settings::write("hex.builtin.setting.general", "hex.builtin.setting.general.upload_crash_logs", 1);
                this->close();
            }
            ImGui::SameLine();
            ImGui::SetCursorPosX(buttonPos(1));
            if (ImGui::Button("hex.builtin.welcome.server_contact.crash_logs_only"_lang, buttonSize)) {
                ContentRegistry::Settings::write("hex.builtin.setting.general", "hex.builtin.setting.general.server_contact", 0);
                ContentRegistry::Settings::write("hex.builtin.setting.general", "hex.builtin.setting.general.upload_crash_logs", 1);
                this->close();
            }
            ImGui::SameLine();
            ImGui::SetCursorPosX(buttonPos(2));
            if (ImGui::Button("hex.builtin.common.deny"_lang, buttonSize)) {
                this->close();
            }

            ImGui::SetWindowPos((ImHexApi::System::getMainWindowSize() - ImGui::GetWindowSize()) / 2, ImGuiCond_Appearing);
        }

        [[nodiscard]] ImGuiWindowFlags getFlags() const override {
            return ImGuiWindowFlags_AlwaysAutoResize;
        }

        [[nodiscard]] ImVec2 getMinSize() const override {
            return scaled({ 500, 100 });
        }

        [[nodiscard]] ImVec2 getMaxSize() const override {
            return scaled({ 500, 450 });
        }

    private:
        std::string m_uuid;
    };

}