#include <hex/helpers/utils.hpp>

#include <cstdio>
#include <codecvt>

#include <hex/api/imhex_api.hpp>
#include <hex/api/event.hpp>

#include <hex/helpers/fmt.hpp>
#include <hex/helpers/crypto.hpp>
#include <hex/helpers/utils_linux.hpp>

#include <imgui.h>
#include <imgui_internal.h>

#if defined(OS_WINDOWS)
    #include <windows.h>
#elif defined(OS_LINUX)
    #include <unistd.h>
#elif defined(OS_MACOS)
    #include <hex/helpers/utils_macos.hpp>
    #include <unistd.h>
#elif defined(OS_WEB)
    #include "emscripten.h"
#endif

namespace hex {

    float operator""_scaled(long double value) {
        return value * ImHexApi::System::getGlobalScale();
    }

    float operator""_scaled(unsigned long long value) {
        return value * ImHexApi::System::getGlobalScale();
    }

    ImVec2 scaled(const ImVec2 &vector) {
        return vector * ImHexApi::System::getGlobalScale();
    }

    std::string to_string(u128 value) {
        char data[45] = { 0 };

        u8 index = sizeof(data) - 2;
        while (value != 0 && index != 0) {
            data[index] = '0' + value % 10;
            value /= 10;
            index--;
        }

        return { data + index + 1 };
    }

    std::string to_string(i128 value) {
        char data[45] = { 0 };

        u128 unsignedValue = value < 0 ? -value : value;

        u8 index = sizeof(data) - 2;
        while (unsignedValue != 0 && index != 0) {
            data[index] = '0' + unsignedValue % 10;
            unsignedValue /= 10;
            index--;
        }

        if (value < 0) {
            data[index] = '-';
            return { data + index };
        } else
            return { data + index + 1 };
    }

    std::vector<u8> parseHexString(std::string string) {
        if (string.empty())
            return { };

        // Remove common hex prefixes and commas
        string = hex::replaceStrings(string, "0x", "");
        string = hex::replaceStrings(string, "0X", "");
        string = hex::replaceStrings(string, ",", "");

        // Check for non-hex characters
        bool isValidHexString = std::find_if(string.begin(), string.end(), [](char c) {
            return !std::isxdigit(c) && !std::isspace(c);
        }) == string.end();

        if (!isValidHexString)
            return { };

        // Remove all whitespace
        string.erase(std::remove_if(string.begin(), string.end(), [](char c) { return std::isspace(c); }), string.end());

        // Only parse whole bytes
        if (string.length() % 2 != 0)
            return { };

        // Convert hex string to bytes
        return crypt::decode16(string);
    }

    std::optional<u8> parseBinaryString(const std::string &string) {
        if (string.empty())
            return std::nullopt;

        u8 byte = 0x00;
        for (char c : string) {
            byte <<= 1;

            if (c == '1')
                byte |= 0b01;
            else if (c == '0')
                byte |= 0b00;
            else
                return std::nullopt;
        }

        return byte;
    }

    std::string toByteString(u64 bytes) {
        double value = bytes;
        u8 unitIndex = 0;

        while (value > 1024) {
            value /= 1024;
            unitIndex++;

            if (unitIndex == 6)
                break;
        }

        std::string result;

        if (unitIndex == 0)
            result = hex::format("{0:}", value);
        else
            result = hex::format("{0:.2f}", value);

        switch (unitIndex) {
            case 0:
                result += ((value == 1) ? " Byte" : " Bytes");
                break;
            case 1:
                result += " kiB";
                break;
            case 2:
                result += " MiB";
                break;
            case 3:
                result += " GiB";
                break;
            case 4:
                result += " TiB";
                break;
            case 5:
                result += " PiB";
                break;
            case 6:
                result += " EiB";
                break;
            default:
                result = "A lot!";
        }

        return result;
    }

    std::string makeStringPrintable(const std::string &string) {
        std::string result;
        for (char c : string) {
            if (std::isprint(c))
                result += c;
            else
                result += hex::format("\\x{0:02X}", u8(c));
        }

        return result;
    }

