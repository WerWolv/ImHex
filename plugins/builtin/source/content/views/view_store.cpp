#include "content/views/view_store.hpp"
#include <hex/api/theme_manager.hpp>
#include <hex/api/achievement_manager.hpp>

#include <hex/api/content_registry/user_interface.hpp>
#include <hex/api/content_registry/settings.hpp>
#include <hex/api/events/events_interaction.hpp>

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

#include <wolv/io/file.hpp>

namespace hex::plugin::builtin {

    using namespace std::literals::chrono_literals;

    ViewStore::ViewStore() : View::Floating("hex.builtin.view.store.name", ICON_VS_EXTENSIONS) {
        ContentRegistry::UserInterface::addMenuItem({ "hex.builtin.menu.extras", "hex.builtin.view.store.name" }, ICON_VS_EXTENSIONS, 1000, Shortcut::None, [&, this] {
            if (m_requestStatus == RequestStatus::NotAttempted)
                this->requestStore(false);

            this->getWindowOpenState() = true;
        });

        addCategory("hex.builtin.view.store.tab.patterns",     "patterns",      &paths::Patterns);
        addCategory("hex.builtin.view.store.tab.includes",     "includes",      &paths::PatternsInclude);
        addCategory("hex.builtin.view.store.tab.magic",        "magic",         &paths::Magic, []{
            magic::compile();
        });
        addCategory("hex.builtin.view.store.tab.nodes",        "nodes",         &paths::Nodes);
        addCategory("hex.builtin.view.store.tab.encodings",    "encodings",     &paths::Encodings);
        addCategory("hex.builtin.view.store.tab.disassemblers","disassemblers", &paths::Disassemblers);
        addCategory("hex.builtin.view.store.tab.constants",    "constants",     &paths::Constants);
        addCategory("hex.builtin.view.store.tab.themes",       "themes",        &paths::Themes, [this]{
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
            const auto path = folder / std::fs::path(storeEntry.fileName);

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
            if (ImGui::BeginTable("##pattern_language", 4, ImGuiTableFlags_ScrollY | ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg)) {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn("hex.builtin.view.store.row.name"_lang, ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("hex.builtin.view.store.row.authors"_lang, ImGuiTableColumnFlags_WidthStretch, 0.3F);
                ImGui::TableSetupColumn("hex.builtin.view.store.row.description"_lang, ImGuiTableColumnFlags_WidthStretch, 0.7F);
                ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);

                ImGui::TableHeadersRow();

                u32 id = 1;
                for (auto &entry : category.entries) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(entry.name.c_str());

                    // The space makes a padding in the UI
                    ImGui::TableNextColumn();
                    ImGuiExt::TextFormatted("{} ", wolv::util::combineStrings(entry.authors, ", "));

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
                    ImGui::PushID(id);
                    ImGui::BeginDisabled(m_updateAllTask.isRunning() || (m_download.valid() && m_download.wait_for(0s) != std::future_status::ready));
                    {
                        if (entry.downloading) {
                            if (m_download.valid() && m_download.wait_for(0s) == std::future_status::ready) {
                                this->handleDownloadFinished(category, entry);
                            }

                        } else {
                            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));

                            if (entry.hasUpdate) {
                                if (ImGuiExt::DimmedIconButton(ICON_VS_DEBUG_RESTART, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
                                    entry.downloading = this->download(category.path, entry.fileName, entry.link);
                                }
                                ImGui::SetItemTooltip("%s", "hex.builtin.view.store.update"_lang.get());
                            } else if (entry.system) {
                                ImGui::BeginDisabled();
                                ImGuiExt::DimmedIconButton(ICON_VS_REMOVE, ImGui::GetStyleColorVec4(ImGuiCol_Text));
                                ImGui::EndDisabled();
                                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                                    ImGui::BeginTooltip();
                                    ImGui::TextUnformatted("hex.builtin.view.store.system.explanation"_lang);
                                    ImGui::EndTooltip();
                                }
                            } else if (!entry.installed) {
                                if (ImGuiExt::DimmedIconButton(ICON_VS_CLOUD_DOWNLOAD, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
                                    entry.downloading = this->download(category.path, entry.fileName, entry.link);
                                    AchievementManager::unlockAchievement("hex.builtin.achievement.misc", "hex.builtin.achievement.misc.download_from_store.name");
                                }
                                ImGui::SetItemTooltip("%s", "hex.builtin.view.store.download"_lang.get());
                            } else {
                                if (ImGuiExt::DimmedIconButton(ICON_VS_TRASH, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
                                    entry.installed = !this->remove(category.path, entry.fileName);
                                    // remove() will not update the entry to mark it as a system entry, so we do it manually
                                    updateEntryMetadata(entry, category);
                                }
                                ImGui::SetItemTooltip("%s", "hex.builtin.view.store.remove"_lang.get());
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
                } catch (const std::exception &e) {
                    log::error("Failed to parse store response: {}", e.what());
                    m_requestStatus = RequestStatus::Failed;
                    m_apiRequest = { };
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
        ImGuiExt::InfoTooltip(fmt::format("hex.builtin.view.store.update_count"_lang, m_updateCount.load()).c_str());

        ImGui::EndDisabled();

        if (ImGui::BeginTabBar("storeTabs")) {
            for (auto &category : m_categories) {
                this->drawTab(category);
            }

            ImGui::EndTabBar();
        }

    }

    void ViewStore::refresh() {
        this->requestStore(true);
    }

    void ViewStore::requestStore(bool forceRefresh) {
        // Do not refresh if a refresh is already in progress
        if (m_requestStatus == RequestStatus::InProgress)
            return;
        m_requestStatus = RequestStatus::InProgress;

        for (auto &category : m_categories) {
            category.entries.clear();
        }

        m_apiRequest = forceRefresh ? StoreApi::refresh() : StoreApi::get();
    }

    void ViewStore::parseResponse() {
        const auto &response = m_apiRequest.get();
        m_requestStatus = response.isSuccess() ? RequestStatus::Succeeded : RequestStatus::Failed;
        if (!response.isSuccess() && !response.getErrorMessage().empty())
            log::error("Failed to load store response: {}", response.getErrorMessage());

        if (m_requestStatus == RequestStatus::Succeeded) {
            for (auto &category : m_categories) {
                const auto &store = response.getData().categories;
                const auto entries = store.find(category.requestName);
                if (entries == store.end())
                    continue;

                for (const auto &entry : entries->second) {
                    StoreEntry storeEntry = { entry, false, false, false, false };
                    updateEntryMetadata(storeEntry, category);
                    category.entries.push_back(std::move(storeEntry));
                }
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
        m_download = StoreApi::download(pathType, fileName, url);
        return m_download.valid();
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
        m_updateAllTask = TaskManager::createBackgroundTask("hex.builtin.task.updating_store", [this](auto &task) {
            for (auto &category : m_categories) {
                for (auto &entry : category.entries) {
                    if (entry.hasUpdate) {
                        entry.downloading = this->download(category.path, entry.fileName, entry.link);
                        if (!m_download.valid())
                            continue;

                        while (m_download.valid() && m_download.wait_for(100ms) != std::future_status::ready) {
                            task.update();
                        }

                        this->handleDownloadFinished(category, entry);

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

        const auto response = m_download.get();
        if (response.isSuccess()) {
            m_downloadPath = response.getPath();

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
            if (response.getStatus() == StoreApi::DownloadResult::Status::NoWritablePath)
                ui::ToastError::open("hex.builtin.view.store.download_error"_lang);
            else
                log::error("Download failed! {}", response.getErrorMessage());
        }

        m_download = {};
    }

    void ViewStore::drawHelpText() {
        ImGuiExt::TextFormattedWrapped("This view lets you download and update additional content for ImHex, such as pattern files, magic files, themes and more. All content is provided by the ImHex community and can be freely used within ImHex.");
    }

}
