#include <algorithm>
#include <hex.hpp>

#include <hex/api/workspace_manager.hpp>
#include <hex/api/content_registry/settings.hpp>
#include <hex/api/content_registry/views.hpp>
#include <hex/api/content_registry/provider.hpp>
#include <hex/api/content_registry/user_interface.hpp>
#include <hex/api/localization_manager.hpp>
#include <hex/api/theme_manager.hpp>
#include <hex/api/layout_manager.hpp>
#include <hex/api/achievement_manager.hpp>
#include <hex/api_urls.hpp>

#include <hex/api/events/events_provider.hpp>
#include <hex/api/events/events_gui.hpp>
#include <hex/api/events/requests_gui.hpp>

#include <hex/ui/view.hpp>
#include <toasts/toast_notification.hpp>

#include <hex/helpers/http_requests.hpp>
#include <hex/helpers/fs.hpp>
#include <hex/helpers/logger.hpp>
#include <hex/helpers/default_paths.hpp>

#include <hex/api/project_file_manager.hpp>

#include <imgui.h>
#include <hex/ui/imgui_imhex_extensions.h>

#include <nlohmann/json.hpp>
#include <romfs/romfs.hpp>

#include <wolv/io/file.hpp>
#include <wolv/io/fs.hpp>

#include <fonts/vscode_icons.hpp>

#include <content/recent.hpp>

#include <string>
#include <random>
#include <banners/banner_button.hpp>
#include <banners/banner_icon.hpp>
#include <fonts/fonts.hpp>

namespace hex::plugin::builtin {

    namespace {
        AutoReset<ImGuiExt::Texture> s_bannerTexture, s_nightlyTexture, s_backdropTexture, s_infoBannerTexture;

        std::string s_tipOfTheDay;

        bool s_simplifiedWelcomeScreen = false;

        class PopupRestoreBackup : public Popup<PopupRestoreBackup> {
        private:
            std::fs::path m_logFilePath;
            bool m_hasAutoBackups;
            std::function<void()> m_restoreCallback;
            std::function<void()> m_deleteCallback;
            bool m_reportError = true;
        public:
            PopupRestoreBackup(std::fs::path logFilePath, bool hasAutoBackups, const std::function<void()> &restoreCallback, const std::function<void()> &deleteCallback)
                    : Popup("hex.builtin.popup.safety_backup.title"),
                    m_logFilePath(std::move(logFilePath)),
                    m_hasAutoBackups(hasAutoBackups),
                    m_restoreCallback(restoreCallback),
                    m_deleteCallback(deleteCallback) {

                m_reportError = ContentRegistry::Settings::read<bool>("hex.builtin.setting.general", "hex.builtin.setting.general.upload_crash_logs", true);
            }

