#include "content/views/view_store.hpp"
#include <hex/api/theme_manager.hpp>
#include <hex/api/achievement_manager.hpp>
#include <hex/api_urls.hpp>

#include <hex/api/content_registry.hpp>

#include <content/popups/popup_notification.hpp>

#include <imgui.h>

#include <hex/helpers/utils.hpp>
#include <hex/helpers/crypto.hpp>
#include <hex/helpers/logger.hpp>
#include <hex/helpers/magic.hpp>
#include <hex/helpers/fs.hpp>
#include <hex/helpers/tar.hpp>

#include <filesystem>
#include <functional>
#include <nlohmann/json.hpp>

#include <wolv/io/file.hpp>

namespace hex::plugin::builtin {

    using namespace std::literals::string_literals;
    using namespace std::literals::chrono_literals;

    ViewStore::ViewStore() : View("hex.builtin.view.store.name") {
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.extras", "hex.builtin.view.store.name" }, 1000, Shortcut::None, [&, this] {
            if (this->m_requestStatus == RequestStatus::NotAttempted)
                this->refresh();
            TaskManager::doLater([] { ImGui::OpenPopup(View::toWindowName("hex.builtin.view.store.name").c_str()); });
            this->getWindowOpenState() = true;
        });

        this->m_httpRequest.setTimeout(30'0000);

        addCategory("hex.builtin.view.store.tab.patterns"_lang,     "patterns",     fs::ImHexPath::Patterns);
        addCategory("hex.builtin.view.store.tab.includes"_lang,     "includes",     fs::ImHexPath::PatternsInclude);
        addCategory("hex.builtin.view.store.tab.magic"_lang,        "magic",        fs::ImHexPath::Magic, []{
            magic::compile();
        });
        addCategory("hex.builtin.view.store.tab.nodes"_lang,        "nodes",        fs::ImHexPath::Nodes);
        addCategory("hex.builtin.view.store.tab.encodings"_lang,    "encodings",    fs::ImHexPath::Encodings);
        addCategory("hex.builtin.view.store.tab.constants"_lang,    "constants",    fs::ImHexPath::Constants);
        addCategory("hex.builtin.view.store.tab.themes"_lang,       "themes",       fs::ImHexPath::Themes, [this]{
            auto themeFile = wolv::io::File(this->m_downloadPath, wolv::io::File::Mode::Read);

            ThemeManager::addTheme(themeFile.readString());
        });
        addCategory("hex.builtin.view.store.tab.yara"_lang,         "yara",         fs::ImHexPath::Yara);
    }

    void ViewStore::drawTab(hex::plugin::builtin::StoreCategory &category) {
        if (ImGui::BeginTabItem(LangEntry(category.unlocalizedName))) {
            if (ImGui::BeginTable("##pattern_language", 4, ImGuiTableFlags_ScrollY | ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_RowBg)) {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn("hex.builtin.view.store.row.name"_lang, ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("hex.builtin.view.store.row.description"_lang, ImGuiTableColumnFlags_None);
                ImGui::TableSetupColumn("hex.builtin.view.store.row.authors"_lang, ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);

                ImGui::TableHeadersRow();

                u32 id = 1;
                for (auto &entry : category.entries) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(entry.name.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(entry.description.c_str());
                    if (ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        ImGui::PushTextWrapPos(500);
                        ImGui::TextUnformatted(entry.description.c_str());
                        ImGui::PopTextWrapPos();
                        ImGui::EndTooltip();
                    }
                    ImGui::TableNextColumn();
                    // the space makes a padding in the UI
                    ImGui::Text("%s ", wolv::util::combineStrings(entry.authors, ", ").c_str());
                    ImGui::TableNextColumn();

                    const auto buttonSize = ImVec2(100_scaled, ImGui::GetTextLineHeightWithSpacing());

                    ImGui::PushID(id);
                    ImGui::BeginDisabled(this->m_updateAllTask.isRunning() || (this->m_download.valid() && this->m_download.wait_for(0s) != std::future_status::ready));
                    {
                        if (entry.downloading) {
                            ImGui::ProgressBar(this->m_httpRequest.getProgress(), buttonSize, "");

                            if (this->m_download.valid() && this->m_download.wait_for(0s) == std::future_status::ready) {
                                this->handleDownloadFinished(category, entry);
                            }

                        } else {
                            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));

                            if (entry.hasUpdate) {
                                if (ImGui::Button("hex.builtin.view.store.update"_lang, buttonSize)) {
                                    entry.downloading = this->download(category.path, entry.fileName, entry.link, true);
                                }
                            } else if (!entry.installed) {
                                if (ImGui::Button("hex.builtin.view.store.download"_lang, buttonSize)) {
                                    entry.downloading = this->download(category.path, entry.fileName, entry.link, false);
                                    AchievementManager::unlockAchievement("hex.builtin.achievement.misc", "hex.builtin.achievement.misc.download_from_store.name");
                                }
                            } else {
                                if (ImGui::Button("hex.builtin.view.store.remove"_lang, buttonSize)) {
                                    entry.installed = !this->remove(category.path, entry.fileName);
                                }
                            }
                            ImGui::PopStyleVar();
                        }
                    }
                    ImGui::EndDisabled();
                    ImGui::PopID();
                    id++;
                }

                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }
    }

    void ViewStore::drawStore() {
        ImGui::Header("hex.builtin.view.store.desc"_lang, true);

        bool reloading = false;
        if (this->m_apiRequest.valid()) {
            if (this->m_apiRequest.wait_for(0s) != std::future_status::ready)
                reloading = true;
            else
                this->parseResponse();
        }

        ImGui::BeginDisabled(reloading);
        if (ImGui::Button("hex.builtin.view.store.reload"_lang)) {
            this->refresh();
        }
        ImGui::EndDisabled();

        if (reloading) {
            ImGui::SameLine();
            ImGui::TextSpinner("hex.builtin.view.store.loading"_lang);
        }

        // Align the button to the right
        ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::GetCursorPosX() - 25_scaled);
        ImGui::BeginDisabled(this->m_updateAllTask.isRunning() || this->m_updateCount == 0);
        if (ImGui::IconButton(ICON_VS_CLOUD_DOWNLOAD, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
            this->m_updateAllTask = TaskManager::createTask("Update All...", this->m_updateCount, [this](auto &task) {
                u32 progress = 0;
                for (auto &category : this->m_categories) {
                    for (auto &entry : category.entries) {
                        if (entry.hasUpdate) {
                            entry.downloading = this->download(category.path, entry.fileName, entry.link, true);
                            this->m_download.wait();
                            this->handleDownloadFinished(category, entry);
                            task.update(progress);
                        }
                    }
                }
            });
        }
        ImGui::InfoTooltip(hex::format("hex.builtin.view.store.update_count"_lang, this->m_updateCount.load()).c_str());

        ImGui::EndDisabled();

        if (ImGui::BeginTabBar("storeTabs")) {
            for (auto &category : this->m_categories) {
                this->drawTab(category);
            }

            ImGui::EndTabBar();
        }

    }

    void ViewStore::refresh() {
        // do not refresh if a refresh is already in progress
        if (this->m_requestStatus == RequestStatus::InProgress)
            return;
        this->m_requestStatus = RequestStatus::InProgress;

        for (auto &category : this->m_categories) {
            category.entries.clear();
        }

        this->m_httpRequest.setUrl(ImHexApiURL + "/store"s);
        this->m_apiRequest = this->m_httpRequest.execute();
    }

    void ViewStore::parseResponse() {
        auto response = this->m_apiRequest.get();
        this->m_requestStatus = response.isSuccess() ? RequestStatus::Succeeded : RequestStatus::Failed;
        if (this->m_requestStatus == RequestStatus::Succeeded) {
            auto json = nlohmann::json::parse(response.getData());

            auto parseStoreEntries = [](auto storeJson, StoreCategory &category) {
                // Check if the response handles the type of files
                if (storeJson.contains(category.requestName)) {

                    for (auto &entry : storeJson[category.requestName]) {

                        // Check if entry is valid
                        if (entry.contains("name") && entry.contains("desc") && entry.contains("authors") && entry.contains("file") && entry.contains("url") && entry.contains("hash") && entry.contains("folder")) {

                            // Parse entry
                            StoreEntry storeEntry = { entry["name"], entry["desc"], entry["authors"], entry["file"], entry["url"], entry["hash"], entry["folder"], false, false, false };

                            // Check if file is installed already or has an update available
                            for (const auto &folder : fs::getDefaultPaths(category.path)) {

                                auto path = folder / std::fs::path(storeEntry.fileName);

                                if (wolv::io::fs::exists(path) && fs::isPathWritable(folder)) {
                                    storeEntry.installed = true;

                                    wolv::io::File file(path, wolv::io::File::Mode::Read);
                                    auto bytes = file.readVector();

                                    auto fileHash = crypt::sha256(bytes);

                                    // Compare installed file hash with hash of repo file
                                    if (std::vector(fileHash.begin(), fileHash.end()) != crypt::decode16(storeEntry.hash))
                                        storeEntry.hasUpdate = true;
                                }
                            }

                            category.entries.push_back(storeEntry);
                        }
                    }
                }

                std::sort(category.entries.begin(), category.entries.end(), [](const auto &lhs, const auto &rhs) {
                    return lhs.name < rhs.name;
                });
            };

            for (auto &category : this->m_categories) {
                parseStoreEntries(json, category);
            }

            this->m_updateCount = 0;
            for (auto &category : this->m_categories) {
                for (auto &entry : category.entries) {
                    if (entry.hasUpdate)
                        this->m_updateCount += 1;
                }
            }
        }
        this->m_apiRequest = {};
    }

    void ViewStore::drawContent() {
        if (ImGui::BeginPopupModal(View::toWindowName("hex.builtin.view.store.name").c_str(), &this->getWindowOpenState())) {
            if (this->m_requestStatus == RequestStatus::Failed)
                ImGui::TextFormattedColored(ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarRed), "hex.builtin.view.store.netfailed"_lang);
            
            this->drawStore();            

            ImGui::EndPopup();
        } else {
            this->getWindowOpenState() = false;
        }
    }

    bool ViewStore::download(fs::ImHexPath pathType, const std::string &fileName, const std::string &url, bool update) {
        bool downloading = false;
        for (const auto &folderPath : fs::getDefaultPaths(pathType)) {
            if (!fs::isPathWritable(folderPath))
                continue;

            // verify that we write the file to the right folder
            // this is to prevent the filename from having elements like ../
            auto fullPath = std::fs::weakly_canonical(folderPath / std::fs::path(fileName));
            auto [folderIter, pathIter] = std::mismatch(folderPath.begin(), folderPath.end(), fullPath.begin());
            if(folderIter != folderPath.end()) {
                log::warn("The destination file name '{}' is invalid", fileName);
                return false;
            }

            if (!update || wolv::io::fs::exists(fullPath)) {
                downloading          = true;
                this->m_downloadPath = fullPath;

                this->m_httpRequest.setUrl(url);
                this->m_download     = this->m_httpRequest.downloadFile(fullPath);
                break;
            }
        }

        if (!downloading) {
            PopupError::open("hex.builtin.view.store.download_error"_lang);
            return false;
        }

        return true;
    }

    bool ViewStore::remove(fs::ImHexPath pathType, const std::string &fileName) {
        bool removed = true;
        for (const auto &path : fs::getDefaultPaths(pathType)) {
            const auto filePath = path / fileName;
            const auto folderPath = (path / std::fs::path(fileName).stem());

            wolv::io::fs::remove(filePath);
            wolv::io::fs::removeAll(folderPath);

            removed = removed && !wolv::io::fs::exists(filePath) && !wolv::io::fs::exists(folderPath);
            EventManager::post<EventStoreContentRemoved>(filePath);
        }

        return removed;
    }

    void ViewStore::addCategory(const std::string &unlocalizedName, const std::string &requestName, fs::ImHexPath path, std::function<void()> downloadCallback) {
        this->m_categories.push_back({ unlocalizedName, requestName, path, { }, std::move(downloadCallback) });
    }

    void ViewStore::handleDownloadFinished(const StoreCategory &category, StoreEntry &entry) {
        entry.downloading = false;

        auto response = this->m_download.get();
        if (response.isSuccess()) {
            if (entry.hasUpdate)
                this->m_updateCount -= 1;

            entry.installed = true;
            entry.hasUpdate = false;

            if (entry.isFolder) {
                Tar tar(this->m_downloadPath, Tar::Mode::Read);
                tar.extractAll(this->m_downloadPath.parent_path() / this->m_downloadPath.stem());
                EventManager::post<EventStoreContentDownloaded>(this->m_downloadPath.parent_path() / this->m_downloadPath.stem());
            } else {
                EventManager::post<EventStoreContentDownloaded>(this->m_downloadPath);
            }

            category.downloadCallback();
        } else
            log::error("Download failed! HTTP Code {}", response.getStatusCode());


        this->m_download = {};
    }

}