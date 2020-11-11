#include "views/view_hexeditor.hpp"

#include "providers/file_provider.hpp"

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

    void ViewHexEditor::createView() {
        this->m_memoryEditor.DrawWindow("Hex Editor", this, (this->m_dataProvider == nullptr || !this->m_dataProvider->isReadable()) ? 0x00 : this->m_dataProvider->getSize());
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

}