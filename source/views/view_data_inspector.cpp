#include "views/view_data_inspector.hpp"

#include "providers/provider.hpp"
#include "helpers/utils.hpp"

#include <cstring>

extern int ImTextCharFromUtf8(unsigned int* out_char, const char* in_text, const char* in_text_end);

namespace hex {

    ViewDataInspector::ViewDataInspector() : View("Data Inspector") {
        View::subscribeEvent(Events::RegionSelected, [this](const void* userData){
            Region region = *static_cast<const Region*>(userData);

            auto provider = SharedData::currentProvider;

            if (provider == nullptr) {
                this->m_validBytes = 0;
                return;
            }

            if (region.address == (size_t)-1) {
                this->m_validBytes = 0;
                return;
            }

            this->m_validBytes = std::min(u64(provider->getSize() - region.address), u64(sizeof(PreviewData)));
            std::memset(&this->m_previewData, 0x00, sizeof(PreviewData));
            provider->read(region.address, &this->m_previewData, this->m_validBytes);

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

            {
                std::string binary;
                for (u8 i = 0; i < 8; i++)
                    binary += ((this->m_previewData.unsigned8 << i) & 0x80) == 0 ? '0' : '1';
                this->m_cachedData.emplace_back("Binary (8 bit)", binary, sizeof(u8));
            }

            std::string unsignedFormat, signedFormat;

            switch (this->m_numberDisplayStyle) {
                case NumberDisplayStyle::Decimal:
                    unsignedFormat = "%llu";
                    signedFormat = "%lld";
                    break;
                case NumberDisplayStyle::Hexadecimal:
                    unsignedFormat = "0x%llX";
                    signedFormat = "0x%llX";
                    break;
                case NumberDisplayStyle::Octal:
                    unsignedFormat = "0o%llo";
                    signedFormat = "0o%llo";
                    break;
            }

            this->m_cachedData.emplace_back("uint8_t",  hex::format(unsignedFormat.c_str(), hex::changeEndianess(this->m_previewData.unsigned8,  this->m_endian)), sizeof(u8));
            this->m_cachedData.emplace_back("int8_t",   hex::format(signedFormat.c_str(),   hex::changeEndianess(this->m_previewData.signed8,    this->m_endian)), sizeof(s8));
            this->m_cachedData.emplace_back("uint16_t", hex::format(unsignedFormat.c_str(), hex::changeEndianess(this->m_previewData.unsigned16, this->m_endian)), sizeof(u16));
            this->m_cachedData.emplace_back("int16_t",  hex::format(signedFormat.c_str(),   hex::changeEndianess(this->m_previewData.signed16,   this->m_endian)), sizeof(s16));
            this->m_cachedData.emplace_back("uint32_t", hex::format(unsignedFormat.c_str(), hex::changeEndianess(this->m_previewData.unsigned32, this->m_endian)), sizeof(u32));
            this->m_cachedData.emplace_back("int32_t",  hex::format(signedFormat.c_str(),   hex::changeEndianess(this->m_previewData.signed32,   this->m_endian)), sizeof(s32));
            this->m_cachedData.emplace_back("uint64_t", hex::format(unsignedFormat.c_str(), hex::changeEndianess(this->m_previewData.unsigned64, this->m_endian)), sizeof(u64));
            this->m_cachedData.emplace_back("int64_t",  hex::format(signedFormat.c_str(),   hex::changeEndianess(this->m_previewData.signed64,   this->m_endian)), sizeof(s64));

            this->m_cachedData.emplace_back("ASCII Character", hex::format("'%s'", makePrintable(this->m_previewData.ansiChar).c_str()), sizeof(char));
            this->m_cachedData.emplace_back("Wide Character",  hex::format("'%lc'", this->m_previewData.wideChar == 0 ? '\x01' : hex::changeEndianess(this->m_previewData.wideChar, this->m_endian)), sizeof(wchar_t));
            {
                char buffer[5] = { 0 };
                char codepointString[5] = { 0 };
                u32 codepoint = 0;

                std::memcpy(buffer, &this->m_previewData.utf8Char, 4);
                u8 codepointSize = ImTextCharFromUtf8(&codepoint, buffer, buffer + 4);

                std::memcpy(codepointString, &codepoint, std::min(codepointSize, u8(4)));
                this->m_cachedData.emplace_back("UTF-8 code point",  hex::format("'%s' (U+%04lx)",
                    codepoint == 0xFFFD ? "Invalid" :
                    codepoint < 0xFF ? makePrintable(codepoint).c_str() :
                    codepointString
                    , codepoint), sizeof(char8_t));
            }

            this->m_cachedData.emplace_back("float (32 bit)", hex::format("%e", hex::changeEndianess(this->m_previewData.float32, this->m_endian)), sizeof(float));
            this->m_cachedData.emplace_back("double (64 bit)",  hex::format("%e", hex::changeEndianess(this->m_previewData.float64, this->m_endian)), sizeof(double));

            #if defined(OS_WINDOWS) && defined(ARCH_64_BIT)
            {
                auto endianAdjustedTime = hex::changeEndianess(this->m_previewData.time32, this->m_endian);
                std::tm * ptm = _localtime32(&endianAdjustedTime);
                char buffer[32];
                if (ptm != nullptr && std::strftime(buffer, 32, "%a, %d.%m.%Y %H:%M:%S", ptm))
                    this->m_cachedData.emplace_back("__time32_t", buffer, sizeof(__time32_t));
                else
                    this->m_cachedData.emplace_back("__time32_t", "Invalid", sizeof(__time32_t));
            }

            {
                auto endianAdjustedTime = hex::changeEndianess(this->m_previewData.time64, this->m_endian);
                std::tm * ptm = _localtime64(&endianAdjustedTime);
                char buffer[64];
                if (ptm != nullptr && std::strftime(buffer, 64, "%a, %d.%m.%Y %H:%M:%S", ptm) != 0)
                    this->m_cachedData.emplace_back("__time64_t", buffer, sizeof(__time64_t));
                else
                    this->m_cachedData.emplace_back("__time64_t", "Invalid", sizeof(__time64_t));
            }
            #else
            {
                auto endianAdjustedTime = hex::changeEndianess(this->m_previewData.time, this->m_endian);
                std::tm * ptm = localtime(&endianAdjustedTime);
                char buffer[64];
                if (ptm != nullptr && std::strftime(buffer, 64, "%a, %d.%m.%Y %H:%M:%S", ptm) != 0)
                    this->m_cachedData.emplace_back("time_t", buffer, sizeof(time_t));
                else
                    this->m_cachedData.emplace_back("time_t", "Invalid", sizeof(time_t));
            }
            #endif

            this->m_cachedData.emplace_back("GUID", hex::format("%s{%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX}",
                (this->m_previewData.guid.data3 >> 12) <= 5 && ((this->m_previewData.guid.data4[0] >> 4) >= 8 || (this->m_previewData.guid.data4[0] >> 4) == 0) ? "" : "Invalid ",
                hex::changeEndianess(this->m_previewData.guid.data1, this->m_endian),
                hex::changeEndianess(this->m_previewData.guid.data2, this->m_endian),
                hex::changeEndianess(this->m_previewData.guid.data3, this->m_endian),
                this->m_previewData.guid.data4[0], this->m_previewData.guid.data4[1], this->m_previewData.guid.data4[2], this->m_previewData.guid.data4[3],
                this->m_previewData.guid.data4[4], this->m_previewData.guid.data4[5], this->m_previewData.guid.data4[6], this->m_previewData.guid.data4[7]),
                sizeof(GUID));
        }


        if (ImGui::Begin("Data Inspector", &this->getWindowOpenState(), ImGuiWindowFlags_NoCollapse)) {
            auto provider = SharedData::currentProvider;

            if (provider != nullptr && provider->isReadable()) {
                if (ImGui::BeginTable("##datainspector", 2,
                    ImGuiTableFlags_ScrollY | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg,
                    ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * (this->m_cachedData.size() + 2)))) {
                    ImGui::TableSetupScrollFreeze(0, 1);
                    ImGui::TableSetupColumn("Name");
                    ImGui::TableSetupColumn("Value");

                    ImGui::TableHeadersRow();

                    for (const auto &[name, value, size] : this->m_cachedData) {
                        if (this->m_validBytes < size) continue;

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(name.c_str());
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(value.c_str());
                    }

                    if (this->m_validBytes >= 4) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted("RGBA Color");
                        ImGui::TableNextColumn();
                        ImGui::ColorButton("##nolabel", ImColor(hex::changeEndianess(this->m_previewData.unsigned32, this->m_endian)),
                                           ImGuiColorEditFlags_None, ImVec2(ImGui::GetColumnWidth(), ImGui::GetTextLineHeight()));
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