#include "views/view_hexeditor.hpp"

#include "providers/provider.hpp"
#include "providers/file_provider.hpp"

#include <GLFW/glfw3.h>

#include "helpers/crypto.hpp"
#include "helpers/patches.hpp"
#include "helpers/project_file_handler.hpp"
#include "helpers/loader_script_handler.hpp"

#undef __STRICT_ANSI__
#include <cstdio>

namespace hex {

    ViewHexEditor::ViewHexEditor(prv::Provider* &dataProvider, std::vector<lang::PatternData*> &patternData)
            : View("Hex Editor"), m_dataProvider(dataProvider), m_patternData(patternData) {

        this->m_memoryEditor.ReadFn = [](const ImU8 *data, size_t off) -> ImU8 {
            ViewHexEditor *_this = (ViewHexEditor *) data;

            if (!_this->m_dataProvider->isAvailable() || !_this->m_dataProvider->isReadable())
                return 0x00;

            ImU8 byte;
            _this->m_dataProvider->read(off, &byte, sizeof(ImU8));

            return byte;
        };

        this->m_memoryEditor.WriteFn = [](ImU8 *data, size_t off, ImU8 d) -> void {
            ViewHexEditor *_this = (ViewHexEditor *) data;

            if (!_this->m_dataProvider->isAvailable() || !_this->m_dataProvider->isWritable())
                return;

            _this->m_dataProvider->write(off, &d, sizeof(ImU8));
            _this->postEvent(Events::DataChanged);
            ProjectFile::markDirty();
        };

        this->m_memoryEditor.HighlightFn = [](const ImU8 *data, size_t off, bool next) -> bool {
            ViewHexEditor *_this = (ViewHexEditor *) data;

            for (auto& pattern : _this->m_patternData) {
                if (next && pattern->highlightBytes(off - 1) != pattern->highlightBytes(off)) {
                    return false;
                }

                if (auto color = pattern->highlightBytes(off); color.has_value()) {
                    _this->m_memoryEditor.HighlightColor = color.value();
                    return true;
                }
            }

            _this->m_memoryEditor.HighlightColor = 0x60C08080;
            return false;
        };

        View::subscribeEvent(Events::FileDropped, [this](const void *userData) {
            auto filePath = static_cast<const char*>(userData);

            if (filePath != nullptr)
                this->openFile(filePath);
        });

        View::subscribeEvent(Events::SelectionChangeRequest, [this](const void *userData) {
            const Region &region = *reinterpret_cast<const Region*>(userData);

            auto page = this->m_dataProvider->getPageOfAddress(region.address);
            if (!page.has_value())
                return;

            this->m_dataProvider->setCurrentPage(page.value());
            this->m_memoryEditor.GotoAddr = region.address;
            this->m_memoryEditor.DataPreviewAddr = region.address;
            this->m_memoryEditor.DataPreviewAddrEnd = region.address + region.size - 1;
            View::postEvent(Events::RegionSelected, &region);
        });

        View::subscribeEvent(Events::ProjectFileLoad, [this](const void *userData) {
            this->openFile(ProjectFile::getFilePath());
        });

        View::subscribeEvent(Events::WindowClosing, [this](const void *userData) {
            auto window = const_cast<GLFWwindow*>(static_cast<const GLFWwindow*>(userData));

            if (ProjectFile::hasUnsavedChanges()) {
                glfwSetWindowShouldClose(window, GLFW_FALSE);
                this->getWindowOpenState() = true;
                View::doLater([] { ImGui::OpenPopup("Save Changes"); });
            }
        });
    }

    ViewHexEditor::~ViewHexEditor() {
        if (this->m_dataProvider != nullptr)
            delete this->m_dataProvider;
        this->m_dataProvider = nullptr;
    }

