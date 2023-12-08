#include <hex.hpp>
#include <hex/helpers/http_requests.hpp>
#include <hex/api/event_manager.hpp>
#include <hex/api/content_registry.hpp>
#include <hex/api/localization_manager.hpp>
#include <hex/api/plugin_manager.hpp>
#include <hex/api/theme_manager.hpp>
#include <hex/api/layout_manager.hpp>
#include <hex/api/achievement_manager.hpp>
#include <hex/api_urls.hpp>
#include <hex/ui/view.hpp>
#include <hex/helpers/fs.hpp>
#include <hex/helpers/logger.hpp>

#include <hex/api/project_file_manager.hpp>

#include <imgui.h>
#include <hex/ui/imgui_imhex_extensions.h>

#include <nlohmann/json.hpp>
#include <romfs/romfs.hpp>

#include <wolv/io/file.hpp>
#include <wolv/io/fs.hpp>

#include <fonts/codicons_font.h>

#include <content/popups/popup_telemetry_request.hpp>
#include <content/recent.hpp>

#include <string>
#include <random>

namespace hex::plugin::builtin {

    namespace {
        ImGuiExt::Texture s_bannerTexture, s_backdropTexture, s_infoBannerTexture;

        std::string s_tipOfTheDay;

        bool s_simplifiedWelcomeScreen = false;

        class PopupRestoreBackup : public Popup<PopupRestoreBackup> {
        private:
            std::fs::path m_logFilePath;
            std::function<void()> m_restoreCallback;
            std::function<void()> m_deleteCallback;
            bool m_reportError = true;
        public:
            PopupRestoreBackup(std::fs::path logFilePath, const std::function<void()> &restoreCallback, const std::function<void()> &deleteCallback)
                    : Popup("hex.builtin.popup.safety_backup.title"),
                    m_logFilePath(std::move(logFilePath)),
                    m_restoreCallback(restoreCallback),
                    m_deleteCallback(deleteCallback) {

                this->m_reportError = ContentRegistry::Settings::read("hex.builtin.setting.general", "hex.builtin.setting.general.upload_crash_logs", true);
            }

            void drawContent() override {
                ImGui::TextUnformatted("hex.builtin.popup.safety_backup.desc"_lang);
                if (!this->m_logFilePath.empty()) {
                    ImGui::NewLine();
                    ImGui::TextUnformatted("hex.builtin.popup.safety_backup.log_file"_lang);
                    ImGui::SameLine(0, 2_scaled);
                    if (ImGuiExt::Hyperlink(this->m_logFilePath.filename().string().c_str())) {
                        fs::openFolderWithSelectionExternal(this->m_logFilePath);
                    }

                    ImGui::Checkbox("hex.builtin.popup.safety_backup.report_error"_lang, &this->m_reportError);
                    ImGui::NewLine();
                }

                auto width = ImGui::GetWindowWidth();
                ImGui::SetCursorPosX(width / 9);
                if (ImGui::Button("hex.builtin.popup.safety_backup.restore"_lang, ImVec2(width / 3, 0))) {
                    this->m_restoreCallback();
                    this->m_deleteCallback();

                    if (this->m_reportError) {
                        wolv::io::File logFile(this->m_logFilePath, wolv::io::File::Mode::Read);
                        if (logFile.isValid()) {
                            // Read current log file data
                            auto data = logFile.readString();

                            // Anonymize the log file
                            {
                                for (u32 pathType = 0; pathType < u32(fs::ImHexPath::END); pathType++) {
                                    for (auto &folder : fs::getDefaultPaths(static_cast<fs::ImHexPath>(pathType))) {
                                        auto parent = wolv::util::toUTF8String(folder.parent_path());
                                        data = wolv::util::replaceStrings(data, parent, "<*****>");
                                    }
                                }
                            }

                            TaskManager::createBackgroundTask("Upload Crash report", [path = this->m_logFilePath, data](auto&){
                                HttpRequest request("POST", ImHexApiURL + std::string("/crash_upload"));
                                request.uploadFile(std::vector<u8>(data.begin(), data.end()), "file", path.filename()).wait();
                            });
                        }
                    }

                    ContentRegistry::Settings::write("hex.builtin.setting.general", "hex.builtin.setting.general.upload_crash_logs", this->m_reportError);

                    this->close();
                }
                ImGui::SameLine();
                ImGui::SetCursorPosX(width / 9 * 5);
                if (ImGui::Button("hex.builtin.popup.safety_backup.delete"_lang, ImVec2(width / 3, 0))) {
                    this->m_deleteCallback();

                    this->close();
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
                if (ImGui::Checkbox("hex.builtin.common.dont_show_again"_lang, &dontShowAgain)) {
                    ContentRegistry::Settings::write("hex.builtin.setting.general", "hex.builtin.setting.general.show_tips", !dontShowAgain);
                }

                ImGui::SameLine((ImGui::GetMainViewport()->Size / 3 - ImGui::CalcTextSize("hex.builtin.common.close"_lang) - ImGui::GetStyle().FramePadding).x);

                if (ImGui::Button("hex.builtin.common.close"_lang))
                    Popup::close();
            }
        };

