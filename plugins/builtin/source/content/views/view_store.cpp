#include "content/views/view_store.hpp"

#include <hex/api/content_registry.hpp>

#include <imgui.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>

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
        ContentRegistry::Interface::addMenuItemSeparator({ "hex.builtin.menu.help" }, 2000);

        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.help", "hex.builtin.view.store.name" }, 2050, Shortcut::None, [&, this] {
            if (this->m_requestStatus == RequestStatus::NotAttempted)
                this->refresh();
            TaskManager::doLater([] { ImGui::OpenPopup(View::toWindowName("hex.builtin.view.store.name").c_str()); });
            this->getWindowOpenState() = true;
        });

        this->m_httpRequest.setTimeout(30'0000);
    }

    void ViewStore::drawStore() {
        ImGui::Header("hex.builtin.view.store.desc"_lang, true);

        if (ImGui::Button("hex.builtin.view.store.reload"_lang)) {
            this->refresh();
        }

        auto drawTab = [this](auto title, fs::ImHexPath pathType, auto &content, const std::function<void()> &downloadDoneCallback = []{}) {
            if (ImGui::BeginTabItem(title)) {
                if (ImGui::BeginTable("##pattern_language", 3, ImGuiTableFlags_ScrollY | ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_RowBg)) {
                    ImGui::TableSetupScrollFreeze(0, 1);
                    ImGui::TableSetupColumn("hex.builtin.view.store.row.name"_lang, ImGuiTableColumnFlags_WidthFixed);
                    ImGui::TableSetupColumn("hex.builtin.view.store.row.description"_lang, ImGuiTableColumnFlags_None);
                    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);

                    ImGui::TableHeadersRow();

                    u32 id = 1;
                    for (auto &entry : content) {
                        ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetTextLineHeight() + 4.0F * ImGui::GetStyle().FramePadding.y);
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(entry.name.c_str());
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(entry.description.c_str());
                        ImGui::TableNextColumn();

                        ImGui::PushID(id);
                        ImGui::BeginDisabled(this->m_download.valid() && this->m_download.wait_for(0s) != std::future_status::ready);
                        {
                            if (entry.downloading) {
                                ImGui::TextSpinner("");

                                if (this->m_download.valid() && this->m_download.wait_for(0s) == std::future_status::ready) {
                                    entry.downloading = false;

                                    auto response = this->m_download.get();
                                    if (response.isSuccess()) {
                                        entry.installed = true;
                                        entry.hasUpdate = false;

                                        if (entry.isFolder) {
                                            Tar tar(this->m_downloadPath, Tar::Mode::Read);
                                            tar.extractAll(this->m_downloadPath.parent_path() / this->m_downloadPath.stem());
                                        }

                                        downloadDoneCallback();
                                    } else
                                        log::error("Download failed! HTTP Code {}", response.getStatusCode());


                                    this->m_download = {};
                                }

                            } else if (entry.hasUpdate) {
                                if (ImGui::Button("hex.builtin.view.store.update"_lang)) {
                                    entry.downloading = this->download(pathType, entry.fileName, entry.link, true);
                                }
                            } else if (!entry.installed) {
                                if (ImGui::Button("hex.builtin.view.store.download"_lang)) {
                                    entry.downloading = this->download(pathType, entry.fileName, entry.link, false);
                                }
                            } else {
                                if (ImGui::Button("hex.builtin.view.store.remove"_lang)) {
                                    entry.installed = !this->remove(pathType, entry.fileName);
                                }
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
        };

        if (ImGui::BeginTabBar("storeTabs")) {
            drawTab("hex.builtin.view.store.tab.patterns"_lang,     fs::ImHexPath::Patterns,        this->m_patterns);
            drawTab("hex.builtin.view.store.tab.libraries"_lang,    fs::ImHexPath::PatternsInclude, this->m_includes);
            drawTab("hex.builtin.view.store.tab.magics"_lang,       fs::ImHexPath::Magic,           this->m_magics, magic::compile);
            drawTab("hex.builtin.view.store.tab.constants"_lang,    fs::ImHexPath::Constants,       this->m_constants);
            drawTab("hex.builtin.view.store.tab.encodings"_lang,    fs::ImHexPath::Encodings,       this->m_encodings);
            drawTab("hex.builtin.view.store.tab.yara"_lang,         fs::ImHexPath::Yara,            this->m_yara);
            drawTab("hex.builtin.view.store.tab.nodes"_lang,        fs::ImHexPath::Nodes,           this->m_nodes);
            drawTab("hex.builtin.view.store.tab.themes"_lang,       fs::ImHexPath::Themes,          this->m_themes);

            ImGui::EndTabBar();
        }
    }

    void ViewStore::refresh() {
        // do not refresh if a refresh is already in progress
        if (this->m_requestStatus == RequestStatus::InProgress)
            return;
        this->m_requestStatus = RequestStatus::InProgress;

        this->m_patterns.clear();
        this->m_includes.clear();
        this->m_magics.clear();
        this->m_constants.clear();
        this->m_yara.clear();
        this->m_encodings.clear();
        this->m_nodes.clear();
        this->m_themes.clear();

        this->m_httpRequest.setUrl(ImHexApiURL + "/store"s);
        this->m_apiRequest = this->m_httpRequest.execute();
    }

    void ViewStore::parseResponse() {
        auto response = this->m_apiRequest.get();
        this->m_requestStatus = response.isSuccess() ? RequestStatus::Succeeded : RequestStatus::Failed;
        if (this->m_requestStatus == RequestStatus::Succeeded) {
            auto json = nlohmann::json::parse(response.getData());

            auto parseStoreEntries = [](auto storeJson, const std::string &name, fs::ImHexPath pathType, std::vector<StoreEntry> &results) {
                // Check if the response handles the type of files
                if (storeJson.contains(name)) {

                    for (auto &entry : storeJson[name]) {

                        // Check if entry is valid
                        if (entry.contains("name") && entry.contains("desc") && entry.contains("file") && entry.contains("url") && entry.contains("hash") && entry.contains("folder")) {

                            // Parse entry
                            StoreEntry storeEntry = { entry["name"], entry["desc"], entry["file"], entry["url"], entry["hash"], entry["folder"], false, false, false };

                            // Check if file is installed already or has an update available
                            for (const auto &folder : fs::getDefaultPaths(pathType)) {

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

                            results.push_back(storeEntry);
                        }
                    }
                }
            };

            parseStoreEntries(json, "patterns", fs::ImHexPath::Patterns, this->m_patterns);
            parseStoreEntries(json, "includes", fs::ImHexPath::PatternsInclude, this->m_includes);
            parseStoreEntries(json, "magic", fs::ImHexPath::Magic, this->m_magics);
            parseStoreEntries(json, "constants", fs::ImHexPath::Constants, this->m_constants);
            parseStoreEntries(json, "yara", fs::ImHexPath::Yara, this->m_yara);
            parseStoreEntries(json, "encodings", fs::ImHexPath::Encodings, this->m_encodings);
            parseStoreEntries(json, "nodes", fs::ImHexPath::Nodes, this->m_nodes);
            parseStoreEntries(json, "themes", fs::ImHexPath::Themes, this->m_themes);
        }
        this->m_apiRequest = {};
    }

    void ViewStore::drawContent() {
        if (ImGui::BeginPopupModal(View::toWindowName("hex.builtin.view.store.name").c_str(), &this->getWindowOpenState())) {
            if (this->m_apiRequest.valid()) {
                if (this->m_apiRequest.wait_for(0s) != std::future_status::ready)
                    ImGui::TextSpinner("hex.builtin.view.store.loading"_lang);
                else
                    this->parseResponse();
            }

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
        for (const auto &path : fs::getDefaultPaths(pathType)) {
            if (!fs::isPathWritable(path))
                continue;

            auto fullPath = path / std::fs::path(fileName);

            if (!update || wolv::io::fs::exists(fullPath)) {
                downloading          = true;
                this->m_downloadPath = fullPath;

                this->m_httpRequest.setUrl(url);
                this->m_download     = this->m_httpRequest.downloadFile(fullPath);
                break;
            }
        }

        if (!downloading) {
            View::showErrorPopup("hex.builtin.view.store.download_error"_lang);
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
        }

        return removed;
    }

}