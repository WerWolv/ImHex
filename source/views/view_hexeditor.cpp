#include "views/view_hexeditor.hpp"

#include <hex/providers/provider.hpp>
#include <hex/api/imhex_api.hpp>
#include <hex/helpers/crypto.hpp>

#include <GLFW/glfw3.h>

#include "providers/file_provider.hpp"
#include "helpers/patches.hpp"
#include "helpers/project_file_handler.hpp"
#include "helpers/loader_script_handler.hpp"

#undef __STRICT_ANSI__
#include <cstdio>

namespace hex {

    ViewHexEditor::ViewHexEditor(std::vector<lang::PatternData*> &patternData)
            : View("hex.view.hexeditor.name"_lang), m_patternData(patternData) {

        this->m_searchStringBuffer.resize(0xFFF, 0x00);
        this->m_searchHexBuffer.resize(0xFFF, 0x00);

        this->m_memoryEditor.ReadFn = [](const ImU8 *data, size_t off) -> ImU8 {
            auto provider = SharedData::currentProvider;
            if (!provider->isAvailable() || !provider->isReadable())
                return 0x00;

            ImU8 byte;
            provider->read(off, &byte, sizeof(ImU8));

            for (auto &overlay : SharedData::currentProvider->getOverlays()) {
                auto overlayAddress = overlay->getAddress();
                if (off >= overlayAddress && off < overlayAddress + overlay->getSize())
                    byte = overlay->getData()[off - overlayAddress];
            }

            return byte;
        };

        this->m_memoryEditor.WriteFn = [](ImU8 *data, size_t off, ImU8 d) -> void {
            auto provider = SharedData::currentProvider;
            if (!provider->isAvailable() || !provider->isWritable())
                return;

            provider->write(off, &d, sizeof(ImU8));
            View::postEvent(Events::DataChanged);
            ProjectFile::markDirty();
        };

        this->m_memoryEditor.HighlightFn = [](const ImU8 *data, size_t off, bool next) -> bool {
            ViewHexEditor *_this = (ViewHexEditor *) data;

            std::optional<u32> currColor, prevColor;

            off += SharedData::currentProvider->getBaseAddress();

            for (const auto &[region, name, comment, color, locked] : ImHexApi::Bookmarks::getEntries()) {
                if (off >= region.address && off < (region.address + region.size))
                    currColor = (color & 0x00FFFFFF) | 0x80000000;
                if ((off - 1) >= region.address && (off - 1) < (region.address + region.size))
                    prevColor = (color & 0x00FFFFFF) | 0x80000000;
            }

            if (_this->m_highlightedBytes.contains(off)) {
                auto color = (_this->m_highlightedBytes[off] & 0x00FFFFFF) | 0x80000000;
                currColor = currColor.has_value() ? ImAlphaBlendColors(color, currColor.value()) : color;
            }
            if (_this->m_highlightedBytes.contains(off - 1)) {
                auto color = (_this->m_highlightedBytes[off - 1] & 0x00FFFFFF) | 0x80000000;
                prevColor = prevColor.has_value() ? ImAlphaBlendColors(color, prevColor.value()) : color;
            }

            if (next && prevColor != currColor) {
                return false;
            }

            if (currColor.has_value() && (currColor.value() & 0x00FFFFFF) != 0x00) {
                _this->m_memoryEditor.HighlightColor = (currColor.value() & 0x00FFFFFF) | 0x40000000;
                return true;
            }

            _this->m_memoryEditor.HighlightColor = 0x60C08080;
            return false;
        };

        this->m_memoryEditor.HoverFn = [](const ImU8 *data, size_t off) {
            bool tooltipShown = false;

            off += SharedData::currentProvider->getBaseAddress();

            for (const auto &[region, name, comment, color, locked] : ImHexApi::Bookmarks::getEntries()) {
                if (off >= region.address && off < (region.address + region.size)) {
                    if (!tooltipShown) {
                        ImGui::BeginTooltip();
                        tooltipShown = true;
                    }
                    ImGui::ColorButton(name.data(), ImColor(color).Value);
                    ImGui::SameLine(0, 10);
                    ImGui::TextUnformatted(name.data());
                }
            }

            if (tooltipShown)
                ImGui::EndTooltip();
        };

        this->m_memoryEditor.DecodeFn = [](const ImU8 *data, size_t addr) -> MemoryEditor::DecodeData {
            ViewHexEditor *_this = (ViewHexEditor *) data;

            if (_this->m_currEncodingFile.getLongestSequence() == 0)
                return { ".", 1, 0xFFFF8000 };

            auto &provider = SharedData::currentProvider;
            size_t size = std::min<size_t>(_this->m_currEncodingFile.getLongestSequence(), provider->getActualSize() - addr);

            std::vector<u8> buffer(size);
            provider->read(addr, buffer.data(), size);

            auto [decoded, advance] = _this->m_currEncodingFile.getEncodingFor(buffer);

            ImColor color;
            if (decoded.length() == 1 && std::isalnum(decoded[0])) color = 0xFFFF8000;
            else if (decoded.length() == 1 && advance == 1) color = 0xFF0000FF;
            else if (decoded.length() > 1 && advance == 1) color = 0xFF00FFFF;
            else if (advance > 1) color = 0xFFFFFFFF;
            else color = 0xFFFF8000;

            return { std::string(decoded), advance, color };
        };

        View::subscribeEvent(Events::FileDropped, [this](auto userData) {
            auto filePath = std::any_cast<const char*>(userData);

            if (filePath != nullptr)
                this->openFile(filePath);
        });

        View::subscribeEvent(Events::SelectionChangeRequest, [this](auto userData) {
            const Region &region = std::any_cast<Region>(userData);

            auto provider = SharedData::currentProvider;
            auto page = provider->getPageOfAddress(region.address);
            if (!page.has_value())
                return;

            provider->setCurrentPage(page.value());
            this->m_memoryEditor.GotoAddr = region.address;
            this->m_memoryEditor.DataPreviewAddr = region.address;
            this->m_memoryEditor.DataPreviewAddrEnd = region.address + region.size - 1;
            View::postEvent(Events::RegionSelected, region);
        });

        View::subscribeEvent(Events::ProjectFileLoad, [this](auto) {
            this->openFile(ProjectFile::getFilePath());
        });

        View::subscribeEvent(Events::WindowClosing, [this](auto userData) {
            auto window = std::any_cast<GLFWwindow*>(userData);

            if (ProjectFile::hasUnsavedChanges()) {
                glfwSetWindowShouldClose(window, GLFW_FALSE);
                View::doLater([] { ImGui::OpenPopup("hex.view.hexeditor.save_changes"_lang); });
            }
        });

        View::subscribeEvent(Events::PatternChanged, [this](auto) {
            this->m_highlightedBytes.clear();

            for (const auto &pattern : this->m_patternData)
                this->m_highlightedBytes.merge(pattern->getHighlightedAddresses());
        });

        View::subscribeEvent(Events::OpenWindow, [this](auto name) {
            if (std::any_cast<const char*>(name) == std::string("Open File")) {
                View::openFileBrowser("hex.view.hexeditor.open_file"_lang, DialogMode::Open, { }, [this](auto path) {
                    this->openFile(path);
                    this->getWindowOpenState() = true;
                });
            } else if (std::any_cast<const char*>(name) == std::string("Open Project")) {
                View::openFileBrowser("hex.view.hexeditor.open_project"_lang, DialogMode::Open, { { "Project File", "hexproj" } }, [this](auto path) {
                    ProjectFile::load(path);
                    View::postEvent(Events::ProjectFileLoad);
                    this->getWindowOpenState() = true;
                });
            }
        });
    }

