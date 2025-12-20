#include <algorithm>
#include <hex/api/imhex_api/provider.hpp>
#include <hex/api/imhex_api/hex_editor.hpp>
#include <hex/api/content_registry/data_inspector.hpp>

#include <hex/helpers/utils.hpp>
#include <hex/helpers/crypto.hpp>

#include <hex/helpers/fmt.hpp>
#include <wolv/types.hpp>
#include <fmt/chrono.h>

#include <hex/providers/provider.hpp>

#include <cstring>
#include <string>

#include <imgui_internal.h>
#include <fonts/vscode_icons.hpp>
#include <hex/helpers/default_paths.hpp>
#include <hex/helpers/encoding_file.hpp>
#include <hex/ui/imgui_imhex_extensions.h>
#include <popups/popup_file_chooser.hpp>

namespace hex::plugin::builtin {

    using Style = ContentRegistry::DataInspector::NumberDisplayStyle;

    struct GUID {
        u32 data1;
        u16 data2;
        u16 data3;
        u8 data4[8];
    };

    template<std::unsigned_integral T, size_t Size = sizeof(T)>
    static ContentRegistry::DataInspector::impl::EditingFunction stringToUnsigned() requires(sizeof(T) <= sizeof(u64)) {
        return ContentRegistry::DataInspector::EditWidget::TextInput([](const std::string &value, std::endian endian) -> std::vector<u8> {
            const auto result = wolv::util::from_chars<u64>(value).value_or(0);
             if (result > std::numeric_limits<T>::max()) return {};

             std::vector<u8> bytes(Size, 0x00);
             std::memcpy(bytes.data(), &result, bytes.size());

             if (endian != std::endian::native)
                 std::ranges::reverse(bytes);

             return bytes;
        });
    }

    template<std::signed_integral T, size_t Size = sizeof(T)>
    static ContentRegistry::DataInspector::impl::EditingFunction stringToSigned() requires(sizeof(T) <= sizeof(u64)) {
        return ContentRegistry::DataInspector::EditWidget::TextInput([](const std::string &value, std::endian endian) -> std::vector<u8> {
            const auto result = wolv::util::from_chars<i64>(value).value_or(0);
            if (result > std::numeric_limits<T>::max() || result < std::numeric_limits<T>::min()) return {};

            std::vector<u8> bytes(Size, 0x00);
            std::memcpy(bytes.data(), &result, bytes.size());

            if (endian != std::endian::native)
                std::ranges::reverse(bytes);

            return bytes;
        });
    }

    template<std::floating_point T>
    static ContentRegistry::DataInspector::impl::EditingFunction stringToFloat() requires(sizeof(T) <= sizeof(long double)) {
        return ContentRegistry::DataInspector::EditWidget::TextInput([](const std::string &value, std::endian endian) -> std::vector<u8> {
            const T result = wolv::util::from_chars<double>(value).value_or(0);

            std::vector<u8> bytes(sizeof(T), 0x00);
            std::memcpy(bytes.data(), &result, bytes.size());

            if (endian != std::endian::native)
                std::ranges::reverse(bytes);

            return bytes;
        });
    }

    template<std::integral T, size_t Size = sizeof(T)>
    static ContentRegistry::DataInspector::impl::EditingFunction stringToInteger() requires(sizeof(T) <= sizeof(u64)) {
        if constexpr (std::unsigned_integral<T>)
            return stringToUnsigned<T, Size>();
        else if constexpr (std::signed_integral<T>)
            return stringToSigned<T, Size>();
        else
            static_assert("Unsupported type for stringToInteger");
    }

    template<std::unsigned_integral T, size_t Size = sizeof(T)>
    static T bufferToInteger(const std::vector<u8> &buffer, std::endian endian) {
        T value = 0x00;
        std::memcpy(&value, buffer.data(), std::min(sizeof(T), Size));

        return hex::changeEndianness(value, Size, endian);
    }

    template<std::signed_integral T, size_t Size = sizeof(T)>
    static T bufferToInteger(const std::vector<u8> &buffer, std::endian endian) {
        T value = 0x00;
        std::memcpy(&value, buffer.data(), std::min(sizeof(T), Size));
        value = hex::changeEndianness(value, Size, endian);
        if constexpr (Size != sizeof(T))
            value = T(hex::signExtend(Size * 8, value));

        return value;
    }

