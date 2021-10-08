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
#include <microtar.h>
#include <hex/helpers/file.hpp>

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

        auto drawTab = [this](auto title, ImHexPath pathType, auto &content, std::function<void(const StoreEntry&)> downloadDoneCallback) {
            if (ImGui::BeginTabItem(title)) {
                if (ImGui::BeginTable("##pattern_language", 3, ImGuiTableFlags_ScrollY | ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_RowBg)) {
                    ImGui::TableSetupScrollFreeze(0, 1);
                    ImGui::TableSetupColumn("hex.view.store.row.name"_lang, ImGuiTableColumnFlags_WidthFixed);
                    ImGui::TableSetupColumn("hex.view.store.row.description"_lang, ImGuiTableColumnFlags_None);
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
                                    if (response.code == 200) {
                                       entry.installed = true;
                                       entry.hasUpdate = false;

                                       if (entry.isFolder) {
                                           mtar_t ctx;
                                           mtar_open(&ctx, this->m_downloadPath.string().c_str(), "r");

                                           mtar_header_t header;
                                           auto extractBasePath = this->m_downloadPath.parent_path() / this->m_downloadPath.stem();
                                           while (mtar_read_header(&ctx, &header) != MTAR_ENULLRECORD) {
                                               auto filePath = extractBasePath / fs::path(header.name);
                                               fs::create_directories(filePath.parent_path());
                                               File outputFile(filePath.string(), File::Mode::Create);

                                               std::vector<u8> buffer(0x10000);
                                               const unsigned long long bufferSize = buffer.size();

                                               for (u64 offset = 0; offset < header.size; offset += bufferSize) {
                                                   auto readSize = std::min(bufferSize, header.size - offset);
                                                   mtar_read_data(&ctx, buffer.data(), readSize);

                                                   buffer.resize(readSize);
                                                   outputFile.write(buffer);
                                               }
                                               mtar_next(&ctx);
                                           }

                                           mtar_finalize(&ctx);
                                           mtar_close(&ctx);
                                       }

                                       downloadDoneCallback(entry);
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
                        ImGui::PopID();
                        id++;
                    }

                    ImGui::EndTable();
                }
                ImGui::EndTabItem();
            }
        };

        if (ImGui::BeginTabBar("storeTabs")) {
            auto extractTar = []{

            };

            drawTab("hex.view.store.tab.patterns"_lang, ImHexPath::Patterns, this->m_patterns, [](auto){});
            drawTab("hex.view.store.tab.libraries"_lang, ImHexPath::PatternsInclude, this->m_includes, [](auto){});
            drawTab("hex.view.store.tab.magics"_lang, ImHexPath::Magic, this->m_magics, [](auto){
                magic::compile();
            });
            drawTab("hex.view.store.tab.constants"_lang, ImHexPath::Constants, this->m_constants, [](auto){});
            drawTab("hex.view.store.tab.yara"_lang, ImHexPath::Yara, this->m_yara, [](auto){});

            ImGui::EndTabBar();
        }
    }

    void ViewStore::refresh() {
        this->m_patterns.clear();
        this->m_includes.clear();
        this->m_magics.clear();
        this->m_constants.clear();
        this->m_yara.clear();

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
                        if (entry.contains("name") && entry.contains("desc") && entry.contains("file") && entry.contains("url") && entry.contains("hash") && entry.contains("folder")) {

                            // Parse entry
                            StoreEntry storeEntry = { entry["name"], entry["desc"], entry["file"], entry["url"], entry["hash"], entry["folder"],false, false, false };

                            // Check if file is installed already or has an update available
                            for (const auto &folder : hex::getPath(pathType)) {

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
            parseStoreEntries(json, "constants", ImHexPath::Constants, this->m_constants);
            parseStoreEntries(json, "yara", ImHexPath::Yara, this->m_yara);

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
            this->m_downloadPath = hex::getPath(pathType).front() / fs::path(fileName);
            this->m_download = this->m_net.downloadFile(url, this->m_downloadPath);
        } else {
            for (const auto &path : hex::getPath(pathType)) {
                auto fullPath = path / fs::path(fileName);
                if (fs::exists(fullPath)) {
                    this->m_downloadPath = fullPath;
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