        void loadDefaultLayout() {
            LayoutManager::loadString(std::string(romfs::get("layouts/default.hexlyt").string()));
        }

        bool isAnyViewOpen() {
            const auto &views = ContentRegistry::Views::impl::getEntries();
            return std::any_of(views.begin(), views.end(),
                [](const auto &entry) {
                    return entry.second->getWindowOpenState();
                });
        }

        void drawWelcomeScreenContentSimplified() {
            const ImVec2 backdropSize = scaled({ 350, 350 });
            ImGui::SetCursorPos((ImGui::GetContentRegionAvail() - backdropSize) / 2);
            ImGui::Image(s_backdropTexture, backdropSize);

            ImGuiExt::TextFormattedCentered("hex.builtin.welcome.drop_file"_lang);
        }

        void drawWelcomeScreenContentFull() {
            const auto availableSpace = ImGui::GetContentRegionAvail();

            ImGui::Image(s_bannerTexture, s_bannerTexture.getSize() / (2 * (1.0F / ImHexApi::System::getGlobalScale())));

            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyleColorVec4(ImGuiCol_PopupBg));
            ON_SCOPE_EXIT { ImGui::PopStyleColor(); };
            ImGui::Indent();
            if (ImGui::BeginTable("Welcome Left", 1, ImGuiTableFlags_NoBordersInBody, ImVec2(availableSpace.x / 2, 0))) {

                ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetTextLineHeightWithSpacing() * 3);
                ImGui::TableNextColumn();

                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + std::min<float>(450_scaled, availableSpace.x / 2 - 50_scaled));
                ImGuiExt::TextFormattedWrapped("A Hex Editor for Reverse Engineers, Programmers and people who value their retinas when working at 3 AM.");
                ImGui::PopTextWrapPos();

                ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetTextLineHeightWithSpacing() * 6);
                ImGui::TableNextColumn();

