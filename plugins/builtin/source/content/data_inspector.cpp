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

        hex::ContentRegistry::DataInspector::add("hex.builtin.inspector.binary", sizeof(u8), [](auto buffer, auto endian, auto style) {
            std::string binary;
            for (u8 i = 0; i < 8; i++)
                binary += ((buffer[0] << i) & 0x80) == 0 ? '0' : '1';

            return [binary] {
                ImGui::TextUnformatted(binary.c_str());
                return binary;
            };
        });

        hex::ContentRegistry::DataInspector::add("hex.builtin.inspector.u8", sizeof(u8), [](auto buffer, auto endian, auto style) {
            auto format = (style == Style::Decimal) ? "{0:d}" : ((style == Style::Hexadecimal) ? "0x{0:X}" : "{0:#o}");
            auto value = hex::format(format, *reinterpret_cast<u8*>(buffer.data()));
            return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
        });

        hex::ContentRegistry::DataInspector::add("hex.builtin.inspector.s8", sizeof(s8), [](auto buffer, auto endian, auto style) {
            auto format = (style == Style::Decimal) ? "{0:d}" : ((style == Style::Hexadecimal) ? "0x{0:X}" : "{0:#o}");
            auto value = hex::format(format, *reinterpret_cast<s8*>(buffer.data()));
            return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
        });

        hex::ContentRegistry::DataInspector::add("hex.builtin.inspector.u16", sizeof(u16), [](auto buffer, auto endian, auto style) {
            auto format = (style == Style::Decimal) ? "{0:d}" : ((style == Style::Hexadecimal) ? "0x{0:X}" : "{0:#o}");
            auto value = hex::format(format, hex::changeEndianess(*reinterpret_cast<u16*>(buffer.data()), endian));
            return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
        });

        hex::ContentRegistry::DataInspector::add("hex.builtin.inspector.s16", sizeof(s16), [](auto buffer, auto endian, auto style) {
            auto format = (style == Style::Decimal) ? "{0:d}" : ((style == Style::Hexadecimal) ? "0x{0:X}" : "{0:#o}");
            auto value = hex::format(format, hex::changeEndianess(*reinterpret_cast<s16*>(buffer.data()), endian));
            return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
        });

        hex::ContentRegistry::DataInspector::add("hex.builtin.inspector.u32", sizeof(u32), [](auto buffer, auto endian, auto style) {
            auto format = (style == Style::Decimal) ? "{0:d}" : ((style == Style::Hexadecimal) ? "0x{0:X}" : "{0:#o}");
            auto value = hex::format(format, hex::changeEndianess(*reinterpret_cast<u32*>(buffer.data()), endian));
            return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
        });

        hex::ContentRegistry::DataInspector::add("hex.builtin.inspector.s32", sizeof(s32), [](auto buffer, auto endian, auto style) {
            auto format = (style == Style::Decimal) ? "{0:d}" : ((style == Style::Hexadecimal) ? "0x{0:X}" : "{0:#o}");
            auto value = hex::format(format, hex::changeEndianess(*reinterpret_cast<s32*>(buffer.data()), endian));
            return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
        });

        hex::ContentRegistry::DataInspector::add("hex.builtin.inspector.u64", sizeof(u64), [](auto buffer, auto endian, auto style) {
            auto format = (style == Style::Decimal) ? "{0:d}" : ((style == Style::Hexadecimal) ? "0x{0:X}" : "{0:#o}");
            auto value = hex::format(format, hex::changeEndianess(*reinterpret_cast<u64*>(buffer.data()), endian));
            return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
        });

        hex::ContentRegistry::DataInspector::add("hex.builtin.inspector.s64", sizeof(s64), [](auto buffer, auto endian, auto style) {
            auto format = (style == Style::Decimal) ? "{0:d}" : ((style == Style::Hexadecimal) ? "0x{0:X}" : "{0:#o}");
            auto value = hex::format(format, hex::changeEndianess(*reinterpret_cast<s64*>(buffer.data()), endian));
            return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
        });

        hex::ContentRegistry::DataInspector::add("hex.builtin.inspector.float", sizeof(float), [](auto buffer, auto endian, auto style) {
            auto value = hex::format("{0:E}", hex::changeEndianess(*reinterpret_cast<float*>(buffer.data()), endian));
            return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
        });

        hex::ContentRegistry::DataInspector::add("hex.builtin.inspector.double", sizeof(double), [](auto buffer, auto endian, auto style) {
            auto value = hex::format("{0:E}", hex::changeEndianess(*reinterpret_cast<float*>(buffer.data()), endian));
            return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
        });

        hex::ContentRegistry::DataInspector::add("hex.builtin.inspector.ascii", sizeof(char8_t), [](auto buffer, auto endian, auto style) {
            auto value = hex::format("'{0}'", makePrintable(*reinterpret_cast<char8_t*>(buffer.data())).c_str());
            return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
        });

        hex::ContentRegistry::DataInspector::add("hex.builtin.inspector.wide", sizeof(char16_t), [](auto buffer, auto endian, auto style) {
            auto c = *reinterpret_cast<char16_t*>(buffer.data());
            auto value = hex::format("'{0}'", c == 0 ? '\x01' : char16_t(hex::changeEndianess(c, endian)));
            return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
        });

        hex::ContentRegistry::DataInspector::add("hex.builtin.inspector.utf8", sizeof(char8_t) * 4, [](auto buffer, auto endian, auto style) {
            char utf8Buffer[5] = { 0 };
            char codepointString[5] = { 0 };
            u32 codepoint = 0;

            std::memcpy(utf8Buffer, reinterpret_cast<char8_t*>(buffer.data()), 4);
            u8 codepointSize = ImTextCharFromUtf8(&codepoint, utf8Buffer, utf8Buffer + 4);

            std::memcpy(codepointString, &codepoint, std::min(codepointSize, u8(4)));
            auto value = hex::format("'{0}' (U+0x{1:04X})",  codepoint == 0xFFFD ? "Invalid" :
                                                        codepoint < 0xFF ? makePrintable(codepoint).c_str() :
                                                        codepointString,
                                     codepoint);

            return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
        });

