#include <content/recent.hpp>

#include <imgui.h>
#include <imgui_internal.h>

#include <hex/api/events/events_provider.hpp>
#include <hex/api/events/events_lifecycle.hpp>
#include <hex/api/content_registry/settings.hpp>
#include <hex/api/content_registry/user_interface.hpp>
#include <hex/api/imhex_api/provider.hpp>
#include <hex/api/project_file_manager.hpp>
#include <hex/api/task_manager.hpp>
#include <hex/providers/provider.hpp>
#include <hex/helpers/default_paths.hpp>

#include <hex/helpers/fmt.hpp>
#include <fmt/chrono.h>

#include <wolv/utils/guards.hpp>
#include <wolv/utils/string.hpp>

#include <toasts/toast_notification.hpp>
#include <fonts/vscode_icons.hpp>

#include <ranges>
#include <unordered_set>
#include <hex/api/content_registry/views.hpp>
#include <hex/helpers/menu_items.hpp>

namespace hex::plugin::builtin::recent {

    constexpr static auto MaxRecentEntries = 5;
    constexpr static auto BackupFileName = "crash_backup.hexproj";

    namespace {

        std::atomic_bool s_recentEntriesUpdating = false;
        std::list<RecentEntry> s_recentEntries;
        std::atomic_bool s_autoBackupsFound = false;

    }

    std::vector<PopupAutoBackups::BackupEntry> PopupAutoBackups::getAutoBackups() {
        std::set<BackupEntry> result;

        for (const auto &backupPath : paths::Backups.read()) {
            for (const auto &entry : std::fs::directory_iterator(backupPath)) {
                if (entry.is_regular_file() && entry.path().extension() == ".hexproj") {
                    // auto_backup.{:%y%m%d_%H%M%S}.hexproj
                    auto fileName = wolv::util::toUTF8String(entry.path().stem());
                    if (!fileName.starts_with("auto_backup."))
                        continue;

                    wolv::io::File backupFile(entry.path(), wolv::io::File::Mode::Read);

                    auto creationTimeString = fileName.substr(12);

                    std::tm utcTm = {};
                    if (sscanf(creationTimeString.c_str(), "%2d%2d%2d_%2d%2d%2d",
                        &utcTm.tm_year,
                        &utcTm.tm_mon,
                        &utcTm.tm_mday,
                        &utcTm.tm_hour,
                        &utcTm.tm_min,
                        &utcTm.tm_sec
                    ) != 6) {
                        continue;
                    }

                    utcTm.tm_year += 100; // Years since 1900
                    utcTm.tm_mon -= 1;    // Months since January

                    auto utcTime = std::mktime(&utcTm);
                    if (utcTime == 0)
                        continue;

                    std::tm *localTm = std::localtime(&utcTime);
                    if (localTm == nullptr)
                        continue;

                    result.emplace(
                        fmt::format("hex.builtin.welcome.start.recent.auto_backups.backup"_lang, *localTm),
                        entry.path(),
                        *localTm
                    );
                }
            }
        }

        return { result.begin(), result.end() };
    }

    PopupAutoBackups::PopupAutoBackups() : Popup("hex.builtin.welcome.start.recent.auto_backups", true, true) {
        m_backups = getAutoBackups();
    }

    void PopupAutoBackups::drawContent() {
        if (ImGui::BeginTable("AutoBackups", 1, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV, ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 5))) {
            for (const auto &backup : m_backups | std::views::reverse | std::views::take(10)) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                if (ImGui::Selectable(backup.displayName.c_str())) {
                    ProjectFile::load(backup.path);
                    Popup::close();
                }
            }