    std::string makePrintable(u8 c) {
        switch (c) {
            case 0:
                return "NUL";
            case 1:
                return "SOH";
            case 2:
                return "STX";
            case 3:
                return "ETX";
            case 4:
                return "EOT";
            case 5:
                return "ENQ";
            case 6:
                return "ACK";
            case 7:
                return "BEL";
            case 8:
                return "BS";
            case 9:
                return "TAB";
            case 10:
                return "LF";
            case 11:
                return "VT";
            case 12:
                return "FF";
            case 13:
                return "CR";
            case 14:
                return "SO";
            case 15:
                return "SI";
            case 16:
                return "DLE";
            case 17:
                return "DC1";
            case 18:
                return "DC2";
            case 19:
                return "DC3";
            case 20:
                return "DC4";
            case 21:
                return "NAK";
            case 22:
                return "SYN";
            case 23:
                return "ETB";
            case 24:
                return "CAN";
            case 25:
                return "EM";
            case 26:
                return "SUB";
            case 27:
                return "ESC";
            case 28:
                return "FS";
            case 29:
                return "GS";
            case 30:
                return "RS";
            case 31:
                return "US";
            case 32:
                return "Space";
            case 127:
                return "DEL";
            default:
                if (c >= 128)
                    return " ";
                else
                    return std::string() + static_cast<char>(c);
        }
    }

    std::vector<std::string> splitString(const std::string &string, const std::string &delimiter) {
        size_t start = 0, end = 0;
        std::string token;
        std::vector<std::string> res;

        while ((end = string.find(delimiter, start)) != std::string::npos) {
            size_t size = end - start;
            if (start + size > string.length())
                break;

            token = string.substr(start, end - start);
            start = end + delimiter.length();
            res.push_back(token);
        }

        res.emplace_back(string.substr(start));
        return res;
    }

    std::string combineStrings(const std::vector<std::string> &strings, const std::string &delimiter) {
        std::string result;
        for (const auto &string : strings) {
            result += string;
            result += delimiter;
        }

        return result.substr(0, result.length() - delimiter.length());
    }

    std::string replaceStrings(std::string string, const std::string &search, const std::string &replace) {
        if (search.empty())
            return string;

        std::size_t pos;
        while ((pos = string.find(search)) != std::string::npos)
            string.replace(pos, search.size(), replace);

        return string;
    }

    std::string toEngineeringString(double value) {
        constexpr static std::array Suffixes = { "a", "f", "p", "n", "u", "m", "", "k", "M", "G", "T", "P", "E" };

        int8_t suffixIndex = 6;

        while (suffixIndex != 0 && suffixIndex != 12 && (value >= 1000 || value < 1) && value != 0) {
            if (value >= 1000) {
                value /= 1000;
                suffixIndex++;
            } else if (value < 1) {
                value *= 1000;
                suffixIndex--;
            }
        }

        return std::to_string(value).substr(0, 5) + Suffixes[suffixIndex];
    }

    void runCommand(const std::string &command) {

        #if defined(OS_WINDOWS)
            hex::unused(system(hex::format("start {0}", command).c_str()));
        #elif defined(OS_MACOS)
            hex::unused(system(hex::format("open {0}", command).c_str()));
        #elif defined(OS_LINUX)
            executeCmd({"xdg-open", command});
        #elif defined(OS_WEB)
            hex::unused(command);
        #endif
    }

