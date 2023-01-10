#include <hex.hpp>
#include <hex/api/event.hpp>
#include <hex/api/content_registry.hpp>
#include <hex/api/localization.hpp>
#include <hex/api/plugin_manager.hpp>
#include <hex/api/theme_manager.hpp>
#include <hex/ui/view.hpp>
#include <hex/helpers/fs.hpp>
#include <hex/helpers/logger.hpp>
#include <hex/helpers/file.hpp>

#include <hex/api/project_file_manager.hpp>

#include <imgui.h>
#include <implot.h>
#include <hex/ui/imgui_imhex_extensions.h>

#include <nlohmann/json.hpp>
#include <romfs/romfs.hpp>

#include <fonts/codicons_font.h>

#include <string>
#include <list>
#include <unordered_set>
#include <random>

namespace hex::plugin::builtin {

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
            for (u32 i = 0; i < recentFilePaths.size() && uniqueProviders.size() < 5; i++) {
                auto &path = recentFilePaths[i];
                try {
                    auto jsonData = nlohmann::json::parse(fs::File(path, fs::File::Mode::Read).readString());
                    uniqueProviders.insert(RecentProvider {
                        .displayName    = jsonData["displayName"],
                        .type           = jsonData["type"],
                        .filePath       = path,
                        .data           = jsonData
                    });
                } catch (...) { }
            }

