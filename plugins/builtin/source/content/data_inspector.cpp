#include <hex/plugin.hpp>

#include <hex/helpers/utils.hpp>

#include <cstring>
#include <ctime>

#include <imgui_internal.h>

namespace hex::plugin::builtin {

    struct GUID {
        u32 data1;
        u16 data2;
        u16 data3;
        u8  data4[8];
    };

    void registerDataInspectorEntries() {

        using Style = hex::ContentRegistry::DataInspector::NumberDisplayStyle;

        hex::ContentRegistry::DataInspector::add("Binary (8 bit)", sizeof(u8), [](auto buffer, auto endian, auto style) {
            std::string binary;
            for (u8 i = 0; i < 8; i++)
                binary += ((buffer[0] << i) & 0x80) == 0 ? '0' : '1';

            return [binary] { ImGui::TextUnformatted(binary.c_str()); };
        });

        hex::ContentRegistry::DataInspector::add("uint8_t", sizeof(u8), [](auto buffer, auto endian, auto style) {
            auto format = (style == Style::Decimal) ? "%u" : ((style == Style::Hexadecimal) ? "0x%X" : "0o%o");
            auto value = hex::format(format, *reinterpret_cast<u8*>(buffer.data()));
            return [value] { ImGui::TextUnformatted(value.c_str()); };
        });

        hex::ContentRegistry::DataInspector::add("int8_t", sizeof(s8), [](auto buffer, auto endian, auto style) {
            auto format = (style == Style::Decimal) ? "%d" : ((style == Style::Hexadecimal) ? "0x%X" : "0o%o");
            auto value = hex::format(format, *reinterpret_cast<s8*>(buffer.data()));
            return [value] { ImGui::TextUnformatted(value.c_str()); };
        });

        hex::ContentRegistry::DataInspector::add("uint16_t", sizeof(u16), [](auto buffer, auto endian, auto style) {
            auto format = (style == Style::Decimal) ? "%u" : ((style == Style::Hexadecimal) ? "0x%X" : "0o%o");
            auto value = hex::format(format, hex::changeEndianess(*reinterpret_cast<u16*>(buffer.data()), endian));
            return [value] { ImGui::TextUnformatted(value.c_str()); };
        });

        hex::ContentRegistry::DataInspector::add("int16_t", sizeof(s16), [](auto buffer, auto endian, auto style) {
            auto format = (style == Style::Decimal) ? "%d" : ((style == Style::Hexadecimal) ? "0x%X" : "0o%o");
            auto value = hex::format(format, hex::changeEndianess(*reinterpret_cast<s16*>(buffer.data()), endian));
            return [value] { ImGui::TextUnformatted(value.c_str()); };
        });

        hex::ContentRegistry::DataInspector::add("uint32_t", sizeof(u32), [](auto buffer, auto endian, auto style) {
            auto format = (style == Style::Decimal) ? "%u" : ((style == Style::Hexadecimal) ? "0x%X" : "0o%o");
            auto value = hex::format(format, hex::changeEndianess(*reinterpret_cast<u32*>(buffer.data()), endian));
            return [value] { ImGui::TextUnformatted(value.c_str()); };
        });

        hex::ContentRegistry::DataInspector::add("int32_t", sizeof(s32), [](auto buffer, auto endian, auto style) {
            auto format = (style == Style::Decimal) ? "%d" : ((style == Style::Hexadecimal) ? "0x%X" : "0o%o");
            auto value = hex::format(format, hex::changeEndianess(*reinterpret_cast<s32*>(buffer.data()), endian));
            return [value] { ImGui::TextUnformatted(value.c_str()); };
        });

        hex::ContentRegistry::DataInspector::add("uint64_t", sizeof(u64), [](auto buffer, auto endian, auto style) {
            auto format = (style == Style::Decimal) ? "%lu" : ((style == Style::Hexadecimal) ? "0x%lX" : "0o%lo");
            auto value = hex::format(format, hex::changeEndianess(*reinterpret_cast<u64*>(buffer.data()), endian));
            return [value] { ImGui::TextUnformatted(value.c_str()); };
        });

        hex::ContentRegistry::DataInspector::add("int64_t", sizeof(s64), [](auto buffer, auto endian, auto style) {
            auto format = (style == Style::Decimal) ? "%ld" : ((style == Style::Hexadecimal) ? "0x%lX" : "0o%lo");
            auto value = hex::format(format, hex::changeEndianess(*reinterpret_cast<s64*>(buffer.data()), endian));
            return [value] { ImGui::TextUnformatted(value.c_str()); };
        });

        hex::ContentRegistry::DataInspector::add("float (32 bit)", sizeof(float), [](auto buffer, auto endian, auto style) {
            auto value = hex::format("%e", hex::changeEndianess(*reinterpret_cast<float*>(buffer.data()), endian));
            return [value] { ImGui::TextUnformatted(value.c_str()); };
        });

        hex::ContentRegistry::DataInspector::add("double (64 bit)", sizeof(double), [](auto buffer, auto endian, auto style) {
            auto value = hex::format("%e", hex::changeEndianess(*reinterpret_cast<double*>(buffer.data()), endian));
            return [value] { ImGui::TextUnformatted(value.c_str()); };
        });

        hex::ContentRegistry::DataInspector::add("ASCII Character", sizeof(char8_t), [](auto buffer, auto endian, auto style) {
            auto value = hex::format("'%s'", makePrintable(*reinterpret_cast<char8_t*>(buffer.data())).c_str());
            return [value] { ImGui::TextUnformatted(value.c_str()); };
        });

        hex::ContentRegistry::DataInspector::add("Wide Character", sizeof(char16_t), [](auto buffer, auto endian, auto style) {
            auto c = *reinterpret_cast<char16_t*>(buffer.data());
            auto value = hex::format("'%lc'", c == 0 ? '\x01' : hex::changeEndianess(c, endian));
            return [value] { ImGui::TextUnformatted(value.c_str()); };
        });

        hex::ContentRegistry::DataInspector::add("UTF-8 code point", sizeof(char8_t) * 4, [](auto buffer, auto endian, auto style) {
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

        hex::ContentRegistry::DataInspector::add("__time32_t", sizeof(__time32_t), [](auto buffer, auto endian, auto style) {
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

            hex::ContentRegistry::DataInspector::add("__time64_t", sizeof(__time64_t), [](auto buffer, auto endian, auto style) {
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

        hex::ContentRegistry::DataInspector::add("time_t", sizeof(time_t), [](auto buffer, auto endian, auto style) {
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

        hex::ContentRegistry::DataInspector::add("GUID", sizeof(GUID), [](auto buffer, auto endian, auto style) {
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

        hex::ContentRegistry::DataInspector::add("RGBA Color", sizeof(u32), [](auto buffer, auto endian, auto style) {
            ImColor value(hex::changeEndianess(*reinterpret_cast<u32*>(buffer.data()), endian));

            return [value] {
                ImGui::ColorButton("##inspectorColor", value,
                                   ImGuiColorEditFlags_None,
                                   ImVec2(ImGui::GetColumnWidth(), ImGui::GetTextLineHeight()));
            };
        });

    }

}