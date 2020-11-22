#include "views/view_data_inspector.hpp"

#include "providers/provider.hpp"
#include "utils.hpp"

#include <cstring>

extern int ImTextCharFromUtf8(unsigned int* out_char, const char* in_text, const char* in_text_end);

namespace hex {

    ViewDataInspector::ViewDataInspector(prv::Provider* &dataProvider) : View(), m_dataProvider(dataProvider) {
        View::subscribeEvent(Events::ByteSelected, [this](const void* userData){
            size_t offset = *static_cast<const size_t*>(userData);

            this->m_validBytes = std::min(this->m_dataProvider->getSize() - offset, sizeof(PreviewData));
            std::memset(&this->m_previewData, 0x00, sizeof(PreviewData));
            this->m_dataProvider->read(offset, &this->m_previewData, sizeof(PreviewData));

            this->m_shouldInvalidate = true;
        });
    }

    ViewDataInspector::~ViewDataInspector() {
        View::unsubscribeEvent(Events::ByteSelected);
    }

    void ViewDataInspector::createView() {
        if (!this->m_windowOpen)
            return;

        if (this->m_shouldInvalidate) {
            this->m_shouldInvalidate = false;

            this->m_cachedData.clear();

            {
                std::string binary;
                for (u8 i = 0; i < 8; i++)
                    binary += ((this->m_previewData.unsigned8 << i) & 0x80) == 0 ? '0' : '1';
                this->m_cachedData.emplace_back("Binary (8 bit)", binary);
            }

            this->m_cachedData.emplace_back("uint8_t",  hex::format("%u",   this->m_previewData.unsigned8));
            this->m_cachedData.emplace_back("int8_t",   hex::format("%d",   this->m_previewData.signed8));
            this->m_cachedData.emplace_back("uint16_t", hex::format("%u",   this->m_previewData.unsigned16));
            this->m_cachedData.emplace_back("int16_t",  hex::format("%d",   this->m_previewData.signed16));
            this->m_cachedData.emplace_back("uint32_t", hex::format("%lu",  this->m_previewData.unsigned32));
            this->m_cachedData.emplace_back("int32_t",  hex::format("%ld",  this->m_previewData.signed32));
            this->m_cachedData.emplace_back("uint64_t", hex::format("%llu", this->m_previewData.unsigned64));
            this->m_cachedData.emplace_back("int64_t",  hex::format("%lld", this->m_previewData.signed64));

            this->m_cachedData.emplace_back("ANSI Character / char8_t",  hex::format("%c", this->m_previewData.ansiChar));
            this->m_cachedData.emplace_back("Wide Character / char16_t",  hex::format("%lc", this->m_previewData.wideChar));
            {
                char buffer[5] = { 0 };
                std::memcpy(buffer, &this->m_previewData.utf8Char, 4);
                u32 utf8 = 0;
                ImTextCharFromUtf8(&utf8, buffer, buffer + 4);
                this->m_cachedData.emplace_back("UTF-8 code point",  hex::format("U+%08lx", utf8));
            }

            this->m_cachedData.emplace_back("float (32 bit)", hex::format("%e", this->m_previewData.float32));
            this->m_cachedData.emplace_back("double (64 bit)",  hex::format("%e", this->m_previewData.float64));

            #if defined(_WIN64)
            {
                std::tm * ptm = _localtime32(&this->m_previewData.time32);
                char buffer[32];
                if (std::strftime(buffer, 32, "%a, %d.%m.%Y %H:%M:%S", ptm))
                    this->m_cachedData.emplace_back("__time32_t", buffer);
                else
                    this->m_cachedData.emplace_back("__time32_t", "Invalid");
            }

            {
                std::tm * ptm = _localtime64(&this->m_previewData.time64);
                char buffer[64];
                if (std::strftime(buffer, 64, "%a, %d.%m.%Y %H:%M:%S", ptm) != 0)
                    this->m_cachedData.emplace_back("__time64_t", buffer);
                else
                    this->m_cachedData.emplace_back("__time64_t", "Invalid");
            }
            #else
            {
                std::tm * ptm = localtime(&this->m_previewData.time);
                char buffer[64];
                if (std::strftime(buffer, 64, "%a, %d.%m.%Y %H:%M:%S", ptm) != 0)
                    this->m_cachedData.emplace_back("time_t", buffer);
                else
                    this->m_cachedData.emplace_back("time_t", "Invalid");
            }
            #endif

            this->m_cachedData.emplace_back("GUID", hex::format("{%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX}",
                this->m_previewData.guid.data1, this->m_previewData.guid.data2, this->m_previewData.guid.data3,
                this->m_previewData.guid.data4[0], this->m_previewData.guid.data4[1], this->m_previewData.guid.data4[2], this->m_previewData.guid.data4[3],
                this->m_previewData.guid.data4[4], this->m_previewData.guid.data4[5], this->m_previewData.guid.data4[6], this->m_previewData.guid.data4[7]));
        }


        if (ImGui::Begin("Data Inspector", &this->m_windowOpen)) {
            if (this->m_dataProvider != nullptr && this->m_dataProvider->isReadable()) {
                if (ImGui::BeginChild("##scrolling")) {
                    if (ImGui::BeginTable("##datainspector", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg)) {
                        ImGui::TableSetupColumn("Name");
                        ImGui::TableSetupColumn("Value");

                        ImGui::TableHeadersRow();
                        u32 rowCount = 0;
                        for (const auto &[name, value] : this->m_cachedData) {
                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted(name.c_str());
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted(value.c_str());
                            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                                                   ((rowCount % 2) == 0) ? 0xFF101010 : 0xFF303030);
                            rowCount++;
                        }

                        {
                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted("RGBA Color");
                            ImGui::TableNextColumn();
                            ImGui::ColorButton("##nolabel", ImColor(this->m_previewData.unsigned32),
                                               ImGuiColorEditFlags_None, ImVec2(ImGui::GetColumnWidth(), 15));
                            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                                                   ((rowCount % 2) == 0) ? 0xFF101010 : 0xFF303030);
                            rowCount++;
                        }


                        ImGui::EndTable();
                    }
                }
                ImGui::EndChild();
            }
        }
        ImGui::End();
    }

    void ViewDataInspector::createMenu() {
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Data Preview View", "", &this->m_windowOpen);
            ImGui::EndMenu();
        }
    }

}