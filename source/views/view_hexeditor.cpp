#include "views/view_hexeditor.hpp"

#include "providers/provider.hpp"
#include "providers/file_provider.hpp"

#include <GLFW/glfw3.h>

namespace hex {

    ViewHexEditor::ViewHexEditor(prv::Provider* &dataProvider, std::vector<Highlight> &highlights)
            : View(), m_dataProvider(dataProvider), m_highlights(highlights) {

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
        };

        this->m_memoryEditor.HighlightFn = [](const ImU8 *data, size_t off, bool next) -> bool {
            ViewHexEditor *_this = (ViewHexEditor *) data;

            for (auto&[offset, size, color, name] : _this->m_highlights) {
                if (next && off == (offset + size)) {
                    return false;
                }

                if (off >= offset && off < (offset + size)) {
                    _this->m_memoryEditor.HighlightColor = color;
                    return true;
                }
            }

            _this->m_memoryEditor.HighlightColor = 0x50C08080;
            return false;
        };
    }

    ViewHexEditor::~ViewHexEditor() {}

    auto findString(prv::Provider* &provider, std::string string) {
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
                    results.push_back({ offset + i - foundCharacters + 1, offset + i + 1 });
                    foundCharacters = 0;
                }
            }
        }



        return results;
    }

    void ViewHexEditor::createView() {
        this->m_memoryEditor.DrawWindow("Hex Editor", this, (this->m_dataProvider == nullptr || !this->m_dataProvider->isReadable()) ? 0x00 : this->m_dataProvider->getSize());

        if (ImGui::BeginPopup("Search")) {
            ImGui::TextUnformatted("Search");
            ImGui::InputText("##nolabel", this->m_searchBuffer, 0xFFFF, ImGuiInputTextFlags_CallbackEdit,
                 [](ImGuiInputTextCallbackData* data) -> int {
                    auto lastSearch = static_cast<std::vector<std::pair<u64, u64>>*>(data->UserData);

                    lastSearch->clear();

                    return 0;
                 }, &this->m_lastSearch);


            if (ImGui::Button("Find")) {
                this->m_lastSearch = findString(this->m_dataProvider, this->m_searchBuffer);
                this->m_lastSearchIndex = 0;

                if (this->m_lastSearch.size() > 0)
                    this->m_memoryEditor.GotoAddrAndHighlight(this->m_lastSearch[0].first, this->m_lastSearch[0].second);
            }

            if (this->m_lastSearch.size() > 0) {

                if ((ImGui::Button("Find Next"))) {
                    if (this->m_lastSearch.size() > 0) {
                        ++this->m_lastSearchIndex %= this->m_lastSearch.size();
                        this->m_memoryEditor.GotoAddrAndHighlight(this->m_lastSearch[this->m_lastSearchIndex].first,
                                                                  this->m_lastSearch[this->m_lastSearchIndex].second);
                    }
                }

                ImGui::SameLine();

                if ((ImGui::Button("Find Prev"))) {
                    if (this->m_lastSearch.size() > 0) {
                        this->m_lastSearchIndex--;

                        if (this->m_lastSearchIndex < 0)
                            this->m_lastSearchIndex = this->m_lastSearch.size() - 1;

                        this->m_lastSearchIndex %= this->m_lastSearch.size();

                        this->m_memoryEditor.GotoAddrAndHighlight(this->m_lastSearch[this->m_lastSearchIndex].first,
                                                                  this->m_lastSearch[this->m_lastSearchIndex].second);
                    }
                }
            }

            ImGui::EndPopup();
        }
    }

    void ViewHexEditor::createMenu() {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open File...")) {
                auto filePath = openFileDialog();
                if (filePath.has_value()) {
                    if (this->m_dataProvider != nullptr)
                        delete this->m_dataProvider;

                    this->m_dataProvider = new prv::FileProvider(filePath.value());
                }

            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Hex View", "", &this->m_memoryEditor.Open);
            ImGui::EndMenu();
        }
    }

    bool ViewHexEditor::handleShortcut(int key, int mods) {
        if (mods & GLFW_MOD_CONTROL && key == GLFW_KEY_F) {
            ImGui::OpenPopup("Search");
            return true;
        }

        return false;
    }

}