#include "views/view_store.hpp"

#include <imgui.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>

#include <hex/helpers/utils.hpp>
#include <hex/helpers/crypto.hpp>
#include <hex/helpers/logger.hpp>
#include <hex/helpers/magic.hpp>

#include <fstream>
#include <filesystem>
#include <functional>
#include <nlohmann/json.hpp>

namespace hex {

    using namespace std::literals::string_literals;
    using namespace std::literals::chrono_literals;
    
    namespace fs = std::filesystem;

    ViewStore::ViewStore() : View("hex.view.store.name") {
        this->refresh();
    }

    ViewStore::~ViewStore() { }

    void ViewStore::drawStore() {
        ImGui::Header("hex.view.store.desc"_lang, true);

        if (ImGui::Button("hex.view.store.reload"_lang)) {
            this->refresh();
        }

        auto drawTab = [this](auto title, ImHexPath pathType, auto &content, std::function<void()> downloadDoneCallback) {
            if (ImGui::BeginTabItem(title)) {
                if (ImGui::BeginTable("##pattern_language", 3, ImGuiTableFlags_ScrollY | ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_RowBg)) {
                    ImGui::TableSetupScrollFreeze(0, 1);
                    ImGui::TableSetupColumn("hex.view.store.name"_lang, ImGuiTableColumnFlags_WidthFixed);
                    ImGui::TableSetupColumn("hex.view.store.description"_lang, ImGuiTableColumnFlags_None);
                    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);

                    ImGui::TableHeadersRow();

                    for (auto &entry : content) {
                        ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetTextLineHeight() + 4.0F * ImGui::GetStyle().FramePadding.y);
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(entry.name.c_str());
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(entry.description.c_str());
                        ImGui::TableNextColumn();

                        ImGui::BeginDisabled(this->m_download.valid() && this->m_download.wait_for(0s) != std::future_status::ready);
                        {
                            if (entry.downloading) {
                                ImGui::TextSpinner("");

                                if (this->m_download.valid() && this->m_download.wait_for(0s) == std::future_status::ready) {
                                    entry.downloading = false;

                                    auto response = this->m_download.get();
                                    if (response.code == 200) {
                                       entry.installed = true;
                                       entry.hasUpdate = false;
                                       downloadDoneCallback();
                                    } else
                                        log::error("Download failed!");


                                    this->m_download = { };
                                }

                            } else if (entry.hasUpdate) {
                                if (ImGui::Button("hex.view.store.update"_lang)) {
                                    this->download(pathType, entry.fileName, entry.link, true);
                                    entry.downloading = true;
                                }
                            } else if (!entry.installed) {
                                if (ImGui::Button("hex.view.store.download"_lang)) {
                                    this->download(pathType, entry.fileName, entry.link, false);
                                    entry.downloading = true;
                                }
                            } else {
                                if (ImGui::Button("hex.view.store.remove"_lang)) {
                                    this->remove(pathType, entry.fileName);
                                    entry.installed = false;
                                }
                            }
                        }
                        ImGui::EndDisabled();
                    }

                    ImGui::EndTable();
                }
                ImGui::EndTabItem();
            }
        };

        if (ImGui::BeginTabBar("storeTabs")) {
            drawTab("hex.view.store.tab.patterns"_lang, ImHexPath::Patterns, this->m_patterns, []{});
            drawTab("hex.view.store.tab.libraries"_lang, ImHexPath::PatternsInclude, this->m_includes, []{});
            drawTab("hex.view.store.tab.magics"_lang, ImHexPath::Magic, this->m_magics, []{
                magic::compile();
            });

            ImGui::EndTabBar();
        }
    }

    void ViewStore::refresh() {
        this->m_patterns.clear();
        this->m_includes.clear();
        this->m_magics.clear();

        this->m_apiRequest = this->m_net.getString(ImHexApiURL + "/store"s);
    }

    void ViewStore::parseResponse() {
        auto response = this->m_apiRequest.get();
        if (response.code == 200) {
            auto json = nlohmann::json::parse(response.body);

            auto parseStoreEntries = [](auto storeJson, const std::string &name, ImHexPath pathType, std::vector<StoreEntry> &results) {
                // Check if the response handles the type of files
                if (storeJson.contains(name)) {

                    for (auto &entry : storeJson[name]) {

                        // Check if entry is valid
                        if (entry.contains("name") && entry.contains("desc") && entry.contains("file") && entry.contains("url") && entry.contains("hash")) {

                            // Parse entry
                            StoreEntry storeEntry = { entry["name"], entry["desc"], entry["file"], entry["url"], entry["hash"], false, false, false };

                            // Check if file is installed already or has an update available
                            for (auto folder : hex::getPath(pathType)) {

                                auto path = folder / fs::path(storeEntry.fileName);

                                if (fs::exists(path)) {
                                    storeEntry.installed = true;

                                    std::ifstream file(path, std::ios::in | std::ios::binary);
                                    std::vector<u8> data(fs::file_size(path), 0x00);
                                    file.read(reinterpret_cast<char*>(data.data()), data.size());

                                    auto fileHash = crypt::sha256(data);

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

            parseStoreEntries(json, "patterns", ImHexPath::Patterns, this->m_patterns);
            parseStoreEntries(json, "includes", ImHexPath::PatternsInclude, this->m_includes);
            parseStoreEntries(json, "magic", ImHexPath::Magic, this->m_magics);
        }
        this->m_apiRequest = { };

    }

    void ViewStore::drawContent() {
        ImGui::SetNextWindowSizeConstraints(ImVec2(600, 400) * SharedData::globalScale, ImVec2(FLT_MAX, FLT_MAX));
        if (ImGui::BeginPopupModal(View::toWindowName("hex.view.store.name").c_str(), &this->getWindowOpenState(), ImGuiWindowFlags_AlwaysAutoResize)) {
            if (this->m_apiRequest.valid()) {
                if (this->m_apiRequest.wait_for(0s) != std::future_status::ready)
                    ImGui::TextSpinner("hex.view.store.loading"_lang);
                else
                    this->parseResponse();
            }

            this->drawStore();

            ImGui::EndPopup();
        } else {
            this->getWindowOpenState() = false;
        }
    }

    void ViewStore::drawMenu() {
        if (ImGui::BeginMenu("hex.menu.help"_lang)) {
            if (ImGui::MenuItem("hex.view.store.name"_lang)) {
                View::doLater([]{ ImGui::OpenPopup(View::toWindowName("hex.view.store.name").c_str()); });
                this->getWindowOpenState() = true;
            }

            ImGui::EndMenu();
        }
    }


    void ViewStore::download(ImHexPath pathType, const std::string &fileName, const std::string &url, bool update) {
        if (!update) {
            this->m_download = this->m_net.downloadFile(url, hex::getPath(pathType).front() / fs::path(fileName));
        } else {
            for (const auto &path : hex::getPath(pathType)) {
                auto fullPath = path / fs::path(fileName);
                if (fs::exists(fullPath)) {
                    this->m_download = this->m_net.downloadFile(url, fullPath);
                }
            }
        }
    }
    
    void ViewStore::remove(ImHexPath pathType, const std::string &fileName) {
        for (const auto &path : hex::getPath(pathType))
            fs::remove(path / fs::path(fileName));
    }

}