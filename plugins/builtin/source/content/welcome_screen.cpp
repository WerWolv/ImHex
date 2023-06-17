#include <hex.hpp>
#include <hex/api/event.hpp>
#include <hex/api/content_registry.hpp>
#include <hex/api/localization.hpp>
#include <hex/api/plugin_manager.hpp>
#include <hex/api/theme_manager.hpp>
#include <hex/api/layout_manager.hpp>
#include <hex/ui/view.hpp>
#include <hex/helpers/fs.hpp>
#include <hex/helpers/logger.hpp>

#include <hex/api/project_file_manager.hpp>

#include <imgui.h>
#include <implot.h>
#include <hex/ui/imgui_imhex_extensions.h>

#include <nlohmann/json.hpp>
#include <romfs/romfs.hpp>

#include <wolv/io/file.hpp>
#include <wolv/io/fs.hpp>
#include <wolv/utils/guards.hpp>

#include <fonts/codicons_font.h>

#include <content/popups/popup_notification.hpp>
#include <content/popups/popup_question.hpp>
#include <content/recent.hpp>

#include <string>
#include <list>
#include <unordered_set>
#include <random>

namespace hex::plugin::builtin {

    static ImGui::Texture s_bannerTexture, s_backdropTexture;

    static std::fs::path s_safetyBackupPath;

    static std::string s_tipOfTheDay;

    class PopupRestoreBackup : public Popup<PopupRestoreBackup> {
    private:
        std::fs::path m_logFilePath;
        std::function<void()> m_restoreCallback;
        std::function<void()> m_deleteCallback;
    public:
        PopupRestoreBackup(std::fs::path logFilePath, std::function<void()> restoreCallback, std::function<void()> deleteCallback)
                : Popup("hex.builtin.popup.safety_backup.title"),
                m_logFilePath(logFilePath),
                m_restoreCallback(restoreCallback),
                m_deleteCallback(deleteCallback) { }

        void drawContent() override {
            ImGui::TextUnformatted("hex.builtin.popup.safety_backup.desc"_lang);
            if (!this->m_logFilePath.empty()) {
                ImGui::TextUnformatted("hex.builtin.popup.safety_backup.log_file"_lang);
                if (ImGui::Hyperlink(this->m_logFilePath.filename().string().c_str())) {
                    fs::openFolderWithSelectionExternal(this->m_logFilePath);
                }
                ImGui::NewLine();
            }
        
            auto width = ImGui::GetWindowWidth();
            ImGui::SetCursorPosX(width / 9);
            if (ImGui::Button("hex.builtin.popup.safety_backup.restore"_lang, ImVec2(width / 3, 0))) {
                this->m_restoreCallback();
                this->m_deleteCallback();

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
            ImGui::Header("hex.builtin.welcome.tip_of_the_day"_lang, true);

            ImGui::TextFormattedWrapped("{}", s_tipOfTheDay.c_str());
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

    static void loadDefaultLayout() {
        LayoutManager::loadString(std::string(romfs::get("layouts/default.hexlyt").string()));
    }

    static bool isAnyViewOpen() {
        const auto &views = ContentRegistry::Views::impl::getEntries();
        return std::any_of(views.begin(), views.end(),
            [](const auto &entry) {
                return entry.second->getWindowOpenState();
            });
    }

    static void drawWelcomeScreenContent() {
        const auto availableSpace = ImGui::GetContentRegionAvail();

        ImGui::Image(s_bannerTexture, s_bannerTexture.getSize() / (2 * (1.0F / ImHexApi::System::getGlobalScale())));

        ImGui::Indent();
        if (ImGui::BeginTable("Welcome Left", 1, ImGuiTableFlags_NoBordersInBody, ImVec2(availableSpace.x / 2, 0))) {

            ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetTextLineHeightWithSpacing() * 3);
            ImGui::TableNextColumn();

            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + std::min<double>(450_scaled, availableSpace.x / 2 - 50_scaled));
            ImGui::TextFormattedWrapped("A Hex Editor for Reverse Engineers, Programmers and people who value their retinas when working at 3 AM.");
            ImGui::PopTextWrapPos();

            ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetTextLineHeightWithSpacing() * 6);
            ImGui::TableNextColumn();


            ImGui::UnderlinedText("hex.builtin.welcome.header.start"_lang);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5_scaled);
            {
                if (ImGui::IconHyperlink(ICON_VS_NEW_FILE, "hex.builtin.welcome.start.create_file"_lang)) {
                    auto newProvider = hex::ImHexApi::Provider::createProvider("hex.builtin.provider.mem_file", true);
                    if (newProvider != nullptr && !newProvider->open())
                        hex::ImHexApi::Provider::remove(newProvider);
                    else
                        EventManager::post<EventProviderOpened>(newProvider);
                }
                if (ImGui::IconHyperlink(ICON_VS_GO_TO_FILE, "hex.builtin.welcome.start.open_file"_lang))
                    EventManager::post<RequestOpenWindow>("Open File");
                if (ImGui::IconHyperlink(ICON_VS_NOTEBOOK, "hex.builtin.welcome.start.open_project"_lang))
                    EventManager::post<RequestOpenWindow>("Open Project");
                if (ImGui::IconHyperlink(ICON_VS_TELESCOPE, "hex.builtin.welcome.start.open_other"_lang))
                    ImGui::OpenPopup("hex.builtin.welcome.start.popup.open_other"_lang);
            }

