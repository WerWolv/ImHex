#pragma once

#include <windows.h>
#include <shobjidl.h>

#include "utils.hpp"
#include "views/view.hpp"

#include "imgui_memory_editor.h"

#include <tuple>
#include <random>
#include <vector>

namespace hex {

    class ViewHexEditor : public View {
    public:
        ViewHexEditor() : View() {
            this->m_memoryEditor.ReadFn = [](const ImU8* data, size_t off) -> ImU8 {
                ViewHexEditor *_this = (ViewHexEditor*)data;

                if (_this->m_file == nullptr)
                    return 0x00;

                fseek(_this->m_file, off, SEEK_SET);

                ImU8 byte;
                fread(&byte, sizeof(ImU8), 1, _this->m_file);

                return byte;
            };

            this->m_memoryEditor.WriteFn = [](ImU8* data, size_t off, ImU8 d) -> void {
                ViewHexEditor *_this = (ViewHexEditor*)data;

                if (_this->m_file == nullptr)
                    return;

                fseek(_this->m_file, off, SEEK_SET);

                fwrite(&d, sizeof(ImU8), 1, _this->m_file);

            };

            this->m_memoryEditor.HighlightFn = [](const ImU8* data, size_t off, bool next) -> bool {
                ViewHexEditor *_this = (ViewHexEditor*)data;

                for (auto& [offset, size, color] : _this->m_highlights) {
                    if (next && off == (offset + size))
                            return false;

                    if (off >= offset && off < (offset + size)) {
                        _this->m_memoryEditor.HighlightColor = color;
                        return true;
                    }
                }

                return false;
            };
        }
        virtual ~ViewHexEditor() { }

        virtual void createView() override {
            this->m_memoryEditor.DrawWindow("Hex Editor", this, this->m_file == nullptr ? 0x00 : this->m_fileSize);
        }

        virtual void createMenu() override {
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
        }

        void setHighlight(u64 offset, size_t size, u32 color = 0) {
            if (color == 0)
                color = std::mt19937(std::random_device()())();

            color |= 0xFF00'0000;

            this->m_highlights.emplace_back(offset, size, color);
        }

        void clearHighlights() {
            this->m_highlights.clear();
        }

    private:
        MemoryEditor m_memoryEditor;

        FILE *m_file = nullptr;
        size_t m_fileSize = 0;

        std::vector<std::tuple<u64, size_t, u32>> m_highlights;
    };

}