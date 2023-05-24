#include <unordered_set>

#include <imgui.h>

#include <hex/api/event.hpp>
#include <hex/api/content_registry.hpp>
#include <hex/api/project_file_manager.hpp>
#include <hex/api/task.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/providers/provider.hpp>

#include <wolv/utils/guards.hpp>

#include <content/recent.hpp>
#include <content/popups/popup_notification.hpp>

namespace hex::plugin::builtin::recent {

    constexpr static auto MaxRecentEntries = 5;

    static std::atomic<bool> s_recentEntriesUpdating = false;
    static std::list<RecentEntry> s_recentEntries;

    void registerEventHandlers() {
        // Save every opened provider as a "recent" shortcut
        (void)EventManager::subscribe<EventProviderOpened>([](prv::Provider *provider) {
            if (ContentRegistry::Settings::read("hex.builtin.setting.general", "hex.builtin.setting.general.save_recent_providers", 1) == 1) {
                auto fileName = hex::format("{:%y%m%d_%H%M%S}.json", fmt::gmtime(std::chrono::system_clock::now()));

                // do not save to recents if the provider is part of a project
                if(ProjectFile::hasPath())return;

                // The recent provider is saved to every "recent" directory
                for (const auto &recentPath : fs::getDefaultPaths(fs::ImHexPath::Recent)) {
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

            updateRecentEntries();
        });

        // save opened projects as a "recent" shortcut
        (void)EventManager::subscribe<EventProjectOpened>([] {
             if (ContentRegistry::Settings::read("hex.builtin.setting.general", "hex.builtin.setting.general.save_recent_providers", 1) == 1) {
                auto fileName = hex::format("{:%y%m%d_%H%M%S}.json", fmt::gmtime(std::chrono::system_clock::now()));

                // The recent provider is saved to every "recent" directory
                for (const auto &recentPath : fs::getDefaultPaths(fs::ImHexPath::Recent)) {
                    wolv::io::File recentFile(recentPath / fileName, wolv::io::File::Mode::Create);
                    if (!recentFile.isValid())
                        continue;

                    std::string displayName = hex::format("[Project] {}", wolv::util::toUTF8String(ProjectFile::getPath().filename()));

                    nlohmann::json recentEntry {
                        {"type", "project"},
                        {"displayName", displayName},
                        {"path", ProjectFile::getPath()}
                    };

                    recentFile.writeString(recentEntry.dump(4));
                }
            }
            
            updateRecentEntries();
        });
    }

    void updateRecentEntries() {
        TaskManager::createBackgroundTask("Updating recent files", [](auto&){
            if (s_recentEntriesUpdating)
                return;

            s_recentEntriesUpdating = true;
            ON_SCOPE_EXIT { s_recentEntriesUpdating = false; };

            s_recentEntries.clear();

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

            std::unordered_set<RecentEntry, RecentEntry::HashFunction> uniqueProviders;
            for (u32 i = 0; i < recentFilePaths.size() && uniqueProviders.size() < MaxRecentEntries; i++) {
                auto &path = recentFilePaths[i];
                try {
                    auto jsonData = nlohmann::json::parse(wolv::io::File(path, wolv::io::File::Mode::Read).readString());
                    uniqueProviders.insert(RecentEntry {
                        .displayName    = jsonData.at("displayName"),
                        .type           = jsonData.at("type"),
                        .entryFilePath       = path,
                        .data           = jsonData
                    });
                } catch (...) { }
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
        });
    }

    void loadRecentEntry(const RecentEntry &recentEntry) {
        if(recentEntry.type == "project"){
            std::fs::path projectPath = recentEntry.data.at("path");
            ProjectFile::load(projectPath);
            return;
        }
        auto *provider = ImHexApi::Provider::createProvider(recentEntry.type, true);
        if (provider != nullptr) {
            provider->loadSettings(recentEntry.data);

            if (!provider->open() || !provider->isAvailable()) {
                PopupError::open(hex::format("hex.builtin.provider.error.open"_lang, provider->getErrorMessage()));
                TaskManager::doLater([provider] { ImHexApi::Provider::remove(provider); });
                return;
            }

            EventManager::post<EventProviderOpened>(provider);

            updateRecentEntries();
        }
    }


    void draw() {
        ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetTextLineHeightWithSpacing() * 9);
        ImGui::TableNextColumn();
        ImGui::UnderlinedText(s_recentEntries.empty() ? "" : "hex.builtin.welcome.start.recent"_lang);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5_scaled);
        {
            if (!s_recentEntriesUpdating) {
                auto it = s_recentEntries.begin();
                while (it != s_recentEntries.end()) {
                    const auto &recentEntry = *it;
                    bool shouldRemove = false;

                    ImGui::PushID(&recentEntry);
                    ON_SCOPE_EXIT { ImGui::PopID(); };

                    if (ImGui::BulletHyperlink(recentEntry.displayName.c_str())) {
                        loadRecentEntry(recentEntry);
                        break;
                    }

                    // Detect right click on recent provider
                    std::string popupID = std::string("RecentEntryMenu.")+std::to_string(recentEntry.getHash());
                    if (ImGui::IsMouseReleased(1) && ImGui::IsItemHovered()) {
                        ImGui::OpenPopup(popupID.c_str());
                    }

                    if (ImGui::BeginPopup(popupID.c_str())) {
                        if (ImGui::MenuItem("Remove")) {
                            shouldRemove = true;
                        }
                        ImGui::EndPopup();
                    }

                    // handle deletion from vector and on disk
                    if (shouldRemove) {
                        wolv::io::fs::remove(recentEntry.entryFilePath);
                        it = s_recentEntries.erase(it);
                    } else {
                        it++;
                    }
                }
            }
        }
    }

    void drawFileMenuItem() {
        ContentRegistry::Interface::addMenuItemSubMenu({ "hex.builtin.menu.file" }, 1200, [] {
            if (ImGui::BeginMenu("hex.builtin.menu.file.open_recent"_lang, !recent::s_recentEntriesUpdating && !s_recentEntries.empty())) {
                // Copy to avoid changing list while iteration
                auto recentEntries = s_recentEntries;
                for (auto &recentEntry : recentEntries) {
                    if (ImGui::MenuItem(recentEntry.displayName.c_str())) {
                        loadRecentEntry(recentEntry);
                    }
                }

                ImGui::Separator();
                if (ImGui::MenuItem("hex.builtin.menu.file.clear_recent"_lang)) {
                    s_recentEntries.clear();

                    // Remove all recent files
                    for (const auto &recentPath : fs::getDefaultPaths(fs::ImHexPath::Recent))
                        for (const auto &entry : std::fs::directory_iterator(recentPath))
                            std::fs::remove(entry.path());
                }

                ImGui::EndMenu();
            }
        });
    }
}