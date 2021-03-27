#include "views/view_data_inspector.hpp"

#include <hex/providers/provider.hpp>

#include <cstring>

extern int ImTextCharFromUtf8(unsigned int* out_char, const char* in_text, const char* in_text_end);

namespace hex {

    using NumberDisplayStyle = ContentRegistry::DataInspector::NumberDisplayStyle;

    ViewDataInspector::ViewDataInspector() : View("hex.view.data_inspector.name") {
        EventManager::subscribe<EventRegionSelected>(this, [this](Region region) {
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
        EventManager::unsubscribe<EventRegionSelected>(this);
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

                this->m_cachedData.push_back({ entry.unlocalizedName, entry.generatorFunction(buffer, this->m_endian, this->m_numberDisplayStyle) });
            }
        }


        if (ImGui::Begin(View::toWindowName("hex.view.data_inspector.name").c_str(), &this->getWindowOpenState(), ImGuiWindowFlags_NoCollapse)) {
            auto provider = SharedData::currentProvider;

            if (provider != nullptr && provider->isReadable()) {
                if (ImGui::BeginTable("##datainspector", 2,
                    ImGuiTableFlags_ScrollY | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg,
                    ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * (this->m_cachedData.size() + 1)))) {
                    ImGui::TableSetupScrollFreeze(0, 1);
                    ImGui::TableSetupColumn("hex.view.data_inspector.table.name"_lang);
                    ImGui::TableSetupColumn("hex.view.data_inspector.table.value"_lang);

                    ImGui::TableHeadersRow();

                    u32 i = 0;
                    for (const auto &[unlocalizedName, function] : this->m_cachedData) {
                        ImGui::PushID(i);
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(LangEntry(unlocalizedName));
                        ImGui::TableNextColumn();
                        const auto &copyValue = function();
                        ImGui::SameLine();
                        if (ImGui::Selectable("##InspectorLine", false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap)) {
                            ImGui::SetClipboardText(copyValue.c_str());
                        }

                        ImGui::PopID();
                        i++;
                    }

                    ImGui::EndTable();
                }

                ImGui::NewLine();

                if (ImGui::RadioButton("hex.common.little_endian"_lang, this->m_endian == std::endian::little)) {
                    this->m_endian = std::endian::little;
                    this->m_shouldInvalidate = true;
                }
                ImGui::SameLine();
                if (ImGui::RadioButton("hex.common.big_endian"_lang, this->m_endian == std::endian::big)) {
                    this->m_endian = std::endian::big;
                    this->m_shouldInvalidate = true;
                }

                if (ImGui::RadioButton("hex.common.decimal"_lang, this->m_numberDisplayStyle == NumberDisplayStyle::Decimal)) {
                    this->m_numberDisplayStyle = NumberDisplayStyle::Decimal;
                    this->m_shouldInvalidate = true;
                }
                ImGui::SameLine();
                if (ImGui::RadioButton("hex.common.hexadecimal"_lang, this->m_numberDisplayStyle == NumberDisplayStyle::Hexadecimal)) {
                    this->m_numberDisplayStyle = NumberDisplayStyle::Hexadecimal;
                    this->m_shouldInvalidate = true;
                }
                ImGui::SameLine();
                if (ImGui::RadioButton("hex.common.octal"_lang, this->m_numberDisplayStyle == NumberDisplayStyle::Octal)) {
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