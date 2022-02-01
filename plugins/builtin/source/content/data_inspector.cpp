#include <hex/api/content_registry.hpp>

#include <hex/helpers/utils.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/api/event.hpp>

#include <hex/providers/provider.hpp>

#include <cstring>
#include <codecvt>
#include <locale>
#include <string>
#include <vector>

#include <imgui_internal.h>

namespace hex::plugin::builtin {

    struct GUID {
        u32 data1;
        u16 data2;
        u16 data3;
        u8 data4[8];
    };

    void registerDataInspectorEntries() {

        using Style = ContentRegistry::DataInspector::NumberDisplayStyle;

        ContentRegistry::DataInspector::add("hex.builtin.inspector.binary", sizeof(u8), [](auto buffer, auto endian, auto style) {
            std::string binary = "0b";
            for (u8 i = 0; i < 8; i++)
                binary += ((buffer[0] << i) & 0x80) == 0 ? '0' : '1';

            return [binary] {
                ImGui::TextUnformatted(binary.c_str());
                return binary;
            };
        });

        ContentRegistry::DataInspector::add("hex.builtin.inspector.u8", sizeof(u8), [](auto buffer, auto endian, auto style) {
            auto format = (style == Style::Decimal) ? "{0:d}" : ((style == Style::Hexadecimal) ? "0x{0:02X}" : "0o{0:03o}");

            auto value = hex::format(format, *reinterpret_cast<u8 *>(buffer.data()));

            return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
        });

        ContentRegistry::DataInspector::add("hex.builtin.inspector.i8", sizeof(i8), [](auto buffer, auto endian, auto style) {
            auto format = (style == Style::Decimal) ? "{0}{1:d}" : ((style == Style::Hexadecimal) ? "{0}0x{1:02X}" : "{0}0o{1:03o}");

            auto number = hex::changeEndianess(*reinterpret_cast<i8 *>(buffer.data()), endian);
            bool negative = number < 0;
            auto value = hex::format(format, negative ? "-" : "", std::abs(number));

            return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
        });

        ContentRegistry::DataInspector::add("hex.builtin.inspector.u16", sizeof(u16), [](auto buffer, auto endian, auto style) {
            auto format = (style == Style::Decimal) ? "{0:d}" : ((style == Style::Hexadecimal) ? "0x{0:04X}" : "0o{0:06o}");

            auto value = hex::format(format, hex::changeEndianess(*reinterpret_cast<u16 *>(buffer.data()), endian));

            return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
        });

        ContentRegistry::DataInspector::add("hex.builtin.inspector.i16", sizeof(i16), [](auto buffer, auto endian, auto style) {
            auto format = (style == Style::Decimal) ? "{0}{1:d}" : ((style == Style::Hexadecimal) ? "{0}0x{1:04X}" : "{0}0o{1:06o}");

            auto number = hex::changeEndianess(*reinterpret_cast<i16 *>(buffer.data()), endian);
            bool negative = number < 0;
            auto value = hex::format(format, negative ? "-" : "", std::abs(number));

            return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
        });

        ContentRegistry::DataInspector::add("hex.builtin.inspector.u32", sizeof(u32), [](auto buffer, auto endian, auto style) {
            auto format = (style == Style::Decimal) ? "{0:d}" : ((style == Style::Hexadecimal) ? "0x{0:08X}" : "0o{0:011o}");

            auto value = hex::format(format, hex::changeEndianess(*reinterpret_cast<u32 *>(buffer.data()), endian));

            return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
        });

        ContentRegistry::DataInspector::add("hex.builtin.inspector.i32", sizeof(i32), [](auto buffer, auto endian, auto style) {
            auto format = (style == Style::Decimal) ? "{0}{1:d}" : ((style == Style::Hexadecimal) ? "{0}0x{1:08X}" : "{0}0o{1:011o}");

            auto number = hex::changeEndianess(*reinterpret_cast<i32 *>(buffer.data()), endian);
            bool negative = number < 0;
            auto value = hex::format(format, negative ? "-" : "", std::abs(number));

            return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
        });

        ContentRegistry::DataInspector::add("hex.builtin.inspector.u64", sizeof(u64), [](auto buffer, auto endian, auto style) {
            auto format = (style == Style::Decimal) ? "{0:d}" : ((style == Style::Hexadecimal) ? "0x{0:016X}" : "0o{0:022o}");

            auto value = hex::format(format, hex::changeEndianess(*reinterpret_cast<u64 *>(buffer.data()), endian));

            return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
        });

        ContentRegistry::DataInspector::add("hex.builtin.inspector.i64", sizeof(i64), [](auto buffer, auto endian, auto style) {
            auto format = (style == Style::Decimal) ? "{0}{1:d}" : ((style == Style::Hexadecimal) ? "{0}0x{1:016X}" : "{0}0o{1:022o}");

            auto number = hex::changeEndianess(*reinterpret_cast<i64 *>(buffer.data()), endian);
            bool negative = number < 0;
            auto value = hex::format(format, negative ? "-" : "", std::abs(number));

            return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
        });

        ContentRegistry::DataInspector::add("hex.builtin.inspector.float16", sizeof(u16), [](auto buffer, auto endian, auto style) {
            auto value = hex::format("{0:G}", hex::changeEndianess(float16ToFloat32(*reinterpret_cast<u16 *>(buffer.data())), endian));
            return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
        });

        ContentRegistry::DataInspector::add("hex.builtin.inspector.float", sizeof(float), [](auto buffer, auto endian, auto style) {
            auto value = hex::format("{0:G}", hex::changeEndianess(*reinterpret_cast<float *>(buffer.data()), endian));
            return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
        });

        ContentRegistry::DataInspector::add("hex.builtin.inspector.double", sizeof(double), [](auto buffer, auto endian, auto style) {
            auto value = hex::format("{0:G}", hex::changeEndianess(*reinterpret_cast<float *>(buffer.data()), endian));
            return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
        });

        ContentRegistry::DataInspector::add("hex.builtin.inspector.ascii", sizeof(char8_t), [](auto buffer, auto endian, auto style) {
            auto value = hex::format("'{0}'", makePrintable(*reinterpret_cast<char8_t *>(buffer.data())).c_str());
            return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
        });

        ContentRegistry::DataInspector::add("hex.builtin.inspector.wide", sizeof(wchar_t), [](auto buffer, auto endian, auto style) {
            wchar_t wideChar = '\x00';
            std::memcpy(&wideChar, buffer.data(), std::min(sizeof(wchar_t), buffer.size()));

            auto c = hex::changeEndianess(wideChar, endian);

            std::wstring_convert<std::codecvt_utf8<wchar_t>> converter("Invalid");

            auto value = hex::format("'{0}'", c <= 255 ? makePrintable(c) : converter.to_bytes(c));
            return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
        });

        ContentRegistry::DataInspector::add("hex.builtin.inspector.utf8", sizeof(char8_t) * 4, [](auto buffer, auto endian, auto style) {
            char utf8Buffer[5] = { 0 };
            char codepointString[5] = { 0 };
            u32 codepoint = 0;

            std::memcpy(utf8Buffer, reinterpret_cast<char8_t *>(buffer.data()), 4);
            u8 codepointSize = ImTextCharFromUtf8(&codepoint, utf8Buffer, utf8Buffer + 4);

            std::memcpy(codepointString, utf8Buffer, std::min(codepointSize, u8(4)));
            auto value = hex::format("'{0}' (U+0x{1:04X})",
                                     codepoint == 0xFFFD ? "Invalid" : (codepointSize == 1 ? makePrintable(codepointString[0]) : codepointString),
                                     codepoint);

            return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
        });

        ContentRegistry::DataInspector::add("hex.builtin.inspector.string", 1, [](auto buffer, auto endian, auto style) {
            Region currSelection = { 0 };
            EventManager::post<QuerySelection>(currSelection);

            constexpr static auto MaxStringLength = 32;

            std::string stringBuffer(std::min<ssize_t>(MaxStringLength, currSelection.size), 0x00);
            ImHexApi::Provider::get()->read(currSelection.address, stringBuffer.data(), stringBuffer.size());
            if (currSelection.size > MaxStringLength)
                stringBuffer += "...";

            for (auto &c : stringBuffer)
                if (c < 0x20)
                    c = ' ';


            auto value = hex::format("\"{0}\"", stringBuffer);

            return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
        });

#if defined(OS_WINDOWS) && defined(ARCH_64_BIT)

        ContentRegistry::DataInspector::add("hex.builtin.inspector.time32", sizeof(u32), [](auto buffer, auto endian, auto style) {
            auto endianAdjustedTime = hex::changeEndianess(*reinterpret_cast<u32 *>(buffer.data()), endian);

            std::string value;
            try {
                value = hex::format("{0:%a, %d.%m.%Y %H:%M:%S}", fmt::localtime(endianAdjustedTime));
            } catch (fmt::format_error &e) {
                value = "Invalid";
            }

            return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
        });

        ContentRegistry::DataInspector::add("hex.builtin.inspector.time64", sizeof(u64), [](auto buffer, auto endian, auto style) {
            auto endianAdjustedTime = hex::changeEndianess(*reinterpret_cast<u64 *>(buffer.data()), endian);

            std::string value;
            try {
                value = hex::format("{0:%a, %d.%m.%Y %H:%M:%S}", fmt::localtime(endianAdjustedTime));
            } catch (fmt::format_error &e) {
                value = "Invalid";
            }

            return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
        });

#else

        ContentRegistry::DataInspector::add("hex.builtin.inspector.time", sizeof(time_t), [](auto buffer, auto endian, auto style) {
            auto endianAdjustedTime = hex::changeEndianess(*reinterpret_cast<time_t *>(buffer.data()), endian);

            std::string value;
            try {
                value = hex::format("{0:%a, %d.%m.%Y %H:%M:%S}", fmt::localtime(endianAdjustedTime));
            } catch (fmt::format_error &e) {
                value = "Invalid";
            }

            return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
        });

#endif

        ContentRegistry::DataInspector::add("hex.builtin.inspector.guid", sizeof(GUID), [](auto buffer, auto endian, auto style) {
            GUID guid;
            std::memcpy(&guid, buffer.data(), sizeof(GUID));
            auto value = hex::format("{}{{{:08X}-{:04X}-{:04X}-{:02X}{:02X}-{:02X}{:02X}{:02X}{:02X}{:02X}{:02X}}}",
                                     (hex::changeEndianess(guid.data3, endian) >> 12) <= 5 && ((guid.data4[0] >> 4) >= 8 || (guid.data4[0] >> 4) == 0) ? "" : "Invalid ",
                                     hex::changeEndianess(guid.data1, endian),
                                     hex::changeEndianess(guid.data2, endian),
                                     hex::changeEndianess(guid.data3, endian),
                                     guid.data4[0],
                                     guid.data4[1],
                                     guid.data4[2],
                                     guid.data4[3],
                                     guid.data4[4],
                                     guid.data4[5],
                                     guid.data4[6],
                                     guid.data4[7]);

            return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
        });

        ContentRegistry::DataInspector::add("hex.builtin.inspector.rgba8", sizeof(u32), [](auto buffer, auto endian, auto style) {
            ImColor value(hex::changeEndianess(*reinterpret_cast<u32 *>(buffer.data()), endian));

            auto stringValue = hex::format("(0x{:02X}, 0x{:02X}, 0x{:02X}, 0x{:02X})", u8(0xFF * (value.Value.x)), u8(0xFF * (value.Value.y)), u8(0xFF * (value.Value.z)), u8(0xFF * (value.Value.w)));

            return [value, stringValue] {
                ImGui::ColorButton("##inspectorColor", value, ImGuiColorEditFlags_None, ImVec2(ImGui::GetColumnWidth(), ImGui::GetTextLineHeight()));
                return stringValue;
            };
        });
    }

}