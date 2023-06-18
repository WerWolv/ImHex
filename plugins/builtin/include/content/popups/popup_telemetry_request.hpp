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
            this->m_uuid = ContentRegistry::Settings::read("hex.builtin.setting.telemetry", "hex.builtin.setting.telemetry.uuid", "");
            if(this->m_uuid.empty()) {
                // Generate a new uuid
                this->m_uuid = wolv::hash::generateUUID();
                // Save
                ContentRegistry::Settings::write("hex.builtin.setting.telemetry", "hex.builtin.setting.telemetry.uuid", this->m_uuid);
            }
        }

        void drawContent() override {
            static std::string message = "hex.builtin.welcome.server_contact_text"_lang;
            ImGui::TextFormattedWrapped("{}", message.c_str());
            ImGui::NewLine();
            ImGui::Separator();

            if(ImGui::CollapsingHeader("hex.builtin.welcome.server_contact.data_collected_title"_lang)) {
                if(ImGui::BeginTable("hex.builtin.welcome.server_contact.data_collected_table"_lang, 2,
                                     ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollX | ImGuiTableFlags_NoHostExtendY,
                                     ImVec2(500_scaled, 100_scaled))) {
                    ImGui::TableSetupColumn("hex.builtin.welcome.server_contact.data_collected_table.key"_lang);
                    ImGui::TableSetupColumn("hex.builtin.welcome.server_contact.data_collected_table.value"_lang, ImGuiTableColumnFlags_WidthStretch);
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
                    ImGui::TextUnformatted(IMHEX_VERSION);

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted("hex.builtin.welcome.server_contact.data_collected.os"_lang);
                    ImGui::TableNextColumn();
                    ImGui::TextWrapped("%s/%s/%s", ImHexApi::System::getOSName().c_str(), ImHexApi::System::getOSVersion().c_str(), ImHexApi::System::getArchitecture().c_str());

                    ImGui::EndTable();
                }
            }

            auto width = ImGui::GetWindowWidth();
            ImGui::SetCursorPosX(width / 9);
            if (ImGui::Button("hex.builtin.common.yes"_lang, ImVec2(width / 3, 0))) {
                ContentRegistry::Settings::write("hex.builtin.setting.general", "hex.builtin.setting.general.server_contact", 1);
                this->close();
            }
            ImGui::SameLine();
            ImGui::SetCursorPosX(width / 9 * 5);
            if (ImGui::Button("hex.builtin.common.no"_lang, ImVec2(width / 3, 0))) {
                this->close();
            }

            ImGui::SetWindowPos((ImHexApi::System::getMainWindowSize() - ImGui::GetWindowSize()) / 2, ImGuiCond_Appearing);
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
        std::string m_uuid;
        int m_crashCount;
    };

}