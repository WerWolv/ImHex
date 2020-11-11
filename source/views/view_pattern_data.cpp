#include "views/view_pattern_data.hpp"

#include <cstring>

namespace hex {

    ViewPatternData::ViewPatternData(prv::Provider* &dataProvider, std::vector<Highlight> &highlights)
        : View(), m_dataProvider(dataProvider), m_highlights(highlights) {

    }

    ViewPatternData::~ViewPatternData() {

    }

    std::string makeDisplayable(u8 *data, size_t size) {
        std::string result;
        for (u8* c = data; c < (data + size - 1); c++) {
            if (iscntrl(*c) || *c > 0x7F)
                result += " ";
            else
                result += *c;
        }

        return result;
    }

    void ViewPatternData::createView() {
        if (!this->m_windowOpen)
            return;

        if (ImGui::Begin("Pattern Data", &this->m_windowOpen)) {
            ImGui::BeginChild("##scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav);

            for (auto& [offset, size, color, name] : this->m_highlights) {
                std::vector<u8> buffer(size + 1, 0x00);

                this->m_dataProvider->read(offset, buffer.data(), size);

                if (size <= 8) {
                    u64 data = 0;
                    std::memcpy(&data, buffer.data(), size);

                    ImGui::LabelText(name.c_str(), "[0x%08lx:0x%08lx]   %lu (0x%08lx) \"%s\"", offset, offset + size, data, data, makeDisplayable(buffer.data(), buffer.size()).c_str());
                }
                else
                    ImGui::LabelText(name.c_str(), "[0x%08lx:0x%08lx]   [ ARRAY ] \"%s\"", offset, offset + size, makeDisplayable(buffer.data(), buffer.size()).c_str());
            }

            ImGui::EndChild();
            ImGui::End();
        }
    }

    void ViewPatternData::createMenu() {
        if (ImGui::BeginMenu("Window")) {
            ImGui::MenuItem("Data View", "", &this->m_windowOpen);
            ImGui::EndMenu();
        }
    }

}