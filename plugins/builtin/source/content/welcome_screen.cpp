#include <hex.hpp>
#include <hex/api/event.hpp>
#include <hex/api/content_registry.hpp>
#include <hex/api/localization.hpp>
#include <hex/api/plugin_manager.hpp>
#include <hex/ui/view.hpp>
#include <hex/helpers/fs.hpp>
#include <hex/helpers/logger.hpp>


#include <hex/helpers/project_file_handler.hpp>

#include <imgui.h>
#include <implot.h>
#include <imnodes.h>
#include <hex/ui/imgui_imhex_extensions.h>

#include <nlohmann/json.hpp>
#include <romfs/romfs.hpp>

#include <fonts/fontawesome_font.h>
#include <fonts/codicons_font.h>

#include <list>

namespace hex::plugin::builtin {

    static ImGui::Texture s_bannerTexture, s_backdropTexture;
    static std::list<std::fs::path> s_recentFilePaths;

    static std::fs::path s_safetyBackupPath;

    static std::string s_tipOfTheDay;

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
                ProjectFile::markDirty();

                ProjectFile::clearProjectFilePath();
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

        ImGui::Image(s_bannerTexture, s_bannerTexture.size() / (2 * (1.0F / ImHexApi::System::getGlobalScale())));

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
                if (ImGui::IconHyperlink(ICON_VS_NEW_FILE, "hex.builtin.welcome.start.create_file"_lang))
                    EventManager::post<RequestOpenWindow>("Create File");
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
                for (auto &path : s_recentFilePaths) {
                    if (ImGui::BulletHyperlink(std::fs::path(path).filename().string().c_str())) {
                        EventManager::post<RequestOpenFile>(path);
                        break;
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
        if (isAnyViewOpen() && ImHexApi::Provider::isValid()) return;

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
        (void)EventManager::subscribe<EventFrameBegin>(drawWelcomeScreen);
        (void)EventManager::subscribe<EventFrameBegin>(drawNoViewsBackground);

        (void)EventManager::subscribe<EventSettingsChanged>([]() {
            {
                auto theme = ContentRegistry::Settings::getSetting("hex.builtin.setting.interface", "hex.builtin.setting.interface.color");

                if (theme.is_number()) {
                    static int lastTheme = 0;

                    if (const int thisTheme = theme.get<int>(); thisTheme != lastTheme) {
                        EventManager::post<RequestChangeTheme>(thisTheme);
                        lastTheme = thisTheme;
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

        (void)EventManager::subscribe<RequestChangeTheme>([](u32 theme) {
            auto changeTexture = [&](const std::string &path, const ImGui::Texture &texture) {
                auto textureData = romfs::get(path);
                auto oldTexture  = texture;
                auto newTexture  = ImGui::LoadImageFromMemory(reinterpret_cast<const ImU8 *>(textureData.data()), textureData.size());

                if (oldTexture.valid()) { ImGui::UnloadImage(oldTexture); }

                return newTexture;
            };

            switch (theme) {
                default:
                case 1: /* Dark theme */
                    {
                        ImGui::StyleColorsDark();
                        ImGui::StyleCustomColorsDark();
                        ImPlot::StyleColorsDark();
                        s_bannerTexture = changeTexture("banner_dark.png", s_bannerTexture);
                        s_backdropTexture = changeTexture("backdrop_dark.png", s_backdropTexture);

                        break;
                    }
                case 2: /* Light theme */
                    {
                        ImGui::StyleColorsLight();
                        ImGui::StyleCustomColorsLight();
                        ImPlot::StyleColorsLight();
                        s_bannerTexture = changeTexture("banner_light.png", s_bannerTexture);
                        s_backdropTexture = changeTexture("backdrop_light.png", s_backdropTexture);

                        break;
                    }
                case 3: /* Classic theme */
                    {
                        ImGui::StyleColorsClassic();
                        ImGui::StyleCustomColorsClassic();
                        ImPlot::StyleColorsClassic();
                        s_bannerTexture = changeTexture("banner_dark.png", s_bannerTexture);
                        s_backdropTexture = changeTexture("backdrop_dark.png", s_backdropTexture);

                        break;
                    }
            }

            ImGui::GetStyle().Colors[ImGuiCol_DockingEmptyBg]   = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
            ImGui::GetStyle().Colors[ImGuiCol_TitleBg]          = ImGui::GetStyle().Colors[ImGuiCol_MenuBarBg];
            ImGui::GetStyle().Colors[ImGuiCol_TitleBgActive]    = ImGui::GetStyle().Colors[ImGuiCol_MenuBarBg];
            ImGui::GetStyle().Colors[ImGuiCol_TitleBgCollapsed] = ImGui::GetStyle().Colors[ImGuiCol_MenuBarBg];

            if (!s_bannerTexture.valid()) {
                log::error("Failed to load banner texture!");
            }
        });

        (void)EventManager::subscribe<EventFileLoaded>([](const auto &path) {
            s_recentFilePaths.push_front(path);

            {
                std::list<std::fs::path> uniques;
                for (auto &file : s_recentFilePaths) {

                    bool exists = false;
                    for (auto &unique : uniques) {
                        if (file == unique)
                            exists = true;
                    }

                    if (!exists && !file.empty())
                        uniques.push_back(file);

                    if (uniques.size() > 5)
                        break;
                }
                s_recentFilePaths = uniques;
            }

            {
                std::vector<std::string> recentFilesVector;
                for (const auto &recentPath : s_recentFilePaths)
                    recentFilesVector.push_back(recentPath.string());

                ContentRegistry::Settings::write("hex.builtin.setting.imhex", "hex.builtin.setting.imhex.recent_files", recentFilesVector);
            }
        });

        EventManager::subscribe<EventProviderCreated>([](auto) {
            if (!isAnyViewOpen())
                loadDefaultLayout();
        });

        ContentRegistry::Interface::addMenuItem("hex.builtin.menu.file", 1075, [&] {
            if (ImGui::BeginMenu("hex.builtin.menu.file.open_recent"_lang, !s_recentFilePaths.empty())) {
                // Copy to avoid changing list while iteration
                std::list<std::fs::path> recentFilePaths = s_recentFilePaths;
                for (auto &path : recentFilePaths) {
                    auto filename = std::fs::path(path).filename().string();
                    if (ImGui::MenuItem(filename.c_str())) {
                        EventManager::post<RequestOpenFile>(path);
                    }
                }

                ImGui::Separator();
                if (ImGui::MenuItem("hex.builtin.menu.file.clear_recent"_lang)) {
                    s_recentFilePaths.clear();
                    ContentRegistry::Settings::write(
                        "hex.builtin.setting.imhex",
                        "hex.builtin.setting.imhex.recent_files",
                        std::vector<std::string> {});
                }

                ImGui::EndMenu();
            }
        });


        constexpr auto CrashBackupFileName = "crash_backup.hexproj";
        for (const auto &path : fs::getDefaultPaths(fs::ImHexPath::Config)) {
            if (auto filePath = std::fs::path(path) / CrashBackupFileName; fs::exists(filePath)) {
                s_safetyBackupPath = filePath;
                ImHexApi::Tasks::doLater([] { ImGui::OpenPopup("hex.builtin.welcome.safety_backup.title"_lang); });
            }
        }

        for (const auto &pathString : ContentRegistry::Settings::read("hex.builtin.setting.imhex", "hex.builtin.setting.imhex.recent_files")) {
            std::fs::path path = std::u8string(pathString.begin(), pathString.end());
            if (fs::exists(path))
                s_recentFilePaths.emplace_back(path);
        }

        if (ImHexApi::System::getInitArguments().contains("tip-of-the-day")) {
            s_tipOfTheDay = ImHexApi::System::getInitArguments()["tip-of-the-day"];

            bool showTipOfTheDay = ContentRegistry::Settings::read("hex.builtin.setting.general", "hex.builtin.setting.general.show_tips", 1);
            if (showTipOfTheDay)
                ImHexApi::Tasks::doLater([] { ImGui::OpenPopup("hex.builtin.welcome.tip_of_the_day"_lang); });
        }
    }

}
