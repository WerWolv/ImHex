#include <hex.hpp>
#include <hex/api/event.hpp>
#include <hex/api/content_registry.hpp>
#include <hex/api/localization.hpp>
#include <hex/api/plugin_manager.hpp>
#include <hex/api/theme_manager.hpp>
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

#include <string>
#include <list>
#include <unordered_set>
#include <random>

namespace hex::plugin::builtin {

    constexpr static auto MaxRecentProviders = 5;

    static ImGui::Texture s_bannerTexture, s_backdropTexture;

    static std::fs::path s_safetyBackupPath;

    static std::string s_tipOfTheDay;

    struct RecentProvider {
        std::string displayName;
        std::string type;
        std::fs::path filePath;

        nlohmann::json data;

        bool operator==(const RecentProvider &other) const {
            return HashFunction()(*this) == HashFunction()(other);
        }

        struct HashFunction {
            std::size_t operator()(const RecentProvider& provider) const {
                return
                    (std::hash<std::string>()(provider.displayName)) ^
                    (std::hash<std::string>()(provider.type) << 1);
            }
        };

    };

    class PopupRestoreBackup : public Popup<PopupRestoreBackup> {
    public:
        PopupRestoreBackup() : Popup("hex.builtin.popup.safety_backup.title") { }

        void drawContent() override {
            ImGui::TextUnformatted("hex.builtin.popup.safety_backup.desc"_lang);
            ImGui::NewLine();

            auto width = ImGui::GetWindowWidth();
            ImGui::SetCursorPosX(width / 9);
            if (ImGui::Button("hex.builtin.popup.safety_backup.restore"_lang, ImVec2(width / 3, 0))) {
                ProjectFile::load(s_safetyBackupPath);
                ProjectFile::clearPath();

                for (const auto &provider : ImHexApi::Provider::getProviders())
                    provider->markDirty();

                wolv::io::fs::remove(s_safetyBackupPath);

                Popup::close();
            }
            ImGui::SameLine();
            ImGui::SetCursorPosX(width / 9 * 5);
            if (ImGui::Button("hex.builtin.popup.safety_backup.delete"_lang, ImVec2(width / 3, 0))) {
                wolv::io::fs::remove(s_safetyBackupPath);

                Popup::close();
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

    static std::atomic<bool> s_recentProvidersUpdating = false;
    static std::list<RecentProvider> s_recentProviders;

    static void updateRecentProviders() {
        TaskManager::createBackgroundTask("Updating recent files", [](auto&){
            if (s_recentProvidersUpdating)
                return;

            s_recentProvidersUpdating = true;
            ON_SCOPE_EXIT { s_recentProvidersUpdating = false; };

            s_recentProviders.clear();

            // Query all recent providers
            std::vector<std::fs::path> recentFilePaths;
            for (const auto &folder : fs::getDefaultPaths(fs::ImHexPath::Recent)) {
                for (const auto &entry : std::fs::directory_iterator(folder)) {
                    if (entry.is_regular_file())
                        recentFilePaths.push_back(entry.path());
                }
            }

            // Sort recent provider files by last modified time
            std::sort(recentFilePaths.begin(), recentFilePaths.end(), [](const auto &a, const auto &b) {
                return std::fs::last_write_time(a) > std::fs::last_write_time(b);
            });

            std::unordered_set<RecentProvider, RecentProvider::HashFunction> uniqueProviders;
            for (u32 i = 0; i < recentFilePaths.size() && uniqueProviders.size() < MaxRecentProviders; i++) {
                auto &path = recentFilePaths[i];
                try {
                    auto jsonData = nlohmann::json::parse(wolv::io::File(path, wolv::io::File::Mode::Read).readString());
                    uniqueProviders.insert(RecentProvider {
                        .displayName    = jsonData["displayName"],
                        .type           = jsonData["type"],
                        .filePath       = path,
                        .data           = jsonData
                    });
                } catch (...) { }
            }

            // Delete all recent provider files that are not in the list
            for (const auto &path : recentFilePaths) {
                bool found = false;
                for (const auto &provider : uniqueProviders) {
                    if (path == provider.filePath) {
                        found = true;
                        break;
                    }
                }

                if (!found)
                    wolv::io::fs::remove(path);
            }

            std::copy(uniqueProviders.begin(), uniqueProviders.end(), std::front_inserter(s_recentProviders));
        });
    }

    static void loadRecentProvider(const RecentProvider &recentProvider) {
        auto *provider = ImHexApi::Provider::createProvider(recentProvider.type, true);
        if (provider != nullptr) {
            provider->loadSettings(recentProvider.data);

            if (!provider->open() || !provider->isAvailable()) {
                PopupError::open("hex.builtin.popup.error.open"_lang);
                TaskManager::doLater([provider] { ImHexApi::Provider::remove(provider); });
                return;
            }

            EventManager::post<EventProviderOpened>(provider);

            updateRecentProviders();
        }
    }

    static void loadDefaultLayout() {
        auto layouts = ContentRegistry::Interface::impl::getLayouts();
        if (!layouts.empty()) {

            for (auto &[viewName, view] : ContentRegistry::Views::impl::getEntries()) {
                view->getWindowOpenState() = false;
            }

            auto dockId = ImHexApi::System::getMainDockSpaceId();

            ImGui::DockBuilderRemoveNode(dockId);
            ImGui::DockBuilderAddNode(dockId);
            layouts.front().callback(dockId);
            ImGui::DockBuilderFinish(dockId);
        }
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

            ImGui::TextFormattedWrapped("A Hex Editor for Reverse Engineers, Programmers and people who value their retinas when working at 3 AM.");

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

            ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetTextLineHeightWithSpacing() * 9);
            ImGui::TableNextColumn();
            ImGui::UnderlinedText(s_recentProviders.empty() ? "" : "hex.builtin.welcome.start.recent"_lang);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5_scaled);
            {
                if (!s_recentProvidersUpdating) {
                    for (const auto &recentProvider : s_recentProviders) {
                        ImGui::PushID(&recentProvider);
                        ON_SCOPE_EXIT { ImGui::PopID(); };

                        if (ImGui::BulletHyperlink(recentProvider.displayName.c_str())) {
                            loadRecentProvider(recentProvider);
                            break;
                        }
                    }
                }
            }

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

        ImGui::SetCursorPos(ImVec2(ImGui::GetContentRegionAvail().x, 0));
        if (ImGui::Hyperlink("X")) {
            auto provider = ImHexApi::Provider::createProvider("hex.builtin.provider.null");
            if (provider != nullptr)
                if (provider->open())
                    EventManager::post<EventProviderOpened>(provider);
        }
    }