            void drawContent() override {
                ImGui::TextUnformatted("hex.builtin.popup.safety_backup.desc"_lang);
                if (!m_logFilePath.empty()) {
                    ImGui::NewLine();
                    ImGui::TextUnformatted("hex.builtin.popup.safety_backup.log_file"_lang);
                    ImGui::SameLine(0, 2_scaled);
                    if (ImGuiExt::Hyperlink(m_logFilePath.filename().string().c_str())) {
                        fs::openFolderWithSelectionExternal(m_logFilePath);
                    }

                    ImGui::Checkbox("hex.builtin.popup.safety_backup.report_error"_lang, &m_reportError);
                    ImGui::NewLine();
                }

                auto width = ImGui::GetWindowWidth();
                ImGui::SetCursorPosX(width / 9);
                if (ImGui::Button("hex.builtin.popup.safety_backup.restore"_lang, ImVec2(width / 3, 0))) {
                    m_restoreCallback();
                    m_deleteCallback();

                    if (m_reportError) {
                        wolv::io::File logFile(m_logFilePath, wolv::io::File::Mode::Read);
                        if (logFile.isValid()) {
                            // Read current log file data
                            auto data = logFile.readString();

                            // Anonymize the log file
                            {
                                for (const auto &paths : paths::All) {
                                    for (auto &folder : paths->all()) {
                                        auto parent = wolv::util::toUTF8String(folder.parent_path());
                                        data = wolv::util::replaceStrings(data, parent, "<*****>");
                                    }
                                }
                            }

                            TaskManager::createBackgroundTask("hex.builtin.task.uploading_crash", [path = m_logFilePath, data](auto&){
                                HttpRequest request("POST", ImHexApiURL + std::string("/crash_upload"));
                                request.uploadFile(std::vector<u8>(data.begin(), data.end()), "file", path.filename()).wait();
                            });
                        }
                    }

                    ContentRegistry::Settings::write<int>("hex.builtin.setting.general", "hex.builtin.setting.general.upload_crash_logs", m_reportError);

                    this->close();
                }
                ImGui::SameLine();
                ImGui::SetCursorPosX(width / 9 * 5);
                if (ImGui::Button("hex.builtin.popup.safety_backup.delete"_lang, ImVec2(width / 3, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                    m_deleteCallback();

                    this->close();
                }

                if (m_hasAutoBackups) {
                    if (ImGui::Button("hex.builtin.popup.safety_backup.show_auto_backups"_lang, ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                        recent::PopupAutoBackups::open();
                    }
                }
            }
        };

        class PopupTipOfTheDay : public Popup<PopupTipOfTheDay> {
        public:
            PopupTipOfTheDay() : Popup("hex.builtin.popup.tip_of_the_day.title", true, false) { }

            void drawContent() override {
                ImGuiExt::Header("hex.builtin.welcome.tip_of_the_day"_lang, true);

                ImGuiExt::TextFormattedWrapped("{}", s_tipOfTheDay.c_str());
                ImGui::NewLine();

                static bool dontShowAgain = false;
                if (ImGui::Checkbox("hex.ui.common.dont_show_again"_lang, &dontShowAgain)) {
                    ContentRegistry::Settings::write<bool>("hex.builtin.setting.general", "hex.builtin.setting.general.show_tips", !dontShowAgain);
                }

                ImGui::SameLine((ImGui::GetMainViewport()->Size / 3 - ImGui::CalcTextSize("hex.ui.common.close"_lang) - ImGui::GetStyle().FramePadding).x);

                if (ImGui::Button("hex.ui.common.close"_lang) || ImGui::IsKeyPressed(ImGuiKey_Escape))
                    Popup::close();
            }
        };

        void loadDefaultLayout() {
            LayoutManager::loadFromString(std::string(romfs::get("layouts/default.hexlyt").string()));
        }

        bool isAnyViewOpen() {
            const auto &views = ContentRegistry::Views::impl::getEntries();
            return std::ranges::any_of(views,
                [](const auto &entry) {
                    return entry.second->getWindowOpenState();
                });
        }

        void drawTiles(ImDrawList *drawList, ImVec2 position, ImVec2 size, float lineDistance) {
            const auto tileCount = size / lineDistance;

            struct Segment {
                i32 x, y;
                bool operator==(const Segment&) const = default;
            };
            static std::list<Segment> segments;
            static std::optional<Segment> colTile;
            static ImGuiDir direction;
            static auto rng = std::mt19937(std::random_device{}());
            static bool over = true;
            static i32 overCounter = 0;
            static u32 spaceCount = 0;

            if (ImGui::IsKeyPressed(ImGuiKey_Space, false)) {
                spaceCount += 1;

                if (spaceCount >= 5) {
                    spaceCount = 0;
                    segments = { { 10, 10 }, { 10, 11 }, { 10, 12 } };
                    direction = ImGuiDir_Right;
                    colTile.reset();
                    over = false;
                }
            }

            if (over)
                return;

            ImHexApi::System::unlockFrameRate();

            const auto drawTile = [&](u32 x, u32 y) {
                drawList->AddRectFilled(
                    {
                        position.x + (float(x) * lineDistance),
                        position.y + (float(y) * lineDistance)
                    },
                    {
                        position.x + (float(x + 1) * lineDistance) - 1_scaled,
                        position.y + (float(y + 1) * lineDistance) - 1_scaled
                    }, ImGui::GetColorU32(ImGuiCol_Text, 0.1F)
                );
            };

            if (colTile.has_value()) {
                drawTile(colTile->x, colTile->y);
            }

            for (const auto &[x, y] : segments) {
                drawTile(x, y);
            }

            if (overCounter != 0) {
                for (u32 x = 0; x < u32(tileCount.x); x += 1) {
                    for (u32 y = 0; y < u32(tileCount.y); y += 1) {
                        if ((x + y) % 2 == u32(overCounter % 2))
                            drawTile(x, y);
                    }
                }
            }

            static double lastTick = 0;
            double tick = ImGui::GetTime();
            if (lastTick + 0.2 < tick) {
                Segment nextSegment = segments.front();
                switch (direction) {
                    case ImGuiDir_Up:       nextSegment.y -= 1; break;
                    case ImGuiDir_Down:     nextSegment.y += 1; break;
                    case ImGuiDir_Left:     nextSegment.x -= 1; break;
                    case ImGuiDir_Right:    nextSegment.x += 1; break;
                    default: break;
                }

                if (overCounter == 0) {
                    for (const auto &segment : segments) {
                        if (segment == nextSegment) overCounter = 5;
                        if (segment.x < 0 || segment.y < 0) overCounter = 5;
                        if (segment.x > i32(tileCount.x) || segment.y > i32(tileCount.y)) overCounter = 5;
                    }

                    segments.push_front(nextSegment);

                    if (colTile.has_value() && nextSegment != *colTile) {
                        segments.pop_back();
                    } else {
                        colTile = { i32(rng() % u32(tileCount.x)), i32(rng() % u32(tileCount.x)) };
                    }
                } else {
                    overCounter -= 1;
                    if (overCounter <= 0)
                      over = true;
                }

                lastTick = tick;
            }

            if (ImGui::IsKeyDown(ImGuiKey_UpArrow)    && direction != ImGuiDir_Down)   direction = ImGuiDir_Up;
            if (ImGui::IsKeyDown(ImGuiKey_DownArrow)  && direction != ImGuiDir_Up)     direction = ImGuiDir_Down;
            if (ImGui::IsKeyDown(ImGuiKey_LeftArrow)  && direction != ImGuiDir_Right)  direction = ImGuiDir_Left;
            if (ImGui::IsKeyDown(ImGuiKey_RightArrow) && direction != ImGuiDir_Left)   direction = ImGuiDir_Right;
        }

        void drawWelcomeScreenBackground() {
            const auto position = ImGui::GetWindowPos();
            const auto size = ImGui::GetWindowSize();
            auto drawList = ImGui::GetWindowDrawList();

            const auto lineDistance = 20_scaled;
            const auto lineColor = ImGui::GetColorU32(ImGuiCol_Text, 0.03F);

            for (auto x = position.x; x < position.x + size.x + lineDistance; x += lineDistance) {
                drawList->AddLine({ x, position.y }, { x, position.y + size.y }, lineColor);
            }
            for (auto y = position.y; y < position.y + size.y + lineDistance; y += lineDistance) {
                drawList->AddLine({ position.x, y }, { position.x + size.x, y }, lineColor);
            }

            drawTiles(drawList, position, size, lineDistance);
        }

        void drawWelcomeScreenContentSimplified() {
            const ImVec2 backdropSize = scaled({ 350, 350 });
            ImGui::SetCursorPos((ImGui::GetContentRegionAvail() - backdropSize) / 2);
            ImGui::Image(*s_backdropTexture, backdropSize);

            ImGuiExt::TextFormattedCentered("hex.builtin.welcome.drop_file"_lang);
        }

        void drawWelcomeScreenContentFull() {
            const ImVec2 margin = scaled({ 30, 20 });

            ImGui::SetCursorPos(margin);
            if (ImGui::BeginTable("Welcome Outer", 1, ImGuiTableFlags_None, ImGui::GetContentRegionAvail() - margin)) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyleColorVec4(ImGuiCol_PopupBg));
                ON_SCOPE_EXIT { ImGui::PopStyleColor(); };

                const auto availableSpace = ImGui::GetContentRegionAvail();
                if (ImGui::BeginTable("Welcome Left", 1, ImGuiTableFlags_NoBordersInBody, ImVec2(availableSpace.x / 2, 0))) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Image(*s_bannerTexture, s_bannerTexture->getSize());

                    if (ImHexApi::System::isNightlyBuild()) {
                        auto cursor = ImGui::GetCursorPos();

                        ImGui::SameLine(0);
                        ImGui::SetCursorPosX(ImGui::GetCursorPosX() - 15_scaled);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5_scaled);

                        ImGui::Image(*s_nightlyTexture, s_nightlyTexture->getSize());
                        ImGuiExt::InfoTooltip(fmt::format("{0}\n\n", "hex.builtin.welcome.nightly_build"_lang).c_str());

                        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
                        fonts::Default().push(0.75F);
                        ImGuiExt::InfoTooltip(fmt::format("{0}@{1}", ImHexApi::System::getCommitBranch(), ImHexApi::System::getCommitHash(true)).c_str());
                        fonts::Default().pop();
                        ImGui::PopStyleColor();

                        ImGui::SetCursorPos(cursor);
                    }

                    ImGui::NewLine();

                    ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetTextLineHeightWithSpacing() * 6);
                    ImGui::TableNextColumn();