            ImGui::EndTable();
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Escape))
            this->close();
    }

    [[nodiscard]] ImGuiWindowFlags PopupAutoBackups::getFlags() const {
        return ImGuiWindowFlags_AlwaysAutoResize;
    }

    static void saveCurrentProjectAsRecent() {
        if (!ContentRegistry::Settings::read<bool>("hex.builtin.setting.general", "hex.builtin.setting.general.save_recent_providers", true)) {
            return;
        }
        auto fileName = fmt::format("{:%y%m%d_%H%M%S}.json", fmt::gmtime(std::chrono::system_clock::now()));

        auto projectFileName = ProjectFile::getPath().filename();
        if (projectFileName == BackupFileName)
            return;

        // The recent provider is saved to every "recent" directory
        for (const auto &recentPath : paths::Recent.write()) {
            wolv::io::File recentFile(recentPath / fileName, wolv::io::File::Mode::Create);
            if (!recentFile.isValid())
                continue;

            nlohmann::json recentEntry {
                { "type", "project" },
                { "displayName", wolv::util::toUTF8String(projectFileName) },
                { "path", wolv::util::toUTF8String(ProjectFile::getPath()) }
            };

            recentFile.writeString(recentEntry.dump(4));
        }

        updateRecentEntries();
    }

    void registerEventHandlers() {
        // Save every opened provider as a "recent" shortcut
        (void)EventProviderOpened::subscribe([](const prv::Provider *provider) {
            if (ContentRegistry::Settings::read<bool>("hex.builtin.setting.general", "hex.builtin.setting.general.save_recent_providers", true)) {
                auto fileName = fmt::format("{:%y%m%d_%H%M%S}.json", fmt::gmtime(std::chrono::system_clock::now()));

                // Do not save to recents if the provider is part of a project
                if (ProjectFile::hasPath())
                    return;

                // Do not save to recents if the provider doesn't want it
                if (!provider->isSavableAsRecent())
                    return;

                // The recent provider is saved to every "recent" directory
                for (const auto &recentPath : paths::Recent.write()) {
                    wolv::io::File recentFile(recentPath / fileName, wolv::io::File::Mode::Create);
                    if (!recentFile.isValid())
                        continue;

                    {
                        auto path = ProjectFile::getPath();
                        ProjectFile::clearPath();

                        if (auto settings = provider->storeSettings({}); !settings.is_null())
                            recentFile.writeString(settings.dump(4));

                        ProjectFile::setPath(path);
                    }
                }
            }

            updateRecentEntries();
        });

        // Add opened projects to "recents" shortcuts
        (void)EventProjectOpened::subscribe(saveCurrentProjectAsRecent);
        // When saving a project, update its "recents" entry. This is mostly useful when using saving a new project
        (void)EventProjectSaved::subscribe(saveCurrentProjectAsRecent);
    }

    void updateRecentEntries() {
        TaskManager::createBackgroundTask("hex.builtin.task.updating_recents", [](auto&) {
            if (s_recentEntriesUpdating)
                return;

            s_recentEntriesUpdating = true;
            ON_SCOPE_EXIT { s_recentEntriesUpdating = false; };

            s_recentEntries.clear();

            // Query all recent providers
            std::vector<std::fs::path> recentFilePaths;
            for (const auto &folder : paths::Recent.read()) {
                for (const auto &entry : std::fs::directory_iterator(folder)) {
                    if (entry.is_regular_file())
                        recentFilePaths.push_back(entry.path());
                }
            }

            // Sort recent provider files by last modified time
            std::sort(recentFilePaths.begin(), recentFilePaths.end(), [](const auto &a, const auto &b) {
                return std::fs::last_write_time(a) > std::fs::last_write_time(b);
            });

            std::unordered_set<RecentEntry, RecentEntry::HashFunction> alreadyAddedProviders;
            for (const auto &path : recentFilePaths) {
                if (s_recentEntries.size() >= MaxRecentEntries)
                    break;

                try {
                    wolv::io::File recentFile(path, wolv::io::File::Mode::Read);
                    if (!recentFile.isValid()) {
                        continue;
                    }

                    auto content = recentFile.readString();
                    if (content.empty()) {
                        continue;
                    }

                    auto jsonData = nlohmann::json::parse(content);

                    auto entry = RecentEntry {
                        .displayName    = jsonData.at("displayName"),
                        .type           = jsonData.at("type"),
                        .entryFilePath  = path,
                        .data           = jsonData
                    };

                    // Do not add entry twice
                    if (!alreadyAddedProviders.insert(entry).second)
                        continue;

                    s_recentEntries.push_back(entry);
                } catch (const std::exception &e) {
                    log::error("Failed to parse recent file: {}", e.what());
                }
            }

            // Delete all recent provider files that are not in the list
            for (const auto &path : recentFilePaths) {
                bool found = false;
                for (const auto &provider : s_recentEntries) {
                    if (path == provider.entryFilePath) {
                        found = true;
                        break;
                    }
                }

                if (!found)
                    wolv::io::fs::remove(path);
            }

            s_autoBackupsFound = false;
            for (const auto &backupPath : paths::Backups.read()) {
                for (const auto &entry : std::fs::directory_iterator(backupPath)) {
                    if (entry.is_regular_file() && entry.path().extension() == ".hexproj") {
                        s_autoBackupsFound = true;
                        break;
                    }
                }
            }
        });
    }

    void loadRecentEntry(const RecentEntry &recentEntry) {
        if (recentEntry.type == "project") {
            std::fs::path projectPath = recentEntry.data["path"].get<std::string>();
            if (!ProjectFile::load(projectPath)) {
                ui::ToastError::open(fmt::format("hex.builtin.popup.error.project.load"_lang, wolv::util::toUTF8String(projectPath)));
            }
            return;
        }

        auto provider = ImHexApi::Provider::createProvider(recentEntry.type, true);
        if (provider != nullptr) {
            provider->loadSettings(recentEntry.data);

            ImHexApi::Provider::openProvider(provider);

            updateRecentEntries();
        }
    }


    void draw() {
        if (s_recentEntries.empty() && !s_autoBackupsFound)
            return;

        static bool collapsed = false;
        if (ImGuiExt::BeginSubWindow("hex.builtin.welcome.start.recent"_lang, &collapsed, ImVec2(), ImGuiChildFlags_AutoResizeX)) {
            if (!s_recentEntriesUpdating) {
                for (auto it = s_recentEntries.begin(); it != s_recentEntries.end();) {
                    const auto &recentEntry = *it;
                    bool shouldRemove = false;

                    const bool isProject = recentEntry.type == "project";

                    ImGui::PushID(&recentEntry);
                    ON_SCOPE_EXIT { ImGui::PopID(); };

                    const char* icon;
                    if (isProject) {
                        icon = ICON_VS_NOTEBOOK;
                    } else {
                        icon = ICON_VS_FILE_BINARY;
                    }
                  
                    if (ImGuiExt::IconHyperlink(icon, hex::limitStringLength(recentEntry.displayName, 32).c_str())) {
                        loadRecentEntry(recentEntry);
                        break;
                    }
                    ImGui::SetItemTooltip("%s", recentEntry.displayName.c_str());

                    if (ImGui::IsItemHovered() && ImGui::GetIO().KeyShift) {
                        if (ImGui::BeginTooltip()) {
                            if (ImGui::BeginTable("##RecentEntryTooltip", 2, ImGuiTableFlags_RowBg)) {
                                ImGui::TableNextRow();
                                ImGui::TableNextColumn();
                                ImGui::TextUnformatted("hex.ui.common.name"_lang);
                                ImGui::TableNextColumn();
                                ImGui::TextUnformatted(recentEntry.displayName.c_str());

                                ImGui::TableNextRow();
                                ImGui::TableNextColumn();
                                ImGui::TextUnformatted("hex.ui.common.type"_lang);
                                ImGui::TableNextColumn();

                                if (isProject) {
                                    ImGui::TextUnformatted("hex.ui.common.project"_lang);

                                    ImGui::TableNextRow();
                                    ImGui::TableNextColumn();
                                    ImGui::TextUnformatted("hex.ui.common.path"_lang);
                                    ImGui::TableNextColumn();
                                    ImGui::TextUnformatted(recentEntry.data["path"].get<std::string>().c_str());
                                } else {
                                    ImGui::TextUnformatted(Lang(recentEntry.type));
                                }

                                ImGui::EndTable();
                            }
                            ImGui::EndTooltip();
                        }
                    }

                    ImGui::PushID(recentEntry.getHash());

                    // Detect right click on recent provider
                    constexpr static auto PopupID = "RecentEntryMenu";
                    if (ImGui::IsMouseReleased(1) && ImGui::IsItemHovered()) {
                        ImGui::OpenPopup(PopupID);
                    }

                    if (ImGui::BeginPopup(PopupID)) {
                        if (ImGui::MenuItemEx("hex.ui.common.remove"_lang, ICON_VS_REMOVE)) {
                            shouldRemove = true;
                        }
                        ImGui::EndPopup();
                    }

                    ImGui::PopID();

                    // Handle deletion from vector and on disk
                    if (shouldRemove) {
                        wolv::io::fs::remove(recentEntry.entryFilePath);
                        it = s_recentEntries.erase(it);
                    } else {
                        ++it;
                    }
                }

                if (s_autoBackupsFound) {
                    ImGui::Separator();
                    if (ImGuiExt::IconHyperlink(ICON_VS_ARCHIVE, "hex.builtin.welcome.start.recent.auto_backups"_lang))
                        PopupAutoBackups::open();
                }
            }

        } else {
            ImGui::TextUnformatted("...");
        }
        ImGuiExt::EndSubWindow();
    }

    void addMenuItems() {
        #if defined(OS_WEB)
            return;
        #endif

        ContentRegistry::UserInterface::addMenuItemSubMenu({ "hex.builtin.menu.file" }, 1200, [] {
            if (menu::beginMenuEx("hex.builtin.menu.file.open_recent"_lang, ICON_VS_ARCHIVE, !recent::s_recentEntriesUpdating && !s_recentEntries.empty())) {
                // Copy to avoid changing list while iteration
                auto recentEntries = s_recentEntries;
                for (auto &recentEntry : recentEntries) {
                    if (menu::menuItem(recentEntry.displayName.c_str())) {
                        loadRecentEntry(recentEntry);
                    }
                }

                menu::menuSeparator();
                if (menu::menuItem("hex.builtin.menu.file.clear_recent"_lang)) {
                    s_recentEntries.clear();

                    // Remove all recent files
                    for (const auto &recentPath : paths::Recent.write()) {
                        for (const auto &entry : std::fs::directory_iterator(recentPath))
                            std::fs::remove(entry.path());
                    }
                }

                menu::endMenu();
            }
        }, [] {
            return TaskManager::getRunningTaskCount() == 0 && !s_recentEntriesUpdating && !s_recentEntries.empty();
        }, ContentRegistry::Views::getViewByName("hex.builtin.view.hex_editor.name"), true);
    }
}
