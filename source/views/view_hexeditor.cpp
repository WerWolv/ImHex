#include "views/view_hexeditor.hpp"

namespace hex {

    ViewHexEditor::ViewHexEditor(FILE *&file, std::vector<Highlight> &highlights)
            : View(), m_file(file), m_highlights(highlights) {

        this->m_memoryEditor.ReadFn = [](const ImU8 *data, size_t off) -> ImU8 {
            ViewHexEditor *_this = (ViewHexEditor *) data;

            if (_this->m_file == nullptr)
                return 0x00;

            fseek(_this->m_file, off, SEEK_SET);

            ImU8 byte;
            fread(&byte, sizeof(ImU8), 1, _this->m_file);

            return byte;
        };

        this->m_memoryEditor.WriteFn = [](ImU8 *data, size_t off, ImU8 d) -> void {
            ViewHexEditor *_this = (ViewHexEditor *) data;

            if (_this->m_file == nullptr)
                return;

            fseek(_this->m_file, off, SEEK_SET);

            fwrite(&d, sizeof(ImU8), 1, _this->m_file);

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
        this->m_memoryEditor.DrawWindow("Hex Editor", this, this->m_file == nullptr ? 0x00 : this->m_fileSize);
    }

    void ViewHexEditor::createMenu() {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open File...")) {
                auto filePath = openFileDialog();
                if (filePath.has_value()) {
                    if (this->m_file != nullptr)
                        fclose(this->m_file);

                    this->m_file = fopen(filePath->c_str(), "r+b");

                    fseek(this->m_file, 0, SEEK_END);
                    this->m_fileSize = ftell(this->m_file);
                    rewind(this->m_file);
                }

            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Window")) {
            ImGui::MenuItem("Hex View", "", &this->m_memoryEditor.Open);
            ImGui::EndMenu();
        }
    }

}