                    static bool otherProvidersVisible = false;
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5_scaled);

                    {
                        auto startPos = ImGui::GetCursorPos();
                        if (ImGuiExt::BeginSubWindow("hex.builtin.welcome.header.start"_lang, nullptr, ImVec2(), ImGuiChildFlags_AutoResizeX)) {
                            if (ImGuiExt::IconHyperlink(ICON_VS_NEW_FILE, "hex.builtin.welcome.start.create_file"_lang)) {
                                auto newProvider = hex::ImHexApi::Provider::createProvider("hex.builtin.provider.mem_file", true);
                                if (newProvider != nullptr && newProvider->open().isFailure())
                                    hex::ImHexApi::Provider::remove(newProvider.get());
                                else
                                    EventProviderOpened::post(newProvider.get());
                            }
                            if (ImGuiExt::IconHyperlink(ICON_VS_GO_TO_FILE, "hex.builtin.welcome.start.open_file"_lang))
                                RequestOpenWindow::post("Open File");
                            if (ImGuiExt::IconHyperlink(ICON_VS_NOTEBOOK, "hex.builtin.welcome.start.open_project"_lang))
                                RequestOpenWindow::post("Open Project");
                            if (ImGuiExt::IconHyperlink(ICON_VS_TELESCOPE, "hex.builtin.welcome.start.open_other"_lang))
                                otherProvidersVisible = !otherProvidersVisible;
                        }
                        ImGuiExt::EndSubWindow();

                        auto endPos = ImGui::GetCursorPos();

                        if (otherProvidersVisible) {
                            ImGui::SameLine(0, 2_scaled);
                            ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(0, (endPos - startPos).y / 2));
                            ImGui::TextUnformatted(ICON_VS_ARROW_RIGHT);
                            ImGui::SameLine(0, 2_scaled);

                            if (ImGuiExt::BeginSubWindow("hex.builtin.welcome.start.open_other"_lang, nullptr, ImVec2(200_scaled, ImGui::GetTextLineHeightWithSpacing() * 5.8), ImGuiChildFlags_AutoResizeX)) {
                                for (const auto &[unlocalizedProviderName, icon] : ContentRegistry::Provider::impl::getEntries()) {
                                    if (ImGuiExt::IconHyperlink(icon, Lang(unlocalizedProviderName))) {
                                        ImHexApi::Provider::createProvider(unlocalizedProviderName);
                                        otherProvidersVisible = false;
                                    }
                                }
                            }
                            ImGuiExt::EndSubWindow();
                        }
                    }