    ViewHexEditor::~ViewHexEditor() {

    }

    void ViewHexEditor::drawContent() {
        auto provider = SharedData::currentProvider;

        size_t dataSize = (provider == nullptr || !provider->isReadable()) ? 0x00 : provider->getSize();

        this->m_memoryEditor.DrawWindow("hex.view.hexeditor.name"_lang, &this->getWindowOpenState(), this, dataSize, dataSize == 0 ? 0x00 : provider->getBaseAddress());

        if (dataSize != 0x00) {
            if (ImGui::Begin("hex.view.hexeditor.name"_lang)) {

                if (ImGui::IsMouseReleased(ImGuiMouseButton_Right) && ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows))
                    ImGui::OpenPopup("hex.menu.edit"_lang);

                if (ImGui::BeginPopup("hex.menu.edit"_lang)) {
                    this->drawEditPopup();
                    ImGui::EndPopup();
                }

                ImGui::SameLine();
                ImGui::Spacing();
                ImGui::SameLine();
                ImGui::Text("hex.view.hexeditor.page"_lang, provider->getCurrentPage() + 1, provider->getPageCount());
                ImGui::SameLine();

                if (ImGui::ArrowButton("prevPage", ImGuiDir_Left)) {
                    provider->setCurrentPage(provider->getCurrentPage() - 1);

                    Region dataPreview = { std::min(this->m_memoryEditor.DataPreviewAddr, this->m_memoryEditor.DataPreviewAddrEnd), 1 };
                    View::postEvent(Events::RegionSelected, dataPreview);
                }

                ImGui::SameLine();

                if (ImGui::ArrowButton("nextPage", ImGuiDir_Right)) {
                    provider->setCurrentPage(provider->getCurrentPage() + 1);

                    Region dataPreview = { std::min(this->m_memoryEditor.DataPreviewAddr, this->m_memoryEditor.DataPreviewAddrEnd), 1 };
                    View::postEvent(Events::RegionSelected, dataPreview);
                }
            }
            ImGui::End();

            this->drawSearchPopup();
            this->drawGotoPopup();
        }
    }

    static void save() {
        auto provider = SharedData::currentProvider;
        for (const auto &[address, value] : provider->getPatches())
            provider->writeRaw(address, &value, sizeof(u8));
    }

    static void saveAs() {
        View::openFileBrowser("hex.view.hexeditor.save_as"_lang, View::DialogMode::Save, { }, [](auto path) {
            FILE *file = fopen(path.c_str(), "wb");

            if (file != nullptr) {
                std::vector<u8> buffer(0xFF'FFFF, 0x00);
                size_t bufferSize = buffer.size();

                fseek(file, 0, SEEK_SET);

                auto provider = SharedData::currentProvider;
                for (u64 offset = 0; offset < provider->getActualSize(); offset += bufferSize) {
                    if (bufferSize > provider->getActualSize() - offset)
                        bufferSize = provider->getActualSize() - offset;

                    provider->read(offset, buffer.data(), bufferSize);
                    fwrite(buffer.data(), 1, bufferSize, file);
                }

                fclose(file);
            }
        });
    }

    void ViewHexEditor::drawAlwaysVisible() {
        auto provider = SharedData::currentProvider;

        if (ImGui::BeginPopupModal("hex.view.hexeditor.save_changes.title"_lang, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::NewLine();
            ImGui::TextUnformatted("hex.view.hexeditor.save_changes.desc"_lang);
            ImGui::NewLine();

            confirmButtons("Yes", "No", [] { View::postEvent(Events::CloseImHex); }, [] { ImGui::CloseCurrentPopup(); });

            if (ImGui::IsKeyDown(ImGui::GetKeyIndex(ImGuiKey_Escape)))
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }

        if (ImGui::BeginPopupModal("hex.view.hexeditor.script.title"_lang, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::SetCursorPosX(10);
            ImGui::TextWrapped("hex.view.hexeditor.script.desc"_lang);

            ImGui::NewLine();
            ImGui::InputText("##nolabel", this->m_loaderScriptScriptPath.data(), this->m_loaderScriptScriptPath.length(), ImGuiInputTextFlags_ReadOnly);
            ImGui::SameLine();
            if (ImGui::Button("hex.view.hexeditor.script.script"_lang)) {
                View::openFileBrowser("hex.view.hexeditor.script.script.title"_lang, DialogMode::Open, { { "Python Script", "py" } }, [this](auto path) {
                    this->m_loaderScriptScriptPath = path;
                });
            }
            ImGui::InputText("##nolabel", this->m_loaderScriptFilePath.data(), this->m_loaderScriptFilePath.length(), ImGuiInputTextFlags_ReadOnly);
            ImGui::SameLine();
            if (ImGui::Button("hex.view.hexeditor.script.file"_lang)) {
                View::openFileBrowser("hex.view.hexeditor.script.file.title"_lang, DialogMode::Open, { }, [this](auto path) {
                    this->m_loaderScriptFilePath = path;
                });
            }
            if (ImGui::IsKeyDown(ImGui::GetKeyIndex(ImGuiKey_Escape)))
                ImGui::CloseCurrentPopup();

            ImGui::NewLine();

            confirmButtons("hex.common.load"_lang, "hex.common.cancel"_lang,
                           [this, &provider] {
                               if (!this->m_loaderScriptScriptPath.empty() && !this->m_loaderScriptFilePath.empty()) {
                                   this->openFile(this->m_loaderScriptFilePath);
                                   LoaderScript::setFilePath(this->m_loaderScriptFilePath);
                                   LoaderScript::setDataProvider(provider);
                                   LoaderScript::processFile(this->m_loaderScriptScriptPath);
                                   ImGui::CloseCurrentPopup();
                               }
                           },
                           [] {
                               ImGui::CloseCurrentPopup();
                           }
            );

            ImGui::EndPopup();

        }

        if (ImGui::BeginPopupModal("hex.view.hexeditor.menu.edit.set_base"_lang, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::InputText("hex.common.address"_lang, this->m_baseAddressBuffer, 16, ImGuiInputTextFlags_CharsHexadecimal);
            ImGui::NewLine();

            confirmButtons("hex.common.set"_lang, "hex.common.cancel"_lang,
                           [this, &provider]{
                               provider->setBaseAddress(strtoull(this->m_baseAddressBuffer, nullptr, 16));
                               ImGui::CloseCurrentPopup();
                           }, []{
                        ImGui::CloseCurrentPopup();
                    });

            if (ImGui::IsKeyDown(ImGui::GetKeyIndex(ImGuiKey_Escape)))
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }
    }

    void ViewHexEditor::drawMenu() {
        auto provider = SharedData::currentProvider;

        if (ImGui::BeginMenu("hex.menu.file"_lang)) {
            if (ImGui::MenuItem("hex.view.hexeditor.menu.file.open_file"_lang, "CTRL + O")) {

                View::openFileBrowser("hex.view.hexeditor.open_file"_lang, DialogMode::Open, { }, [this](auto path) {
                    this->openFile(path);
                    this->getWindowOpenState() = true;
                });
            }

            if (ImGui::MenuItem("hex.view.hexeditor.menu.file.save"_lang, "CTRL + S", false, provider != nullptr && provider->isWritable())) {
                save();
            }

            if (ImGui::MenuItem("hex.view.hexeditor.menu.file.save_as"_lang, "CTRL + SHIFT + S", false, provider != nullptr && provider->isWritable())) {
                saveAs();
            }

            ImGui::Separator();

            if (ImGui::MenuItem("hex.view.hexeditor.menu.file.open_project"_lang, "")) {
                View::openFileBrowser("hex.view.hexeditor.menu.file.open_project"_lang, DialogMode::Open, { { "Project File", "hexproj" } }, [this](auto path) {
                    ProjectFile::load(path);
                    View::postEvent(Events::ProjectFileLoad);
                    this->getWindowOpenState() = true;
                });
            }

            if (ImGui::MenuItem("hex.view.hexeditor.menu.file.save_project"_lang, "", false, provider != nullptr && provider->isWritable())) {
                View::postEvent(Events::ProjectFileStore);

                if (ProjectFile::getProjectFilePath() == "") {
                    View::openFileBrowser("hex.view.hexeditor.save_project"_lang, DialogMode::Save, { { "Project File", "hexproj" } }, [](auto path) {
                        ProjectFile::store(path);
                    });
                }
                else
                    ProjectFile::store();
            }

            if (ImGui::MenuItem("hex.view.hexeditor.menu.file.load_encoding_file"_lang)) {
                View::openFileBrowser("hex.view.hexeditor.load_enconding_file"_lang, DialogMode::Open, { }, [this](auto path) {
                    this->m_currEncodingFile = EncodingFile(EncodingFile::Type::Thingy, path);
                });
            }

            ImGui::Separator();

            if (ImGui::BeginMenu("hex.view.hexeditor.menu.file.import"_lang)) {
                if (ImGui::MenuItem("hex.view.hexeditor.menu.file.import.base64"_lang)) {

                    View::openFileBrowser("hex.view.hexeditor.menu.file.import.base64"_lang, DialogMode::Open, { }, [this](auto path) {
                        std::vector<u8> base64;
                        this->loadFromFile(path, base64);

                        if (!base64.empty()) {
                            this->m_dataToSave = crypt::decode64(base64);

                            if (this->m_dataToSave.empty())
                                View::showErrorPopup("hex.view.hexeditor.base64.import_error"_lang);
                            else
                                ImGui::OpenPopup("hex.view.hexeditor.save_data"_lang);
                            this->getWindowOpenState() = true;
                        } else View::showErrorPopup("hex.view.hexeditor.file_open_error"_lang);
                    });
                }

                ImGui::Separator();

                if (ImGui::MenuItem("hex.view.hexeditor.menu.file.import.ips"_lang)) {

                   View::openFileBrowser("hex.view.hexeditor.open_file"_lang, DialogMode::Open, { }, [this](auto path) {
                        auto patchData = hex::readFile(path);
                        auto patch = hex::loadIPSPatch(patchData);

                        for (auto &[address, value] : patch) {
                            SharedData::currentProvider->write(address, &value, 1);
                        }
                       this->getWindowOpenState() = true;
                   });


                }

                if (ImGui::MenuItem("hex.view.hexeditor.menu.file.import.ips32"_lang)) {
                    View::openFileBrowser("hex.view.hexeditor.open_file"_lang, DialogMode::Open, { }, [this](auto path) {
                        auto patchData = hex::readFile(path);
                        auto patch = hex::loadIPS32Patch(patchData);

                        for (auto &[address, value] : patch) {
                            SharedData::currentProvider->write(address, &value, 1);
                        }
                        this->getWindowOpenState() = true;
                    });
                }

                if (ImGui::MenuItem("hex.view.hexeditor.menu.file.import.script"_lang)) {
                    this->m_loaderScriptFilePath.clear();
                    this->m_loaderScriptScriptPath.clear();
                    View::doLater([]{ ImGui::OpenPopup("hex.view.hexeditor.script.title"_lang); });
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("hex.view.hexeditor.menu.file.export"_lang, provider != nullptr && provider->isWritable())) {
                if (ImGui::MenuItem("hex.view.hexeditor.menu.file.export.ips"_lang)) {
                    Patches patches = provider->getPatches();
                    if (!patches.contains(0x00454F45) && patches.contains(0x00454F46)) {
                        u8 value = 0;
                        provider->read(0x00454F45, &value, sizeof(u8));
                        patches[0x00454F45] = value;
                    }

                    this->m_dataToSave = generateIPSPatch(patches);
                    View::openFileBrowser("hex.view.hexeditor.menu.file.export.title"_lang, DialogMode::Save, { }, [this](auto path) {
                        this->saveToFile(path, this->m_dataToSave);
                    });
                }
                if (ImGui::MenuItem("hex.view.hexeditor.menu.file.export.ips32"_lang)) {
                    Patches patches = provider->getPatches();
                    if (!patches.contains(0x00454F45) && patches.contains(0x45454F46)) {
                        u8 value = 0;
                        provider->read(0x45454F45, &value, sizeof(u8));
                        patches[0x45454F45] = value;
                    }

                    this->m_dataToSave = generateIPS32Patch(patches);
                    View::openFileBrowser("hex.view.hexeditor.menu.file.export.title"_lang, DialogMode::Save, { }, [this](auto path) {
                        this->saveToFile(path, this->m_dataToSave);
                    });
                }

                ImGui::EndMenu();
            }



            ImGui::Separator();

            if (ImGui::MenuItem("hex.view.hexeditor.menu.file.search"_lang, "CTRL + F")) {
                this->getWindowOpenState() = true;
                View::doLater([]{ ImGui::OpenPopup("hex.view.hexeditor.menu.file.search"_lang); });
            }

            if (ImGui::MenuItem("hex.view.hexeditor.menu.file.goto"_lang, "CTRL + G")) {
                this->getWindowOpenState() = true;
                View::doLater([]{ ImGui::OpenPopup("hex.view.hexeditor.menu.file.goto"_lang); });
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("hex.menu.edit"_lang)) {
            this->drawEditPopup();
            ImGui::EndMenu();
        }
    }

    bool ViewHexEditor::handleShortcut(int key, int mods) {
        if (mods == GLFW_MOD_CONTROL && key == GLFW_KEY_S) {
            save();
            return true;
        } else if (mods == (GLFW_MOD_CONTROL | GLFW_MOD_SHIFT) && key == GLFW_KEY_S) {
            saveAs();
            return true;
        } else if (mods == GLFW_MOD_CONTROL && key == GLFW_KEY_F) {
            View::doLater([]{ ImGui::OpenPopup("hex.view.hexeditor.menu.file.search"_lang); });
            return true;
        } else if (mods == GLFW_MOD_CONTROL && key == GLFW_KEY_G) {
            View::doLater([]{ ImGui::OpenPopup("hex.view.hexeditor.menu.file.goto"_lang); });
            return true;
        } else if (mods == GLFW_MOD_CONTROL && key == GLFW_KEY_O) {
            View::doLater([]{ ImGui::OpenPopup("hex.view.hexeditor.open_file"_lang); });
            return true;
        } else if (mods == (GLFW_MOD_CONTROL | GLFW_MOD_ALT) && key == GLFW_KEY_C) {
            this->copyBytes();
            return true;
        } else if (mods == (GLFW_MOD_CONTROL | GLFW_MOD_SHIFT) && key == GLFW_KEY_C) {
            this->copyString();
            return true;
        }

        return false;
    }


    void ViewHexEditor::openFile(std::string path) {
        auto& provider = SharedData::currentProvider;

        if (provider != nullptr)
            delete provider;

        provider = new prv::FileProvider(path);
        if (!provider->isWritable()) {
            this->m_memoryEditor.ReadOnly = true;
            View::showErrorPopup("hex.view.hexeditor.error.read_only"_lang);
        } else {
            this->m_memoryEditor.ReadOnly = false;
        }

        if (!provider->isAvailable()) {
            View::showErrorPopup("hex.view.hexeditor.error.open"_lang);
            delete provider;
            provider = nullptr;

            return;
        }

        ProjectFile::setFilePath(path);

        this->getWindowOpenState() = true;

        View::postEvent(Events::FileLoaded, path);
        View::postEvent(Events::DataChanged);
        View::postEvent(Events::PatternChanged);
    }

    bool ViewHexEditor::saveToFile(std::string path, const std::vector<u8>& data) {
        FILE *file = fopen(path.c_str(), "wb");

        if (file == nullptr)
            return false;

        fwrite(data.data(), 1, data.size(), file);
        fclose(file);

        return true;
    }

    bool ViewHexEditor::loadFromFile(std::string path, std::vector<u8>& data) {
        FILE *file = fopen(path.c_str(), "rb");

        if (file == nullptr)
            return false;

        fseek(file, 0, SEEK_END);
        size_t size = ftello64(file);
        rewind(file);

        data.resize(size);
        fread(data.data(), 1, data.size(), file);
        fclose(file);

        return true;
    }

    void ViewHexEditor::copyBytes() {
        auto provider = SharedData::currentProvider;

        size_t start = std::min(this->m_memoryEditor.DataPreviewAddr, this->m_memoryEditor.DataPreviewAddrEnd);
        size_t end = std::max(this->m_memoryEditor.DataPreviewAddr, this->m_memoryEditor.DataPreviewAddrEnd);

        size_t copySize = (end - start) + 1;

        std::vector<u8> buffer(copySize, 0x00);
        provider->read(start, buffer.data(), buffer.size());

        std::string str;
        for (const auto &byte : buffer)
            str += hex::format("%02X ", byte);
        str.pop_back();

        ImGui::SetClipboardText(str.c_str());
    }

    void ViewHexEditor::copyString() {
        auto provider = SharedData::currentProvider;

        size_t start = std::min(this->m_memoryEditor.DataPreviewAddr, this->m_memoryEditor.DataPreviewAddrEnd);
        size_t end = std::max(this->m_memoryEditor.DataPreviewAddr, this->m_memoryEditor.DataPreviewAddrEnd);

        size_t copySize = (end - start) + 1;

        std::string buffer(copySize, 0x00);
        buffer.reserve(copySize + 1);
        provider->read(start, buffer.data(), copySize);

        ImGui::SetClipboardText(buffer.c_str());
    }

    void ViewHexEditor::copyLanguageArray(Language language) {
        auto provider = SharedData::currentProvider;

        size_t start = std::min(this->m_memoryEditor.DataPreviewAddr, this->m_memoryEditor.DataPreviewAddrEnd);
        size_t end = std::max(this->m_memoryEditor.DataPreviewAddr, this->m_memoryEditor.DataPreviewAddrEnd);

        size_t copySize = (end - start) + 1;

        std::vector<u8> buffer(copySize, 0x00);
        provider->read(start, buffer.data(), buffer.size());

        std::string str;
        switch (language) {
            case Language::C:
                str += "const unsigned char data[" + std::to_string(buffer.size()) + "] = { ";

                for (const auto &byte : buffer)
                    str += hex::format("0x%02X, ", byte);

                // Remove trailing comma
                str.pop_back();
                str.pop_back();

                str += " };";
                break;
            case Language::Cpp:
                str += "constexpr std::array<unsigned char, " + std::to_string(buffer.size()) + "> data = { ";

                for (const auto &byte : buffer)
                    str += hex::format("0x%02X, ", byte);

                // Remove trailing comma
                str.pop_back();
                str.pop_back();

                str += " };";
                break;
            case Language::Java:
                str += "final byte[] data = { ";

                for (const auto &byte : buffer)
                    str += hex::format("0x%02X, ", byte);

                // Remove trailing comma
                str.pop_back();
                str.pop_back();

                str += " };";
                break;
            case Language::CSharp:
                str += "const byte[] data = { ";

                for (const auto &byte : buffer)
                    str += hex::format("0x%02X, ", byte);

                // Remove trailing comma
                str.pop_back();
                str.pop_back();

                str += " };";
                break;
            case Language::Rust:
                str += "let data: [u8, " + std::to_string(buffer.size()) + "] = [ ";

                for (const auto &byte : buffer)
                    str += hex::format("0x%02X, ", byte);

                // Remove trailing comma
                str.pop_back();
                str.pop_back();

                str += " ];";
                break;
            case Language::Python:
                str += "data = bytes([ ";

                for (const auto &byte : buffer)
                    str += hex::format("0x%02X, ", byte);

                // Remove trailing comma
                str.pop_back();
                str.pop_back();

                str += " ]);";
                break;
            case Language::JavaScript:
                str += "const data = new Uint8Array([ ";

                for (const auto &byte : buffer)
                    str += hex::format("0x%02X, ", byte);

                // Remove trailing comma
                str.pop_back();
                str.pop_back();

                str += " ]);";
                break;
        }

        ImGui::SetClipboardText(str.c_str());
    }

    void ViewHexEditor::copyHexView() {
        auto provider = SharedData::currentProvider;

        size_t start = std::min(this->m_memoryEditor.DataPreviewAddr, this->m_memoryEditor.DataPreviewAddrEnd);
        size_t end = std::max(this->m_memoryEditor.DataPreviewAddr, this->m_memoryEditor.DataPreviewAddrEnd);

        size_t copySize = (end - start) + 1;

        std::vector<u8> buffer(copySize, 0x00);
        provider->read(start, buffer.data(), buffer.size());

        std::string str = "Hex View  00 01 02 03 04 05 06 07  08 09 0A 0B 0C 0D 0E 0F\n\n";


        for (u32 col = start >> 4; col <= (end >> 4); col++) {
            str += hex::format("%08lX  ", col << 4);
            for (u64 i = 0 ; i < 16; i++) {

                if (col == (start >> 4) && i < (start & 0xF) || col == (end >> 4) && i > (end & 0xF))
                    str += "   ";
                else
                    str += hex::format("%02lX ", buffer[((col << 4) - start) + i]);

                if ((i & 0xF) == 0x7)
                    str += " ";
            }

            str += " ";

            for (u64 i = 0 ; i < 16; i++) {

                if (col == (start >> 4) && i < (start & 0xF) || col == (end >> 4) && i > (end & 0xF))
                    str += " ";
                else {
                    u8 c = buffer[((col << 4) - start) + i];
                    char displayChar = (c < 32 || c >= 128) ? '.' : c;
                    str += hex::format("%c", displayChar);
                }
            }

            str += "\n";
        }


        ImGui::SetClipboardText(str.c_str());
    }

    void ViewHexEditor::copyHexViewHTML() {
        auto provider = SharedData::currentProvider;

        size_t start = std::min(this->m_memoryEditor.DataPreviewAddr, this->m_memoryEditor.DataPreviewAddrEnd);
        size_t end = std::max(this->m_memoryEditor.DataPreviewAddr, this->m_memoryEditor.DataPreviewAddrEnd);

        size_t copySize = (end - start) + 1;

        std::vector<u8> buffer(copySize, 0x00);
        provider->read(start, buffer.data(), buffer.size());

        std::string str =
R"(
<div>
    <style type="text/css">
        .offsetheader { color:#0000A0; line-height:200% }
        .offsetcolumn { color:#0000A0 }
        .hexcolumn { color:#000000 }
        .textcolumn { color:#000000 }
    </style>

    <code>
        <span class="offsetheader">Hex View&nbsp&nbsp00 01 02 03 04 05 06 07&nbsp 08 09 0A 0B 0C 0D 0E 0F</span><br/>
)";


        for (u32 col = start >> 4; col <= (end >> 4); col++) {
            str += hex::format("        <span class=\"offsetcolumn\">%08lX</span>&nbsp&nbsp<span class=\"hexcolumn\">", col << 4);
            for (u64 i = 0 ; i < 16; i++) {

                if (col == (start >> 4) && i < (start & 0xF) || col == (end >> 4) && i > (end & 0xF))
                    str += "&nbsp&nbsp ";
                else
                    str += hex::format("%02lX ", buffer[((col << 4) - start) + i]);

                if ((i & 0xF) == 0x7)
                    str += "&nbsp";
            }

            str += "</span>&nbsp&nbsp<span class=\"textcolumn\">";

            for (u64 i = 0 ; i < 16; i++) {

                if (col == (start >> 4) && i < (start & 0xF) || col == (end >> 4) && i > (end & 0xF))
                    str += "&nbsp";
                else {
                    u8 c = buffer[((col << 4) - start) + i];
                    char displayChar = (c < 32 || c >= 128) ? '.' : c;
                    str += hex::format("%c", displayChar);
                }
            }

            str += "</span><br/>\n";
        }

        str +=
R"(
    </code>
</div>
)";


        ImGui::SetClipboardText(str.c_str());
    }

    static std::vector<std::pair<u64, u64>> findString(prv::Provider* &provider, std::string string) {
        std::vector<std::pair<u64, u64>> results;

        u32 foundCharacters = 0;

        std::vector<u8> buffer(1024, 0x00);
        size_t dataSize = provider->getSize();
        for (u64 offset = 0; offset < dataSize; offset += 1024) {
            size_t usedBufferSize = std::min(u64(buffer.size()), dataSize - offset);
            provider->read(offset, buffer.data(), usedBufferSize);

            for (u64 i = 0; i < usedBufferSize; i++) {
                if (buffer[i] == string[foundCharacters])
                    foundCharacters++;
                else
                    foundCharacters = 0;

                if (foundCharacters == string.size()) {
                    results.emplace_back(offset + i - foundCharacters + 1, offset + i + 1);
                    foundCharacters = 0;
                }
            }
        }

        return results;
    }

    static std::vector<std::pair<u64, u64>> findHex(prv::Provider* &provider, std::string string) {
        std::vector<std::pair<u64, u64>> results;

        if ((string.size() % 2) == 1)
            string = "0" + string;

        std::vector<u8> hex;
        hex.reserve(string.size() / 2);

        for (u32 i = 0; i < string.size(); i += 2) {
            char byte[3] = { string[i], string[i + 1], 0 };
            hex.push_back(strtoul(byte, nullptr, 16));
        }

        u32 foundCharacters = 0;

        std::vector<u8> buffer(1024, 0x00);
        size_t dataSize = provider->getSize();
        for (u64 offset = 0; offset < dataSize; offset += 1024) {
            size_t usedBufferSize = std::min(u64(buffer.size()), dataSize - offset);
            provider->read(offset, buffer.data(), usedBufferSize);

            for (u64 i = 0; i < usedBufferSize; i++) {
                if (buffer[i] == hex[foundCharacters])
                    foundCharacters++;
                else
                    foundCharacters = 0;

                if (foundCharacters == hex.size()) {
                    results.emplace_back(offset + i - foundCharacters + 1, offset + i + 1);
                    foundCharacters = 0;
                }
            }
        }

        return results;
    }


    void ViewHexEditor::drawSearchPopup() {
        static auto InputCallback = [](ImGuiInputTextCallbackData* data) -> int {
            auto _this = static_cast<ViewHexEditor*>(data->UserData);
            auto provider = SharedData::currentProvider;

            *_this->m_lastSearchBuffer = _this->m_searchFunction(provider, data->Buf);
            _this->m_lastSearchIndex = 0;

            if (_this->m_lastSearchBuffer->size() > 0)
                _this->m_memoryEditor.GotoAddrAndHighlight((*_this->m_lastSearchBuffer)[0].first, (*_this->m_lastSearchBuffer)[0].second);

            return 0;
        };

        static auto Find = [this](char *buffer) {
            auto provider = SharedData::currentProvider;

            *this->m_lastSearchBuffer = this->m_searchFunction(provider, buffer);
            this->m_lastSearchIndex = 0;

            if (this->m_lastSearchBuffer->size() > 0)
                this->m_memoryEditor.GotoAddrAndHighlight((*this->m_lastSearchBuffer)[0].first, (*this->m_lastSearchBuffer)[0].second);
        };

        static auto FindNext = [this]() {
            if (this->m_lastSearchBuffer->size() > 0) {
                ++this->m_lastSearchIndex %= this->m_lastSearchBuffer->size();
                this->m_memoryEditor.GotoAddrAndHighlight((*this->m_lastSearchBuffer)[this->m_lastSearchIndex].first,
                                                          (*this->m_lastSearchBuffer)[this->m_lastSearchIndex].second);
            }
        };

        static auto FindPrevious = [this]() {
            if (this->m_lastSearchBuffer->size() > 0) {
                this->m_lastSearchIndex--;

                if (this->m_lastSearchIndex < 0)
                    this->m_lastSearchIndex = this->m_lastSearchBuffer->size() - 1;

                this->m_lastSearchIndex %= this->m_lastSearchBuffer->size();

                this->m_memoryEditor.GotoAddrAndHighlight((*this->m_lastSearchBuffer)[this->m_lastSearchIndex].first,
                                                          (*this->m_lastSearchBuffer)[this->m_lastSearchIndex].second);
            }
        };

        if (ImGui::BeginPopupContextVoid("hex.view.hexeditor.menu.file.search"_lang)) {
            ImGui::TextUnformatted("hex.view.hexeditor.menu.file.search"_lang);
            if (ImGui::BeginTabBar("searchTabs")) {
                std::vector<char> *currBuffer = nullptr;
                if (ImGui::BeginTabItem("hex.view.hexeditor.search.string"_lang)) {
                    this->m_searchFunction = findString;
                    this->m_lastSearchBuffer = &this->m_lastStringSearch;
                    currBuffer = &this->m_searchStringBuffer;

                    ImGui::InputText("##nolabel", currBuffer->data(), currBuffer->size(), ImGuiInputTextFlags_CallbackCompletion,
                                     InputCallback, this);
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("hex.view.hexeditor.search.hex"_lang)) {
                    this->m_searchFunction = findHex;
                    this->m_lastSearchBuffer = &this->m_lastHexSearch;
                    currBuffer = &this->m_searchHexBuffer;

                    ImGui::InputText("##nolabel", currBuffer->data(), currBuffer->size(),
                                     ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CallbackCompletion,
                                     InputCallback, this);
                    ImGui::EndTabItem();
                }

                if (currBuffer != nullptr) {
                    if (ImGui::Button("hex.view.hexeditor.search.find"_lang))
                        Find(currBuffer->data());

                    if (this->m_lastSearchBuffer->size() > 0) {
                        if ((ImGui::Button("hex.view.hexeditor.search.find_next"_lang)))
                            FindNext();

                        ImGui::SameLine();

                        if ((ImGui::Button("hex.view.hexeditor.search.find_prev"_lang)))
                            FindPrevious();
                    }
                }

                ImGui::EndTabBar();
            }


            ImGui::EndPopup();
        }
    }

    void ViewHexEditor::drawGotoPopup() {
        auto provider = SharedData::currentProvider;

        if (ImGui::BeginPopup("hex.view.hexeditor.menu.file.goto"_lang)) {
            ImGui::TextUnformatted("hex.view.hexeditor.menu.file.goto"_lang);
            if (ImGui::BeginTabBar("gotoTabs")) {
                s64 newOffset = 0;
                if (ImGui::BeginTabItem("hex.view.hexeditor.goto.offset.begin"_lang)) {
                    ImGui::InputScalar("##nolabel", ImGuiDataType_U64, &this->m_gotoAddress, nullptr, nullptr, "%llx", ImGuiInputTextFlags_CharsHexadecimal);

                    if (this->m_gotoAddress >= provider->getActualSize())
                        this->m_gotoAddress = provider->getActualSize() - 1;

                    newOffset = this->m_gotoAddress;

                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("hex.view.hexeditor.goto.offset.current"_lang)) {
                    ImGui::InputScalar("##nolabel", ImGuiDataType_S64, &this->m_gotoAddress, nullptr, nullptr, "%llx", ImGuiInputTextFlags_CharsHexadecimal);

                    if (this->m_memoryEditor.DataPreviewAddr == -1 || this->m_memoryEditor.DataPreviewAddrEnd == -1) {
                        this->m_memoryEditor.DataPreviewAddr = 0;
                        this->m_memoryEditor.DataPreviewAddrEnd = 0;
                    }

                    s64 currHighlightStart = std::min(this->m_memoryEditor.DataPreviewAddr, this->m_memoryEditor.DataPreviewAddrEnd);

                    newOffset = this->m_gotoAddress + currHighlightStart;
                    if (newOffset >= provider->getActualSize()) {
                        newOffset = provider->getActualSize() - 1;
                        this->m_gotoAddress = (provider->getActualSize() - 1) - currHighlightStart;
                    } else if (newOffset < 0) {
                        newOffset = 0;
                        this->m_gotoAddress = -currHighlightStart;
                    }

                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("hex.view.hexeditor.goto.offset.end"_lang)) {
                    ImGui::InputScalar("##nolabel", ImGuiDataType_U64, &this->m_gotoAddress, nullptr, nullptr, "%llx", ImGuiInputTextFlags_CharsHexadecimal);

                    if (this->m_gotoAddress >= provider->getActualSize())
                        this->m_gotoAddress = provider->getActualSize() - 1;

                    newOffset = (provider->getActualSize() - 1) - this->m_gotoAddress;

                    ImGui::EndTabItem();
                }

                if (ImGui::Button("hex.view.hexeditor.menu.file.goto"_lang)) {
                    provider->setCurrentPage(std::floor(newOffset / double(prv::Provider::PageSize)));
                    this->m_memoryEditor.GotoAddr = newOffset;
                    this->m_memoryEditor.DataPreviewAddr = newOffset;
                    this->m_memoryEditor.DataPreviewAddrEnd = newOffset;
                }

                ImGui::EndTabBar();
            }

            ImGui::EndPopup();
        }
    }

    void ViewHexEditor::drawEditPopup() {
        if (ImGui::BeginMenu("hex.view.hexeditor.menu.edit.copy"_lang, this->m_memoryEditor.DataPreviewAddr != -1 && this->m_memoryEditor.DataPreviewAddrEnd != -1)) {
            if (ImGui::MenuItem("hex.view.hexeditor.copy.bytes"_lang, "CTRL + ALT + C"))
                this->copyBytes();
            if (ImGui::MenuItem("hex.view.hexeditor.copy.hex"_lang, "CTRL + SHIFT + C"))
                this->copyString();

            ImGui::Separator();

            if (ImGui::MenuItem("hex.view.hexeditor.copy.c"_lang))
                this->copyLanguageArray(Language::C);
            if (ImGui::MenuItem("hex.view.hexeditor.copy.cpp"_lang))
                this->copyLanguageArray(Language::Cpp);
            if (ImGui::MenuItem("hex.view.hexeditor.copy.csharp"_lang))
                this->copyLanguageArray(Language::CSharp);
            if (ImGui::MenuItem("hex.view.hexeditor.copy.rust"_lang))
                this->copyLanguageArray(Language::Rust);
            if (ImGui::MenuItem("hex.view.hexeditor.copy.python"_lang))
                this->copyLanguageArray(Language::Python);
            if (ImGui::MenuItem("hex.view.hexeditor.copy.java"_lang))
                this->copyLanguageArray(Language::Java);
            if (ImGui::MenuItem("hex.view.hexeditor.copy.js"_lang))
                this->copyLanguageArray(Language::JavaScript);

            ImGui::Separator();

            if (ImGui::MenuItem("hex.view.hexeditor.copy.ascii"_lang))
                this->copyHexView();
            if (ImGui::MenuItem("hex.view.hexeditor.copy.html"_lang))
                this->copyHexViewHTML();

            ImGui::EndMenu();
        }

        if (ImGui::MenuItem("hex.view.hexeditor.menu.edit.bookmark"_lang, nullptr, false, this->m_memoryEditor.DataPreviewAddr != -1 && this->m_memoryEditor.DataPreviewAddrEnd != -1)) {
            auto base = SharedData::currentProvider->getBaseAddress();

            size_t start = base + std::min(this->m_memoryEditor.DataPreviewAddr, this->m_memoryEditor.DataPreviewAddrEnd);
            size_t end = base + std::max(this->m_memoryEditor.DataPreviewAddr, this->m_memoryEditor.DataPreviewAddrEnd);

            ImHexApi::Bookmarks::add(start, end - start + 1, { }, { });
        }

        auto provider = SharedData::currentProvider;
        if (ImGui::MenuItem("hex.view.hexeditor.menu.edit.set_base"_lang, nullptr, false, provider != nullptr && provider->isReadable())) {
            std::memset(this->m_baseAddressBuffer, 0x00, sizeof(this->m_baseAddressBuffer));
            View::doLater([]{ ImGui::OpenPopup("hex.view.hexeditor.menu.edit.set_base"_lang); });
        }
    }

}