#if defined(OS_WINDOWS) && defined(ARCH_64_BIT)

        hex::ContentRegistry::DataInspector::add("hex.builtin.inspector.time32", sizeof(__time32_t), [](auto buffer, auto endian, auto style) {
                auto endianAdjustedTime = hex::changeEndianess(*reinterpret_cast<__time32_t*>(buffer.data()), endian);
                struct tm *ptm = _localtime32(&endianAdjustedTime);
                std::string value;
                if (ptm != nullptr)
                    value = hex::format("{0:%a, %d.%m.%Y %H:%M:%S}", *ptm);
                else
                    value = "Invalid";

                return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
            });

            hex::ContentRegistry::DataInspector::add("hex.builtin.inspector.time64", sizeof(__time64_t), [](auto buffer, auto endian, auto style) {
                auto endianAdjustedTime = hex::changeEndianess(*reinterpret_cast<__time64_t*>(buffer.data()), endian);
                struct tm *ptm = _localtime64(&endianAdjustedTime);
                std::string value;
                if (ptm != nullptr)
                    value = hex::format("{0:%a, %d.%m.%Y %H:%M:%S}", *ptm);
                else
                    value = "Invalid";

                return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
            });

#else

        hex::ContentRegistry::DataInspector::add("hex.builtin.inspector.time", sizeof(time_t), [](auto buffer, auto endian, auto style) {
            auto endianAdjustedTime = hex::changeEndianess(*reinterpret_cast<time_t*>(buffer.data()), endian);
            struct tm *ptm = localtime(&endianAdjustedTime);
            std::string value;
            if (ptm != nullptr)
                value = hex::format("{0:%a, %d.%m.%Y %H:%M:%S}", *ptm);
            else
                value = "Invalid";

            return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
        });

#endif

        hex::ContentRegistry::DataInspector::add("hex.builtin.inspector.guid", sizeof(GUID), [](auto buffer, auto endian, auto style) {
            GUID guid;
            std::memcpy(&guid, buffer.data(), sizeof(GUID));
            auto value = hex::format("{}{{{:08X}-{:04X}-{:04X}-{:02X}{:02X}-{:02X}{:02X}{:02X}{:02X}{:02X}{:02X}}}",
                                     (hex::changeEndianess(guid.data3, endian) >> 12) <= 5 && ((guid.data4[0] >> 4) >= 8 || (guid.data4[0] >> 4) == 0) ? "" : "Invalid ",
                                     hex::changeEndianess(guid.data1, endian),
                                     hex::changeEndianess(guid.data2, endian),
                                     hex::changeEndianess(guid.data3, endian),
                                     guid.data4[0], guid.data4[1], guid.data4[2], guid.data4[3],
                                     guid.data4[4], guid.data4[5], guid.data4[6], guid.data4[7]);

            return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
        });

        hex::ContentRegistry::DataInspector::add("hex.builtin.inspector.rgba8", sizeof(u32), [](auto buffer, auto endian, auto style) {
            ImColor value(hex::changeEndianess(*reinterpret_cast<u32*>(buffer.data()), endian));

            auto stringValue = hex::format("(0x{:02X}, 0x{:02X}, 0x{:02X}, 0x{:02X})", u8(0xFF * (value.Value.x)), u8(0xFF * (value.Value.y)), u8(0xFF * (value.Value.z)), u8(0xFF * (value.Value.w)));

            return [value, stringValue] {
                ImGui::ColorButton("##inspectorColor", value,
                                   ImGuiColorEditFlags_None,
                                   ImVec2(ImGui::GetColumnWidth(), ImGui::GetTextLineHeight()));
                return stringValue;
            };
        });

    }

}