    template<std::integral T, size_t Size = sizeof(T)>
    static std::string bufferToIntegerString(const std::vector<u8> &buffer, std::endian endian, Style style) requires(sizeof(T) <= sizeof(u64)) {
        if (buffer.size() < Size)
            return { };

        const auto formatString = (style == Style::Decimal) ? "{0:d}" : ((style == Style::Hexadecimal) ? fmt::format("0x{{0:0{}X}}", Size * 2) : fmt::format("0o{{0:0{}o}}", Size * 3));
        return fmt::format(fmt::runtime(formatString), bufferToInteger<T, Size>(buffer, endian));
    }

    template<std::integral T, size_t Size = sizeof(T)>
    static std::string integerToString(const std::vector<u8> &buffer, std::endian endian, Style style) requires(sizeof(T) <= sizeof(u64)) {
        if constexpr (std::integral<T>)
            return bufferToIntegerString<T, Size>(buffer, endian, style);
        else
            return {};
    }

    template<typename T, size_t Size = sizeof(T), typename Func = void>
    static hex::ContentRegistry::DataInspector::impl::GeneratorFunction drawString(Func func) {
        return [func](const std::vector<u8> &buffer, std::endian endian, Style style) {
            return [buffer, endian, value = func(buffer, endian, style)]() -> std::string {
                ContentRegistry::DataInspector::drawMenuItems([&] {
                    if (ImGui::MenuItemEx("hex.builtin.inspector.jump_to_address"_lang, ICON_VS_DEBUG_STEP_OUT)) {
                        auto address = bufferToInteger<T, Size>(buffer, endian);
                        if (address >= 0) {
                            ImHexApi::HexEditor::setSelection(Region { .address=u64(address), .size=sizeof(u8) });
                        }
                    }
                });

                ImGui::TextUnformatted(value.c_str());

                return value;
            };
        };

    }

