#include "content/views/view_store.hpp"
#include <hex/api/theme_manager.hpp>
#include <hex/api/achievement_manager.hpp>
#include <hex/api_urls.hpp>

#include <hex/api/content_registry.hpp>

#include <popups/popup_notification.hpp>
#include <toasts/toast_notification.hpp>

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

    ViewStore::ViewStore() : View::Floating("hex.builtin.view.store.name") {
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.extras", "hex.builtin.view.store.name" }, ICON_VS_GLOBE, 1000, Shortcut::None, [&, this] {
            if (m_requestStatus == RequestStatus::NotAttempted)
                this->refresh();

            this->getWindowOpenState() = true;
        });

        m_httpRequest.setTimeout(30'0000);

        addCategory("hex.builtin.view.store.tab.patterns",     "patterns",     &paths::Patterns);
        addCategory("hex.builtin.view.store.tab.includes",     "includes",     &paths::PatternsInclude);
        addCategory("hex.builtin.view.store.tab.magic",        "magic",        &paths::Magic, []{
            magic::compile();
        });
        addCategory("hex.builtin.view.store.tab.nodes",        "nodes",        &paths::Nodes);
        addCategory("hex.builtin.view.store.tab.encodings",    "encodings",    &paths::Encodings);
        addCategory("hex.builtin.view.store.tab.constants",    "constants",    &paths::Constants);
        addCategory("hex.builtin.view.store.tab.themes",       "themes",       &paths::Themes, [this]{
            auto themeFile = wolv::io::File(m_downloadPath, wolv::io::File::Mode::Read);

            ThemeManager::addTheme(themeFile.readString());
        });
        addCategory("hex.builtin.view.store.tab.yara",         "yara",         &paths::Yara);

        TaskManager::doLater([this] {
            // Force update all installed items after an update so that there's no old and incompatible versions around anymore
            {
                const auto prevUpdateVersion = ContentRegistry::Settings::read<std::string>("hex.builtin.setting.general", "hex.builtin.setting.general.prev_launch_version", "");
                if (SemanticVersion(prevUpdateVersion) != ImHexApi::System::getImHexVersion()) {
                    updateAll();
                }
            }
        });
    }


    void updateEntryMetadata(StoreEntry &storeEntry, const StoreCategory &category) {
        // Check if file is installed already or has an update available
        for (const auto &folder : category.path->write()) {
            auto path = folder / std::fs::path(storeEntry.fileName);

            if (wolv::io::fs::exists(path)) {
                storeEntry.installed = true;

                wolv::io::File file(path, wolv::io::File::Mode::Read);
                auto bytes = file.readVector();

                auto fileHash = crypt::sha256(bytes);

                // Compare installed file hash with hash of repo file
                if (std::vector(fileHash.begin(), fileHash.end()) != crypt::decode16(storeEntry.hash))
                    storeEntry.hasUpdate = true;

                storeEntry.system = !fs::isPathWritable(folder);
                return;
            }
        }

        storeEntry.installed = false;
        storeEntry.hasUpdate = false;
        storeEntry.system = false;
    }

    void ViewStore::drawTab(hex::plugin::builtin::StoreCategory &category) {
        if (ImGui::BeginTabItem(Lang(category.unlocalizedName))) {
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
                    // The space makes a padding in the UI
                    ImGuiExt::TextFormatted("{} ", wolv::util::combineStrings(entry.authors, ", "));
                    ImGui::TableNextColumn();

                    const auto buttonSize = ImVec2(100_scaled, ImGui::GetTextLineHeightWithSpacing());

                    ImGui::PushID(id);
                    ImGui::BeginDisabled(m_updateAllTask.isRunning() || (m_download.valid() && m_download.wait_for(0s) != std::future_status::ready));
                    {
                        if (entry.downloading) {
                            ImGui::ProgressBar(m_httpRequest.getProgress(), buttonSize, "");

                            if (m_download.valid() && m_download.wait_for(0s) == std::future_status::ready) {
                                this->handleDownloadFinished(category, entry);
                            }

                        } else {
                            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));

                            if (entry.hasUpdate) {
                                if (ImGui::Button("hex.builtin.view.store.update"_lang, buttonSize)) {
                                    entry.downloading = this->download(category.path, entry.fileName, entry.link);
                                }
                            } else if (entry.system) {
                                ImGui::BeginDisabled();
                                ImGui::Button("hex.builtin.view.store.system"_lang, buttonSize);
                                ImGui::EndDisabled();
                                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                                    ImGui::BeginTooltip();
                                    ImGui::TextUnformatted("hex.builtin.view.store.system.explanation"_lang);
                                    ImGui::EndTooltip();
                                }
                            } else if (!entry.installed) {
                                if (ImGui::Button("hex.builtin.view.store.download"_lang, buttonSize)) {
                                    entry.downloading = this->download(category.path, entry.fileName, entry.link);
                                    AchievementManager::unlockAchievement("hex.builtin.achievement.misc", "hex.builtin.achievement.misc.download_from_store.name");
                                }
                            } else {
                                if (ImGui::Button("hex.builtin.view.store.remove"_lang, buttonSize)) {
                                    entry.installed = !this->remove(category.path, entry.fileName);
                                    // remove() will not update the entry to mark it as a system entry, so we do it manually
                                    updateEntryMetadata(entry, category);
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
        ImGuiExt::Header("hex.builtin.view.store.desc"_lang, true);

        bool reloading = false;
        if (m_apiRequest.valid()) {
            if (m_apiRequest.wait_for(0s) != std::future_status::ready)
                reloading = true;
            else {
                try {
                    this->parseResponse();
                } catch (nlohmann::json::exception &e) {
                    log::error("Failed to parse store response: {}", e.what());
                }
            }
        }

        ImGui::BeginDisabled(reloading);
        if (ImGui::Button("hex.builtin.view.store.reload"_lang)) {
            this->refresh();
        }
        ImGui::EndDisabled();

        if (reloading) {
            ImGui::SameLine();
            ImGuiExt::TextSpinner("hex.builtin.view.store.loading"_lang);
        }

        // Align the button to the right
        ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::GetCursorPosX() - 25_scaled);
        ImGui::BeginDisabled(m_updateAllTask.isRunning() || m_updateCount == 0);
        if (ImGuiExt::IconButton(ICON_VS_CLOUD_DOWNLOAD, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
            this->updateAll();
        }
        ImGuiExt::InfoTooltip(hex::format("hex.builtin.view.store.update_count"_lang, m_updateCount.load()).c_str());

        ImGui::EndDisabled();

        if (ImGui::BeginTabBar("storeTabs")) {
            for (auto &category : m_categories) {
                this->drawTab(category);
            }

            ImGui::EndTabBar();
        }

    }

    void ViewStore::refresh() {
        // Do not refresh if a refresh is already in progress
        if (m_requestStatus == RequestStatus::InProgress)
            return;
        m_requestStatus = RequestStatus::InProgress;

        for (auto &category : m_categories) {
            category.entries.clear();
        }

        m_httpRequest.setUrl(ImHexApiURL + "/store"s);
        m_apiRequest = m_httpRequest.execute();
    }

    void ViewStore::parseResponse() {
        auto response = m_apiRequest.get();
        m_requestStatus = response.isSuccess() ? RequestStatus::Succeeded : RequestStatus::Failed;
        if (m_requestStatus == RequestStatus::Succeeded) {
            auto json = nlohmann::json::parse(response.getData());

            auto parseStoreEntries = [](auto storeJson, StoreCategory &category) {
                // Check if the response handles the type of files
                if (storeJson.contains(category.requestName)) {

                    for (auto &entry : storeJson[category.requestName]) {

                        // Check if entry is valid
                        if (entry.contains("name") && entry.contains("desc") && entry.contains("authors") && entry.contains("file") && entry.contains("url") && entry.contains("hash") && entry.contains("folder")) {

                            // Parse entry
                            StoreEntry storeEntry = { entry["name"], entry["desc"], entry["authors"], entry["file"], entry["url"], entry["hash"], entry["folder"], false, false, false, false };

                            updateEntryMetadata(storeEntry, category);
                            category.entries.push_back(storeEntry);
                        }
                    }
                }

                std::sort(category.entries.begin(), category.entries.end(), [](const auto &lhs, const auto &rhs) {
                    return lhs.name < rhs.name;
                });
            };

            for (auto &category : m_categories) {
                parseStoreEntries(json, category);
            }

            m_updateCount = 0;
            for (auto &category : m_categories) {
                for (auto &entry : category.entries) {
                    if (entry.hasUpdate)
                        m_updateCount += 1;
                }
            }
        }
        m_apiRequest = {};
    }

    void ViewStore::drawContent() {
        if (m_requestStatus == RequestStatus::Failed)
            ImGuiExt::TextFormattedColored(ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_ToolbarRed), "hex.builtin.view.store.netfailed"_lang);

        this->drawStore();
    }

    bool ViewStore::download(const paths::impl::DefaultPath *pathType, const std::string &fileName, const std::string &url) {
        bool downloading = false;
        for (const auto &folderPath : pathType->write()) {
            if (!fs::isPathWritable(folderPath))
                continue;

            // Verify that we write the file to the right folder
            // this is to prevent the filename from having elements like ../
            auto fullPath = std::fs::weakly_canonical(folderPath / std::fs::path(fileName));
            auto [folderIter, pathIter] = std::mismatch(folderPath.begin(), folderPath.end(), fullPath.begin());
            if (folderIter != folderPath.end()) {
                log::warn("The destination file name '{}' is invalid", fileName);
                return false;
            }

            downloading = true;
            m_downloadPath = fullPath;

            m_httpRequest.setUrl(url);
            m_download = m_httpRequest.downloadFile(fullPath);
            break;
        }

        if (!downloading) {
            ui::ToastError::open("hex.builtin.view.store.download_error"_lang);
            return false;
        }

        return downloading;
    }

    bool ViewStore::remove(const paths::impl::DefaultPath *pathType, const std::string &fileName) {
        bool removed = true;
        for (const auto &path : pathType->write()) {
            const auto filePath = path / fileName;
            const auto folderPath = (path / std::fs::path(fileName).stem());

            wolv::io::fs::remove(filePath);
            wolv::io::fs::removeAll(folderPath);

            removed = removed && !wolv::io::fs::exists(filePath) && !wolv::io::fs::exists(folderPath);
            EventStoreContentRemoved::post(filePath);
        }

        return removed;
    }

    void ViewStore::updateAll() {
        m_updateAllTask = TaskManager::createTask("hex.builtin.task.updating_store"_lang, m_updateCount, [this](auto &task) {
            for (auto &category : m_categories) {
                for (auto &entry : category.entries) {
                    if (entry.hasUpdate) {
                        entry.downloading = this->download(category.path, entry.fileName, entry.link);
                        if (!m_download.valid())
                            continue;

                        m_download.wait();

                        while (m_download.valid() && m_download.wait_for(100ms) != std::future_status::ready) {
                            task.update();
                        }

                        entry.hasUpdate = false;
                        entry.downloading = false;

                        task.increment();
                    }
                }
            }

            TaskManager::doLater([] {
                ContentRegistry::Settings::write<std::string>("hex.builtin.setting.general", "hex.builtin.setting.general.prev_launch_version", ImHexApi::System::getImHexVersion().get(false));
            });
        });
    }


    void ViewStore::addCategory(const UnlocalizedString &unlocalizedName, const std::string &requestName, const paths::impl::DefaultPath *path, std::function<void()> downloadCallback) {
        m_categories.push_back({ unlocalizedName, requestName, path, { }, std::move(downloadCallback) });
    }

    void ViewStore::handleDownloadFinished(const StoreCategory &category, StoreEntry &entry) {
        entry.downloading = false;

        auto response = m_download.get();
        if (response.isSuccess()) {
            if (entry.hasUpdate)
                m_updateCount -= 1;

            entry.installed = true;
            entry.hasUpdate = false;
            entry.system = false;

            if (entry.isFolder) {
                Tar tar(m_downloadPath, Tar::Mode::Read);
                tar.extractAll(m_downloadPath.parent_path() / m_downloadPath.stem());
                EventStoreContentDownloaded::post(m_downloadPath.parent_path() / m_downloadPath.stem());
            } else {
                EventStoreContentDownloaded::post(m_downloadPath);
            }

            category.downloadCallback();
        } else {
            log::error("Download failed! HTTP Code {}", response.getStatusCode());
        }

        m_download = {};
    }

}