    static void drawWelcomeScreen() {
        if (ImGui::Begin("ImHexDockSpace")) {
            if (!ImHexApi::Provider::isValid()) {
                static char title[256];
                ImFormatString(title, IM_ARRAYSIZE(title), "%s/DockSpace_%08X", ImGui::GetCurrentWindow()->Name, ImGui::GetID("ImHexMainDock"));
                if (ImGui::Begin(title)) {
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10_scaled, 10_scaled));
                    if (ImGui::BeginChild("Welcome Screen", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysUseWindowPadding | ImGuiWindowFlags_NoScrollWithMouse)) {
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

    static void drawNoViewsBackground() {
        if (ImGui::Begin("ImHexDockSpace")) {
            static char title[256];
            ImFormatString(title, IM_ARRAYSIZE(title), "%s/DockSpace_%08X", ImGui::GetCurrentWindow()->Name, ImGui::GetID("ImHexMainDock"));
            if (ImGui::Begin(title)) {
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10_scaled, 10_scaled));
                if (ImGui::BeginChild("NoViewsBackground", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysUseWindowPadding | ImGuiWindowFlags_NoScrollWithMouse)) {
                    auto imageSize = scaled(ImVec2(350, 350));
                    auto pos = (ImGui::GetContentRegionAvail() - imageSize) / 2;

                    ImGui::SetCursorPos(pos);
                    ImGui::Image(s_backdropTexture, imageSize);
                }
                ImGui::EndChild();
                ImGui::PopStyleVar();
            }
            ImGui::End();
        }
        ImGui::End();
    }

    void createWelcomeScreen() {
        updateRecentProviders();

        (void)EventManager::subscribe<EventFrameBegin>(drawWelcomeScreen);
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

        (void)EventManager::subscribe<EventProviderOpened>([](prv::Provider *provider) {
            if (ContentRegistry::Settings::read("hex.builtin.setting.general", "hex.builtin.setting.general.save_recent_providers", 1) == 1) {
                for (const auto &recentPath : fs::getDefaultPaths(fs::ImHexPath::Recent)) {
                    auto fileName = hex::format("{:%y%m%d_%H%M%S}.json", fmt::gmtime(std::chrono::system_clock::now()));
                    wolv::io::File recentFile(recentPath / fileName, wolv::io::File::Mode::Create);
                    if (!recentFile.isValid())
                        continue;

                    {
                        auto path = ProjectFile::getPath();
                        ProjectFile::clearPath();

                        if (auto settings = provider->storeSettings(); !settings.is_null())
                            recentFile.writeString(settings.dump(4));

                        ProjectFile::setPath(path);
                    }
                }
            }

            updateRecentProviders();
        });

        EventManager::subscribe<EventProviderCreated>([](auto) {
            if (!isAnyViewOpen())
                loadDefaultLayout();
        });

        EventManager::subscribe<EventWindowInitialized>([] {
            // documentation of the value above the setting definition
            auto showCheckForUpdates = ContentRegistry::Settings::read("hex.builtin.setting.general", "hex.builtin.setting.general.check_for_updates", 2);
            if (showCheckForUpdates == 2) {
                ContentRegistry::Settings::write("hex.builtin.setting.general", "hex.builtin.setting.general.check_for_updates", 0);
                PopupQuestion::open("hex.builtin.welcome.check_for_updates_text"_lang,
                    [] {
                        ContentRegistry::Settings::write("hex.builtin.setting.general", "hex.builtin.setting.general.check_for_updates", 1);
                    },
                    [] {

                    }
                );
            }
        });

        // Clear project context if we go back to the welcome screen
        EventManager::subscribe<EventProviderChanged>([](hex::prv::Provider *oldProvider, hex::prv::Provider *newProvider) {
            hex::unused(oldProvider);
            if (newProvider == nullptr) {
                ProjectFile::clearPath();
                EventManager::post<RequestUpdateWindowTitle>();
            }
        });

        ContentRegistry::Interface::addMenuItemSubMenu({ "hex.builtin.menu.file" }, 1200, [] {
            if (ImGui::BeginMenu("hex.builtin.menu.file.open_recent"_lang, !s_recentProvidersUpdating && !s_recentProviders.empty())) {
                // Copy to avoid changing list while iteration
                auto recentProviders = s_recentProviders;
                for (auto &recentProvider : recentProviders) {
                    if (ImGui::MenuItem(recentProvider.displayName.c_str())) {
                        loadRecentProvider(recentProvider);
                    }
                }

                ImGui::Separator();
                if (ImGui::MenuItem("hex.builtin.menu.file.clear_recent"_lang)) {
                    s_recentProviders.clear();

                    // Remove all recent files
                    for (const auto &recentPath : fs::getDefaultPaths(fs::ImHexPath::Recent))
                        for (const auto &entry : std::fs::directory_iterator(recentPath))
                            std::fs::remove(entry.path());
                }

                ImGui::EndMenu();
            }
        });

        // Check for crash backup
        constexpr static auto CrashBackupFileName = "crash_backup.hexproj";
        for (const auto &path : fs::getDefaultPaths(fs::ImHexPath::Config)) {
            if (auto filePath = std::fs::path(path) / CrashBackupFileName; wolv::io::fs::exists(filePath)) {
                s_safetyBackupPath = filePath;
                PopupRestoreBackup::open();
            }
        }

        // Tip of the day
        auto tipsData = romfs::get("tips.json");
        if(s_safetyBackupPath.empty() && tipsData.valid()){
            auto tipsCategories = nlohmann::json::parse(tipsData.string());

            auto now = std::chrono::system_clock::now();
            auto days_since_epoch = std::chrono::duration_cast<std::chrono::days>(now.time_since_epoch());
            std::mt19937 random(days_since_epoch.count());

            auto chosenCategory = tipsCategories[random()%tipsCategories.size()]["tips"];
            auto chosenTip = chosenCategory[random()%chosenCategory.size()];
            s_tipOfTheDay = chosenTip.get<std::string>();

            bool showTipOfTheDay = ContentRegistry::Settings::read("hex.builtin.setting.general", "hex.builtin.setting.general.show_tips", 1);
            if (showTipOfTheDay)
                PopupTipOfTheDay::open();
        }
    }

}