    void openWebpage(std::string url) {
        if (!url.contains("://"))
            url = "https://" + url;

        #if defined(OS_WINDOWS)
            ShellExecute(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        #elif defined(OS_MACOS)
            openWebpageMacos(url.c_str());
        #elif defined(OS_LINUX)
            executeCmd({"xdg-open", url});
        #elif defined(OS_WEB)
            EM_ASM({
                window.open(UTF8ToString($0), '_blank');
            }, url.c_str());
        #else
            #warning "Unknown OS, can't open webpages"
        #endif
    }

    std::optional<u8> hexCharToValue(char c) {
        if (std::isdigit(c))
            return c - '0';
        else if (std::isxdigit(c))
            return std::toupper(c) - 'A' + 0x0A;
        else
            return { };
    }

    std::string encodeByteString(const std::vector<u8> &bytes) {
        std::string result;

        for (u8 byte : bytes) {
            if (std::isprint(byte) && byte != '\\')
                result += char(byte);
            else {
                switch (byte) {
                    case '\\':
                        result += "\\";
                        break;
                    case '\a':
                        result += "\\a";
                        break;
                    case '\b':
                        result += "\\b";
                        break;
                    case '\f':
                        result += "\\f";
                        break;
                    case '\n':
                        result += "\\n";
                        break;
                    case '\r':
                        result += "\\r";
                        break;
                    case '\t':
                        result += "\\t";
                        break;
                    case '\v':
                        result += "\\v";
                        break;
                    default:
                        result += hex::format("\\x{:02X}", byte);
                        break;
                }
            }
        }

        return result;
    }

    std::vector<u8> decodeByteString(const std::string &string) {
        u32 offset = 0;
        std::vector<u8> result;

        while (offset < string.length()) {
            auto c = [&] { return string[offset]; };

            if (c() == '\\') {
                if ((offset + 2) > string.length()) return {};

                offset++;

                char escapeChar = c();

                offset++;

                switch (escapeChar) {
                    case 'a':
                        result.push_back('\a');
                        break;
                    case 'b':
                        result.push_back('\b');
                        break;
                    case 'f':
                        result.push_back('\f');
                        break;
                    case 'n':
                        result.push_back('\n');
                        break;
                    case 'r':
                        result.push_back('\r');
                        break;
                    case 't':
                        result.push_back('\t');
                        break;
                    case 'v':
                        result.push_back('\v');
                        break;
                    case '\\':
                        result.push_back('\\');
                        break;
                    case 'x':
                        {
                            u8 byte = 0x00;
                            if ((offset + 1) >= string.length()) return {};

                            for (u8 i = 0; i < 2; i++) {
                                byte <<= 4;
                                if (auto hexValue = hexCharToValue(c()); hexValue.has_value())
                                    byte |= hexValue.value();
                                else
                                    return {};

                                offset++;
                            }

                            result.push_back(byte);
                        }
                        break;
                    default:
                        return {};
                }
            } else {
                result.push_back(c());
                offset++;
            }
        }

        return result;
    }

    float float16ToFloat32(u16 float16) {
        u32 sign     = float16 >> 15;
        u32 exponent = (float16 >> 10) & 0x1F;
        u32 mantissa = float16 & 0x3FF;

        u32 result = 0x00;

        if (exponent == 0) {
            if (mantissa == 0) {
                // +- Zero
                result = sign << 31;
            } else {
                // Subnormal value
                exponent = 0x7F - 14;

                while ((mantissa & (1 << 10)) == 0) {
                    exponent--;
                    mantissa <<= 1;
                }

                mantissa &= 0x3FF;
                result = (sign << 31) | (exponent << 23) | (mantissa << 13);
            }
        } else if (exponent == 0x1F) {
            // +-Inf or +-NaN
            result = (sign << 31) | (0xFF << 23) | (mantissa << 13);
        } else {
            // Normal value
            result = (sign << 31) | ((exponent + (0x7F - 15)) << 23) | (mantissa << 13);
        }

        float floatResult = 0;
        std::memcpy(&floatResult, &result, sizeof(float));

        return floatResult;
    }

    bool isProcessElevated() {
        #if defined(OS_WINDOWS)
            bool elevated = false;
            HANDLE token  = INVALID_HANDLE_VALUE;

            if (::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token)) {
                TOKEN_ELEVATION elevation;
                DWORD elevationSize = sizeof(TOKEN_ELEVATION);

                if (::GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &elevationSize))
                    elevated = elevation.TokenIsElevated;
            }

            if (token != INVALID_HANDLE_VALUE)
                ::CloseHandle(token);

            return elevated;
        #elif defined(OS_LINUX) || defined(OS_MACOS)
            return getuid() == 0 || getuid() != geteuid();
        #else
            return false;
        #endif
    }

    std::optional<std::string> getEnvironmentVariable(const std::string &env) {
        auto value = std::getenv(env.c_str());

        if (value == nullptr)
            return std::nullopt;
        else
            return value;
    }

    static std::optional<std::fs::path> fileToOpen;
    extern "C" void openFile(const char *path) {
        fileToOpen = path;
    }

    std::optional<std::fs::path> getInitialFilePath() {
        return fileToOpen;
    }

}