            ImGui::SetNextWindowPos(ImGui::GetWindowPos() + ImGui::GetCursorPos());
            if (ImGui::BeginPopup("hex.builtin.welcome.start.popup.open_other"_lang)) {

                for (const auto &unlocalizedProviderName : ContentRegistry::Provider::impl::getEntries()) {
                    if (ImGui::Hyperlink(LangEntry(unlocalizedProviderName))) {
                        ImHexApi::Provider::createProvider(unlocalizedProviderName);
                        ImGui::CloseCurrentPopup();
                    }
                }

                ImGui::EndPopup();
            }

            // draw recent entries
            recent::draw();

            if (ImHexApi::System::getInitArguments().contains("update-available")) {
                ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetTextLineHeightWithSpacing() * 5);
                ImGui::TableNextColumn();
                ImGui::UnderlinedText("hex.builtin.welcome.header.update"_lang);
                {
                    if (ImGui::DescriptionButton("hex.builtin.welcome.update.title"_lang, hex::format("hex.builtin.welcome.update.desc"_lang, ImHexApi::System::getInitArguments()["update-available"]).c_str(), ImVec2(ImGui::GetContentRegionAvail().x * 0.8F, 0)))
                        hex::openWebpage("hex.builtin.welcome.update.link"_lang);
                }
            }

            ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetTextLineHeightWithSpacing() * 6);
            ImGui::TableNextColumn();
            ImGui::UnderlinedText("hex.builtin.welcome.header.help"_lang);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5_scaled);
            {
                if (ImGui::IconHyperlink(ICON_VS_GITHUB, "hex.builtin.welcome.help.repo"_lang)) hex::openWebpage("hex.builtin.welcome.help.repo.link"_lang);
                if (ImGui::IconHyperlink(ICON_VS_ORGANIZATION, "hex.builtin.welcome.help.gethelp"_lang)) hex::openWebpage("hex.builtin.welcome.help.gethelp.link"_lang);
                if (ImGui::IconHyperlink(ICON_VS_COMMENT_DISCUSSION, "hex.builtin.welcome.help.discord"_lang)) hex::openWebpage("hex.builtin.welcome.help.discord.link"_lang);
            }

            ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetTextLineHeightWithSpacing() * 5);
            ImGui::TableNextColumn();
            ImGui::UnderlinedText("hex.builtin.welcome.header.plugins"_lang);
            {
                const auto &plugins = PluginManager::getPlugins();

                if (!plugins.empty()) {
                    if (ImGui::BeginTable("plugins", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit, ImVec2((ImGui::GetContentRegionAvail().x * 5) / 6, ImGui::GetTextLineHeightWithSpacing() * 5))) {
                        ImGui::TableSetupScrollFreeze(0, 1);
                        ImGui::TableSetupColumn("hex.builtin.welcome.plugins.plugin"_lang, ImGuiTableColumnFlags_WidthStretch, 0.2);
                        ImGui::TableSetupColumn("hex.builtin.welcome.plugins.author"_lang, ImGuiTableColumnFlags_WidthStretch, 0.2);
                        ImGui::TableSetupColumn("hex.builtin.welcome.plugins.desc"_lang, ImGuiTableColumnFlags_WidthStretch, 0.6);
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

            ImGui::EndTable();
        }
        ImGui::SameLine();
        if (ImGui::BeginTable("Welcome Right", 1, ImGuiTableFlags_NoBordersInBody, ImVec2(availableSpace.x / 2, 0))) {
            ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetTextLineHeightWithSpacing() * 5);
            ImGui::TableNextColumn();
            ImGui::UnderlinedText("hex.builtin.welcome.header.customize"_lang);
            {
                if (ImGui::DescriptionButton("hex.builtin.welcome.customize.settings.title"_lang, "hex.builtin.welcome.customize.settings.desc"_lang, ImVec2(ImGui::GetContentRegionAvail().x * 0.8F, 0)))
                    EventManager::post<RequestOpenWindow>("Settings");
            }
            ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetTextLineHeightWithSpacing() * 5);
            ImGui::TableNextColumn();
            ImGui::UnderlinedText("hex.builtin.welcome.header.learn"_lang);
            {
                if (ImGui::DescriptionButton("hex.builtin.welcome.learn.latest.title"_lang, "hex.builtin.welcome.learn.latest.desc"_lang, ImVec2(ImGui::GetContentRegionAvail().x * 0.8F, 0)))
                    hex::openWebpage("hex.builtin.welcome.learn.latest.link"_lang);
                if (ImGui::DescriptionButton("hex.builtin.welcome.learn.pattern.title"_lang, "hex.builtin.welcome.learn.pattern.desc"_lang, ImVec2(ImGui::GetContentRegionAvail().x * 0.8F, 0)))
                    hex::openWebpage("hex.builtin.welcome.learn.pattern.link"_lang);
                if (ImGui::DescriptionButton("hex.builtin.welcome.learn.plugins.title"_lang, "hex.builtin.welcome.learn.plugins.desc"_lang, ImVec2(ImGui::GetContentRegionAvail().x * 0.8F, 0)))
                    hex::openWebpage("hex.builtin.welcome.learn.plugins.link"_lang);
            }

            auto extraWelcomeScreenEntries = ContentRegistry::Interface::impl::getWelcomeScreenEntries();
            if (!extraWelcomeScreenEntries.empty()) {
                ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetTextLineHeightWithSpacing() * 5);
                ImGui::TableNextColumn();
                ImGui::UnderlinedText("hex.builtin.welcome.header.various"_lang);
                {
                    for (const auto &callback : extraWelcomeScreenEntries)
                        callback();
                }
            }


            ImGui::EndTable();
        }

        ImGui::SetCursorPos(ImVec2(ImGui::GetContentRegionAvail().x + ImGui::GetStyle().FramePadding.x, ImGui::GetStyle().FramePadding.y * 2 - 1));
        if (ImGui::DimmedIconButton(ICON_VS_CLOSE, ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarRed))) {
            auto provider = ImHexApi::Provider::createProvider("hex.builtin.provider.null");
            if (provider != nullptr)
                if (provider->open())
                    EventManager::post<EventProviderOpened>(provider);
        }
    }

    static void drawWelcomeScreen() {
        if (ImGui::Begin("ImHexDockSpace")) {
            if (!ImHexApi::Provider::isValid()) {
                static std::array<char, 256> title;
                ImFormatString(title.data(), title.size(), "%s/DockSpace_%08X", ImGui::GetCurrentWindow()->Name, ImGui::GetID("ImHexMainDock"));
                if (ImGui::Begin(title.data())) {
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10_scaled, 10_scaled));

                    ImGui::SetNextWindowScroll(ImVec2(0.0F, -1.0F));
                    if (ImGui::BeginChild("Welcome Screen", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysUseWindowPadding)) {
                        drawWelcomeScreenContent();
                    }
                    ImGui::EndChild();
                    ImGui::PopStyleVar();
                }
                ImGui::End();
            }
        }
        ImGui::End();
    }
    /**
     * @brief Draw some default background if there are no views avaialble in the current layout
     */
    static void drawNoViewsBackground() {
        if (ImGui::Begin("ImHexDockSpace")) {
            static char title[256];
            ImFormatString(title, IM_ARRAYSIZE(title), "%s/DockSpace_%08X", ImGui::GetCurrentWindow()->Name, ImGui::GetID("ImHexMainDock"));
            if (ImGui::Begin(title)) {
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10_scaled, 10_scaled));
                if (ImGui::BeginChild("NoViewsBackground", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysUseWindowPadding | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
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
                    if (ImGui::DimmedButton(loadDefaultText)){
                        loadDefaultLayout();
                    }
                }
                ImGui::EndChild();
                ImGui::PopStyleVar();
            }
            ImGui::End();
        }
        ImGui::End();
    }

    /**
     * @brief Registers the event handlers related to the welcome screen
    * should only be called once, at startup
     */
    void createWelcomeScreen() {
        recent::registerEventHandlers();
        recent::updateRecentEntries();

        (void)EventManager::subscribe<EventFrameBegin>(drawWelcomeScreen);

        // Sets a background when they are no views
        (void)EventManager::subscribe<EventFrameBegin>([]{
            if (ImHexApi::Provider::isValid() && !isAnyViewOpen())
                drawNoViewsBackground();
        });

        (void)EventManager::subscribe<EventSettingsChanged>([]() {
            {
                auto theme = ContentRegistry::Settings::read("hex.builtin.setting.interface", "hex.builtin.setting.interface.color", ThemeManager::NativeTheme);

                if (theme != ThemeManager::NativeTheme) {
                    static std::string lastTheme;

                    if (theme != lastTheme) {
                        EventManager::post<RequestChangeTheme>(theme);
                        lastTheme = theme;
                    }
                }
            }

            {
                auto language = ContentRegistry::Settings::read("hex.builtin.setting.interface", "hex.builtin.setting.interface.language", "en-US");

                LangEntry::loadLanguage(language);
            }

            {
                auto targetFps = ContentRegistry::Settings::read("hex.builtin.setting.interface", "hex.builtin.setting.interface.fps", 14);

                ImHexApi::System::setTargetFPS(targetFps);
            }
        });

        (void)EventManager::subscribe<RequestChangeTheme>([](const std::string &theme) {
            auto changeTexture = [&](const std::string &path) {
                auto textureData = romfs::get(path);

                return ImGui::Texture(reinterpret_cast<const ImU8*>(textureData.data()), textureData.size());
            };

            ThemeManager::changeTheme(theme);
            s_bannerTexture = changeTexture(hex::format("banner{}.png", ThemeManager::getThemeImagePostfix()));
            s_backdropTexture = changeTexture(hex::format("backdrop{}.png", ThemeManager::getThemeImagePostfix()));

            if (!s_bannerTexture.isValid()) {
                log::error("Failed to load banner texture!");
            }
        });

        EventManager::subscribe<EventProviderCreated>([](auto) {
            if (!isAnyViewOpen())
                loadDefaultLayout();
        });

#if defined(HEX_UPDATE_CHECK)
        EventManager::subscribe<EventWindowInitialized>([] {
            // documentation of the value above the setting definition
            auto allowServerContact = ContentRegistry::Settings::read("hex.builtin.setting.general", "hex.builtin.setting.general.server_contact", 2);
            if (allowServerContact == 2) {
                ContentRegistry::Settings::write("hex.builtin.setting.general", "hex.builtin.setting.general.server_contact", 0);
                PopupQuestion::open("hex.builtin.welcome.server_contact_text"_lang,
                    [] {
                        ContentRegistry::Settings::write("hex.builtin.setting.general", "hex.builtin.setting.general.server_contact", 1);
                    },
                    [] {

                    }
                );
            }
        });
#endif

        // Clear project context if we go back to the welcome screen
        EventManager::subscribe<EventProviderChanged>([](hex::prv::Provider *oldProvider, hex::prv::Provider *newProvider) {
            hex::unused(oldProvider);
            if (newProvider == nullptr) {
                ProjectFile::clearPath();
                EventManager::post<RequestUpdateWindowTitle>();
            }
        });


        recent::drawFileMenuItem();

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

                // increase the counter
                ContentRegistry::Settings::write("hex.builtin.setting.telemetry", "hex.builtin.setting.telemetry.crash_count",
                                                 ContentRegistry::Settings::read("hex.builtin.setting.telemetry", "hex.builtin.setting.telemetry.crash_count", 0) + 1);
                
                PopupRestoreBackup::open(
                    // path of log file
                    crashFileData.value("logFile", ""),
                    // restore callback
                    [=]{
                        if (hasBackupFile) {
                            ProjectFile::load(backupFilePath);
                            if (hasProject) {
                                ProjectFile::setPath(crashFileData["project"].get<std::string>());
                            } else {
                                ProjectFile::setPath("");
                            }
                            EventManager::post<RequestUpdateWindowTitle>();
                        }else{
                            if (hasProject) {
                                ProjectFile::setPath(crashFileData["project"].get<std::string>());
                            }
                        }
                    },
                    // delete callback (also executed after restore)
                    [crashFilePath, backupFilePath]{
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

            bool showTipOfTheDay = ContentRegistry::Settings::read("hex.builtin.setting.general", "hex.builtin.setting.general.show_tips", 1);
            if (showTipOfTheDay)
                PopupTipOfTheDay::open();
        }
    }

}