                static bool otherProvidersVisible = false;
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5_scaled);

                {
                    auto startPos = ImGui::GetCursorPos();
                    ImGuiExt::BeginSubWindow("hex.builtin.welcome.header.start"_lang, ImVec2(), ImGuiChildFlags_AutoResizeX);
                    {
                        if (ImGuiExt::IconHyperlink(ICON_VS_NEW_FILE, "hex.builtin.welcome.start.create_file"_lang)) {
                            auto newProvider = hex::ImHexApi::Provider::createProvider("hex.builtin.provider.mem_file", true);
                            if (newProvider != nullptr && !newProvider->open())
                                hex::ImHexApi::Provider::remove(newProvider);
                            else
                                EventProviderOpened::post(newProvider);
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

                        ImGuiExt::BeginSubWindow("hex.builtin.welcome.start.open_other"_lang, ImVec2(200_scaled, ImGui::GetTextLineHeightWithSpacing() * 6), ImGuiChildFlags_AutoResizeX);
                        for (const auto &unlocalizedProviderName : ContentRegistry::Provider::impl::getEntries()) {
                            if (ImGuiExt::Hyperlink(Lang(unlocalizedProviderName))) {
                                ImHexApi::Provider::createProvider(unlocalizedProviderName);
                                otherProvidersVisible = false;
                            }
                        }
                        ImGuiExt::EndSubWindow();
                    }
                }

                // Draw recent entries
                ImGui::Dummy({});
                recent::draw();

                if (ImHexApi::System::getInitArguments().contains("update-available")) {
                    ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetTextLineHeightWithSpacing() * 5);
                    ImGui::TableNextColumn();
                    ImGuiExt::UnderlinedText("hex.builtin.welcome.header.update"_lang);
                    {
                        if (ImGuiExt::DescriptionButton("hex.builtin.welcome.update.title"_lang, hex::format("hex.builtin.welcome.update.desc"_lang, ImHexApi::System::getInitArguments()["update-available"]).c_str(), ImVec2(ImGui::GetContentRegionAvail().x * 0.8F, 0)))
                            ImHexApi::System::updateImHex(ImHexApi::System::UpdateType::Stable);
                    }
                }

                ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetTextLineHeightWithSpacing() * 6);
                ImGui::TableNextColumn();

                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5_scaled);
                ImGuiExt::BeginSubWindow("hex.builtin.welcome.header.help"_lang, ImVec2(), ImGuiChildFlags_AutoResizeX);
                {
                    if (ImGuiExt::IconHyperlink(ICON_VS_GITHUB, "hex.builtin.welcome.help.repo"_lang)) hex::openWebpage("hex.builtin.welcome.help.repo.link"_lang);
                    if (ImGuiExt::IconHyperlink(ICON_VS_ORGANIZATION, "hex.builtin.welcome.help.gethelp"_lang)) hex::openWebpage("hex.builtin.welcome.help.gethelp.link"_lang);
                    if (ImGuiExt::IconHyperlink(ICON_VS_COMMENT_DISCUSSION, "hex.builtin.welcome.help.discord"_lang)) hex::openWebpage("hex.builtin.welcome.help.discord.link"_lang);
                }
                ImGuiExt::EndSubWindow();

                ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetTextLineHeightWithSpacing() * 5);
                ImGui::TableNextColumn();

                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
                ImGuiExt::BeginSubWindow("hex.builtin.welcome.header.plugins"_lang, ImVec2(ImGui::GetContentRegionAvail().x * 0.8F, 0), ImGuiChildFlags_AutoResizeX);
                {
                    const auto &plugins = PluginManager::getPlugins();

                    if (!plugins.empty()) {
                        if (ImGui::BeginTable("plugins", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit, ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 5))) {
                            ImGui::TableSetupScrollFreeze(0, 1);
                            ImGui::TableSetupColumn("hex.builtin.welcome.plugins.plugin"_lang);
                            ImGui::TableSetupColumn("hex.builtin.welcome.plugins.author"_lang);
                            ImGui::TableSetupColumn("hex.builtin.welcome.plugins.desc"_lang, ImGuiTableColumnFlags_WidthStretch, 0.5);
                            ImGui::TableSetupColumn("##loaded", ImGuiTableColumnFlags_WidthFixed, ImGui::GetTextLineHeight());

                            ImGui::TableHeadersRow();

                            ImGuiListClipper clipper;
                            clipper.Begin(plugins.size());

                            while (clipper.Step()) {
                                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                                    const auto &plugin = plugins[i];

                                    ImGui::TableNextRow();
                                    ImGui::TableNextColumn();
                                    ImGui::TextUnformatted(plugin.getPluginName().c_str());
                                    ImGui::TableNextColumn();
                                    ImGui::TextUnformatted(plugin.getPluginAuthor().c_str());
                                    ImGui::TableNextColumn();
                                    ImGui::TextUnformatted(plugin.getPluginDescription().c_str());
                                    ImGui::TableNextColumn();
                                    ImGui::TextUnformatted(plugin.isLoaded() ? ICON_VS_CHECK : ICON_VS_CLOSE);
                                }
                            }

                            clipper.End();

                            ImGui::EndTable();
                        }
                    }
                }
                ImGuiExt::EndSubWindow();
                ImGui::PopStyleVar();

                ImGui::EndTable();
            }
            ImGui::SameLine();
            if (ImGui::BeginTable("Welcome Right", 1, ImGuiTableFlags_NoBordersInBody, ImVec2(availableSpace.x / 2, 0))) {
                ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetTextLineHeightWithSpacing() * 5);
                ImGui::TableNextColumn();

                auto windowPadding = ImGui::GetStyle().WindowPadding.x * 3;

                ImGuiExt::BeginSubWindow("hex.builtin.welcome.header.customize"_lang, ImVec2(ImGui::GetContentRegionAvail().x - windowPadding, 0), ImGuiChildFlags_AutoResizeX);
                {
                    if (ImGuiExt::DescriptionButton("hex.builtin.welcome.customize.settings.title"_lang, "hex.builtin.welcome.customize.settings.desc"_lang, ImVec2(ImGui::GetContentRegionAvail().x, 0)))
                        RequestOpenWindow::post("Settings");
                }
                ImGuiExt::EndSubWindow();
                ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetTextLineHeightWithSpacing() * 5);
                ImGui::TableNextColumn();

                ImGuiExt::BeginSubWindow("hex.builtin.welcome.header.learn"_lang, ImVec2(ImGui::GetContentRegionAvail().x - windowPadding, 0), ImGuiChildFlags_AutoResizeX);
                {
                    const auto size = ImVec2(ImGui::GetContentRegionAvail().x, 0);
                    if (ImGuiExt::DescriptionButton("hex.builtin.welcome.learn.latest.title"_lang, "hex.builtin.welcome.learn.latest.desc"_lang, size))
                        hex::openWebpage("hex.builtin.welcome.learn.latest.link"_lang);
                    if (ImGuiExt::DescriptionButton("hex.builtin.welcome.learn.imhex.title"_lang, "hex.builtin.welcome.learn.imhex.desc"_lang, size)) {
                        AchievementManager::unlockAchievement("hex.builtin.achievement.starting_out", "hex.builtin.achievement.starting_out.docs.name");
                        hex::openWebpage("hex.builtin.welcome.learn.imhex.link"_lang);
                    }
                    if (ImGuiExt::DescriptionButton("hex.builtin.welcome.learn.pattern.title"_lang, "hex.builtin.welcome.learn.pattern.desc"_lang, size))
                        hex::openWebpage("hex.builtin.welcome.learn.pattern.link"_lang);
                    if (ImGuiExt::DescriptionButton("hex.builtin.welcome.learn.plugins.title"_lang, "hex.builtin.welcome.learn.plugins.desc"_lang, size))
                        hex::openWebpage("hex.builtin.welcome.learn.plugins.link"_lang);

                    if (auto [unlocked, total] = AchievementManager::getProgress(); unlocked != total) {
                        if (ImGuiExt::DescriptionButtonProgress("hex.builtin.welcome.learn.achievements.title"_lang, "hex.builtin.welcome.learn.achievements.desc"_lang, float(unlocked) / float(total), size)) {
                            RequestOpenWindow::post("Achievements");
                        }
                    }
                }
                ImGuiExt::EndSubWindow();

                auto extraWelcomeScreenEntries = ContentRegistry::Interface::impl::getWelcomeScreenEntries();
                if (!extraWelcomeScreenEntries.empty()) {
                    ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetTextLineHeightWithSpacing() * 5);
                    ImGui::TableNextColumn();

                    ImGuiExt::BeginSubWindow("hex.builtin.welcome.header.various"_lang, ImVec2(ImGui::GetContentRegionAvail().x - windowPadding, 0));
                    {
                        for (const auto &callback : extraWelcomeScreenEntries)
                            callback();
                    }
                    ImGuiExt::EndSubWindow();
                }

                if (s_infoBannerTexture.isValid()) {
                    auto width = ImGui::GetContentRegionAvail().x - windowPadding;

                    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
                    ImGuiExt::BeginSubWindow("hex.builtin.welcome.header.info"_lang, ImVec2(), ImGuiChildFlags_AutoResizeX);
                    {
                        ImGui::Image(s_infoBannerTexture, ImVec2(width, width / s_infoBannerTexture.getAspectRatio()));

                        if (ImGui::IsItemClicked()) {
                            hex::openWebpage(ImHexApiURL + hex::format("/info/{}/link", hex::toLower(ImHexApi::System::getOSName())));
                        }
                    }
                    ImGuiExt::EndSubWindow();
                    ImGui::PopStyleVar();
                }


                ImGui::EndTable();
            }

            ImGui::SetCursorPos(ImVec2(ImGui::GetContentRegionAvail().x + ImGui::GetStyle().FramePadding.x, ImGui::GetStyle().FramePadding.y * 2 - 1));
            if (ImGuiExt::DimmedIconButton(ICON_VS_CLOSE, ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_ToolbarRed))) {
                auto provider = ImHexApi::Provider::createProvider("hex.builtin.provider.null");
                if (provider != nullptr)
                    if (provider->open())
                        EventProviderOpened::post(provider);
            }
        }

        void drawWelcomeScreen() {
            ImGui::PushStyleColor(ImGuiCol_WindowShadow, 0x00);
            if (ImGui::Begin("ImHexDockSpace")) {
                if (!ImHexApi::Provider::isValid()) {
                    static std::array<char, 256> title;
                    ImFormatString(title.data(), title.size(), "%s/DockSpace_%08X", ImGui::GetCurrentWindow()->Name, ImGui::GetID("ImHexMainDock"));
                    if (ImGui::Begin(title.data(), nullptr, ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBringToFrontOnFocus)) {
                        ImGui::Dummy({});
                        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, scaled({ 10, 10 }));

                        ImGui::SetNextWindowScroll({ 0.0F, -1.0F });
                        ImGui::SetNextWindowSize(ImGui::GetContentRegionAvail() + scaled({ 0, 10 }));
                        ImGui::SetNextWindowPos(ImGui::GetCursorScreenPos() - ImVec2(0, ImGui::GetStyle().FramePadding.y + 2_scaled));
                        if (ImGui::Begin("Welcome Screen", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
                            if (s_simplifiedWelcomeScreen)
                                drawWelcomeScreenContentSimplified();
                            else
                                drawWelcomeScreenContentFull();

                            static bool hovered = false;
                            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, hovered ? 1.0F : 0.3F);
                            {
                                const ImVec2 windowSize = scaled({ 150, 60 });
                                ImGui::SetCursorPos(ImGui::GetWindowSize() - windowSize - ImGui::GetStyle().WindowPadding);
                                ImGuiExt::BeginSubWindow("hex.builtin.welcome.header.quick_settings"_lang, windowSize);
                                {
                                    if (ImGuiExt::ToggleSwitch("hex.builtin.welcome.quick_settings.simplified"_lang, &s_simplifiedWelcomeScreen))
                                        ContentRegistry::Settings::write("hex.builtin.setting.interface", "hex.builtin.setting.interface.simplified_welcome_screen", s_simplifiedWelcomeScreen);
                                }
                                ImGuiExt::EndSubWindow();
                                hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenOverlappedByItem | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);

                            }
                            ImGui::PopStyleVar();
                        }
                        ImGui::End();
                        ImGui::PopStyleVar();
                    }
                    ImGui::End();
                }
            }
            ImGui::End();
            ImGui::PopStyleColor();
        }
        /**
         * @brief Draw some default background if there are no views available in the current layout
         */
        void drawNoViewsBackground() {
            if (ImGui::Begin("ImHexDockSpace")) {
                static std::array<char, 256> title;
                ImFormatString(title.data(), title.size(), "%s/DockSpace_%08X", ImGui::GetCurrentWindow()->Name, ImGui::GetID("ImHexMainDock"));
                if (ImGui::Begin(title.data())) {
                    ImGui::Dummy({});
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, scaled({ 10, 10 }));

                    ImGui::SetNextWindowScroll({ 0.0F, -1.0F });
                    ImGui::SetNextWindowSize(ImGui::GetContentRegionAvail() + scaled({ 0, 10 }));
                    ImGui::SetNextWindowPos(ImGui::GetCursorScreenPos() - ImVec2(0, ImGui::GetStyle().FramePadding.y + 2_scaled));
                    if (ImGui::Begin("Welcome Screen", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
                        auto imageSize = scaled(ImVec2(350, 350));
                        auto imagePos = (ImGui::GetContentRegionAvail() - imageSize) / 2;

                        ImGui::SetCursorPos(imagePos);
                        ImGui::Image(s_backdropTexture, imageSize);

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
        }
    }

    /**
     * @brief Registers the event handlers related to the welcome screen
    * should only be called once, at startup
     */
    void createWelcomeScreen() {
        recent::registerEventHandlers();
        recent::updateRecentEntries();

        (void)EventFrameBegin::subscribe(drawWelcomeScreen);

        // Sets a background when they are no views
        (void)EventFrameBegin::subscribe([]{
            if (ImHexApi::Provider::isValid() && !isAnyViewOpen())
                drawNoViewsBackground();
        });

        (void)EventSettingsChanged::subscribe([] {
            {
                auto theme = ContentRegistry::Settings::read("hex.builtin.setting.interface", "hex.builtin.setting.interface.color", ThemeManager::NativeTheme).get<std::string>();

                if (theme != ThemeManager::NativeTheme) {
                    static std::string lastTheme;

                    if (theme != lastTheme) {
                        RequestChangeTheme::post(theme);
                        lastTheme = theme;
                    }
                }

                s_simplifiedWelcomeScreen = ContentRegistry::Settings::read("hex.builtin.setting.interface", "hex.builtin.setting.interface.simplified_welcome_screen", false);
            }

            {
                auto language = ContentRegistry::Settings::read("hex.builtin.setting.interface", "hex.builtin.setting.interface.language", "en-US").get<std::string>();

                if (language != LocalizationManager::getSelectedLanguage())
                    LocalizationManager::loadLanguage(language);
            }

            {
                auto targetFps = ContentRegistry::Settings::read("hex.builtin.setting.interface", "hex.builtin.setting.interface.fps", 14).get<int>();

                ImHexApi::System::setTargetFPS(static_cast<float>(targetFps));
            }
        });

        (void)RequestChangeTheme::subscribe([](const std::string &theme) {
            auto changeTexture = [&](const std::string &path) {
                return ImGuiExt::Texture(romfs::get(path).span());
            };

            ThemeManager::changeTheme(theme);
            s_bannerTexture = changeTexture(hex::format("assets/{}/banner.png", ThemeManager::getImageTheme()));
            s_backdropTexture = changeTexture(hex::format("assets/{}/backdrop.png", ThemeManager::getImageTheme()));

            if (!s_bannerTexture.isValid()) {
                log::error("Failed to load banner texture!");
            }
        });

        EventProviderCreated::subscribe([](auto) {
            if (!isAnyViewOpen())
                loadDefaultLayout();
        });

        EventWindowInitialized::subscribe([] {
            // Documentation of the value above the setting definition
            auto allowServerContact = ContentRegistry::Settings::read("hex.builtin.setting.general", "hex.builtin.setting.general.server_contact", 2);
            if (allowServerContact == 2) {
                ContentRegistry::Settings::write("hex.builtin.setting.general", "hex.builtin.setting.general.server_contact", 0);

                // Open the telemetry popup but only on desktop versions
                #if !defined(OS_WEB)
                    PopupTelemetryRequest::open();
                #endif
            }
        });

        // Clear project context if we go back to the welcome screen
        EventProviderChanged::subscribe([](const hex::prv::Provider *oldProvider, const hex::prv::Provider *newProvider) {
            hex::unused(oldProvider);
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

        for (const auto &path : fs::getDefaultPaths(fs::ImHexPath::Config)) {
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
                bool hasBackupFile = wolv::io::fs::exists(backupFilePath);

                PopupRestoreBackup::open(
                    // Path of log file
                    crashFileData.value("logFile", ""),

                    // Restore callback
                    [=] {
                        if (hasBackupFile) {
                            ProjectFile::load(backupFilePath);
                            if (hasProject) {
                                ProjectFile::setPath(crashFileData["project"].get<std::string>());
                            } else {
                                ProjectFile::setPath("");
                            }
                            RequestUpdateWindowTitle::post();
                        }else{
                            if (hasProject) {
                                ProjectFile::setPath(crashFileData["project"].get<std::string>());
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
            auto days_since_epoch = std::chrono::duration_cast<std::chrono::days>(now.time_since_epoch());
            std::mt19937 random(days_since_epoch.count());

            auto chosenCategory = tipsCategories[random()%tipsCategories.size()].at("tips");
            auto chosenTip = chosenCategory[random()%chosenCategory.size()];
            s_tipOfTheDay = chosenTip.get<std::string>();

            bool showTipOfTheDay = ContentRegistry::Settings::read("hex.builtin.setting.general", "hex.builtin.setting.general.show_tips", false);
            if (showTipOfTheDay)
                PopupTipOfTheDay::open();
        }

        if (hasCrashed) {
            TaskManager::doLater([]{
                AchievementManager::unlockAchievement("hex.builtin.achievement.starting_out", "hex.builtin.achievement.starting_out.crash.name");
            });
        }

        // Load info banner texture either locally or from the server
        TaskManager::doLater([] {
            for (const auto &defaultPath : fs::getDefaultPaths(fs::ImHexPath::Resources)) {
                const auto infoBannerPath = defaultPath / "info_banner.png";
                if (wolv::io::fs::exists(infoBannerPath)) {
                    s_infoBannerTexture = ImGuiExt::Texture(wolv::util::toUTF8String(infoBannerPath).c_str());

                    if (s_infoBannerTexture.isValid())
                        break;
                }
            }

            if (!s_infoBannerTexture.isValid()) {
                TaskManager::createBackgroundTask("Load banner", [](auto&) {
                    HttpRequest request("GET",
                        ImHexApiURL + hex::format("/info/{}/image", hex::toLower(ImHexApi::System::getOSName())));

                    auto response = request.downloadFile().get();

                    if (response.isSuccess()) {
                        const auto &data = response.getData();
                        TaskManager::doLater([data] {
                            s_infoBannerTexture = ImGuiExt::Texture(data.data(), data.size());
                        });
                    }
                });
            }
        });
    }

}
