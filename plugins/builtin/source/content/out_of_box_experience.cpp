#include <imgui.h>
#include <imgui_internal.h>
#include <fonts/vscode_icons.hpp>
#include <hex/ui/imgui_imhex_extensions.h>

#include <hex/api/content_registry.hpp>
#include <hex/api/event_manager.hpp>
#include <hex/api/task_manager.hpp>
#include <hex/api/theme_manager.hpp>
#include <hex/api/tutorial_manager.hpp>
#include <hex/helpers/utils.hpp>

#include <romfs/romfs.hpp>
#include <wolv/hash/uuid.hpp>
#include <wolv/utils/guards.hpp>

#include <list>
#include <numbers>
#include <ranges>
#include <string>

namespace hex::plugin::builtin {

    namespace {

        ImGuiExt::Texture s_imhexBanner;
        ImGuiExt::Texture s_compassTexture, s_globeTexture;
        std::list<std::pair<std::fs::path, ImGuiExt::Texture>> s_screenshots;
        nlohmann::json s_screenshotDescriptions;


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

                float square = t * t;
                return square / (2.0F * (square - t) + 1.0F);
            }

            void reset() {
                m_time = 0;
            }

        private:
            float m_time;
            float m_start, m_end;
        };

        EventManager::EventList::iterator s_drawEvent;
        void drawOutOfBoxExperience() {
            static float windowAlpha = 1.0F;
            static bool oobeDone = false;
            static bool tutorialEnabled = false;

            ImGui::SetNextWindowPos(ImHexApi::System::getMainWindowPosition());
            ImGui::SetNextWindowSize(ImHexApi::System::getMainWindowSize());

            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, windowAlpha);
            ON_SCOPE_EXIT { ImGui::PopStyleVar(); };

            if (ImGui::Begin("##oobe", nullptr, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize)) {
                ImGui::BringWindowToFocusFront(ImGui::GetCurrentWindowRead());

                static Blend bannerSlideIn(-0.2F, 1.5F);
                static Blend bannerFadeIn(-0.2F, 1.5F);

                // Draw banner
                ImGui::SetCursorPos(scaled({ 25 * bannerSlideIn, 25 }));
                const auto bannerSize = s_imhexBanner.getSize() / (3.0F * (1.0F / ImHexApi::System::getGlobalScale()));
                ImGui::Image(
                    s_imhexBanner,
                    bannerSize,
                    { 0, 0 }, { 1, 1 },
                    { 1, 1, 1, (bannerFadeIn - 0.5F) * 2.0F }
                );

                static u32 page = 0;
                switch (page) {
                    // Landing page
                    case 0: {
                        static Blend textFadeIn(2.0F, 2.5F);
                        static Blend buttonFadeIn(2.5F, 3.0F);

                        // Draw welcome text
                        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, textFadeIn);
                        ImGui::SameLine();
                        if (ImGui::BeginChild("Text", ImVec2(ImGui::GetContentRegionAvail().x, bannerSize.y))) {
                            ImGuiExt::TextFormattedCentered("Welcome to ImHex!\n\nA powerful data analysis and visualization suite for Reverse Engineers, Hackers and Security Researchers.");
                        }
                        ImGui::EndChild();

                        if (!s_screenshots.empty()) {
                            const auto imageSize = s_screenshots.front().second.getSize() * ImHexApi::System::getGlobalScale();
                            const auto padding = ImGui::GetStyle().CellPadding.x;
                            const auto stride = imageSize.x + padding * 2;
                            static bool imageHovered = false;
                            static std::string clickedImage;

                            // Move last screenshot to the front of the list when the last screenshot is out of view
                            static float position = 0;
                            if (position >= stride) {
                                position = 0;
                                s_screenshots.splice(s_screenshots.begin(), s_screenshots, std::prev(s_screenshots.end()), s_screenshots.end());
                            }

                            if (!imageHovered && clickedImage.empty())
                                position += (ImGui::GetIO().DeltaTime) * 40;

                            imageHovered = false;

                            auto drawList = ImGui::GetWindowDrawList();
                            const auto drawImage = [&](const std::fs::path &fileName, const ImGuiExt::Texture &screenshot) {
                                auto pos = ImGui::GetCursorScreenPos();

                                // Draw image
                                ImGui::Image(screenshot, imageSize);
                                imageHovered = imageHovered || ImGui::IsItemHovered();
                                auto currentHovered = ImGui::IsItemHovered();

                                if (ImGui::IsItemClicked()) {
                                    clickedImage = fileName.string();
                                    ImGui::OpenPopup("FeatureDescription");
                                }

                                // Draw shadow
                                auto &style = ImGui::GetStyle();
                                float shadowSize = style.WindowShadowSize * (currentHovered ? 3.0F : 1.0F);
                                ImU32 shadowCol = ImGui::GetColorU32(ImGuiCol_WindowShadow, currentHovered ? 2.0F : 1.0F);
                                ImVec2 shadowOffset = ImVec2(ImCos(style.WindowShadowOffsetAngle), ImSin(style.WindowShadowOffsetAngle)) * style.WindowShadowOffsetDist;
                                drawList->AddShadowRect(pos, pos + imageSize, shadowCol, shadowSize, shadowOffset, ImDrawFlags_ShadowCutOutShapeBackground);

                                ImGui::SameLine();
                            };

                            ImGui::NewLine();

                            u32 repeatCount = std::ceil(std::ceil(ImHexApi::System::getMainWindowSize().x / stride) / s_screenshots.size());
                            if (repeatCount == 0)
                                repeatCount = 1;

                            // Draw top screenshot row
                            ImGui::SetCursorPosX(-position);
                            for (u32 i = 0; i < repeatCount; i += 1) {
                                for (const auto &[fileName, screenshot] : s_screenshots | std::views::reverse) {
                                    drawImage(fileName, screenshot);
                                }
                            }

                            ImGui::NewLine();

                            // Draw bottom screenshot row
                            ImGui::SetCursorPosX(-stride + position);
                            for (u32 i = 0; i < repeatCount; i += 1) {
                                for (const auto &[fileName, screenshot] : s_screenshots) {
                                    drawImage(fileName, screenshot);
                                }
                            }

                            ImGui::SetNextWindowPos(ImGui::GetWindowPos() + (ImGui::GetWindowSize() / 2), ImGuiCond_Always, ImVec2(0.5F, 0.5F));
                            ImGui::SetNextWindowSize(ImVec2(400_scaled, 0), ImGuiCond_Always);
                            if (ImGui::BeginPopup("FeatureDescription")) {
                                const auto &description = s_screenshotDescriptions[clickedImage];
                                ImGuiExt::Header(description["title"].get<std::string>().c_str(), true);
                                ImGuiExt::TextFormattedWrapped("{}", description["description"].get<std::string>().c_str());
                                ImGui::EndPopup();
                            } else {
                                clickedImage.clear();
                            }

                            // Continue button
                            const auto buttonSize = scaled({ 100, 50 });
                            ImGui::SetCursorPos(ImHexApi::System::getMainWindowSize() - buttonSize - scaled({ 10, 10 }));
                            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, buttonFadeIn);
                            if (ImGuiExt::DimmedButton(hex::format("{} {}", "hex.ui.common.continue"_lang, ICON_VS_ARROW_RIGHT).c_str(), buttonSize))
                                page += 1;
                            ImGui::PopStyleVar();
                        }


                        ImGui::PopStyleVar();


                        break;
                    }

                    // Language selection page
                    case 1: {
                        static const auto &languages = LocalizationManager::getSupportedLanguages();
                        static auto currLanguage = languages.begin();
                        static float prevTime = 0;

                        ImGui::NewLine();

                        ImGui::NewLine();
                        ImGui::NewLine();
                        ImGui::NewLine();

                        static Blend textFadeOut(2.5F, 2.9F);
                        static Blend textFadeIn(0.1F, 0.5F);

                        auto currTime = ImGui::GetTime();
                        if ((currTime - prevTime) > 3) {
                            prevTime = currTime;
                            ++currLanguage;

                            textFadeIn.reset();
                            textFadeOut.reset();
                        }

                        if (currLanguage == languages.end())
                            currLanguage = languages.begin();

                        // Draw globe image
                        const auto imageSize = s_compassTexture.getSize() / (1.5F * (1.0F / ImHexApi::System::getGlobalScale()));
                        ImGui::SetCursorPos((ImGui::GetWindowSize() / 2 - imageSize / 2) - ImVec2(0, 50_scaled));
                        ImGui::Image(s_globeTexture, imageSize);

                        ImGui::NewLine();
                        ImGui::NewLine();


                        // Draw information text
                        ImGui::SetCursorPosX(0);

                        const auto availableWidth = ImGui::GetContentRegionAvail().x;
                        if (ImGui::BeginChild("##language_text", ImVec2(availableWidth, 30_scaled))) {
                            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_Text, textFadeIn - textFadeOut));
                            ImGuiExt::TextFormattedCentered("{}", LocalizationManager::getLocalizedString("hex.builtin.setting.interface.language", currLanguage->first));
                            ImGui::PopStyleColor();
                        }
                        ImGui::EndChild();

                        ImGui::NewLine();

                        // Draw language selection list
                        ImGui::SetCursorPosX(availableWidth / 3);
                        if (ImGui::BeginListBox("##language", ImVec2(availableWidth / 3, 0))) {
                            for (const auto &[langId, language] : LocalizationManager::getSupportedLanguages()) {
                                if (ImGui::Selectable(language.c_str(), langId == LocalizationManager::getSelectedLanguage())) {
                                    LocalizationManager::loadLanguage(langId);
                                }
                            }
                            ImGui::EndListBox();
                        }

                        // Continue button
                        const auto buttonSize = scaled({ 100, 50 });
                        ImGui::SetCursorPos(ImHexApi::System::getMainWindowSize() - buttonSize - scaled({ 10, 10 }));
                        if (ImGuiExt::DimmedButton(hex::format("{} {}", "hex.ui.common.continue"_lang, ICON_VS_ARROW_RIGHT).c_str(), buttonSize))
                            page += 1;

                        break;
                    }

                    // Server contact page
                    case 2: {
                        static ImVec2 subWindowSize = { 0, 0 };
                        const auto windowSize = ImHexApi::System::getMainWindowSize();

                        // Draw telemetry subwindow
                        ImGui::SetCursorPos((windowSize - subWindowSize) / 2);
                        if (ImGuiExt::BeginSubWindow("hex.builtin.oobe.server_contact"_lang, nullptr, subWindowSize, ImGuiChildFlags_AutoResizeY)) {
                            // Draw telemetry information
                            auto yBegin = ImGui::GetCursorPosY();
                            std::string message = "hex.builtin.oobe.server_contact.text"_lang;
                            ImGuiExt::TextFormattedWrapped("{}", message.c_str());
                            ImGui::NewLine();

                            // Draw table containing everything that's being reported
                            if (ImGui::CollapsingHeader("hex.builtin.oobe.server_contact.data_collected_title"_lang)) {
                                if (ImGui::BeginTable("hex.builtin.oobe.server_contact.data_collected_table", 2,
                                                     ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_NoHostExtendY,
                                                     ImVec2(ImGui::GetContentRegionAvail().x, 150_scaled))) {
                                    ImGui::TableSetupColumn("hex.builtin.oobe.server_contact.data_collected_table.key"_lang);
                                    ImGui::TableSetupColumn("hex.builtin.oobe.server_contact.data_collected_table.value"_lang, ImGuiTableColumnFlags_WidthStretch);
                                    ImGui::TableSetupScrollFreeze(0, 1);

                                    ImGui::TableHeadersRow();

                                    ImGui::TableNextRow();
                                    ImGui::TableNextColumn();
                                    ImGui::TextUnformatted("hex.builtin.oobe.server_contact.data_collected.uuid"_lang);
                                    ImGui::TableNextColumn();
                                    ImGui::TextWrapped("%s", s_uuid.c_str());

                                    ImGui::TableNextRow();
                                    ImGui::TableNextColumn();
                                    ImGui::TextUnformatted("hex.builtin.oobe.server_contact.data_collected.version"_lang);
                                    ImGui::TableNextColumn();
                                    ImGuiExt::TextFormattedWrapped("{}\n{}@{}\n{}",
                                                                ImHexApi::System::getImHexVersion().get(),
                                                                ImHexApi::System::getCommitHash(true),
                                                                ImHexApi::System::getCommitBranch(),
                                                                ImHexApi::System::isPortableVersion() ? "Portable" : "Installed"
                                     );

                                    ImGui::TableNextRow();
                                    ImGui::TableNextColumn();
                                    ImGui::TextUnformatted("hex.builtin.oobe.server_contact.data_collected.os"_lang);
                                    ImGui::TableNextColumn();
                                    ImGuiExt::TextFormattedWrapped("{}\n{}\n{}\n{}\nCorporate Environment: {}",
                                                                ImHexApi::System::getOSName(),
                                                                ImHexApi::System::getOSVersion(),
                                                                ImHexApi::System::getArchitecture(),
                                                                ImHexApi::System::getGPUVendor(),
                                                                ImHexApi::System::isCorporateEnvironment() ? "Yes" : "No");

                                    ImGui::EndTable();
                                }
                            }

                            ImGui::NewLine();

                            const auto width = ImGui::GetWindowWidth();
                            const auto buttonSize = ImVec2(width / 3 - ImGui::GetStyle().FramePadding.x * 3, 0);
                            const auto buttonPos = [&](u8 index) { return ImGui::GetStyle().FramePadding.x + (buttonSize.x + ImGui::GetStyle().FramePadding.x * 3) * index; };

                            // Draw allow button
                            ImGui::SetCursorPosX(buttonPos(0));
                            if (ImGui::Button("hex.ui.common.allow"_lang, buttonSize)) {
                                ContentRegistry::Settings::write<int>("hex.builtin.setting.general", "hex.builtin.setting.general.server_contact", 1);
                                ContentRegistry::Settings::write<int>("hex.builtin.setting.general", "hex.builtin.setting.general.upload_crash_logs", 1);
                                page += 1;
                            }

                            ImGui::SameLine();

                            // Draw crash logs only button
                            ImGui::SetCursorPosX(buttonPos(1));
                            if (ImGui::Button("hex.builtin.oobe.server_contact.crash_logs_only"_lang, buttonSize)) {
                                ContentRegistry::Settings::write<int>("hex.builtin.setting.general", "hex.builtin.setting.general.server_contact", 0);
                                ContentRegistry::Settings::write<int>("hex.builtin.setting.general", "hex.builtin.setting.general.upload_crash_logs", 1);
                                page += 1;
                            }

                            ImGui::SameLine();

                            // Draw deny button
                            ImGui::SetCursorPosX(buttonPos(2));
                            if (ImGui::Button("hex.ui.common.deny"_lang, buttonSize)) {
                                ContentRegistry::Settings::write<int>("hex.builtin.setting.general", "hex.builtin.setting.general.server_contact", 0);
                                ContentRegistry::Settings::write<int>("hex.builtin.setting.general", "hex.builtin.setting.general.upload_crash_logs", 0);
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
                    case 3: {
                        ImGui::NewLine();

                        ImGui::NewLine();
                        ImGui::NewLine();
                        ImGui::NewLine();

                        // Draw compass image
                        const auto imageSize = s_compassTexture.getSize() / (1.5F * (1.0F / ImHexApi::System::getGlobalScale()));
                        ImGui::SetCursorPos((ImGui::GetWindowSize() / 2 - imageSize / 2) - ImVec2(0, 50_scaled));
                        ImGui::Image(s_compassTexture, imageSize);

                        // Draw information text about playing the tutorial
                        ImGui::SetCursorPosX(0);
                        ImGuiExt::TextFormattedCentered("hex.builtin.oobe.tutorial_question"_lang);

                        // Draw no button
                        const auto buttonSize = scaled({ 100, 50 });
                        ImGui::SetCursorPos(ImHexApi::System::getMainWindowSize() - ImVec2(buttonSize.x * 2 + 20, buttonSize.y + 10));
                        if (ImGuiExt::DimmedButton("hex.ui.common.no"_lang, buttonSize)) {
                            oobeDone = true;
                        }

                        // Draw yes button
                        ImGui::SetCursorPos(ImHexApi::System::getMainWindowSize() - ImVec2(buttonSize.x + 10, buttonSize.y + 10));
                        if (ImGuiExt::DimmedButton("hex.ui.common.yes"_lang, buttonSize)) {
                            tutorialEnabled = true;
                            oobeDone = true;
                        }
                        break;
                    }
                    default:
                        page = 0;
                }
            }
            ImGui::End();

            // Handle finishing the out of box experience
            if (oobeDone) {
                static Blend backgroundFadeOut(0.0F, 1.0F);
                windowAlpha = 1.0F - backgroundFadeOut;

                if (backgroundFadeOut >= 1.0F) {
                    if (tutorialEnabled) {
                        TutorialManager::startTutorial("hex.builtin.tutorial.introduction");
                    } else {
                        ContentRegistry::Settings::write<bool>("hex.builtin.setting.interface", "hex.builtin.setting.interface.achievement_popup", false);
                    }

                    TaskManager::doLater([] {
                        ImHexApi::System::setWindowResizable(true);
                        EventFrameBegin::unsubscribe(s_drawEvent);
                    });
                }
            }
        }

    }

    void setupOutOfBoxExperience() {
        // Don't show the out of box experience in the web version
        #if defined(OS_WEB)
            return;
        #endif

        // Check if there is a telemetry uuid
        s_uuid = ContentRegistry::Settings::read<std::string>("hex.builtin.setting.general", "hex.builtin.setting.general.uuid", "");

        if (s_uuid.empty()) {
            // Generate a new UUID
            s_uuid = wolv::hash::generateUUID();

            // Save UUID to settings
            ContentRegistry::Settings::write<std::string>("hex.builtin.setting.general", "hex.builtin.setting.general.uuid", s_uuid);
        }

        EventFirstLaunch::subscribe([] {
            ImHexApi::System::setWindowResizable(false);

            const auto imageTheme = ThemeManager::getImageTheme();
            s_imhexBanner    = ImGuiExt::Texture::fromSVG(romfs::get(hex::format("assets/{}/banner.svg", imageTheme)).span<std::byte>());
            s_compassTexture = ImGuiExt::Texture::fromImage(romfs::get("assets/common/compass.png").span<std::byte>());
            s_globeTexture   = ImGuiExt::Texture::fromImage(romfs::get("assets/common/globe.png").span<std::byte>());
            s_screenshotDescriptions = nlohmann::json::parse(romfs::get("assets/screenshot_descriptions.json").string());

            for (const auto &path : romfs::list("assets/screenshots")) {
                s_screenshots.emplace_back(path.filename(), ImGuiExt::Texture::fromImage(romfs::get(path).span<std::byte>(), ImGuiExt::Texture::Filter::Linear));
            }

            s_drawEvent = EventFrameBegin::subscribe(drawOutOfBoxExperience);
        });
    }

}
