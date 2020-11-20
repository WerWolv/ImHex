#include "views/view_hexeditor.hpp"

#include "providers/provider.hpp"
#include "providers/file_provider.hpp"

#include <GLFW/glfw3.h>

namespace hex {

    ViewHexEditor::ViewHexEditor(prv::Provider* &dataProvider, std::vector<lang::PatternData*> &patternData)
            : View(), m_dataProvider(dataProvider), m_patternData(patternData) {

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
    }

    ViewHexEditor::~ViewHexEditor() {
        if (this->m_dataProvider != nullptr)
            delete this->m_dataProvider;
        this->m_dataProvider = nullptr;
    }

    void ViewHexEditor::createView() {
        if (!this->m_memoryEditor.Open)
            return;

        size_t dataSize = (this->m_dataProvider == nullptr || !this->m_dataProvider->isReadable()) ? 0x00 : this->m_dataProvider->getSize();

        this->m_memoryEditor.DrawWindow("Hex Editor", this, dataSize);

        if (dataSize != 0x00) {
            this->drawSearchPopup();
            this->drawGotoPopup();
        }

        if (this->m_fileBrowser.showFileDialog("Open File", imgui_addons::ImGuiFileBrowser::DialogMode::OPEN)) {
            this->openFile(this->m_fileBrowser.selected_path);
        }

    }

    void ViewHexEditor::openFile(std::string path) {
        if (this->m_dataProvider != nullptr)
            delete this->m_dataProvider;

        this->m_dataProvider = new prv::FileProvider(path);
        View::postEvent(Events::DataChanged);
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

        std::string buffer;
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

    void ViewHexEditor::createMenu() {

        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open File...", "CTRL + O")) {
                View::doLater([]{ ImGui::OpenPopup("Open File"); });
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Search", "CTRL + F")) {
                View::doLater([]{ ImGui::OpenPopup("Search"); });
            }

            if (ImGui::MenuItem("Goto", "CTRL + G")) {
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


            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Hex View", "", &this->m_memoryEditor.Open);
            ImGui::EndMenu();
        }
    }

    bool ViewHexEditor::handleShortcut(int key, int mods) {
        if (mods == GLFW_MOD_CONTROL && key == GLFW_KEY_F) {
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


    static std::vector<std::pair<u64, u64>> findString(prv::Provider* &provider, std::string string) {
        std::vector<std::pair<u64, u64>> results;

        u32 foundCharacters = 0;

        std::vector<u8> buffer(1024, 0x00);
        size_t dataSize = provider->getSize();
        for (u64 offset = 0; offset < dataSize; offset += 1024) {
            size_t usedBufferSize = std::min(buffer.size(), dataSize - offset);
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
            size_t usedBufferSize = std::min(buffer.size(), dataSize - offset);
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

                    if (this->m_gotoAddress >= this->m_dataProvider->getSize())
                        this->m_gotoAddress = this->m_dataProvider->getSize() - 1;

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
                    if (newOffset >= this->m_dataProvider->getSize()) {
                        newOffset = this->m_dataProvider->getSize() - 1;
                        this->m_gotoAddress = (this->m_dataProvider->getSize() - 1) - currHighlightStart;
                    } else if (newOffset < 0) {
                        newOffset = 0;
                        this->m_gotoAddress = -currHighlightStart;
                    }

                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("End")) {
                    ImGui::InputScalar("##nolabel", ImGuiDataType_U64, &this->m_gotoAddress, nullptr, nullptr, "%llx", ImGuiInputTextFlags_CharsHexadecimal);

                    if (this->m_gotoAddress >= this->m_dataProvider->getSize())
                        this->m_gotoAddress = this->m_dataProvider->getSize() - 1;

                    newOffset = (this->m_dataProvider->getSize() - 1) - this->m_gotoAddress;

                    ImGui::EndTabItem();
                }

                if (ImGui::Button("Goto")) {
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