    // clang-format off
    void registerDataInspectorEntries() {

        ContentRegistry::DataInspector::add("hex.builtin.inspector.binary", sizeof(u8),
            [](auto buffer, auto endian, auto style) {
                std::ignore = endian;
                std::ignore = style;

                std::string binary = fmt::format("0b{:08b}", buffer[0]);

                return [binary] {
                    ImGui::TextUnformatted(binary.c_str());
                    return binary;
                };
            },
            ContentRegistry::DataInspector::EditWidget::TextInput([](const std::string &value, std::endian endian) -> std::vector<u8> {
                std::ignore = endian;

                std::string binary = value;
                if (binary.starts_with("0b"))
                    binary = binary.substr(2);

                if (binary.size() > 8) return { };

                if (auto result = hex::parseBinaryString(binary); result.has_value())
                    return { result.value() };
                else
                    return { };
            })
        );



        ContentRegistry::DataInspector::add("hex.builtin.inspector.u8", sizeof(u8),
            drawString<u8>(integerToString<u8>),
            stringToInteger<u8>()
        );

        ContentRegistry::DataInspector::add("hex.builtin.inspector.i8", sizeof(i8),
            drawString<i8>(integerToString<i8>),
            stringToInteger<i8>()
        );

        ContentRegistry::DataInspector::add("hex.builtin.inspector.u16", sizeof(u16),
            drawString<u16>(integerToString<u16>),
            stringToInteger<u16>()
        );

        ContentRegistry::DataInspector::add("hex.builtin.inspector.i16", sizeof(i16),
            drawString<i16>(integerToString<i16>),
            stringToInteger<i16>()
        );

        ContentRegistry::DataInspector::add("hex.builtin.inspector.u24", 3,
            drawString<u32, 3>(integerToString<u32, 3>),
            stringToInteger<u32, 3>()
        );

        ContentRegistry::DataInspector::add("hex.builtin.inspector.i24", 3,
            drawString<i32, 3>(integerToString<i32, 3>),
            stringToInteger<i32, 3>()
        );

        ContentRegistry::DataInspector::add("hex.builtin.inspector.u32", sizeof(u32),
            drawString<u32>(integerToString<u32>),
            stringToInteger<u32>()
        );

        ContentRegistry::DataInspector::add("hex.builtin.inspector.i32", sizeof(i32),
            drawString<i32>(integerToString<i32>),
            stringToInteger<i32>()
        );

        ContentRegistry::DataInspector::add("hex.builtin.inspector.u48", 6,
            drawString<u64, 6>(integerToString<u64, 6>),
            stringToInteger<u64, 6>()
        );

        ContentRegistry::DataInspector::add("hex.builtin.inspector.i48", 6,
            drawString<i64, 6>(integerToString<i64, 6>),
            stringToInteger<i64, 6>()
        );

        ContentRegistry::DataInspector::add("hex.builtin.inspector.u64", sizeof(u64),
            drawString<u64>(integerToString<u64>),
            stringToInteger<u64>()
        );

        ContentRegistry::DataInspector::add("hex.builtin.inspector.i64", sizeof(i64),
            drawString<i64>(integerToString<i64>),
            stringToInteger<i64>()
        );

        ContentRegistry::DataInspector::add("hex.builtin.inspector.float16", sizeof(u16),
            [](auto buffer, auto endian, auto style) {
                u16 result = 0;
                std::memcpy(&result, buffer.data(), sizeof(u16));

                const auto formatString = style == Style::Hexadecimal ? "{0:a}" : "{0:G}";

                auto value = fmt::format(fmt::runtime(formatString), customFloatToFloat32<5, 10>(hex::changeEndianness(result, endian)));

                return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
            }
        );

        ContentRegistry::DataInspector::add("hex.builtin.inspector.float", sizeof(float),
            [](auto buffer, auto endian, auto style) {
                float result = 0;
                std::memcpy(&result, buffer.data(), sizeof(float));

                const auto formatString = style == Style::Hexadecimal ? "{0:a}" : "{0:G}";

                auto value = fmt::format(fmt::runtime(formatString), hex::changeEndianness(result, endian));
                return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
            },
            stringToFloat<float>()
        );

        ContentRegistry::DataInspector::add("hex.builtin.inspector.double", sizeof(double),
            [](auto buffer, auto endian, auto style) {
                double result = 0;
                std::memcpy(&result, buffer.data(), sizeof(double));

                const auto formatString = style == Style::Hexadecimal ? "{0:a}" : "{0:G}";

                auto value = fmt::format(fmt::runtime(formatString), hex::changeEndianness(result, endian));
                return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
            },
            stringToFloat<double>()
        );

        ContentRegistry::DataInspector::add("hex.builtin.inspector.long_double", sizeof(long double),
            [](auto buffer, auto endian, auto style) {
                long double result = 0;
                std::memcpy(&result, buffer.data(), sizeof(long double));

                const auto formatString = style == Style::Hexadecimal ? "{0:a}" : "{0:G}";

                auto value = fmt::format(fmt::runtime(formatString), hex::changeEndianness(result, endian));
                return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
            },
            stringToFloat<long double>()
        );

        ContentRegistry::DataInspector::add("hex.builtin.inspector.bfloat16", sizeof(u16),
            [](auto buffer, auto endian, auto style) {
                u16 result = 0;
                std::memcpy(&result, buffer.data(), sizeof(u16));

                const auto formatString = style == Style::Hexadecimal ? "{0:a}" : "{0:G}";

                auto value = fmt::format(fmt::runtime(formatString), customFloatToFloat32<8, 7>(hex::changeEndianness(result, endian)));

                return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
            }
        );

        ContentRegistry::DataInspector::add("hex.builtin.inspector.fp24", 3,
            [](auto buffer, auto endian, auto style) {
                u32 result = 0;
                std::memcpy(&result, buffer.data(), 3);

                const auto formatString = style == Style::Hexadecimal ? "{0:a}" : "{0:G}";

                auto value = fmt::format(fmt::runtime(formatString), customFloatToFloat32<7, 16>(hex::changeEndianness(result, endian)));

                return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
            }
        );

        ContentRegistry::DataInspector::add("hex.builtin.inspector.fixed_point", 1, [totalBits = 16, fractionBits = 8](const std::vector<u8> &, std::endian endian, Style style) mutable {
            std::string value;

            auto provider = ImHexApi::Provider::get();
            const auto currSelection = ImHexApi::HexEditor::getSelection();
            const u32 sizeBytes = (totalBits + 7) / 8;
            if (currSelection.has_value() && currSelection->getStartAddress() <= provider->getActualSize() - sizeBytes) {
                u64 fixedPointValue = 0x00;
                provider->read(currSelection->address, &fixedPointValue, std::min<size_t>(sizeBytes, sizeof(fixedPointValue)));

                fixedPointValue = changeEndianness(fixedPointValue, sizeBytes, endian);
                fixedPointValue &= u64(hex::bitmask(totalBits));

                const auto formatString = style == Style::Hexadecimal ? "{0:a}" : "{0:G}";
                value = fmt::format(fmt::runtime(formatString), double(fixedPointValue) / double(1ULL << fractionBits));
            } else {
                value = "???";
            }

            return [&, value, totalBitsCopy = totalBits, fractionBitsCopy = fractionBits]() mutable -> std::string {
                ContentRegistry::DataInspector::drawMenuItems([&] {
                    ImGui::SliderInt("##total_bits", &totalBits, 1, 64, fmt::format("hex.builtin.inspector.fixed_point.total"_lang, totalBits).c_str(), ImGuiSliderFlags_AlwaysClamp);
                    ImGui::SliderInt("##fractional_bits", &fractionBits, 0, totalBits - 1, fmt::format("hex.builtin.inspector.fixed_point.fraction"_lang, fractionBits).c_str(), ImGuiSliderFlags_AlwaysClamp);

                    if (fractionBits >= totalBits) {
                        fractionBits = totalBits - 1;
                    }
                });

                ImGui::TextUnformatted(value.c_str());
                ImGui::SameLine();
                ImGui::TextDisabled("(fp%d.%d)", totalBitsCopy - fractionBitsCopy, fractionBitsCopy);

                return value;
            };
        });

        ContentRegistry::DataInspector::add("hex.builtin.inspector.sleb128", 1, (16 * 8 / 7) + 1,
            [](auto buffer, auto endian, auto style) {
                std::ignore = endian;

                auto formatString = (style == Style::Decimal) ? "{0}{1:d}" : ((style == Style::Hexadecimal) ? "{0}0x{1:X}" : "{0}0o{1:o}");

                auto number   = hex::crypt::decodeSleb128(buffer);
                bool negative = number < 0;
                auto value    = fmt::format(fmt::runtime(formatString), negative ? "-" : "", negative ? -number : number);

                return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
            },
            ContentRegistry::DataInspector::EditWidget::TextInput([](const std::string &value, std::endian endian) -> std::vector<u8> {
                std::ignore = endian;

                return hex::crypt::encodeSleb128(wolv::util::from_chars<i64>(value).value_or(0));
            })
        );

        ContentRegistry::DataInspector::add("hex.builtin.inspector.uleb128", 1, (sizeof(u128) * 8 / 7) + 1,
            [](auto buffer, auto endian, auto style) {
                std::ignore = endian;

                auto formatString = (style == Style::Decimal) ? "{0:d}" : ((style == Style::Hexadecimal) ? "0x{0:X}" : "0o{0:o}");

                auto value = fmt::format(fmt::runtime(formatString), hex::crypt::decodeUleb128(buffer));

                return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
            },
            ContentRegistry::DataInspector::EditWidget::TextInput([](const std::string &value, std::endian endian) -> std::vector<u8> {
                std::ignore = endian;

                return hex::crypt::encodeUleb128(wolv::util::from_chars<u64>(value).value_or(0));
            })
        );

        ContentRegistry::DataInspector::add("hex.builtin.inspector.bool", sizeof(bool),
            [](auto buffer, auto endian, auto style) {
                std::ignore = endian;
                std::ignore = style;

                std::string value = [buffer] {
                    switch (buffer[0]) {
                        case false:
                            return "false";
                        case true:
                            return "true";
                        default:
                            return "Invalid";
                    }
                }();

                return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
            }
        );

        ContentRegistry::DataInspector::add("hex.builtin.inspector.ascii", sizeof(char8_t),
            [](auto buffer, auto endian, auto style) {
                std::ignore = endian;
                std::ignore = style;

                auto value = makePrintable(*reinterpret_cast<char8_t *>(buffer.data()));
                return [value] { ImGuiExt::TextFormatted("'{0}'", value.c_str()); return value; };
            },
            ContentRegistry::DataInspector::EditWidget::TextInput([](const std::string &value, std::endian endian) -> std::vector<u8> {
                std::ignore = endian;

                if (value.length() > 1) return { };

                return { u8(value[0]) };
            })
        );

        ContentRegistry::DataInspector::add("hex.builtin.inspector.wide", sizeof(wchar_t),
            [](auto buffer, auto endian, auto style) {
                std::ignore = style;

                wchar_t wideChar = '\x00';
                std::memcpy(&wideChar, buffer.data(), std::min(sizeof(wchar_t), buffer.size()));

                auto c = hex::changeEndianness(wideChar, endian);

                auto value = fmt::format("{0}", c <= 255 ? makePrintable(c) : wolv::util::wstringToUtf8(std::wstring(&c, 1)).value_or("???"));
                return [value] { ImGuiExt::TextFormatted("L'{0}'", value.c_str()); return value; };
            },
            ContentRegistry::DataInspector::EditWidget::TextInput([](const std::string &value, std::endian endian) -> std::vector<u8> {
                std::vector<u8> bytes;
                auto wideString = wolv::util::utf8ToWstring(value);
				if (!wideString.has_value())
				    return bytes;

				bytes.resize(wideString->size() * sizeof(wchar_t));
				std::memcpy(bytes.data(), wideString->data(), bytes.size());

                if (endian != std::endian::native)
                    std::ranges::reverse(bytes);

                return bytes;
            })
        );

        ContentRegistry::DataInspector::add("hex.builtin.inspector.char16", sizeof(char16_t),
            [](auto buffer, auto endian, auto style) {
                std::ignore = style;

                char16_t wideChar = '\x00';
                std::memcpy(&wideChar, buffer.data(), std::min(sizeof(char16_t), buffer.size()));

                auto c = hex::changeEndianness(wideChar, endian);

                auto value = fmt::format("{0}", c <= 255 ? makePrintable(c) : wolv::util::utf16ToUtf8(std::u16string(&c, 1)).value_or("???"));
                return [value] { ImGuiExt::TextFormatted("u'{0}'", value.c_str()); return value; };
            },
            ContentRegistry::DataInspector::EditWidget::TextInput([](const std::string &value, std::endian endian) -> std::vector<u8> {
                std::vector<u8> bytes;
                auto wideString = wolv::util::utf8ToUtf16(value);
                if (!wideString.has_value())
                    return bytes;

                bytes.resize(wideString->size() * sizeof(char16_t));
                std::memcpy(bytes.data(), wideString->data(), bytes.size());

                if (endian != std::endian::native)
                    std::ranges::reverse(bytes);

                return bytes;
            })
        );

        ContentRegistry::DataInspector::add("hex.builtin.inspector.char32", sizeof(char32_t),
            [](auto buffer, auto endian, auto style) {
                std::ignore = style;

                char32_t wideChar = '\x00';
                std::memcpy(&wideChar, buffer.data(), std::min(sizeof(char32_t), buffer.size()));

                auto c = hex::changeEndianness(wideChar, endian);

                auto value = fmt::format("{0}", c <= 255 ? makePrintable(c) : wolv::util::utf32ToUtf8(std::u32string(&c, 1)).value_or("???"));
                return [value] { ImGuiExt::TextFormatted("U'{0}'", value.c_str()); return value; };
            },
            ContentRegistry::DataInspector::EditWidget::TextInput([](const std::string &value, std::endian endian) -> std::vector<u8> {
                std::vector<u8> bytes;
                auto wideString = wolv::util::utf8ToUtf32(value);
                if (!wideString.has_value())
                    return bytes;

                bytes.resize(wideString->size() * sizeof(char32_t));
                std::memcpy(bytes.data(), wideString->data(), bytes.size());

                if (endian != std::endian::native)
                    std::ranges::reverse(bytes);

                return bytes;
            })
        );

        ContentRegistry::DataInspector::add("hex.builtin.inspector.utf8", sizeof(char8_t) * 4,
            [](auto buffer, auto endian, auto style) {
                std::ignore = endian;
                std::ignore = style;

                char utf8Buffer[5]      = { 0 };
                char codepointString[5] = { 0 };
                u32 codepoint           = 0;

                std::memcpy(utf8Buffer, reinterpret_cast<char8_t *>(buffer.data()), 4);
                u8 codepointSize = ImTextCharFromUtf8(&codepoint, utf8Buffer, nullptr);

                std::memcpy(codepointString, utf8Buffer, std::min(codepointSize, u8(4)));
                auto value = fmt::format("'{0}' (U+{1:04X})",
                    codepoint == 0xFFFD ? "Invalid" : (codepointSize == 1 ? makePrintable(codepointString[0]) : codepointString),
                    codepoint);

                return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
            }
        );

        constexpr static auto MaxStringLength = 64;

        ContentRegistry::DataInspector::add("hex.builtin.inspector.string", 1,
            [](auto buffer, auto endian, auto style) {
                std::ignore = buffer;
                std::ignore = endian;
                std::ignore = style;

                std::string value, copyValue;

                auto currSelection = ImHexApi::HexEditor::getSelection();
                if (currSelection.has_value()) {
                    std::vector<u8> stringBuffer(std::min<size_t>(currSelection->size, 0x1000), 0x00);
                    ImHexApi::Provider::get()->read(currSelection->address, stringBuffer.data(), stringBuffer.size());

                    value = copyValue = hex::encodeByteString(stringBuffer);

                    copyValue = value;
                    value = hex::limitStringLength(value, MaxStringLength, false);
                } else {
                    value = "";
                    copyValue = "";
                }

                return [value, copyValue] { ImGuiExt::TextFormatted("\"{0}\"", value.c_str()); return copyValue; };
            },
            ContentRegistry::DataInspector::EditWidget::TextInput([](const std::string &value, std::endian endian) -> std::vector<u8> {
                std::ignore = endian;

                return hex::decodeByteString(value);
            })
        );

        ContentRegistry::DataInspector::add("hex.builtin.inspector.wstring", sizeof(wchar_t),
            [](auto buffer, auto endian, auto style) {
                std::ignore = buffer;
                std::ignore = endian;
                std::ignore = style;

                auto currSelection = ImHexApi::HexEditor::getSelection();

                std::string value, copyValue;

                if (currSelection.has_value()) {
                    std::wstring stringBuffer(std::min<size_t>(alignTo<size_t>(currSelection->size, sizeof(wchar_t)), 0x1000), 0x00);
                    ImHexApi::Provider::get()->read(currSelection->address, stringBuffer.data(), stringBuffer.size());

                    for (auto &c : stringBuffer)
                        c = hex::changeEndianness(c, endian);

                    std::erase_if(stringBuffer, [](auto c) { return c == 0x00; });

                    auto string = wolv::util::wstringToUtf8(stringBuffer).value_or("Invalid");

                    copyValue = string;
                    value = hex::limitStringLength(string, MaxStringLength, false);
                } else {
                    value = "";
                    copyValue = "";
                }

                return [value, copyValue] { ImGuiExt::TextFormatted("L\"{0}\"", value.c_str()); return copyValue; };
            },
            ContentRegistry::DataInspector::EditWidget::TextInput([](const std::string &value, std::endian endian) -> std::vector<u8> {
                auto utf8 =  hex::decodeByteString(value);
                auto wstring = wolv::util::utf8ToWstring({ utf8.begin(), utf8.end() });
                if (!wstring.has_value())
                    return {};

                for (auto &c : wstring.value()) {
                    c = hex::changeEndianness(c, endian);
                }

                std::vector<u8> bytes(wstring->size() * sizeof(wchar_t), 0x00);
                std::memcpy(bytes.data(), wstring->data(), bytes.size());
                return bytes;
            })
        );

        ContentRegistry::DataInspector::add("hex.builtin.inspector.string16", sizeof(char16_t),
            [](auto buffer, auto endian, auto style) {
                std::ignore = buffer;
                std::ignore = endian;
                std::ignore = style;

                auto currSelection = ImHexApi::HexEditor::getSelection();

                std::string value, copyValue;

                if (currSelection.has_value()) {
                    std::u16string stringBuffer(std::min<size_t>(alignTo<size_t>(currSelection->size, sizeof(char16_t)), 0x1000), 0x00);
                    ImHexApi::Provider::get()->read(currSelection->address, stringBuffer.data(), stringBuffer.size());

                    for (auto &c : stringBuffer)
                        c = hex::changeEndianness(c, endian);

                    std::erase_if(stringBuffer, [](auto c) { return c == 0x00; });

                    auto string = wolv::util::utf16ToUtf8(stringBuffer).value_or("Invalid");

                    copyValue = string;
                    value = hex::limitStringLength(string, MaxStringLength, false);
                } else {
                    value = "";
                    copyValue = "";
                }

                return [value, copyValue] { ImGuiExt::TextFormatted("u\"{0}\"", value.c_str()); return copyValue; };
            },
            ContentRegistry::DataInspector::EditWidget::TextInput([](const std::string &value, std::endian endian) -> std::vector<u8> {
                auto utf8 =  hex::decodeByteString(value);
                auto utf16 = wolv::util::utf8ToUtf16({ utf8.begin(), utf8.end() });
                if (!utf16.has_value())
                    return {};

                for (auto &c : utf16.value()) {
                    c = hex::changeEndianness(c, endian);
                }

                std::vector<u8> bytes(utf16->size() * sizeof(char16_t), 0x00);
                std::memcpy(bytes.data(), utf16->data(), bytes.size());
                return bytes;
            })
        );

        ContentRegistry::DataInspector::add("hex.builtin.inspector.string32", sizeof(char32_t),
            [](auto buffer, auto endian, auto style) {
                std::ignore = buffer;
                std::ignore = endian;
                std::ignore = style;

                auto currSelection = ImHexApi::HexEditor::getSelection();

                std::string value, copyValue;

                if (currSelection.has_value()) {
                    std::u32string stringBuffer(std::min<size_t>(alignTo<size_t>(currSelection->size, sizeof(char32_t)), 0x1000), 0x00);
                    ImHexApi::Provider::get()->read(currSelection->address, stringBuffer.data(), stringBuffer.size());

                    for (auto &c : stringBuffer)
                        c = hex::changeEndianness(c, endian);

                    std::erase_if(stringBuffer, [](auto c) { return c == 0x00; });

                    auto string = wolv::util::utf32ToUtf8(stringBuffer).value_or("Invalid");

                    copyValue = string;
                    value = hex::limitStringLength(string, MaxStringLength, false);
                } else {
                    value = "";
                    copyValue = "";
                }

                return [value, copyValue] { ImGuiExt::TextFormatted("U\"{0}\"", value.c_str()); return copyValue; };
            },
            ContentRegistry::DataInspector::EditWidget::TextInput([](const std::string &value, std::endian endian) -> std::vector<u8> {
                auto utf8 =  hex::decodeByteString(value);
                auto utf32 = wolv::util::utf8ToUtf32({ utf8.begin(), utf8.end() });
                if (!utf32.has_value())
                    return {};

                for (auto &c : utf32.value()) {
                    c = hex::changeEndianness(c, endian);
                }

                std::vector<u8> bytes(utf32->size() * sizeof(char32_t), 0x00);
                std::memcpy(bytes.data(), utf32->data(), bytes.size());
                return bytes;
            })
        );

        ContentRegistry::DataInspector::add("hex.builtin.inspector.custom_encoding", 1, [encodingFile = EncodingFile()](const std::vector<u8> &, std::endian, Style) mutable {
            std::string value, copyValue;

            if (encodingFile.valid()) {
                auto currSelection = ImHexApi::HexEditor::getSelection();

                if (currSelection.has_value()) {
                    std::vector<u8> stringBuffer(std::min<size_t>(currSelection->size, 0x1000), 0x00);
                    ImHexApi::Provider::get()->read(currSelection->address, stringBuffer.data(), stringBuffer.size());

                    copyValue = value = encodingFile.decodeAll(stringBuffer);

                    if (value.size() > MaxStringLength) {
                        value.resize(MaxStringLength);
                        value += "...";
                    }
                } else {
                    value = "";
                    copyValue = "";
                }
            } else {
                value = "Invalid";
            }
            return [&, value, copyValue]() mutable -> std::string {
                ContentRegistry::DataInspector::drawMenuItems([&] {
                    if (ImGui::MenuItemEx("hex.builtin.inspector.custom_encoding.change"_lang, "あ")) {
                        const auto basePaths = paths::Encodings.read();
                        std::vector<std::fs::path> paths;
                        for (const auto &basePath : basePaths) {
                            for (const auto &entry : std::filesystem::directory_iterator(basePath)) {
                                paths.push_back(entry.path());
                            }
                        }

                        ui::PopupFileChooser::open(basePaths, paths, std::vector<fs::ItemFilter>{ {"Thingy Table File", "tbl"} }, false, [&](const auto &path) {
                            encodingFile = EncodingFile(EncodingFile::Type::Thingy, path);
                        });
                    }
                });

                if (encodingFile.valid() && !value.empty()) {
                    ImGuiExt::TextFormatted("({})\"{}\"", encodingFile.getName(), value);
                } else {
                    ImGuiExt::TextFormattedDisabled("hex.builtin.inspector.custom_encoding.no_encoding"_lang);
                }

                return copyValue;
            };
        });

#if defined(OS_WINDOWS)

        ContentRegistry::DataInspector::add("hex.builtin.inspector.time32", sizeof(u32), [](auto buffer, auto endian, auto style) {
            std::ignore = style;

            time_t endianAdjustedTime = hex::changeEndianness(*reinterpret_cast<u32 *>(buffer.data()), endian);

            std::string value;
            try {
                auto time = std::localtime(&endianAdjustedTime);
                if (time == nullptr) {
                    value = "Invalid";
                } else {
                    value = fmt::format("{0:%a, %d.%m.%Y %H:%M:%S}", *time);
                }
            } catch (fmt::format_error &) {
                value = "Invalid";
            }

            return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
        });

        ContentRegistry::DataInspector::add("hex.builtin.inspector.time64", sizeof(u64), [](auto buffer, auto endian, auto style) {
            std::ignore = style;

            time_t endianAdjustedTime = hex::changeEndianness(*reinterpret_cast<u64 *>(buffer.data()), endian);

            std::string value;
            try {
                auto time = std::localtime(&endianAdjustedTime);
                if (time == nullptr) {
                    value = "Invalid";
                } else {
                    value = fmt::format("{0:%a, %d.%m.%Y %H:%M:%S}", *time);
                }
            } catch (fmt::format_error &) {
                value = "Invalid";
            }

            return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
        });

#else

        ContentRegistry::DataInspector::add("hex.builtin.inspector.time", sizeof(time_t), [](auto buffer, auto endian, auto style) {
            std::ignore = style;

            time_t endianAdjustedTime = hex::changeEndianness(*reinterpret_cast<time_t *>(buffer.data()), endian);

            std::string value;
            try {
                auto time = std::localtime(&endianAdjustedTime);
                if (time == nullptr) {
                    value = "Invalid";
                } else {
                    value = fmt::format("{0:%a, %d.%m.%Y %H:%M:%S}", *time);
                }
            } catch (const fmt::format_error &e) {
                value = "Invalid";
            }

            return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
        });

#endif

        struct DOSDate {
            unsigned day   : 5;
            unsigned month : 4;
            unsigned year  : 7;
        };

        struct DOSTime {
            unsigned seconds : 5;
            unsigned minutes : 6;
            unsigned hours   : 5;
        };

        ContentRegistry::DataInspector::add("hex.builtin.inspector.dos_date", sizeof(DOSDate), [](auto buffer, auto endian, auto style) {
            std::ignore = style;

            DOSDate date = { };
            std::memcpy(&date, buffer.data(), sizeof(DOSDate));
            date = hex::changeEndianness(date, endian);

            auto value = fmt::format("{}/{}/{}", u8(date.day), u8(date.month), u8(date.year) + 1980);

            return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
        });

        ContentRegistry::DataInspector::add("hex.builtin.inspector.dos_time", sizeof(DOSTime), [](auto buffer, auto endian, auto style) {
            std::ignore = style;

            DOSTime time = { };
            std::memcpy(&time, buffer.data(), sizeof(DOSTime));
            time = hex::changeEndianness(time, endian);

            auto value = fmt::format("{:02}:{:02}:{:02}", u8(time.hours), u8(time.minutes), u8(time.seconds) * 2);

            return [value] { ImGui::TextUnformatted(value.c_str()); return value; };
        });

        ContentRegistry::DataInspector::add("hex.builtin.inspector.guid", sizeof(GUID), [](auto buffer, auto endian, auto style) {
            std::ignore = style;

            GUID guid = { };
            std::memcpy(&guid, buffer.data(), sizeof(GUID));
            auto value = fmt::format("{}{{{:08X}-{:04X}-{:04X}-{:02X}{:02X}-{:02X}{:02X}{:02X}{:02X}{:02X}{:02X}}}",
                (hex::changeEndianness(guid.data3, endian) >> 12) <= 5 && ((guid.data4[0] >> 4) >= 8 || (guid.data4[0] >> 4) == 0) ? "" : "Invalid ",
                hex::changeEndianness(guid.data1, endian),
                hex::changeEndianness(guid.data2, endian),
                hex::changeEndianness(guid.data3, endian),
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
            std::ignore = style;

            ImColor value(hex::changeEndianness(*reinterpret_cast<u32 *>(buffer.data()), endian));

            auto copyValue = fmt::format("#{:02X}{:02X}{:02X}{:02X}", u8(0xFF * (value.Value.x)), u8(0xFF * (value.Value.y)), u8(0xFF * (value.Value.z)), u8(0xFF * (value.Value.w)));

            return [value, copyValue] {
                ImGui::ColorButton("##inspectorColor", value, ImGuiColorEditFlags_None, ImVec2(ImGui::GetColumnWidth(), ImGui::GetTextLineHeight()));
                return copyValue;
            };
        });

        ContentRegistry::DataInspector::add("hex.builtin.inspector.rgb565", sizeof(u16), [](auto buffer, auto endian, auto style) {
            std::ignore = style;

            auto value = hex::changeEndianness(*reinterpret_cast<u16 *>(buffer.data()), endian);
            ImColor color((value & 0x1F) << 3, ((value >> 5) & 0x3F) << 2, ((value >> 11) & 0x1F) << 3, 0xFF);

            auto copyValue = fmt::format("#{:02X}{:02X}{:02X}", u8(0xFF * (color.Value.x)), u8(0xFF * (color.Value.y)), u8(0xFF * (color.Value.z)), 0xFF);

            return [color, copyValue] {
                ImGui::ColorButton("##inspectorColor", color, ImGuiColorEditFlags_AlphaOpaque, ImVec2(ImGui::GetColumnWidth(), ImGui::GetTextLineHeight()));
                return copyValue;
            };
        });
    }
    // clang-format on

}
