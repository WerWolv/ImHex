#include "views/view_data_inspector.hpp"

#include <hex/providers/provider.hpp>

#include <cstring>

extern int ImTextCharFromUtf8(unsigned int* out_char, const char* in_text, const char* in_text_end);

namespace hex {

    using NumberDisplayStyle = ContentRegistry::DataInspector::NumberDisplayStyle;

    ViewDataInspector::ViewDataInspector() : View("Data Inspector") {
        View::subscribeEvent(Events::RegionSelected, [this](auto userData) {
            auto region = std::any_cast<Region>(userData);

            auto provider = SharedData::currentProvider;

            if (provider == nullptr) {
                this->m_validBytes = 0;
                return;
            }

            if (region.address == (size_t)-1) {
                this->m_validBytes = 0;
                return;
            }

            this->m_validBytes = u64(provider->getSize() - region.address);
            this->m_startAddress = region.address;

            this->m_shouldInvalidate = true;
        });
    }

    ViewDataInspector::~ViewDataInspector() {
        View::unsubscribeEvent(Events::RegionSelected);
    }

    void ViewDataInspector::drawContent() {
        if (this->m_shouldInvalidate) {
            this->m_shouldInvalidate = false;

            this->m_cachedData.clear();
            auto provider = SharedData::currentProvider;
            for (auto &entry : ContentRegistry::DataInspector::getEntries()) {
                if (this->m_validBytes < entry.requiredSize)
                    continue;

                std::vector<u8> buffer(entry.requiredSize);
                provider->read(this->m_startAddress, buffer.data(), buffer.size());
                this->m_cachedData.emplace_back(entry.name, entry.generatorFunction(buffer, this->m_endian, this->m_numberDisplayStyle));
            }
        }


        if (ImGui::Begin("Data Inspector", &this->getWindowOpenState(), ImGuiWindowFlags_NoCollapse)) {
            auto provider = SharedData::currentProvider;

            if (provider != nullptr && provider->isReadable()) {
                if (ImGui::BeginTable("##datainspector", 2,
                    ImGuiTableFlags_ScrollY | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg,
                    ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * (this->m_cachedData.size() + 1)))) {
                    ImGui::TableSetupScrollFreeze(0, 1);
                    ImGui::TableSetupColumn("Name");
                    ImGui::TableSetupColumn("Value");

                    ImGui::TableHeadersRow();

                    for (const auto &[name, function] : this->m_cachedData) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(name.c_str());
                        ImGui::TableNextColumn();
                        function();
                    }

                    ImGui::EndTable();
                }

                ImGui::NewLine();

                if (ImGui::RadioButton("Little Endian", this->m_endian == std::endian::little)) {
                    this->m_endian = std::endian::little;
                    this->m_shouldInvalidate = true;
                }
                ImGui::SameLine();
                if (ImGui::RadioButton("Big Endian", this->m_endian == std::endian::big)) {
                    this->m_endian = std::endian::big;
                    this->m_shouldInvalidate = true;
                }

                if (ImGui::RadioButton("Decimal", this->m_numberDisplayStyle == NumberDisplayStyle::Decimal)) {
                    this->m_numberDisplayStyle = NumberDisplayStyle::Decimal;
                    this->m_shouldInvalidate = true;
                }
                ImGui::SameLine();
                if (ImGui::RadioButton("Hexadecimal", this->m_numberDisplayStyle == NumberDisplayStyle::Hexadecimal)) {
                    this->m_numberDisplayStyle = NumberDisplayStyle::Hexadecimal;
                    this->m_shouldInvalidate = true;
                }
                ImGui::SameLine();
                if (ImGui::RadioButton("Octal", this->m_numberDisplayStyle == NumberDisplayStyle::Octal)) {
                    this->m_numberDisplayStyle = NumberDisplayStyle::Octal;
                    this->m_shouldInvalidate = true;
                }
            }
        }
        ImGui::End();
    }

    void ViewDataInspector::drawMenu() {

    }

}