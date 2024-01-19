#include <imgui.h>
#include <imgui_internal.h>
#include <fonts/codicons_font.h>
#include <hex/ui/imgui_imhex_extensions.h>

#include <hex/api/content_registry.hpp>
#include <hex/api/event_manager.hpp>
#include <hex/api/theme_manager.hpp>
#include <hex/helpers/utils.hpp>

#include <romfs/romfs.hpp>
#include <wolv/hash/uuid.hpp>

namespace hex::plugin::builtin {

    namespace {

        ImGuiExt::Texture s_imhexBanner;
        std::string s_uuid;

        class Blend {
        public:
            Blend(float start, float end) : m_time(0), m_start(start), m_end(end) {}

            [[nodiscard]] operator float() {
                m_time += ImGui::GetIO().DeltaTime;

                float t = m_time;
                t -= m_start;
                t /= (m_end - m_start);
                t = std::clamp(t, 0.0F, 1.0F);
                log::info("{}", t);

                float square = t * t;
                return square / (2.0F * (square - t) + 1.0F);
            }

        private:
            float m_time;
            float m_start, m_end;
        };

        EventManager::EventList::iterator s_drawEvent;
        void drawOutOfBoxExperience() {
            ImGui::SetNextWindowPos(ImHexApi::System::getMainWindowPosition());
            ImGui::SetNextWindowSize(ImHexApi::System::getMainWindowSize());
            if (ImGui::Begin("##oobe", nullptr, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize)) {
                ImGui::BringWindowToFocusFront(ImGui::GetCurrentWindowRead());

                static u32 page = 0;
                switch (page) {
                    // Landing page
                    case 0: {
                        static Blend bannerSlideIn(-0.2F, 1.5F);
                        static Blend bannerFadeIn(-0.2F, 1.5F);
                        static Blend textFadeIn(2.0F, 2.5F);
                        static Blend buttonFadeIn(2.5F, 3.0F);

                        // Draw banner
                        ImGui::SetCursorPos(scaled({ 25 * bannerSlideIn, 25 }));
                        ImGui::Image(
                            s_imhexBanner,
                            s_imhexBanner.getSize() / (1.5F * (1.0F / ImHexApi::System::getGlobalScale())),
                            { 0, 0 }, { 1, 1 },
                            { 1, 1, 1, (bannerFadeIn - 0.5F) * 2.0F }
                        );

                        // Draw welcome text
                        ImGui::SetCursorPos({ 0, 0 });
                        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, textFadeIn);
                        ImGuiExt::TextFormattedCentered("Welcome to ImHex!\n\nA powerful data analysis and visualization suite for Reverse Engineers, Hackers and Security Researchers.");
                        ImGui::PopStyleVar();

                        // Continue button
                        const auto buttonSize = scaled({ 100, 50 });
                        ImGui::SetCursorPos(ImHexApi::System::getMainWindowSize() - buttonSize - scaled({ 10, 10 }));
                        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, buttonFadeIn);
                        if (ImGuiExt::DimmedButton(hex::format("Continue {}", ICON_VS_ARROW_RIGHT).c_str(), buttonSize))
                            page += 1;
                        ImGui::PopStyleVar();

                        break;
                    }

                    // Server contact page
                    case 1: {
                        static ImVec2 subWindowSize = { 0, 0 };
                        const auto windowSize = ImHexApi::System::getMainWindowSize();

                        // Draw banner
                        ImGui::SetCursorPos(scaled({ 25, 25 }));
                        ImGui::Image(
                            s_imhexBanner,
                            s_imhexBanner.getSize() / (1.5F * (1.0F / ImHexApi::System::getGlobalScale()))
                        );


                        ImGui::SetCursorPos((windowSize - subWindowSize) / 2);
                        ImGuiExt::BeginSubWindow("Data Reporting", subWindowSize, ImGuiChildFlags_AutoResizeY);
                        {
                            auto yBegin = ImGui::GetCursorPosY();
                            std::string message = "hex.builtin.welcome.server_contact_text"_lang;
                            ImGuiExt::TextFormattedWrapped("{}", message.c_str());
                            ImGui::NewLine();

                            if (ImGui::CollapsingHeader("hex.builtin.welcome.server_contact.data_collected_title"_lang)) {
                                if (ImGui::BeginTable("hex.builtin.welcome.server_contact.data_collected_table"_lang, 2,
                                                     ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_NoHostExtendY,
                                                     ImVec2(ImGui::GetContentRegionAvail().x, 150_scaled))) {
                                    ImGui::TableSetupColumn("hex.builtin.welcome.server_contact.data_collected_table.key"_lang);
                                    ImGui::TableSetupColumn("hex.builtin.welcome.server_contact.data_collected_table.value"_lang, ImGuiTableColumnFlags_WidthStretch);
                                    ImGui::TableSetupScrollFreeze(0, 1);

                                    ImGui::TableHeadersRow();

                                    ImGui::TableNextRow();
                                    ImGui::TableNextColumn();
                                    ImGui::TextUnformatted("hex.builtin.welcome.server_contact.data_collected.uuid"_lang);
                                    ImGui::TableNextColumn();
                                    ImGui::TextWrapped("%s", s_uuid.c_str());

                                    ImGui::TableNextRow();
                                    ImGui::TableNextColumn();
                                    ImGui::TextUnformatted("hex.builtin.welcome.server_contact.data_collected.version"_lang);
                                    ImGui::TableNextColumn();
                                    ImGuiExt::TextFormattedWrapped("{}\n{}@{}\n{}",
                                                                ImHexApi::System::getImHexVersion(),
                                                                ImHexApi::System::getCommitHash(true),
                                                                ImHexApi::System::getCommitBranch(),
                                                                ImHexApi::System::isPortableVersion() ? "Portable" : "Installed"
                                     );

                                    ImGui::TableNextRow();
                                    ImGui::TableNextColumn();
                                    ImGui::TextUnformatted("hex.builtin.welcome.server_contact.data_collected.os"_lang);
                                    ImGui::TableNextColumn();
                                    ImGuiExt::TextFormattedWrapped("{}\n{}\n{}\n{}",
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
                            if (ImGui::Button("hex.ui.common.allow"_lang, buttonSize)) {
                                ContentRegistry::Settings::write("hex.builtin.setting.general", "hex.builtin.setting.general.server_contact", 1);
                                ContentRegistry::Settings::write("hex.builtin.setting.general", "hex.builtin.setting.general.upload_crash_logs", 1);
                                page += 1;
                            }
                            ImGui::SameLine();
                            ImGui::SetCursorPosX(buttonPos(1));
                            if (ImGui::Button("hex.builtin.welcome.server_contact.crash_logs_only"_lang, buttonSize)) {
                                ContentRegistry::Settings::write("hex.builtin.setting.general", "hex.builtin.setting.general.server_contact", 0);
                                ContentRegistry::Settings::write("hex.builtin.setting.general", "hex.builtin.setting.general.upload_crash_logs", 1);
                                page += 1;
                            }
                            ImGui::SameLine();
                            ImGui::SetCursorPosX(buttonPos(2));
                            if (ImGui::Button("hex.ui.common.deny"_lang, buttonSize)) {
                                page += 1;
                            }

                            auto yEnd = ImGui::GetCursorPosY();
                            subWindowSize = ImGui::GetWindowSize();
                            subWindowSize.y = (yEnd - yBegin) + 35_scaled;
                        }
                        ImGuiExt::EndSubWindow();

                        break;
                    }

                    // Tutorial page
                    case 2: {
                        
                    }
                    default:
                        EventFrameBegin::unsubscribe(s_drawEvent);
                }
            }
            ImGui::End();
        }

    }

    void setupOutOfBoxExperience() {
        // Check if there is a telemetry uuid
        s_uuid = ContentRegistry::Settings::read("hex.builtin.setting.general", "hex.builtin.setting.general.uuid", "").get<std::string>();

        if (s_uuid.empty()) {
            // Generate a new UUID
            s_uuid = wolv::hash::generateUUID();

            // Save UUID to settings
            ContentRegistry::Settings::write("hex.builtin.setting.general", "hex.builtin.setting.general.uuid", s_uuid);
        }

        EventFirstLaunch::subscribe([] {
            const auto imageTheme = ThemeManager::getImageTheme();
            s_imhexBanner = ImGuiExt::Texture(romfs::get(hex::format("assets/{}/banner.png", imageTheme)).span<std::byte>());

            s_drawEvent = EventFrameBegin::subscribe(drawOutOfBoxExperience);
        });
    }

}