                    // Draw recent entries
                    ImGui::Dummy({});

                    #if !defined(OS_WEB)
                        recent::draw();
                    #endif

                    ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetTextLineHeightWithSpacing() * 6);
                    ImGui::TableNextColumn();

                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5_scaled);
                    if (ImGuiExt::BeginSubWindow("hex.builtin.welcome.header.help"_lang, nullptr, ImVec2(), ImGuiChildFlags_AutoResizeX)) {
                        if (ImGuiExt::IconHyperlink(ICON_VS_GITHUB, "hex.builtin.welcome.help.repo"_lang)) hex::openWebpage("hex.builtin.welcome.help.repo.link"_lang);
                        if (ImGuiExt::IconHyperlink(ICON_VS_ORGANIZATION, "hex.builtin.welcome.help.gethelp"_lang)) hex::openWebpage("hex.builtin.welcome.help.gethelp.link"_lang);
                        if (ImGuiExt::IconHyperlink(ICON_VS_COMMENT_DISCUSSION, "hex.builtin.welcome.help.discord"_lang)) hex::openWebpage("hex.builtin.welcome.help.discord.link"_lang);
                    }
                    ImGuiExt::EndSubWindow();

                    ImGui::EndTable();
                }
                ImGui::SameLine();
                if (ImGui::BeginTable("Welcome Right", 1, ImGuiTableFlags_NoBordersInBody, ImVec2(availableSpace.x / 2, 0))) {
                    ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetTextLineHeightWithSpacing() * 5);
                    ImGui::TableNextColumn();

                    ImGui::NewLine();
                    ImGui::NewLine();
                    ImGui::NewLine();
                    ImGui::NewLine();
                    ImGui::NewLine();

                    auto windowPadding = ImGui::GetStyle().WindowPadding.x * 3;

                    if (ImGuiExt::BeginSubWindow("hex.builtin.welcome.header.customize"_lang, nullptr, ImVec2(ImGui::GetContentRegionAvail().x - windowPadding, 0), ImGuiChildFlags_AutoResizeX)) {
                        if (ImGuiExt::DescriptionButton("hex.builtin.welcome.customize.settings.title"_lang, "hex.builtin.welcome.customize.settings.desc"_lang, ICON_VS_SETTINGS_GEAR, ImVec2(ImGui::GetContentRegionAvail().x, 0)))
                            RequestOpenWindow::post("Settings");
                    }
                    ImGuiExt::EndSubWindow();

                    ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetTextLineHeightWithSpacing() * 5);
                    ImGui::TableNextColumn();

                    if (ImGuiExt::BeginSubWindow("hex.builtin.welcome.header.learn"_lang, nullptr, ImVec2(ImGui::GetContentRegionAvail().x - windowPadding, 0), ImGuiChildFlags_AutoResizeX)) {
                        const auto size = ImVec2(ImGui::GetContentRegionAvail().x, 0);
                        if (ImGuiExt::DescriptionButton("hex.builtin.welcome.learn.latest.title"_lang, "hex.builtin.welcome.learn.latest.desc"_lang, ICON_VS_GITHUB, size))
                            hex::openWebpage("hex.builtin.welcome.learn.latest.link"_lang);
                        if (ImGuiExt::DescriptionButton("hex.builtin.welcome.learn.imhex.title"_lang, "hex.builtin.welcome.learn.imhex.desc"_lang, ICON_VS_BOOK, size)) {
                            AchievementManager::unlockAchievement("hex.builtin.achievement.starting_out", "hex.builtin.achievement.starting_out.docs.name");
                            hex::openWebpage("hex.builtin.welcome.learn.imhex.link"_lang);
                        }
                        if (ImGuiExt::DescriptionButton("hex.builtin.welcome.learn.pattern.title"_lang, "hex.builtin.welcome.learn.pattern.desc"_lang, ICON_VS_SYMBOL_NAMESPACE, size))
                            hex::openWebpage("hex.builtin.welcome.learn.pattern.link"_lang);
                        if (ImGuiExt::DescriptionButton("hex.builtin.welcome.learn.plugins.title"_lang, "hex.builtin.welcome.learn.plugins.desc"_lang, ICON_VS_PLUG, size))
                            hex::openWebpage("hex.builtin.welcome.learn.plugins.link"_lang);

                        ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal, 3_scaled);

                        if (ImGuiExt::DescriptionButton("hex.builtin.welcome.learn.interactive_tutorial.title"_lang, "hex.builtin.welcome.learn.interactive_tutorial.desc"_lang, ICON_VS_COMPASS, size)) {
                            RequestOpenWindow::post("Tutorials");
                        }
                        if (auto [unlocked, total] = AchievementManager::getProgress(); unlocked != total) {
                            if (ImGuiExt::DescriptionButtonProgress("hex.builtin.welcome.learn.achievements.title"_lang, "hex.builtin.welcome.learn.achievements.desc"_lang, ICON_VS_SPARKLE, float(unlocked) / float(total), size)) {
                                RequestOpenWindow::post("Achievements");
                            }
                        }
                    }
                    ImGuiExt::EndSubWindow();

                    auto extraWelcomeScreenEntries = ContentRegistry::UserInterface::impl::getWelcomeScreenEntries();
                    if (!extraWelcomeScreenEntries.empty()) {
                        ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetTextLineHeightWithSpacing() * 5);
                        ImGui::TableNextColumn();

                        if (ImGuiExt::BeginSubWindow("hex.builtin.welcome.header.various"_lang, nullptr, ImVec2(ImGui::GetContentRegionAvail().x - windowPadding, 0))) {
                            for (const auto &callback : extraWelcomeScreenEntries)
                                callback();
                        }
                        ImGuiExt::EndSubWindow();
                    }

                    if (s_infoBannerTexture->isValid()) {
                        static bool hovered = false;

                        ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetStyleColorVec4(hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Border));
                        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

                        if (ImGuiExt::BeginSubWindow("hex.builtin.welcome.header.info"_lang, nullptr, ImVec2(), ImGuiChildFlags_AutoResizeX)) {
                            const auto height = 80_scaled;
                            ImGui::Image(*s_infoBannerTexture, ImVec2(height * s_infoBannerTexture->getAspectRatio(), height));
                            hovered = ImGui::IsItemHovered();

                            if (ImGui::IsItemClicked()) {
                                hex::openWebpage(ImHexApiURL + fmt::format("/info/{}/link", hex::toLower(ImHexApi::System::getOSName())));
                            }
                        }
                        ImGuiExt::EndSubWindow();

                        ImGui::PopStyleVar();
                        ImGui::PopStyleColor();

                    }


                    ImGui::EndTable();
                }

                ImGui::EndTable();
            }

            ImGui::SetCursorPos(ImVec2(ImGui::GetContentRegionAvail().x - ImGui::GetStyle().FramePadding.x * 2, ImGui::GetStyle().FramePadding.y * 2 - 1));
            if (ImGuiExt::DimmedIconButton(ICON_VS_CLOSE, ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_ToolbarRed))) {
                auto provider = ImHexApi::Provider::createProvider("hex.builtin.provider.null");
                if (provider != nullptr)
                    std::ignore = provider->open();
            }
        }

        void drawWelcomeScreen() {
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0);
            ImGui::PushStyleColor(ImGuiCol_WindowShadow, 0x00);
            if (ImGui::Begin("ImHexDockSpace", nullptr, ImGuiWindowFlags_NoBringToFrontOnFocus)) {
                if (!ImHexApi::Provider::isValid()) {
                    static auto title = []{
                        std::array<char, 256> result = {};
                        ImFormatString(result.data(), result.size(), "%s/DockSpace_%08X", ImGui::GetCurrentWindowRead()->Name, ImGui::GetID("ImHexMainDock"));
                        return result;
                    }();

                    if (ImGui::Begin(title.data(), nullptr, ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBringToFrontOnFocus)) {
                        ImGui::Dummy({});
                        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, scaled({ 10, 10 }));

                        ImGui::SetNextWindowScroll({ 0.0F, -1.0F });
                        ImGui::SetNextWindowSize(ImGui::GetContentRegionAvail() + scaled({ 0, 10 }));
                        ImGui::SetNextWindowPos(ImGui::GetCursorScreenPos() - ImVec2(0, ImGui::GetStyle().FramePadding.y + 2_scaled));
                        ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
                        if (ImGui::Begin("Welcome Screen", nullptr, ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
                            ImGui::BringWindowToDisplayBack(ImGui::GetCurrentWindowRead());

                            if (const auto &fullScreenView = ContentRegistry::Views::impl::getFullScreenView(); fullScreenView != nullptr) {
                                fullScreenView->draw();
                            } else {
                                drawWelcomeScreenBackground();

                                if (s_simplifiedWelcomeScreen)
                                    drawWelcomeScreenContentSimplified();
                                else
                                    drawWelcomeScreenContentFull();

                                static bool hovered = false;
                                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, hovered ? 1.0F : 0.3F);
                                {
                                    const auto &quickSettings = ContentRegistry::UserInterface::impl::getWelcomeScreenQuickSettingsToggles();
                                    if (!quickSettings.empty()) {
                                        const auto padding = ImGui::GetStyle().FramePadding.y;
                                        const ImVec2 windowSize = { 150_scaled, 2 * ImGui::GetTextLineHeightWithSpacing() + padding + std::ceil(quickSettings.size() / 5.0F) * (ImGui::GetTextLineHeightWithSpacing() + padding) };
                                        ImGui::SetCursorScreenPos(ImGui::GetWindowPos() + ImGui::GetWindowSize() - windowSize - ImGui::GetStyle().WindowPadding);
                                        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyleColorVec4(ImGuiCol_WindowBg));
                                        if (ImGuiExt::BeginSubWindow("hex.builtin.welcome.header.quick_settings"_lang, nullptr, windowSize, ImGuiChildFlags_AutoResizeY)) {
                                            u32 id = 0;
                                            for (auto &[onIcon, offIcon, unlocalizedTooltip, toggleCallback, state] : quickSettings) {
                                                ImGui::PushID(id + 1);
                                                if (ImGuiExt::DimmedIconToggle(onIcon.c_str(), offIcon.c_str(), &state)) {
                                                    ContentRegistry::Settings::write<bool>("hex.builtin.settings.quick_settings", unlocalizedTooltip, state);
                                                }
                                                if (id % 5 > 0)
                                                    ImGui::SameLine();
                                                ImGui::SetItemTooltip("%s", Lang(unlocalizedTooltip).get());
                                                ImGui::PopID();
                                                id += 1;
                                            }
                                        }
                                        ImGuiExt::EndSubWindow();

                                        ImGui::PopStyleColor();
                                        hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenOverlappedByItem | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
                                    }

                                }
                                ImGui::PopStyleVar();
                            }

                        }
                        ImGui::End();
                        ImGui::PopStyleVar();
                    }
                    ImGui::End();
                    ImGui::BringWindowToDisplayBack(ImGui::GetCurrentWindowRead());
                }
            }
            ImGui::End();
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
        }
        /**
         * @brief Draw some default background if there are no views available in the current layout
         */
        void drawNoViewsBackground() {
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0);
            ImGui::PushStyleColor(ImGuiCol_WindowShadow, 0x00);
            if (ImGui::Begin("ImHexDockSpace", nullptr, ImGuiWindowFlags_NoBringToFrontOnFocus)) {
                static std::array<char, 256> title;
                ImFormatString(title.data(), title.size(), "%s/DockSpace_%08X", ImGui::GetCurrentWindowRead()->Name, ImGui::GetID("ImHexMainDock"));
                if (ImGui::Begin(title.data())) {
                    ImGui::Dummy({});
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, scaled({ 10, 10 }));

                    ImGui::SetNextWindowScroll({ 0.0F, -1.0F });
                    ImGui::SetNextWindowSize(ImGui::GetContentRegionAvail() + scaled({ 0, 10 }));
                    ImGui::SetNextWindowPos(ImGui::GetCursorScreenPos() - ImVec2(0, ImGui::GetStyle().FramePadding.y + 2_scaled));
                    ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
                    if (ImGui::Begin("Welcome Screen", nullptr, ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
                        auto imageSize = scaled(ImVec2(350, 350));
                        auto imagePos = (ImGui::GetContentRegionAvail() - imageSize) / 2;

                        ImGui::SetCursorPos(imagePos);
                        ImGui::Image(*s_backdropTexture, imageSize);

                        auto loadDefaultText = "hex.builtin.layouts.none.restore_default"_lang;
                        auto textSize = ImGui::CalcTextSize(loadDefaultText);

                        auto textPos = ImVec2(
                            (ImGui::GetContentRegionAvail().x - textSize.x) / 2,
                            imagePos.y + imageSize.y
                        );

                        ImGui::SetCursorPos(textPos);
                        if (ImGuiExt::DimmedButton(loadDefaultText)){
                            loadDefaultLayout();
                        }
                    }

                    ImGui::End();
                    ImGui::PopStyleVar();
                }
                ImGui::End();
            }
            ImGui::End();
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
        }
    }

    /**
     * @brief Registers the event handlers related to the welcome screen
    * should only be called once, at startup
     */
    void createWelcomeScreen() {
        recent::registerEventHandlers();
        recent::updateRecentEntries();

        EventFrameBegin::subscribe(drawWelcomeScreen);

        // Sets a background when they are no views
        EventFrameBegin::subscribe([]{
            if (ImHexApi::Provider::isValid() && !isAnyViewOpen())
                drawNoViewsBackground();
        });

        ContentRegistry::Settings::onChange("hex.builtin.setting.interface", "hex.builtin.setting.interface.color", [](const ContentRegistry::Settings::SettingsValue &value) {
            auto theme = value.get<std::string>("Dark");
            if (theme != ThemeManager::NativeTheme) {
                static std::string lastTheme;

                if (theme != lastTheme) {
                    RequestChangeTheme::post(theme);
                    lastTheme = theme;
                }
            }
        });
        ContentRegistry::Settings::onChange("hex.builtin.setting.interface", "hex.builtin.setting.interface.simplified_welcome_screen", [](const ContentRegistry::Settings::SettingsValue &value) {
            s_simplifiedWelcomeScreen = value.get<bool>(false);
        });
        ContentRegistry::Settings::onChange("hex.builtin.setting.interface", "hex.builtin.setting.interface.language", [](const ContentRegistry::Settings::SettingsValue &value) {
            auto language = value.get<std::string>("en-US");
            if (language != LocalizationManager::getSelectedLanguageId())
                LocalizationManager::setLanguage(language);
        });
        ContentRegistry::Settings::onChange("hex.builtin.setting.interface", "hex.builtin.setting.interface.fps", [](const ContentRegistry::Settings::SettingsValue &value) {
            ImHexApi::System::setTargetFPS(static_cast<float>(value.get<int>(14)));
        });


        ContentRegistry::UserInterface::addWelcomeScreenQuickSettingsToggle(ICON_VS_COMPASS_ACTIVE, ICON_VS_COMPASS, "hex.builtin.welcome.quick_settings.simplified", false, [](bool state) {
            s_simplifiedWelcomeScreen = state;
            WorkspaceManager::switchWorkspace(s_simplifiedWelcomeScreen ? "Minimal" : "Default");
        });

        EventImHexStartupFinished::subscribe([]() {
            for (const auto &quickSetting : ContentRegistry::UserInterface::impl::getWelcomeScreenQuickSettingsToggles()) {
                auto &setting = quickSetting.unlocalizedTooltip;
                ContentRegistry::Settings::onChange("hex.builtin.settings.quick_settings", setting, [setting](const ContentRegistry::Settings::SettingsValue &value) {
                    for (auto &[onIcon, offIcon, unlocalizedTooltip, toggleCallback, state] : ContentRegistry::UserInterface::impl::getWelcomeScreenQuickSettingsToggles()) {
                        if (unlocalizedTooltip == setting) {
                            state = value.get<bool>(state);
                            toggleCallback(state);
                            break;
                        }
                    }
                });

                bool state = ContentRegistry::Settings::read<bool>("hex.builtin.settings.quick_settings", quickSetting.unlocalizedTooltip, quickSetting.state);
                quickSetting.state = state;
                quickSetting.callback(state);
            }
        });

        static auto updateTextures = [](float scale) {
            auto changeTexture = [&](const std::string &path) {
                return ImGuiExt::Texture::fromImage(romfs::get(path).span(), ImGuiExt::Texture::Filter::Nearest);
            };

            auto changeTextureSvg = [&](const std::string &path, float width) {
                return ImGuiExt::Texture::fromSVG(romfs::get(path).span(), width, 0, ImGuiExt::Texture::Filter::Linear);
            };

            s_bannerTexture = changeTextureSvg(fmt::format("assets/{}/banner.svg", ThemeManager::getImageTheme()), 300 * scale);
            s_nightlyTexture = changeTextureSvg(fmt::format("assets/{}/nightly.svg", "common"), 35 * scale);
            s_backdropTexture = changeTexture(fmt::format("assets/{}/backdrop.png", ThemeManager::getImageTheme()));
        };

        RequestChangeTheme::subscribe([]() { updateTextures(ImHexApi::System::getGlobalScale()); });
        EventDPIChanged::subscribe([](float oldScale, float newScale) {
            if (oldScale == newScale)
                return;

            updateTextures(newScale);
        });

        // Clear project context if we go back to the welcome screen
        EventProviderChanged::subscribe([](const hex::prv::Provider *oldProvider, const hex::prv::Provider *newProvider) {
            std::ignore = oldProvider;
            if (newProvider == nullptr) {
                ProjectFile::clearPath();
                RequestUpdateWindowTitle::post();
            }
        });


        recent::addMenuItems();

        // Check for crash backup
        constexpr static auto CrashFileName = "crash.json";
        constexpr static auto BackupFileName = "crash_backup.hexproj";
        bool hasCrashed = false;

        for (const auto &path : paths::Config.read()) {
            if (auto crashFilePath = std::fs::path(path) / CrashFileName; wolv::io::fs::exists(crashFilePath)) {
                hasCrashed = true;
                
                log::info("Found crash.json file at {}", wolv::util::toUTF8String(crashFilePath));
                wolv::io::File crashFile(crashFilePath, wolv::io::File::Mode::Read);
                nlohmann::json crashFileData;
                try {
                    crashFileData = nlohmann::json::parse(crashFile.readString());
                } catch (nlohmann::json::exception &e) {
                    log::error("Failed to parse crash.json file: {}", e.what());
                    crashFile.remove();
                    continue;
                }

                bool hasProject = !crashFileData.value("project", "").empty();

                auto backupFilePath = path / BackupFileName;
                auto backupFilePathOld = path / BackupFileName;
                backupFilePathOld.replace_extension(".hexproj.old");

                bool autoBackupsEnabled = ContentRegistry::Settings::read<int>("hex.builtin.setting.general", "hex.builtin.setting.general.backups.auto_backup_time", 0) > 0;
                auto autoBackups = recent::PopupAutoBackups::getAutoBackups();
                bool hasAutoBackups = autoBackupsEnabled && !autoBackups.empty();

                bool hasBackupFile = wolv::io::fs::exists(backupFilePath);

                if (!hasProject && !hasBackupFile) {
                    log::warn("No project file or backup file found in crash.json file");

                    crashFile.close();

                    // Delete crash.json file
                    wolv::io::fs::remove(crashFilePath);

                    // Delete old backup file
                    wolv::io::fs::remove(backupFilePathOld);

                    // Try to move current backup file to the old backup location
                    if (wolv::io::fs::copyFile(backupFilePath, backupFilePathOld)) {
                        wolv::io::fs::remove(backupFilePath);
                    }
                    continue;
                }

                PopupRestoreBackup::open(
                    // Path of log file
                    crashFileData.value("logFile", ""),
                    hasAutoBackups,

                    // Restore callback
                    [crashFileData, backupFilePath, hasProject, hasBackupFile, hasAutoBackups, autoBackups = std::move(autoBackups)] {
                        if (hasBackupFile) {
                            if (ProjectFile::load(backupFilePath)) {
                                if (hasProject) {
                                    ProjectFile::setPath(crashFileData["project"].get<std::string>());
                                } else if (hasAutoBackups) {
                                    ProjectFile::setPath(autoBackups.front().path);
                                } else {
                                    ProjectFile::setPath("");
                                }
                                RequestUpdateWindowTitle::post();
                            } else {
                                ui::ToastError::open(fmt::format("hex.builtin.popup.error.project.load"_lang, wolv::util::toUTF8String(backupFilePath)));
                            }
                        } else {
                            if (hasProject) {
                                ProjectFile::setPath(crashFileData["project"].get<std::string>());
                            } else if (hasAutoBackups) {
                                ProjectFile::setPath(autoBackups.front().path);
                            }
                        }
                    },

                    // Delete callback (also executed after restore)
                    [crashFilePath, backupFilePath] {
                        wolv::io::fs::remove(crashFilePath);
                        wolv::io::fs::remove(backupFilePath);
                    }
                );
            }
        }

        // Tip of the day
        auto tipsData = romfs::get("tips.json");
        if (!hasCrashed && tipsData.valid()) {
            auto tipsCategories = nlohmann::json::parse(tipsData.string());

            auto now = std::chrono::system_clock::now();
            auto daysSinceEpoch = std::chrono::duration_cast<std::chrono::days>(now.time_since_epoch());
            std::mt19937 random(daysSinceEpoch.count());

            auto chosenCategory = tipsCategories[random()%tipsCategories.size()].at("tips");
            const auto& chosenTip = chosenCategory[random()%chosenCategory.size()];
            s_tipOfTheDay = chosenTip.get<std::string>();

            bool showTipOfTheDay = ContentRegistry::Settings::read<bool>("hex.builtin.setting.general", "hex.builtin.setting.general.show_tips", false);
            if (showTipOfTheDay)
                PopupTipOfTheDay::open();
        }

        if (hasCrashed) {
            TaskManager::doLater([]{
                AchievementManager::unlockAchievement("hex.builtin.achievement.starting_out", "hex.builtin.achievement.starting_out.crash.name");
            });
        } else {
            std::random_device rd;
            if (ImHexApi::System::isCorporateEnvironment()) {
                if (rd() % 25 == 0) {
                    ui::BannerButton::open(ICON_VS_HEART, "Using ImHex for professional work? Ask your boss to sponsor us and get private E-Mail support and more!", ImColor(0x68, 0xA7, 0x70), "Donate Now!", [] {
                        hex::openWebpage("https://imhex.werwolv.net/donate_work");
                    });
                }
            } else {
                if (rd() % 75 == 0) {
                    ui::BannerButton::open(ICON_VS_HEART, "ImHex needs your help to stay alive! Donate now to fund infrastructure and further development", ImColor(0x68, 0xA7, 0x70), "Donate Now!", [] {
                        hex::openWebpage("https://github.com/sponsors/WerWolv");
                    });
                }
            }
        }

        // Load info banner texture either locally or from the server
        TaskManager::doLater([] {
            for (const auto &defaultPath : paths::Resources.read()) {
                const auto infoBannerPath = defaultPath / "info_banner.png";
                if (wolv::io::fs::exists(infoBannerPath)) {
                    s_infoBannerTexture = ImGuiExt::Texture::fromImage(infoBannerPath, ImGuiExt::Texture::Filter::Linear);

                    if (s_infoBannerTexture->isValid())
                        break;
                }
            }

            auto allowNetworking = ContentRegistry::Settings::read<bool>("hex.builtin.setting.general", "hex.builtin.setting.general.network_interface", false)
                && ContentRegistry::Settings::read<int>("hex.builtin.setting.general", "hex.builtin.setting.general.server_contact", 0) != 0;
            if (!s_infoBannerTexture->isValid() && allowNetworking) {
                TaskManager::createBackgroundTask("hex.builtin.task.loading_banner", [](auto&) {
                    HttpRequest request("GET",
                        ImHexApiURL + fmt::format("/info/{}/image", hex::toLower(ImHexApi::System::getOSName())));

                    auto response = request.downloadFile().get();

                    if (response.isSuccess()) {
                        const auto &data = response.getData();
                        if (!data.empty()) {
                            TaskManager::doLater([data] {
                                s_infoBannerTexture = ImGuiExt::Texture::fromImage(data.data(), data.size(), ImGuiExt::Texture::Filter::Linear);
                            });
                        }
                    }
                });
            }
        });
    }

}
