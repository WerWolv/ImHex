#include "views/view_hexeditor.hpp"

#include <hex/providers/provider.hpp>
#include <hex/api/imhex_api.hpp>
#include "providers/file_provider.hpp"

#include <GLFW/glfw3.h>

#include "helpers/crypto.hpp"
#include "helpers/patches.hpp"
#include "helpers/project_file_handler.hpp"
#include "helpers/loader_script_handler.hpp"

#undef __STRICT_ANSI__
#include <cstdio>

namespace hex {

    ViewHexEditor::ViewHexEditor(std::vector<lang::PatternData*> &patternData)
            : View("Hex Editor"), m_patternData(patternData) {

        this->m_memoryEditor.ReadFn = [](const ImU8 *data, size_t off) -> ImU8 {
            auto provider = SharedData::currentProvider;
            if (!provider->isAvailable() || !provider->isReadable())
                return 0x00;

            ImU8 byte;
            provider->read(off, &byte, sizeof(ImU8));

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

            for (const auto &[region, name, comment, color] : ImHexApi::Bookmarks::getEntries()) {
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

        this->m_memoryEditor.HoverFn = [](const ImU8 *data, size_t addr) {
            bool tooltipShown = false;

            for (const auto &[region, name, comment, color] : ImHexApi::Bookmarks::getEntries()) {
                if (addr >= region.address && addr < (region.address + region.size)) {
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
                this->getWindowOpenState() = true;
                View::doLater([] { ImGui::OpenPopup("Save Changes"); });
            }
        });

        View::subscribeEvent(Events::PatternChanged, [this](auto) {
           this->m_highlightedBytes.clear();

           for (const auto &pattern : this->m_patternData)
               this->m_highlightedBytes.merge(pattern->getHighlightedAddresses());
        });
    }

    ViewHexEditor::~ViewHexEditor() {

    }

    void ViewHexEditor::drawContent() {
        auto provider = SharedData::currentProvider;

        size_t dataSize = (provider == nullptr || !provider->isReadable()) ? 0x00 : provider->getSize();

        this->m_memoryEditor.DrawWindow("Hex Editor", &this->getWindowOpenState(), this, dataSize, dataSize == 0 ? 0x00 : provider->getBaseAddress());

        if (dataSize != 0x00) {
            if (ImGui::Begin("Hex Editor")) {

                if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows))
                    ImGui::OpenPopup("Edit");

                if (ImGui::BeginPopup("Edit")) {
                    this->drawEditPopup();
                    ImGui::EndPopup();
                }

                ImGui::SameLine();
                ImGui::Spacing();
                ImGui::SameLine();
                ImGui::Text("Page %d / %d", provider->getCurrentPage() + 1, provider->getPageCount());
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


        if (ImGui::BeginPopupModal("Save Changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            constexpr auto Message = "You have unsaved changes made to your Project.\nAre you sure you want to exit?";
            ImGui::NewLine();
            ImGui::TextUnformatted(Message);
            ImGui::NewLine();

            confirmButtons("Yes", "No", [] { std::exit(0); }, [] { ImGui::CloseCurrentPopup(); });

            if (ImGui::IsKeyDown(ImGui::GetKeyIndex(ImGuiKey_Escape)))
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }

        if (ImGui::BeginPopupModal("Load File with Loader Script", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            constexpr auto Message = "Load a file using a Python loader script.";
            ImGui::SetCursorPosX(10);
            ImGui::TextWrapped(Message);

            ImGui::NewLine();
            ImGui::InputText("##nolabel", this->m_loaderScriptScriptPath.data(), this->m_loaderScriptScriptPath.length(), ImGuiInputTextFlags_ReadOnly);
            ImGui::SameLine();
            if (ImGui::Button("Script"))
                ImGui::OpenPopup("Loader Script: Open Script");
            ImGui::InputText("##nolabel", this->m_loaderScriptFilePath.data(), this->m_loaderScriptFilePath.length(), ImGuiInputTextFlags_ReadOnly);
            ImGui::SameLine();
            if (ImGui::Button("File"))
                ImGui::OpenPopup("Loader Script: Open File");

            if (ImGui::IsKeyDown(ImGui::GetKeyIndex(ImGuiKey_Escape)))
                ImGui::CloseCurrentPopup();

            if (this->m_fileBrowser.showFileDialog("Loader Script: Open Script", imgui_addons::ImGuiFileBrowser::DialogMode::OPEN, ImVec2(0, 0), ".py")) {
                this->m_loaderScriptScriptPath = this->m_fileBrowser.selected_path;
            }
            if (this->m_fileBrowser.showFileDialog("Loader Script: Open File", imgui_addons::ImGuiFileBrowser::DialogMode::OPEN)) {
                this->m_loaderScriptFilePath = this->m_fileBrowser.selected_path;
            }

            ImGui::NewLine();

            confirmButtons("Load", "Cancel",
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

        if (ImGui::BeginPopupModal("Set base address", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::InputText("Address", this->m_baseAddressBuffer, 16, ImGuiInputTextFlags_CharsHexadecimal);
            ImGui::NewLine();

            confirmButtons("Set", "Cancel",
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


        if (this->m_fileBrowser.showFileDialog("Open File", imgui_addons::ImGuiFileBrowser::DialogMode::OPEN)) {
            this->openFile(this->m_fileBrowser.selected_path);
        }

        if (this->m_fileBrowser.showFileDialog("Open Base64 File", imgui_addons::ImGuiFileBrowser::DialogMode::OPEN)) {
            std::vector<u8> base64;
            this->loadFromFile(this->m_fileBrowser.selected_path, base64);

            if (!base64.empty()) {
                this->m_dataToSave = decode64(base64);

                if (this->m_dataToSave.empty())
                    View::showErrorPopup("File is not in a valid Base64 format!");
                else
                    ImGui::OpenPopup("Save Data");
            } else View::showErrorPopup("Failed to open file!");

        }


        if (this->m_fileBrowser.showFileDialog("Open Project", imgui_addons::ImGuiFileBrowser::DialogMode::OPEN, ImVec2(0, 0), ".hexproj")) {
            ProjectFile::load(this->m_fileBrowser.selected_path);
            View::postEvent(Events::ProjectFileLoad);
        }

        if (this->m_fileBrowser.showFileDialog("Save Project", imgui_addons::ImGuiFileBrowser::DialogMode::SAVE, ImVec2(0, 0), ".hexproj")) {
            ProjectFile::store(this->m_fileBrowser.selected_path);
        }


        if (this->m_fileBrowser.showFileDialog("Export File", imgui_addons::ImGuiFileBrowser::DialogMode::SAVE)) {
            this->saveToFile(this->m_fileBrowser.selected_path, this->m_dataToSave);
        }

        if (this->m_fileBrowser.showFileDialog("Apply IPS Patch", imgui_addons::ImGuiFileBrowser::DialogMode::OPEN)) {
            auto patchData = hex::readFile(this->m_fileBrowser.selected_path);
            auto patch = hex::loadIPSPatch(patchData);

            for (auto &[address, value] : patch) {
                provider->write(address, &value, 1);
            }
        }

        if (this->m_fileBrowser.showFileDialog("Apply IPS32 Patch", imgui_addons::ImGuiFileBrowser::DialogMode::OPEN)) {
            auto patchData = hex::readFile(this->m_fileBrowser.selected_path);
            auto patch = hex::loadIPS32Patch(patchData);

            for (auto &[address, value] : patch) {
                provider->write(address, &value, 1);
            }
        }

        if (this->m_fileBrowser.showFileDialog("Save As", imgui_addons::ImGuiFileBrowser::DialogMode::SAVE)) {
            FILE *file = fopen(this->m_fileBrowser.selected_path.c_str(), "wb");

            if (file != nullptr) {
                std::vector<u8> buffer(0xFF'FFFF, 0x00);
                size_t bufferSize = buffer.size();

                fseek(file, 0, SEEK_SET);

                for (u64 offset = 0; offset < provider->getActualSize(); offset += bufferSize) {
                    if (bufferSize > provider->getActualSize() - offset)
                        bufferSize = provider->getActualSize() - offset;

                    provider->read(offset, buffer.data(), bufferSize);
                    fwrite(buffer.data(), 1, bufferSize, file);
                }

                fclose(file);
            }
        }
    }

    void ViewHexEditor::drawMenu() {
        auto provider = SharedData::currentProvider;

        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open File...", "CTRL + O")) {
                this->getWindowOpenState() = true;
                View::doLater([]{ ImGui::OpenPopup("Open File"); });
            }

            if (ImGui::MenuItem("Save", "CTRL + S", false, provider != nullptr && provider->isWritable())) {
                for (const auto &[address, value] : provider->getPatches())
                    provider->writeRaw(address, &value, sizeof(u8));
            }

            if (ImGui::MenuItem("Save As...", "CTRL + SHIFT + S", false, provider != nullptr && provider->isWritable())) {
                View::doLater([]{ ImGui::OpenPopup("Save As"); });
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Open Project", "")) {
                this->getWindowOpenState() = true;
                View::doLater([]{ ImGui::OpenPopup("Open Project"); });
            }

            if (ImGui::MenuItem("Save Project", "", false, provider != nullptr && provider->isWritable())) {
                View::postEvent(Events::ProjectFileStore);

                if (ProjectFile::getProjectFilePath() == "")
                    View::doLater([] { ImGui::OpenPopup("Save Project"); });
                else
                    ProjectFile::store();
            }

            ImGui::Separator();

            if (ImGui::BeginMenu("Import...")) {
                if (ImGui::MenuItem("Base64 File")) {
                    this->getWindowOpenState() = true;
                    View::doLater([]{ ImGui::OpenPopup("Open Base64 File"); });
                }

                ImGui::Separator();

                if (ImGui::MenuItem("IPS Patch")) {
                    this->getWindowOpenState() = true;
                    View::doLater([]{ ImGui::OpenPopup("Apply IPS Patch"); });
                }

                if (ImGui::MenuItem("IPS32 Patch")) {
                    this->getWindowOpenState() = true;
                    View::doLater([]{ ImGui::OpenPopup("Apply IPS32 Patch"); });
                }

                if (ImGui::MenuItem("File with Loader Script")) {
                    this->getWindowOpenState() = true;
                    this->m_loaderScriptFilePath.clear();
                    this->m_loaderScriptScriptPath.clear();
                    View::doLater([]{ ImGui::OpenPopup("Load File with Loader Script"); });
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Export...", provider != nullptr && provider->isWritable())) {
                if (ImGui::MenuItem("IPS Patch")) {
                    Patches patches = provider->getPatches();
                    if (!patches.contains(0x00454F45) && patches.contains(0x00454F46)) {
                        u8 value = 0;
                        provider->read(0x00454F45, &value, sizeof(u8));
                        patches[0x00454F45] = value;
                    }

                    this->m_dataToSave = generateIPSPatch(patches);
                    View::doLater([]{ ImGui::OpenPopup("Export File"); });
                }
                if (ImGui::MenuItem("IPS32 Patch")) {
                    Patches patches = provider->getPatches();
                    if (!patches.contains(0x00454F45) && patches.contains(0x45454F46)) {
                        u8 value = 0;
                        provider->read(0x45454F45, &value, sizeof(u8));
                        patches[0x45454F45] = value;
                    }

                    this->m_dataToSave = generateIPS32Patch(patches);
                    View::doLater([]{ ImGui::OpenPopup("Export File"); });
                }

                ImGui::EndMenu();
            }



            ImGui::Separator();

            if (ImGui::MenuItem("Search", "CTRL + F")) {
                this->getWindowOpenState() = true;
                View::doLater([]{ ImGui::OpenPopup("Search"); });
            }

            if (ImGui::MenuItem("Goto", "CTRL + G")) {
                this->getWindowOpenState() = true;
                View::doLater([]{ ImGui::OpenPopup("Goto"); });
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            this->drawEditPopup();
            ImGui::EndMenu();
        }
    }

    bool ViewHexEditor::handleShortcut(int key, int mods) {
        if (mods == GLFW_MOD_CONTROL && key == GLFW_KEY_S) {
            auto provider = SharedData::currentProvider;
            for (const auto &[address, value] : provider->getPatches())
                provider->writeRaw(address, &value, sizeof(u8));
            return true;
        } else if (mods == (GLFW_MOD_CONTROL | GLFW_MOD_SHIFT) && key == GLFW_KEY_S) {
            ImGui::OpenPopup("Save As");
            return true;
        } else if (mods == GLFW_MOD_CONTROL && key == GLFW_KEY_F) {
            ImGui::OpenPopup("Search");
            return true;
        } else if (mods == GLFW_MOD_CONTROL && key == GLFW_KEY_G) {
            ImGui::OpenPopup("Goto");
            return true;
        } else if (mods == GLFW_MOD_CONTROL && key == GLFW_KEY_O) {
            ImGui::OpenPopup("Open File");
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
        this->m_memoryEditor.ReadOnly = !provider->isWritable();

        if (provider->isAvailable())
            ProjectFile::setFilePath(path);

        this->getWindowOpenState() = true;

        View::postEvent(Events::FileLoaded);
        View::postEvent(Events::DataChanged);
        View::postEvent(Events::PatternChanged);
        ProjectFile::markDirty();
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

        if (ImGui::BeginPopup("Search")) {
            ImGui::TextUnformatted("Search");
            if (ImGui::BeginTabBar("searchTabs")) {
                char *currBuffer;
                if (ImGui::BeginTabItem("String")) {
                    this->m_searchFunction = findString;
                    this->m_lastSearchBuffer = &this->m_lastStringSearch;
                    currBuffer = this->m_searchStringBuffer;

                    ImGui::InputText("##nolabel", currBuffer, 0xFFFF, ImGuiInputTextFlags_CallbackCompletion,
                                     InputCallback, this);
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Hex")) {
                    this->m_searchFunction = findHex;
                    this->m_lastSearchBuffer = &this->m_lastHexSearch;
                    currBuffer = this->m_searchHexBuffer;

                    ImGui::InputText("##nolabel", currBuffer, 0xFFFF,
                                     ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CallbackCompletion,
                                     InputCallback, this);
                    ImGui::EndTabItem();
                }

                if (ImGui::Button("Find"))
                    Find(currBuffer);

                if (this->m_lastSearchBuffer->size() > 0) {
                    if ((ImGui::Button("Find Next")))
                        FindNext();

                    ImGui::SameLine();

                    if ((ImGui::Button("Find Prev")))
                        FindPrevious();
                }

                ImGui::EndTabBar();
            }


            ImGui::EndPopup();
        }
    }

    void ViewHexEditor::drawGotoPopup() {
        auto provider = SharedData::currentProvider;

        if (ImGui::BeginPopup("Goto")) {
            ImGui::TextUnformatted("Goto");
            if (ImGui::BeginTabBar("gotoTabs")) {
                s64 newOffset = 0;
                if (ImGui::BeginTabItem("Begin")) {
                    ImGui::InputScalar("##nolabel", ImGuiDataType_U64, &this->m_gotoAddress, nullptr, nullptr, "%llx", ImGuiInputTextFlags_CharsHexadecimal);

                    if (this->m_gotoAddress >= provider->getActualSize())
                        this->m_gotoAddress = provider->getActualSize() - 1;

                    newOffset = this->m_gotoAddress;

                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Current")) {
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
                if (ImGui::BeginTabItem("End")) {
                    ImGui::InputScalar("##nolabel", ImGuiDataType_U64, &this->m_gotoAddress, nullptr, nullptr, "%llx", ImGuiInputTextFlags_CharsHexadecimal);

                    if (this->m_gotoAddress >= provider->getActualSize())
                        this->m_gotoAddress = provider->getActualSize() - 1;

                    newOffset = (provider->getActualSize() - 1) - this->m_gotoAddress;

                    ImGui::EndTabItem();
                }

                if (ImGui::Button("Goto")) {
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
        if (ImGui::BeginMenu("Copy as...", this->m_memoryEditor.DataPreviewAddr != -1 && this->m_memoryEditor.DataPreviewAddrEnd != -1)) {
            if (ImGui::MenuItem("Bytes", "CTRL + ALT + C"))
                this->copyBytes();
            if (ImGui::MenuItem("Hex String", "CTRL + SHIFT + C"))
                this->copyString();

            ImGui::Separator();

            if (ImGui::MenuItem("C Array"))
                this->copyLanguageArray(Language::C);
            if (ImGui::MenuItem("C++ Array"))
                this->copyLanguageArray(Language::Cpp);
            if (ImGui::MenuItem("C# Array"))
                this->copyLanguageArray(Language::CSharp);
            if (ImGui::MenuItem("Rust Array"))
                this->copyLanguageArray(Language::Rust);
            if (ImGui::MenuItem("Python Array"))
                this->copyLanguageArray(Language::Python);
            if (ImGui::MenuItem("Java Array"))
                this->copyLanguageArray(Language::Java);
            if (ImGui::MenuItem("JavaScript Array"))
                this->copyLanguageArray(Language::JavaScript);

            ImGui::Separator();

            if (ImGui::MenuItem("Editor View"))
                this->copyHexView();
            if (ImGui::MenuItem("HTML"))
                this->copyHexViewHTML();

            ImGui::EndMenu();
        }

        if (ImGui::MenuItem("Create bookmark", nullptr, false, this->m_memoryEditor.DataPreviewAddr != -1 && this->m_memoryEditor.DataPreviewAddrEnd != -1)) {
            size_t start = std::min(this->m_memoryEditor.DataPreviewAddr, this->m_memoryEditor.DataPreviewAddrEnd);
            size_t end = std::max(this->m_memoryEditor.DataPreviewAddr, this->m_memoryEditor.DataPreviewAddrEnd);

            ImHexApi::Bookmarks::add(start, end - start + 1, { }, { });
        }

        auto provider = SharedData::currentProvider;
        if (ImGui::MenuItem("Set base address", nullptr, false, provider != nullptr && provider->isReadable())) {
            std::memset(this->m_baseAddressBuffer, 0x00, sizeof(this->m_baseAddressBuffer));
            View::doLater([]{ ImGui::OpenPopup("Set base address"); });
        }
    }

}