    void ViewHexEditor::createView() {
        size_t dataSize = (this->m_dataProvider == nullptr || !this->m_dataProvider->isReadable()) ? 0x00 : this->m_dataProvider->getSize();

        this->m_memoryEditor.DrawWindow("Hex Editor", &this->getWindowOpenState(), this, dataSize, dataSize == 0 ? 0x00 : this->m_dataProvider->getBaseAddress());

        if (dataSize != 0x00) {
            ImGui::Begin("Hex Editor");
            ImGui::SameLine();
            ImGui::Text("Page %d / %d", this->m_dataProvider->getCurrentPage() + 1, this->m_dataProvider->getPageCount());
            ImGui::SameLine();

            if (ImGui::ArrowButton("prevPage", ImGuiDir_Left)) {
                this->m_dataProvider->setCurrentPage(this->m_dataProvider->getCurrentPage() - 1);

                Region dataPreview = { std::min(this->m_memoryEditor.DataPreviewAddr, this->m_memoryEditor.DataPreviewAddrEnd), 1 };
                View::postEvent(Events::RegionSelected, &dataPreview);
            }

            ImGui::SameLine();

            if (ImGui::ArrowButton("nextPage", ImGuiDir_Right)) {
                this->m_dataProvider->setCurrentPage(this->m_dataProvider->getCurrentPage() + 1);

                Region dataPreview = { std::min(this->m_memoryEditor.DataPreviewAddr, this->m_memoryEditor.DataPreviewAddrEnd), 1 };
                View::postEvent(Events::RegionSelected, &dataPreview);
            }

            ImGui::End();

            this->drawSearchPopup();
            this->drawGotoPopup();
        }


        if (ImGui::BeginPopupModal("Save Changes", nullptr, ImGuiWindowFlags_NoResize)) {
            constexpr auto Message = "You have unsaved changes made to your Project. Are you sure you want to exit?";
            ImGui::NewLine();
            if (ImGui::BeginChild("##scrolling", ImVec2(300, 50))) {
                ImGui::SetCursorPosX(10);
                ImGui::TextWrapped("%s", Message);
                ImGui::EndChild();
            }
            ImGui::SetCursorPosX(40);
            if (ImGui::Button("Yes", ImVec2(100, 20)))
                std::exit(0);
            ImGui::SameLine();
            ImGui::SetCursorPosX(160);
            if (ImGui::Button("No", ImVec2(100, 20)))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        if (ImGui::BeginPopupModal("Load File with Loader Script", nullptr, ImGuiWindowFlags_NoResize)) {
            constexpr auto Message = "Load a file using a Python loader script.";
            ImGui::NewLine();
            if (ImGui::BeginChild("##scrolling", ImVec2(500, 20))) {
                ImGui::SetCursorPosX(10);
                ImGui::TextWrapped("%s", Message);
                ImGui::EndChild();
            }

            ImGui::NewLine();
            ImGui::InputText("##nolabel", this->m_loaderScriptScriptPath.data(), this->m_loaderScriptScriptPath.length(), ImGuiInputTextFlags_ReadOnly);
            ImGui::SameLine();
            if (ImGui::Button("Script"))
                ImGui::OpenPopup("Loader Script: Open Script");
            ImGui::InputText("##nolabel", this->m_loaderScriptFilePath.data(), this->m_loaderScriptFilePath.length(), ImGuiInputTextFlags_ReadOnly);
            ImGui::SameLine();
            if (ImGui::Button("File"))
                ImGui::OpenPopup("Loader Script: Open File");


            if (this->m_fileBrowser.showFileDialog("Loader Script: Open Script", imgui_addons::ImGuiFileBrowser::DialogMode::OPEN, ImVec2(0, 0), ".py")) {
                this->m_loaderScriptScriptPath = this->m_fileBrowser.selected_path;
            }
            if (this->m_fileBrowser.showFileDialog("Loader Script: Open File", imgui_addons::ImGuiFileBrowser::DialogMode::OPEN)) {
                this->m_loaderScriptFilePath = this->m_fileBrowser.selected_path;
            }

            ImGui::NewLine();

            ImGui::SetCursorPosX(40);
            if (ImGui::Button("Load", ImVec2(100, 20))) {
                if (!this->m_loaderScriptScriptPath.empty() && !this->m_loaderScriptFilePath.empty()) {
                    this->openFile(this->m_loaderScriptFilePath);
                    LoaderScript::setFilePath(this->m_loaderScriptFilePath);
                    LoaderScript::setDataProvider(this->m_dataProvider);
                    LoaderScript::processFile(this->m_loaderScriptScriptPath);
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::SameLine();
            ImGui::SetCursorPosX(160);
            if (ImGui::Button("Cancel", ImVec2(100, 20))) {
                ImGui::CloseCurrentPopup();
            }
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
                this->m_dataProvider->write(address, &value, 1);
            }
        }

        if (this->m_fileBrowser.showFileDialog("Apply IPS32 Patch", imgui_addons::ImGuiFileBrowser::DialogMode::OPEN)) {
            auto patchData = hex::readFile(this->m_fileBrowser.selected_path);
            auto patch = hex::loadIPS32Patch(patchData);

            for (auto &[address, value] : patch) {
                this->m_dataProvider->write(address, &value, 1);
            }
        }

        if (this->m_fileBrowser.showFileDialog("Save As", imgui_addons::ImGuiFileBrowser::DialogMode::SAVE)) {
            FILE *file = fopen(this->m_fileBrowser.selected_path.c_str(), "wb");

            if (file != nullptr) {
                std::vector<u8> buffer(0xFF'FFFF, 0x00);
                size_t bufferSize = buffer.size();

                fseek(file, 0, SEEK_SET);

                for (u64 offset = 0; offset < this->m_dataProvider->getActualSize(); offset += bufferSize) {
                    if (bufferSize > this->m_dataProvider->getActualSize() - offset)
                        bufferSize = this->m_dataProvider->getActualSize() - offset;

                    this->m_dataProvider->read(offset, buffer.data(), bufferSize);
                    fwrite(buffer.data(), 1, bufferSize, file);
                }

                fclose(file);
            }
        }
    }

    void ViewHexEditor::createMenu() {

        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open File...", "CTRL + O")) {
                this->getWindowOpenState() = true;
                View::doLater([]{ ImGui::OpenPopup("Open File"); });
            }

            if (ImGui::MenuItem("Save", "CTRL + S", false, this->m_dataProvider != nullptr && this->m_dataProvider->isWritable())) {
                for (const auto &[address, value] : this->m_dataProvider->getPatches())
                    this->m_dataProvider->writeRaw(address, &value, sizeof(u8));
            }

            if (ImGui::MenuItem("Save As...", "CTRL + SHIFT + S", false, this->m_dataProvider != nullptr && this->m_dataProvider->isWritable())) {
                View::doLater([]{ ImGui::OpenPopup("Save As"); });
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Open Project", "")) {
                this->getWindowOpenState() = true;
                View::doLater([]{ ImGui::OpenPopup("Open Project"); });
            }

            if (ImGui::MenuItem("Save Project", "", false, this->m_dataProvider != nullptr && this->m_dataProvider->isWritable())) {
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

            if (ImGui::BeginMenu("Export...", this->m_dataProvider != nullptr && this->m_dataProvider->isWritable())) {
                if (ImGui::MenuItem("IPS Patch")) {
                    Patches patches = this->m_dataProvider->getPatches();
                    if (!patches.contains(0x00454F45) && patches.contains(0x00454F46)) {
                        u8 value = 0;
                        this->m_dataProvider->read(0x00454F45, &value, sizeof(u8));
                        patches[0x00454F45] = value;
                    }

                    this->m_dataToSave = generateIPSPatch(patches);
                    View::doLater([]{ ImGui::OpenPopup("Export File"); });
                }
                if (ImGui::MenuItem("IPS32 Patch")) {
                    Patches patches = this->m_dataProvider->getPatches();
                    if (!patches.contains(0x00454F45) && patches.contains(0x45454F46)) {
                        u8 value = 0;
                        this->m_dataProvider->read(0x45454F45, &value, sizeof(u8));
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
                Bookmark bookmark = { start, end - start + 1, { }, { } };

                View::postEvent(Events::AddBookmark, &bookmark);
            }

            ImGui::EndMenu();
        }
    }

    bool ViewHexEditor::handleShortcut(int key, int mods) {
        if (mods == GLFW_MOD_CONTROL && key == GLFW_KEY_S) {
            for (const auto &[address, value] : this->m_dataProvider->getPatches())
                this->m_dataProvider->writeRaw(address, &value, sizeof(u8));
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
        if (this->m_dataProvider != nullptr)
            delete this->m_dataProvider;

        this->m_dataProvider = new prv::FileProvider(path);
        this->m_memoryEditor.ReadOnly = !this->m_dataProvider->isWritable();

        if (this->m_dataProvider->isAvailable())
            ProjectFile::setFilePath(path);

        this->getWindowOpenState() = true;

        View::postEvent(Events::FileLoaded);
        View::postEvent(Events::DataChanged);
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
        size_t start = std::min(this->m_memoryEditor.DataPreviewAddr, this->m_memoryEditor.DataPreviewAddrEnd);
        size_t end = std::max(this->m_memoryEditor.DataPreviewAddr, this->m_memoryEditor.DataPreviewAddrEnd);

        size_t copySize = (end - start) + 1;

        std::vector<u8> buffer(copySize, 0x00);
        this->m_dataProvider->read(start, buffer.data(), buffer.size());

        std::string str;
        for (const auto &byte : buffer)
            str += hex::format("%02X ", byte);
        str.pop_back();

        ImGui::SetClipboardText(str.c_str());
    }

    void ViewHexEditor::copyString() {
        size_t start = std::min(this->m_memoryEditor.DataPreviewAddr, this->m_memoryEditor.DataPreviewAddrEnd);
        size_t end = std::max(this->m_memoryEditor.DataPreviewAddr, this->m_memoryEditor.DataPreviewAddrEnd);

        size_t copySize = (end - start) + 1;

        std::string buffer(copySize, 0x00);
        buffer.reserve(copySize + 1);
        this->m_dataProvider->read(start, buffer.data(), copySize);

        ImGui::SetClipboardText(buffer.c_str());
    }

    void ViewHexEditor::copyLanguageArray(Language language) {
        size_t start = std::min(this->m_memoryEditor.DataPreviewAddr, this->m_memoryEditor.DataPreviewAddrEnd);
        size_t end = std::max(this->m_memoryEditor.DataPreviewAddr, this->m_memoryEditor.DataPreviewAddrEnd);

        size_t copySize = (end - start) + 1;

        std::vector<u8> buffer(copySize, 0x00);
        this->m_dataProvider->read(start, buffer.data(), buffer.size());

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
        size_t start = std::min(this->m_memoryEditor.DataPreviewAddr, this->m_memoryEditor.DataPreviewAddrEnd);
        size_t end = std::max(this->m_memoryEditor.DataPreviewAddr, this->m_memoryEditor.DataPreviewAddrEnd);

        size_t copySize = (end - start) + 1;

        std::vector<u8> buffer(copySize, 0x00);
        this->m_dataProvider->read(start, buffer.data(), buffer.size());

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
        size_t start = std::min(this->m_memoryEditor.DataPreviewAddr, this->m_memoryEditor.DataPreviewAddrEnd);
        size_t end = std::max(this->m_memoryEditor.DataPreviewAddr, this->m_memoryEditor.DataPreviewAddrEnd);

        size_t copySize = (end - start) + 1;

        std::vector<u8> buffer(copySize, 0x00);
        this->m_dataProvider->read(start, buffer.data(), buffer.size());

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

            *_this->m_lastSearchBuffer = _this->m_searchFunction(_this->m_dataProvider, data->Buf);
            _this->m_lastSearchIndex = 0;

            if (_this->m_lastSearchBuffer->size() > 0)
                _this->m_memoryEditor.GotoAddrAndHighlight((*_this->m_lastSearchBuffer)[0].first, (*_this->m_lastSearchBuffer)[0].second);

            return 0;
        };

        static auto Find = [this](char *buffer) {
            *this->m_lastSearchBuffer = this->m_searchFunction(this->m_dataProvider, buffer);
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
        if (ImGui::BeginPopup("Goto")) {
            ImGui::TextUnformatted("Goto");
            if (ImGui::BeginTabBar("gotoTabs")) {
                s64 newOffset = 0;
                if (ImGui::BeginTabItem("Begin")) {
                    ImGui::InputScalar("##nolabel", ImGuiDataType_U64, &this->m_gotoAddress, nullptr, nullptr, "%llx", ImGuiInputTextFlags_CharsHexadecimal);

                    if (this->m_gotoAddress >= this->m_dataProvider->getActualSize())
                        this->m_gotoAddress = this->m_dataProvider->getActualSize() - 1;

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
                    if (newOffset >= this->m_dataProvider->getActualSize()) {
                        newOffset = this->m_dataProvider->getActualSize() - 1;
                        this->m_gotoAddress = (this->m_dataProvider->getActualSize() - 1) - currHighlightStart;
                    } else if (newOffset < 0) {
                        newOffset = 0;
                        this->m_gotoAddress = -currHighlightStart;
                    }

                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("End")) {
                    ImGui::InputScalar("##nolabel", ImGuiDataType_U64, &this->m_gotoAddress, nullptr, nullptr, "%llx", ImGuiInputTextFlags_CharsHexadecimal);

                    if (this->m_gotoAddress >= this->m_dataProvider->getActualSize())
                        this->m_gotoAddress = this->m_dataProvider->getActualSize() - 1;

                    newOffset = (this->m_dataProvider->getActualSize() - 1) - this->m_gotoAddress;

                    ImGui::EndTabItem();
                }

                if (ImGui::Button("Goto")) {
                    this->m_dataProvider->setCurrentPage(std::floor(newOffset / double(prv::Provider::PageSize)));
                    this->m_memoryEditor.GotoAddr = newOffset;
                    this->m_memoryEditor.DataPreviewAddr = newOffset;
                    this->m_memoryEditor.DataPreviewAddrEnd = newOffset;
                }

                ImGui::EndTabBar();
            }

            ImGui::EndPopup();
        }
    }

}