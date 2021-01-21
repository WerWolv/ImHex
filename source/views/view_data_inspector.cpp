#include "views/view_data_inspector.hpp"

#include <hex/providers/provider.hpp>
#include <hex/helpers/utils.hpp>

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


        ContentRegistry::DataInspector::add("Binary (8 bit)", sizeof(u8), [](auto buffer, auto endian, auto style) {
            std::string binary;
            for (u8 i = 0; i < 8; i++)
                binary += ((buffer[0] << i) & 0x80) == 0 ? '0' : '1';

            return [binary] { ImGui::TextUnformatted(binary.c_str()); };
        });

        ContentRegistry::DataInspector::add("uint8_t", sizeof(u8), [](auto buffer, auto endian, auto style) {
            auto format = (style == NumberDisplayStyle::Decimal) ? "%u" : ((style == NumberDisplayStyle::Hexadecimal) ? "0x%X" : "0o%o");
            auto value = hex::format(format, *reinterpret_cast<u8*>(buffer.data()));
            return [value] { ImGui::TextUnformatted(value.c_str()); };
        });

        ContentRegistry::DataInspector::add("int8_t", sizeof(s8), [](auto buffer, auto endian, auto style) {
            auto format = (style == NumberDisplayStyle::Decimal) ? "%d" : ((style == NumberDisplayStyle::Hexadecimal) ? "0x%X" : "0o%o");
            auto value = hex::format(format, *reinterpret_cast<s8*>(buffer.data()));
            return [value] { ImGui::TextUnformatted(value.c_str()); };
        });

        ContentRegistry::DataInspector::add("uint16_t", sizeof(u16), [](auto buffer, auto endian, auto style) {
            auto format = (style == NumberDisplayStyle::Decimal) ? "%u" : ((style == NumberDisplayStyle::Hexadecimal) ? "0x%X" : "0o%o");
            auto value = hex::format(format, hex::changeEndianess(*reinterpret_cast<u16*>(buffer.data()), endian));
            return [value] { ImGui::TextUnformatted(value.c_str()); };
        });

        ContentRegistry::DataInspector::add("int16_t", sizeof(s16), [](auto buffer, auto endian, auto style) {
            auto format = (style == NumberDisplayStyle::Decimal) ? "%d" : ((style == NumberDisplayStyle::Hexadecimal) ? "0x%X" : "0o%o");
            auto value = hex::format(format, hex::changeEndianess(*reinterpret_cast<s16*>(buffer.data()), endian));
            return [value] { ImGui::TextUnformatted(value.c_str()); };
        });

        ContentRegistry::DataInspector::add("uint32_t", sizeof(u32), [](auto buffer, auto endian, auto style) {
            auto format = (style == NumberDisplayStyle::Decimal) ? "%u" : ((style == NumberDisplayStyle::Hexadecimal) ? "0x%X" : "0o%o");
            auto value = hex::format(format, hex::changeEndianess(*reinterpret_cast<u32*>(buffer.data()), endian));
            return [value] { ImGui::TextUnformatted(value.c_str()); };
        });

        ContentRegistry::DataInspector::add("int32_t", sizeof(s32), [](auto buffer, auto endian, auto style) {
            auto format = (style == NumberDisplayStyle::Decimal) ? "%d" : ((style == NumberDisplayStyle::Hexadecimal) ? "0x%X" : "0o%o");
            auto value = hex::format(format, hex::changeEndianess(*reinterpret_cast<s32*>(buffer.data()), endian));
            return [value] { ImGui::TextUnformatted(value.c_str()); };
        });

        ContentRegistry::DataInspector::add("uint64_t", sizeof(u64), [](auto buffer, auto endian, auto style) {
            auto format = (style == NumberDisplayStyle::Decimal) ? "%lu" : ((style == NumberDisplayStyle::Hexadecimal) ? "0x%lX" : "0o%lo");
            auto value = hex::format(format, hex::changeEndianess(*reinterpret_cast<u64*>(buffer.data()), endian));
            return [value] { ImGui::TextUnformatted(value.c_str()); };
        });

        ContentRegistry::DataInspector::add("int64_t", sizeof(s64), [](auto buffer, auto endian, auto style) {
            auto format = (style == NumberDisplayStyle::Decimal) ? "%ld" : ((style == NumberDisplayStyle::Hexadecimal) ? "0x%lX" : "0o%lo");
            auto value = hex::format(format, hex::changeEndianess(*reinterpret_cast<s64*>(buffer.data()), endian));
            return [value] { ImGui::TextUnformatted(value.c_str()); };
        });

        ContentRegistry::DataInspector::add("float (32 bit)", sizeof(float), [](auto buffer, auto endian, auto style) {
            auto value = hex::format("%e", hex::changeEndianess(*reinterpret_cast<float*>(buffer.data()), endian));
            return [value] { ImGui::TextUnformatted(value.c_str()); };
        });

        ContentRegistry::DataInspector::add("double (64 bit)", sizeof(double), [](auto buffer, auto endian, auto style) {
            auto value = hex::format("%e", hex::changeEndianess(*reinterpret_cast<double*>(buffer.data()), endian));
            return [value] { ImGui::TextUnformatted(value.c_str()); };
        });

        ContentRegistry::DataInspector::add("ASCII Character", sizeof(char8_t), [](auto buffer, auto endian, auto style) {
            auto value = hex::format("'%s'", makePrintable(*reinterpret_cast<char8_t*>(buffer.data())).c_str());
            return [value] { ImGui::TextUnformatted(value.c_str()); };
        });

        ContentRegistry::DataInspector::add("Wide Character", sizeof(char16_t), [](auto buffer, auto endian, auto style) {
            auto c = *reinterpret_cast<char16_t*>(buffer.data());
            auto value = hex::format("'%lc'", c == 0 ? '\x01' : hex::changeEndianess(c, endian));
            return [value] { ImGui::TextUnformatted(value.c_str()); };
        });

        ContentRegistry::DataInspector::add("UTF-8 code point", sizeof(char8_t) * 4, [](auto buffer, auto endian, auto style) {
            char utf8Buffer[5] = { 0 };
            char codepointString[5] = { 0 };
            u32 codepoint = 0;

            std::memcpy(utf8Buffer, reinterpret_cast<char8_t*>(buffer.data()), 4);
            u8 codepointSize = ImTextCharFromUtf8(&codepoint, utf8Buffer, utf8Buffer + 4);

            std::memcpy(codepointString, &codepoint, std::min(codepointSize, u8(4)));
            auto value = hex::format("'%s' (U+%04lx)",  codepoint == 0xFFFD ? "Invalid" :
                                                               codepoint < 0xFF ? makePrintable(codepoint).c_str() :
                                                               codepointString,
                                                               codepoint);

            return [value] { ImGui::TextUnformatted(value.c_str()); };
        });

        #if defined(OS_WINDOWS) && defined(ARCH_64_BIT)

            ContentRegistry::DataInspector::add("__time32_t", sizeof(__time32_t), [](auto buffer, auto endian, auto style) {
                auto endianAdjustedTime = hex::changeEndianess(*reinterpret_cast<__time32_t*>(buffer.data()), endian);
                std::tm * ptm = _localtime32(&endianAdjustedTime);
                char timeBuffer[32];
                std::string value;
                if (ptm != nullptr && std::strftime(timeBuffer, 32, "%a, %d.%m.%Y %H:%M:%S", ptm))
                    value = timeBuffer;
                else
                    value = "Invalid";

                return [value] { ImGui::TextUnformatted(value.c_str()); };
            });

            ContentRegistry::DataInspector::add("__time64_t", sizeof(__time64_t), [](auto buffer, auto endian, auto style) {
                auto endianAdjustedTime = hex::changeEndianess(*reinterpret_cast<__time64_t*>(buffer.data()), endian);
                std::tm * ptm = _localtime64(&endianAdjustedTime);
                char timeBuffer[64];
                std::string value;
                if (ptm != nullptr && std::strftime(timeBuffer, 64, "%a, %d.%m.%Y %H:%M:%S", ptm))
                    value = timeBuffer;
                else
                    value = "Invalid";

                return [value] { ImGui::TextUnformatted(value.c_str()); };
            });

        #else

            ContentRegistry::DataInspector::add("time_t", sizeof(time_t), [](auto buffer, auto endian, auto style) {
                auto endianAdjustedTime = hex::changeEndianess(*reinterpret_cast<time_t*>(buffer.data()), endian);
                std::tm * ptm = localtime(&endianAdjustedTime);
                char timeBuffer[64];
                std::string value;
                if (ptm != nullptr && std::strftime(timeBuffer, 64, "%a, %d.%m.%Y %H:%M:%S", ptm))
                    value = timeBuffer;
                else
                    value = "Invalid";

                return [value] { ImGui::TextUnformatted(value.c_str()); };
            });

        #endif

        ContentRegistry::DataInspector::add("GUID", sizeof(GUID), [](auto buffer, auto endian, auto style) {
            GUID guid;
            std::memcpy(&guid, buffer.data(), sizeof(GUID));
            auto value = hex::format("%s{%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX}",
                                     (hex::changeEndianess(guid.data3, endian) >> 12) <= 5 && ((guid.data4[0] >> 4) >= 8 || (guid.data4[0] >> 4) == 0) ? "" : "Invalid ",
                                     hex::changeEndianess(guid.data1, endian),
                                     hex::changeEndianess(guid.data2, endian),
                                     hex::changeEndianess(guid.data3, endian),
                                     guid.data4[0], guid.data4[1], guid.data4[2], guid.data4[3],
                                     guid.data4[4], guid.data4[5], guid.data4[6], guid.data4[7]);

            return [value] { ImGui::TextUnformatted(value.c_str()); };
        });

        ContentRegistry::DataInspector::add("RGBA Color", sizeof(u32), [](auto buffer, auto endian, auto style) {
            ImColor value(hex::changeEndianess(*reinterpret_cast<u32*>(buffer.data()), endian));

            return [value] {
                ImGui::ColorButton("##inspectorColor", value,
                                   ImGuiColorEditFlags_None,
                                   ImVec2(ImGui::GetColumnWidth(), ImGui::GetTextLineHeight()));
            };
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