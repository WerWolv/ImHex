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
#include <hex/ui/imgui_imhex_extensions.h>

namespace hex::plugin::builtin {

    struct GUID {
        u32 data1;
        u16 data2;
        u16 data3;
        u8 data4[8];
    };

    template<std::integral T>
    static std::vector<u8> stringToUnsigned(const std::string &value, std::endian endian) requires(sizeof(T) <= sizeof(u64)) {
        u64 result = std::strtoull(value.c_str(), nullptr, 0);
        if (result > std::numeric_limits<T>::max()) return {};

        std::vector<u8> bytes(sizeof(T), 0x00);
        std::memcpy(bytes.data(), &result, bytes.size());

        if (endian != std::endian::native)
            std::reverse(bytes.begin(), bytes.end());

        return bytes;
    }

    template<std::integral T>
    static std::vector<u8> stringToSigned(const std::string &value, std::endian endian) requires(sizeof(T) <= sizeof(u64)) {
        i64 result = std::strtoll(value.c_str(), nullptr, 0);
        if (result > std::numeric_limits<T>::max() || result < std::numeric_limits<T>::min()) return {};

        std::vector<u8> bytes(sizeof(T), 0x00);
        std::memcpy(bytes.data(), &result, bytes.size());

        if (endian != std::endian::native)
            std::reverse(bytes.begin(), bytes.end());

        return bytes;
    }

    template<std::floating_point T>
    static std::vector<u8> stringToFloat(const std::string &value, std::endian endian) requires(sizeof(T) <= sizeof(long double)) {
        auto result = std::strtold(value.c_str(), nullptr);

        std::vector<u8> bytes(sizeof(T), 0x00);
        std::memcpy(bytes.data(), &result, bytes.size());

        if (endian != std::endian::native)
            std::reverse(bytes.begin(), bytes.end());

        return bytes;
    }

    template<std::integral T>
    static std::vector<u8> stringToInteger(const std::string &value, std::endian endian) requires(sizeof(T) <= sizeof(u64)) {
        if constexpr (std::is_unsigned_v<T>)
            return stringToUnsigned<T>(value, endian);
        else if constexpr (std::is_signed_v<T>)
            return stringToSigned<T>(value, endian);
        else
            return {};
    }

