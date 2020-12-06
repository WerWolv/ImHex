#include "views/view_data_inspector.hpp"

#include "providers/provider.hpp"
#include "helpers/utils.hpp"

#include <cstring>

extern int ImTextCharFromUtf8(unsigned int* out_char, const char* in_text, const char* in_text_end);

namespace hex {

    ViewDataInspector::ViewDataInspector(prv::Provider* &dataProvider) : View("Data Inspector"), m_dataProvider(dataProvider) {
        View::subscribeEvent(Events::RegionSelected, [this](const void* userData){
            Region region = *static_cast<const Region*>(userData);

            this->m_validBytes = std::min(u64(this->m_dataProvider->getSize() - region.address), u64(sizeof(PreviewData)));
            std::memset(&this->m_previewData, 0x00, sizeof(PreviewData));
            this->m_dataProvider->read(region.address, &this->m_previewData, this->m_validBytes);

            this->m_shouldInvalidate = true;
        });
    }

    ViewDataInspector::~ViewDataInspector() {
        View::unsubscribeEvent(Events::RegionSelected);
    }

    void ViewDataInspector::createView() {
        if (this->m_shouldInvalidate) {
            this->m_shouldInvalidate = false;

            this->m_cachedData.clear();

            {
                std::string binary;
                for (u8 i = 0; i < 8; i++)
                    binary += ((this->m_previewData.unsigned8 << i) & 0x80) == 0 ? '0' : '1';
                this->m_cachedData.emplace_back("Binary (8 bit)", binary);
            }

            this->m_cachedData.emplace_back("uint8_t",  hex::format("%u",   hex::changeEndianess(this->m_previewData.unsigned8, this->m_endianess)));
            this->m_cachedData.emplace_back("int8_t",   hex::format("%d",   hex::changeEndianess(this->m_previewData.signed8, this->m_endianess)));
            this->m_cachedData.emplace_back("uint16_t", hex::format("%u",   hex::changeEndianess(this->m_previewData.unsigned16, this->m_endianess)));
            this->m_cachedData.emplace_back("int16_t",  hex::format("%d",   hex::changeEndianess(this->m_previewData.signed16, this->m_endianess)));
            this->m_cachedData.emplace_back("uint32_t", hex::format("%lu",  hex::changeEndianess(this->m_previewData.unsigned32, this->m_endianess)));
            this->m_cachedData.emplace_back("int32_t",  hex::format("%ld",  hex::changeEndianess(this->m_previewData.signed32, this->m_endianess)));
            this->m_cachedData.emplace_back("uint64_t", hex::format("%llu", hex::changeEndianess(this->m_previewData.unsigned64, this->m_endianess)));
            this->m_cachedData.emplace_back("int64_t",  hex::format("%lld", hex::changeEndianess(this->m_previewData.signed64, this->m_endianess)));

            this->m_cachedData.emplace_back("ASCII Character",  hex::format("'%s'", makePrintable(this->m_previewData.ansiChar).c_str()));
            this->m_cachedData.emplace_back("Wide Character",  hex::format("'%lc'", this->m_previewData.wideChar == 0 ? '\x01' : hex::changeEndianess(this->m_previewData.wideChar, this->m_endianess)));
            {
                char buffer[5] = { 0 };
                char codepointString[5] = { 0 };
                u32 codepoint = 0;

                std::memcpy(buffer, &this->m_previewData.utf8Char, 4);
                u8 codepointSize = ImTextCharFromUtf8(&codepoint, buffer, buffer + 4);

                std::memcpy(codepointString, &codepoint, std::min(codepointSize, u8(4)));
                this->m_cachedData.emplace_back("UTF-8 code point",  hex::format("'%s' (U+%04lx)", codepoint == 0xFFFD ? "Invalid" : codepointString, codepoint));
            }

            this->m_cachedData.emplace_back("float (32 bit)", hex::format("%e", hex::changeEndianess(this->m_previewData.float32, this->m_endianess)));
            this->m_cachedData.emplace_back("double (64 bit)",  hex::format("%e", hex::changeEndianess(this->m_previewData.float64, this->m_endianess)));

            #if defined(_WIN64)
            {
                auto endianAdjustedTime = hex::changeEndianess(this->m_previewData.time32, this->m_endianess);
                std::tm * ptm = _localtime32(&endianAdjustedTime);
                char buffer[32];
                if (ptm != nullptr && std::strftime(buffer, 32, "%a, %d.%m.%Y %H:%M:%S", ptm))
                    this->m_cachedData.emplace_back("__time32_t", buffer);
                else
                    this->m_cachedData.emplace_back("__time32_t", "Invalid");
            }

            {
                auto endianAdjustedTime = hex::changeEndianess(this->m_previewData.time64, this->m_endianess);
                std::tm * ptm = _localtime64(&endianAdjustedTime);
                char buffer[64];
                if (ptm != nullptr && std::strftime(buffer, 64, "%a, %d.%m.%Y %H:%M:%S", ptm) != 0)
                    this->m_cachedData.emplace_back("__time64_t", buffer);
                else
                    this->m_cachedData.emplace_back("__time64_t", "Invalid");
            }
            #else
            {
                auto endianAdjustedTime = hex::changeEndianess(this->m_previewData.time, this->m_endianess);
                std::tm * ptm = localtime(&endianAdjustedTime);
                char buffer[64];
                if (ptm != nullptr && std::strftime(buffer, 64, "%a, %d.%m.%Y %H:%M:%S", ptm) != 0)
                    this->m_cachedData.emplace_back("time_t", buffer);
                else
                    this->m_cachedData.emplace_back("time_t", "Invalid");
            }
            #endif

            this->m_cachedData.emplace_back("GUID", hex::format("%s{%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX}",
                (this->m_previewData.guid.data3 >> 12) <= 5 && ((this->m_previewData.guid.data4[0] >> 4) >= 8 || (this->m_previewData.guid.data4[0] >> 4) == 0) ? "" : "[INVALID] ",
                hex::changeEndianess(this->m_previewData.guid.data1, this->m_endianess),
                hex::changeEndianess(this->m_previewData.guid.data2, this->m_endianess),
                hex::changeEndianess(this->m_previewData.guid.data3, this->m_endianess),
                this->m_previewData.guid.data4[0], this->m_previewData.guid.data4[1], this->m_previewData.guid.data4[2], this->m_previewData.guid.data4[3],
                this->m_previewData.guid.data4[4], this->m_previewData.guid.data4[5], this->m_previewData.guid.data4[6], this->m_previewData.guid.data4[7]));
        }


        if (ImGui::Begin("Data Inspector", &this->getWindowOpenState(), ImGuiWindowFlags_NoCollapse)) {
            if (this->m_dataProvider != nullptr && this->m_dataProvider->isReadable()) {
                if (ImGui::BeginChild("##scrolling", ImVec2(0, ImGui::GetWindowHeight() - 60))) {
                    if (ImGui::BeginTable("##datainspector", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody)) {
                        ImGui::TableSetupColumn("Name");
                        ImGui::TableSetupColumn("Value");

                        ImGui::TableHeadersRow();

                        for (const auto &[name, value] : this->m_cachedData) {
                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted(name.c_str());
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted(value.c_str());
                        }

                        {
                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted("RGBA Color");
                            ImGui::TableNextColumn();
                            ImGui::ColorButton("##nolabel", ImColor(hex::changeEndianess(this->m_previewData.unsigned32, this->m_endianess)),
                                               ImGuiColorEditFlags_None, ImVec2(ImGui::GetColumnWidth(), 15));
                        }


                        ImGui::EndTable();
                    }
                }
                ImGui::EndChild();

                if (ImGui::RadioButton("Little Endian", this->m_endianess == std::endian::little)) {
                    this->m_endianess = std::endian::little;
                    this->m_shouldInvalidate = true;
                }
                ImGui::SameLine();
                if (ImGui::RadioButton("Big Endian", this->m_endianess == std::endian::big)) {
                    this->m_endianess = std::endian::big;
                    this->m_shouldInvalidate = true;
                }
            }
        }
        ImGui::End();
    }

    void ViewDataInspector::createMenu() {

    }

}