            std::copy(uniqueProviders.begin(), uniqueProviders.end(), std::front_inserter(s_recentProviders));
        });
    }

    static void loadRecentProvider(const RecentProvider &recentProvider) {
        auto *provider = ImHexApi::Provider::createProvider(recentProvider.type, true);
        if (provider != nullptr) {
            provider->loadSettings(recentProvider.data);

            if (!provider->open() || !provider->isAvailable()) {
                View::showErrorPopup("hex.builtin.popup.error.open"_lang);
                TaskManager::doLater([provider] { ImHexApi::Provider::remove(provider); });
                return;
            }

            EventManager::post<EventProviderOpened>(provider);

            updateRecentProviders();
        }
    }

    static void loadDefaultLayout() {
        auto layouts = ContentRegistry::Interface::getLayouts();
        if (!layouts.empty()) {

            for (auto &[viewName, view] : ContentRegistry::Views::getEntries()) {
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
        const auto &views = ContentRegistry::Views::getEntries();
        return std::any_of(views.begin(), views.end(),
            [](const std::pair<std::string, View*> &entry) {
                return entry.second->getWindowOpenState();
            });
    }

    static void drawPopups() {
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5F, 0.5F));
        ImGui::SetNextWindowSize(ImGui::GetMainViewport()->Size / 3, ImGuiCond_Appearing);
        if (ImGui::BeginPopup("hex.builtin.welcome.tip_of_the_day"_lang)) {
            ImGui::Header("hex.builtin.welcome.tip_of_the_day"_lang, true);

            ImGui::TextFormattedWrapped("{}", s_tipOfTheDay.c_str());
            ImGui::NewLine();

            static bool dontShowAgain = false;
            if (ImGui::Checkbox("hex.builtin.common.dont_show_again"_lang, &dontShowAgain)) {
                ContentRegistry::Settings::write("hex.builtin.setting.general", "hex.builtin.setting.general.show_tips", !dontShowAgain);
            }

            ImGui::SameLine((ImGui::GetMainViewport()->Size / 3 - ImGui::CalcTextSize("hex.builtin.common.close"_lang) - ImGui::GetStyle().FramePadding).x);

            if (ImGui::Button("hex.builtin.common.close"_lang))
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }


        // Popup for if there is a safety backup present because ImHex crashed
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5F, 0.5F));
        if (ImGui::BeginPopupModal("hex.builtin.welcome.safety_backup.title"_lang, nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
            ImGui::TextUnformatted("hex.builtin.welcome.safety_backup.desc"_lang);
            ImGui::NewLine();

            auto width = ImGui::GetWindowWidth();
            ImGui::SetCursorPosX(width / 9);
            if (ImGui::Button("hex.builtin.welcome.safety_backup.restore"_lang, ImVec2(width / 3, 0))) {
                ProjectFile::load(s_safetyBackupPath);
                ProjectFile::clearPath();

                for (const auto &provider : ImHexApi::Provider::getProviders())
                    provider->markDirty();

                fs::remove(s_safetyBackupPath);

                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            ImGui::SetCursorPosX(width / 9 * 5);
            if (ImGui::Button("hex.builtin.welcome.safety_backup.delete"_lang, ImVec2(width / 3, 0))) {
                fs::remove(s_safetyBackupPath);

                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
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

                for (const auto &unlocalizedProviderName : ContentRegistry::Provider::getEntries()) {
                    if (ImGui::Hyperlink(LangEntry(unlocalizedProviderName))) {
                        ImHexApi::Provider::createProvider(unlocalizedProviderName);
                        ImGui::CloseCurrentPopup();
                    }
                }

                ImGui::EndPopup();
            }

            ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetTextLineHeightWithSpacing() * 9);
            ImGui::TableNextColumn();
            ImGui::UnderlinedText("hex.builtin.welcome.start.recent"_lang);
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

            auto extraWelcomeScreenEntries = ContentRegistry::Interface::getWelcomeScreenEntries();
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

        drawPopups();
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
                auto theme = ContentRegistry::Settings::getSetting("hex.builtin.setting.interface", "hex.builtin.setting.interface.color");

                if (theme.is_string()) {
                    if (theme != api::ThemeManager::NativeTheme) {
                        static std::string lastTheme;

                        if (const auto thisTheme = theme.get<std::string>(); thisTheme != lastTheme) {
                            EventManager::post<RequestChangeTheme>(thisTheme);
                            lastTheme = thisTheme;
                        }
                    }
                }
            }

            {
                auto language = ContentRegistry::Settings::getSetting("hex.builtin.setting.interface", "hex.builtin.setting.interface.language");

                if (language.is_string()) {
                    LangEntry::loadLanguage(static_cast<std::string>(language));
                } else {
                    // If no language is specified, fall back to English.
                    LangEntry::loadLanguage("en-US");
                }
            }

            {
                auto targetFps = ContentRegistry::Settings::getSetting("hex.builtin.setting.interface", "hex.builtin.setting.interface.fps");

                if (targetFps.is_number())
                    ImHexApi::System::setTargetFPS(targetFps);
            }
        });

        (void)EventManager::subscribe<RequestChangeTheme>([](const std::string &theme) {
            auto changeTexture = [&](const std::string &path) {
                auto textureData = romfs::get(path);

                return ImGui::Texture(reinterpret_cast<const ImU8*>(textureData.data()), textureData.size());
            };

            api::ThemeManager::changeTheme(theme);
            s_bannerTexture = changeTexture(hex::format("banner{}.png", api::ThemeManager::getThemeImagePostfix()));
            s_backdropTexture = changeTexture(hex::format("backdrop{}.png", api::ThemeManager::getThemeImagePostfix()));

            if (!s_bannerTexture.isValid()) {
                log::error("Failed to load banner texture!");
            }
        });

        (void)EventManager::subscribe<EventProviderOpened>([](prv::Provider *provider) {
            {
                auto recentPath = fs::getDefaultPaths(fs::ImHexPath::Recent).front();
                auto fileName = hex::format("{:%y%m%d_%H%M%S}.json", fmt::gmtime(std::chrono::system_clock::now()));
                fs::File recentFile(recentPath / fileName, fs::File::Mode::Create);

                if (auto settings = provider->storeSettings(); !settings.is_null())
                    recentFile.write(settings.dump(4));
            }

            updateRecentProviders();
        });

        EventManager::subscribe<EventProviderCreated>([](auto) {
            if (!isAnyViewOpen())
                loadDefaultLayout();
        });

        EventManager::subscribe<EventWindowInitialized>([] {
            // documentation of the value above the setting definition
            int showCheckForUpdates = ContentRegistry::Settings::read("hex.builtin.setting.general", "hex.builtin.setting.general.check_for_updates", 2);
            if (showCheckForUpdates == 2) {
                ContentRegistry::Settings::write("hex.builtin.setting.general", "hex.builtin.setting.general.check_for_updates", 0); 
                View::showYesNoQuestionPopup("hex.builtin.welcome.check_for_updates_text"_lang,
                    [] { // yes
                        ContentRegistry::Settings::write("hex.builtin.setting.general", "hex.builtin.setting.general.check_for_updates", 1);
                        ImGui::CloseCurrentPopup();
                    }, [] { // no
                        ImGui::CloseCurrentPopup();
                    });
            }
        });

        // clear project context if we go back to the welcome screen
        EventManager::subscribe<EventProviderChanged>([](hex::prv::Provider *oldProvider, hex::prv::Provider *newProvider) {
            hex::unused(oldProvider);
            if (newProvider == nullptr) {
                ProjectFile::clearPath();
                EventManager::post<RequestUpdateWindowTitle>();
            }
        });

        ContentRegistry::Interface::addMenuItem("hex.builtin.menu.file", 1075, [&] {
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


        constexpr static auto CrashBackupFileName = "crash_backup.hexproj";
        for (const auto &path : fs::getDefaultPaths(fs::ImHexPath::Config)) {
            if (auto filePath = std::fs::path(path) / CrashBackupFileName; fs::exists(filePath)) {
                s_safetyBackupPath = filePath;
                TaskManager::doLater([] { ImGui::OpenPopup("hex.builtin.welcome.safety_backup.title"_lang); });
            }
        }

        auto tipsData = romfs::get("tips.json");
        if(tipsData.valid()){
            auto tipsCategories = nlohmann::json::parse(tipsData.string());

            auto now = std::chrono::system_clock::now();
            auto days_since_epoch = std::chrono::duration_cast<std::chrono::days>(now.time_since_epoch());
            std::mt19937 random(days_since_epoch.count());

            auto chosenCategory = tipsCategories[random()%tipsCategories.size()]["tips"];
            auto chosenTip = chosenCategory[random()%chosenCategory.size()];
            s_tipOfTheDay = chosenTip.get<std::string>();

            bool showTipOfTheDay = ContentRegistry::Settings::read("hex.builtin.setting.general", "hex.builtin.setting.general.show_tips", 1);
            if (showTipOfTheDay)
                TaskManager::doLater([] { ImGui::OpenPopup("hex.builtin.welcome.tip_of_the_day"_lang); });
        }
    }

}