    // clang-format off
    void registerDataInspectorEntries() {

        using Style = ContentRegistry::DataInspector::NumberDisplayStyle;

        ContentRegistry::DataInspector::add("hex.builtin.inspector.binary", sizeof(u8),
            [](auto buffer, auto endian, auto style) {
                std::string binary = "0b";
                for (u8 i = 0; i < 8; i++)
                    binary += ((buffer[0] << i) & 0x80) == 0 ? '0' : '1';

                return [binary] {
                    ImGui::TextUnformatted(binary.c_str());
                    return binary;
                };
            }, [](std::string value, std::endian endian) -> std::vector<u8> {
                if (value.starts_with("0b"))
                    value = value.substr(2);

                if (value.size() > 8) return { };
                u8 byte = 0x00;
                for (char c : value) {
                    byte <<= 1;

                    if (c == '1')
                        byte |= 0b01;
                    else if (c == '0')
                        byte |= 0b00;
                    else
                        return { };
                }

                return { byte };
            }
        );

        ContentRegistry::DataInspector::add("hex.builtin.inspector.u8", sizeof(u8),
            [](auto buffer, auto endian, auto style) {
                auto format = (style == Style::Decimal) ? "{0:d}" : ((style == Style::Hexadecimal) ? "0x{0:02X}" : "0o{0:03o}");

                auto value = hex::format(format, *reinterpret_cast<u8 *>(buffer.data()));

                return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
            },
            stringToInteger<u8>
        );

        ContentRegistry::DataInspector::add("hex.builtin.inspector.i8", sizeof(i8),
            [](auto buffer, auto endian, auto style) {
                auto format = (style == Style::Decimal) ? "{0}{1:d}" : ((style == Style::Hexadecimal) ? "{0}0x{1:02X}" : "{0}0o{1:03o}");

                auto number   = hex::changeEndianess(*reinterpret_cast<i8 *>(buffer.data()), endian);
                bool negative = number < 0;
                auto value    = hex::format(format, negative ? "-" : "", std::abs(number));

                return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
            },
            stringToInteger<i8>
        );

        ContentRegistry::DataInspector::add("hex.builtin.inspector.u16", sizeof(u16),
            [](auto buffer, auto endian, auto style) {
                auto format = (style == Style::Decimal) ? "{0:d}" : ((style == Style::Hexadecimal) ? "0x{0:04X}" : "0o{0:06o}");

                auto value = hex::format(format, hex::changeEndianess(*reinterpret_cast<u16 *>(buffer.data()), endian));

                return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
            },
            stringToInteger<u16>
        );

        ContentRegistry::DataInspector::add("hex.builtin.inspector.i16", sizeof(i16),
            [](auto buffer, auto endian, auto style) {
                auto format = (style == Style::Decimal) ? "{0}{1:d}" : ((style == Style::Hexadecimal) ? "{0}0x{1:04X}" : "{0}0o{1:06o}");

                auto number   = hex::changeEndianess(*reinterpret_cast<i16 *>(buffer.data()), endian);
                bool negative = number < 0;
                auto value    = hex::format(format, negative ? "-" : "", std::abs(number));

                return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
            },
            stringToInteger<i16>
        );

        ContentRegistry::DataInspector::add("hex.builtin.inspector.u32", sizeof(u32),
            [](auto buffer, auto endian, auto style) {
                auto format = (style == Style::Decimal) ? "{0:d}" : ((style == Style::Hexadecimal) ? "0x{0:08X}" : "0o{0:011o}");

                auto value = hex::format(format, hex::changeEndianess(*reinterpret_cast<u32 *>(buffer.data()), endian));

                return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
            },
            stringToInteger<u32>
        );

        ContentRegistry::DataInspector::add("hex.builtin.inspector.i32", sizeof(i32),
            [](auto buffer, auto endian, auto style) {
                auto format = (style == Style::Decimal) ? "{0}{1:d}" : ((style == Style::Hexadecimal) ? "{0}0x{1:08X}" : "{0}0o{1:011o}");

                auto number   = hex::changeEndianess(*reinterpret_cast<i32 *>(buffer.data()), endian);
                bool negative = number < 0;
                auto value    = hex::format(format, negative ? "-" : "", std::abs(number));

                return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
            },
            stringToInteger<i32>
        );

        ContentRegistry::DataInspector::add("hex.builtin.inspector.u64", sizeof(u64),
            [](auto buffer, auto endian, auto style) {
                auto format = (style == Style::Decimal) ? "{0:d}" : ((style == Style::Hexadecimal) ? "0x{0:016X}" : "0o{0:022o}");

                auto value = hex::format(format, hex::changeEndianess(*reinterpret_cast<u64 *>(buffer.data()), endian));

                return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
            },
            stringToInteger<u64>
        );

        ContentRegistry::DataInspector::add("hex.builtin.inspector.i64", sizeof(i64),
            [](auto buffer, auto endian, auto style) {
                auto format = (style == Style::Decimal) ? "{0}{1:d}" : ((style == Style::Hexadecimal) ? "{0}0x{1:016X}" : "{0}0o{1:022o}");

                auto number   = hex::changeEndianess(*reinterpret_cast<i64 *>(buffer.data()), endian);
                bool negative = number < 0;
                auto value    = hex::format(format, negative ? "-" : "", std::abs(number));

                return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
            },
            stringToInteger<i64>
        );

        ContentRegistry::DataInspector::add("hex.builtin.inspector.float16", sizeof(u16),
            [](auto buffer, auto endian, auto style) {
                auto value = hex::format("{0:G}", hex::changeEndianess(float16ToFloat32(*reinterpret_cast<u16 *>(buffer.data())), endian));
                return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
            }
        );

        ContentRegistry::DataInspector::add("hex.builtin.inspector.float", sizeof(float),
            [](auto buffer, auto endian, auto style) {
                auto value = hex::format("{0:G}", hex::changeEndianess(*reinterpret_cast<float *>(buffer.data()), endian));
                return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
            },
            stringToFloat<float>
        );

        ContentRegistry::DataInspector::add("hex.builtin.inspector.double", sizeof(double),
            [](auto buffer, auto endian, auto style) {
                auto value = hex::format("{0:G}", hex::changeEndianess(*reinterpret_cast<double *>(buffer.data()), endian));
                return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
            },
            stringToFloat<double>
        );

        ContentRegistry::DataInspector::add("hex.builtin.inspector.long_double", sizeof(long double),
            [](auto buffer, auto endian, auto style) {
                auto value = hex::format("{0:G}", hex::changeEndianess(*reinterpret_cast<long double *>(buffer.data()), endian));
                return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
            },
            stringToFloat<long double>
        );

        ContentRegistry::DataInspector::add("hex.builtin.inspector.ascii", sizeof(char8_t),
            [](auto buffer, auto endian, auto style) {
                auto value = makePrintable(*reinterpret_cast<char8_t *>(buffer.data()));
                return [value] { ImGui::TextFormatted("'{0}'", value.c_str()); return value; };
            },
            [](const std::string &value, std::endian endian) -> std::vector<u8> {
                if (value.length() > 1) return { };

                return { u8(value[0]) };
            }
        );

        ContentRegistry::DataInspector::add("hex.builtin.inspector.wide", sizeof(wchar_t),
            [](auto buffer, auto endian, auto style) {
                wchar_t wideChar = '\x00';
                std::memcpy(&wideChar, buffer.data(), std::min(sizeof(wchar_t), buffer.size()));

                auto c = hex::changeEndianess(wideChar, endian);

                std::wstring_convert<std::codecvt_utf8<wchar_t>> converter("Invalid");

                auto value = hex::format("{0}", c <= 255 ? makePrintable(c) : converter.to_bytes(c));
                return [value] { ImGui::TextFormatted("'{0}'", value.c_str()); return value; };
            },
            [](const std::string &value, std::endian endian) -> std::vector<u8> {
                std::wstring_convert<std::codecvt_utf8<wchar_t>> converter("");

                std::vector<u8> bytes;
                auto wideString = converter.from_bytes(value.c_str());
                std::copy(wideString.begin(), wideString.end(), std::back_inserter(bytes));

                if (endian != std::endian::native)
                    std::reverse(bytes.begin(), bytes.end());

                return bytes;
            }
        );

        ContentRegistry::DataInspector::add("hex.builtin.inspector.utf8", sizeof(char8_t) * 4,
            [](auto buffer, auto endian, auto style) {
                char utf8Buffer[5]      = { 0 };
                char codepointString[5] = { 0 };
                u32 codepoint           = 0;

                std::memcpy(utf8Buffer, reinterpret_cast<char8_t *>(buffer.data()), 4);
                u8 codepointSize = ImTextCharFromUtf8(&codepoint, utf8Buffer, utf8Buffer + 4);

                std::memcpy(codepointString, utf8Buffer, std::min(codepointSize, u8(4)));
                auto value = hex::format("'{0}' (U+0x{1:04X})",
                    codepoint == 0xFFFD ? "Invalid" : (codepointSize == 1 ? makePrintable(codepointString[0]) : codepointString),
                    codepoint);

                return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
            }
        );

        ContentRegistry::DataInspector::add("hex.builtin.inspector.string", 1,
            [](auto buffer, auto endian, auto style) {
                Region currSelection = { 0 };
                EventManager::post<QuerySelection>(currSelection);

                constexpr static auto MaxStringLength = 32;

                std::vector<u8> stringBuffer(std::min<ssize_t>(MaxStringLength, currSelection.size), 0x00);
                ImHexApi::Provider::get()->read(currSelection.address, stringBuffer.data(), stringBuffer.size());

                auto value = hex::encodeByteString(stringBuffer);
                auto copyValue = hex::encodeByteString(buffer);

                if (currSelection.size > MaxStringLength)
                    value += "...";

                return [value, copyValue] { ImGui::TextFormatted("\"{0}\"", value.c_str()); return copyValue; };
            },
            [](const std::string &value, std::endian endian) -> std::vector<u8> {
                return hex::decodeByteString(value);
            }
        );

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
            GUID guid = { 0 };
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

            auto copyValue = hex::format("#{:02X}{:02X}{:02X}{:02X}", u8(0xFF * (value.Value.x)), u8(0xFF * (value.Value.y)), u8(0xFF * (value.Value.z)), u8(0xFF * (value.Value.w)));

            return [value, copyValue] {
                ImGui::ColorButton("##inspectorColor", value, ImGuiColorEditFlags_None, ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetTextLineHeight()));
                return copyValue;
            };
        });
    }
    // clang-format on

}