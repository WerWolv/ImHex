#include <imgui.h>
#include <imgui_internal.h>

#include <hex/api/event_manager.hpp>
#include <hex/api/content_registry.hpp>
#include <hex/api/project_file_manager.hpp>
#include <hex/api/task_manager.hpp>
#include <hex/providers/provider.hpp>
#include <hex/helpers/default_paths.hpp>

#include <hex/helpers/fmt.hpp>
#include <fmt/chrono.h>

#include <wolv/utils/guards.hpp>
#include <wolv/utils/string.hpp>

#include <content/recent.hpp>
#include <toasts/toast_notification.hpp>
#include <fonts/vscode_icons.hpp>

#include <ranges>
#include <unordered_set>
#include <ui/menu_items.hpp>

namespace hex::plugin::builtin::recent {

    constexpr static auto MaxRecentEntries = 5;
    constexpr static auto BackupFileName = "crash_backup.hexproj";

    namespace {

        std::atomic_bool s_recentEntriesUpdating = false;
        std::list<RecentEntry> s_recentEntries;
        std::atomic_bool s_autoBackupsFound = false;


        class PopupAutoBackups : public Popup<PopupAutoBackups> {
        private:
            struct BackupEntry {
                std::string displayName;
                std::fs::path path;
            };
        public:
            PopupAutoBackups() : Popup("hex.builtin.welcome.start.recent.auto_backups", true, true) {
                for (const auto &backupPath : paths::Backups.read()) {
                    for (const auto &entry : std::fs::directory_iterator(backupPath)) {
                        if (entry.is_regular_file() && entry.path().extension() == ".hexproj") {
                            wolv::io::File backupFile(entry.path(), wolv::io::File::Mode::Read);

                            m_backups.emplace_back(
                                hex::format("hex.builtin.welcome.start.recent.auto_backups.backup"_lang, fmt::gmtime(backupFile.getFileInfo()->st_ctime)),
                                entry.path()
                            );
                        }
                    }
                }
            }

            void drawContent() override {
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

            [[nodiscard]] ImGuiWindowFlags getFlags() const override {
                return ImGuiWindowFlags_AlwaysAutoResize;
            }

        private:
            std::vector<BackupEntry> m_backups;
        };

    }


    void registerEventHandlers() {
        // Save every opened provider as a "recent" shortcut
        (void)EventProviderOpened::subscribe([](const prv::Provider *provider) {
            if (ContentRegistry::Settings::read<bool>("hex.builtin.setting.general", "hex.builtin.setting.general.save_recent_providers", true)) {
                auto fileName = hex::format("{:%y%m%d_%H%M%S}.json", fmt::gmtime(std::chrono::system_clock::now()));

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

        // Save opened projects as a "recent" shortcut
        (void)EventProjectOpened::subscribe([] {
             if (ContentRegistry::Settings::read<bool>("hex.builtin.setting.general", "hex.builtin.setting.general.save_recent_providers", true)) {
                auto fileName = hex::format("{:%y%m%d_%H%M%S}.json", fmt::gmtime(std::chrono::system_clock::now()));

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
            }
            
            updateRecentEntries();
        });
    }

    void updateRecentEntries() {
        TaskManager::createBackgroundTask("hex.builtin.task.updating_recents"_lang, [](auto&) {
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

            std::unordered_set<RecentEntry, RecentEntry::HashFunction> uniqueProviders;
            for (const auto &path : recentFilePaths) {
                if (uniqueProviders.size() >= MaxRecentEntries)
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
                    uniqueProviders.insert(RecentEntry {
                        .displayName    = jsonData.at("displayName"),
                        .type           = jsonData.at("type"),
                        .entryFilePath  = path,
                        .data           = jsonData
                    });
                } catch (const std::exception &e) {
                    log::error("Failed to parse recent file: {}", e.what());
                }
            }

            // Delete all recent provider files that are not in the list
            for (const auto &path : recentFilePaths) {
                bool found = false;
                for (const auto &provider : uniqueProviders) {
                    if (path == provider.entryFilePath) {
                        found = true;
                        break;
                    }
                }

                if (!found)
                    wolv::io::fs::remove(path);
            }

            std::copy(uniqueProviders.begin(), uniqueProviders.end(), std::front_inserter(s_recentEntries));

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
                ui::ToastError::open(hex::format("hex.builtin.popup.error.project.load"_lang, wolv::util::toUTF8String(projectPath)));
            }
            return;
        }
        auto *provider = ImHexApi::Provider::createProvider(recentEntry.type, true);
        if (provider != nullptr) {
            provider->loadSettings(recentEntry.data);

            if (!provider->open() || !provider->isAvailable()) {
                ui::ToastError::open(hex::format("hex.builtin.provider.error.open"_lang, provider->getErrorMessage()));
                TaskManager::doLater([provider] { ImHexApi::Provider::remove(provider); });
                return;
            }

            EventProviderOpened::post(provider);

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
                        icon = ICON_VS_PROJECT;
                    } else {
                        icon = ICON_VS_FILE_BINARY;
                    }
                  
                    if (ImGuiExt::Hyperlink(hex::format("{} {}", icon, hex::limitStringLength(recentEntry.displayName, 32)).c_str())) {
                        loadRecentEntry(recentEntry);
                        break;
                    }

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

                    // Detect right click on recent provider
                    std::string popupID = hex::format("RecentEntryMenu.{}", recentEntry.getHash());
                    if (ImGui::IsMouseReleased(1) && ImGui::IsItemHovered()) {
                        ImGui::OpenPopup(popupID.c_str());
                    }

                    if (ImGui::BeginPopup(popupID.c_str())) {
                        if (ImGui::MenuItemEx("hex.ui.common.remove"_lang, ICON_VS_REMOVE)) {
                            shouldRemove = true;
                        }
                        ImGui::EndPopup();
                    }

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
                    if (ImGuiExt::Hyperlink(hex::format("{} {}", ICON_VS_ARCHIVE, "hex.builtin.welcome.start.recent.auto_backups"_lang).c_str()))
                        PopupAutoBackups::open();
                }
            }

        }
        ImGuiExt::EndSubWindow();
    }

    void addMenuItems() {
        #if defined(OS_WEB)
            return;
        #endif

        ContentRegistry::Interface::addMenuItemSubMenu({ "hex.builtin.menu.file" }, 1200, [] {
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
        });
    }
}
