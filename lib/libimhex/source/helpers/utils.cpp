#include <hex/helpers/utils.hpp>

#include <hex/api/imhex_api.hpp>

#include <hex/helpers/fmt.hpp>
#include <hex/helpers/crypto.hpp>

#include <hex/providers/buffered_reader.hpp>

#include <imgui.h>
#include <imgui_internal.h>

#include <GLFW/glfw3.h>
#include <hex/api/events/events_lifecycle.hpp>

#include <wolv/utils/string.hpp>

#include <clocale>
#include <sstream>

#if defined(OS_WINDOWS)
    #include <windows.h>
    #include <shellapi.h>

    #include <wolv/utils/guards.hpp>
#elif defined(OS_LINUX)
    #include <unistd.h>
    #include <dlfcn.h>
    #include <hex/helpers/utils_linux.hpp>
#elif defined(OS_MACOS)
    #include <unistd.h>
    #include <dlfcn.h>
    #include <hex/helpers/utils_macos.hpp>
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

    ImVec2 scaled(float x, float y) {
        return ImVec2(x, y) * ImHexApi::System::getGlobalScale();
    }

    std::string to_string(u128 value) {
        char data[45] = { 0 };

        u8 index = sizeof(data) - 2;
        while (value != 0 && index != 0) {
            data[index] = static_cast<char>('0' + (value % 10));
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
            data[index] = static_cast<char>('0' + (unsignedValue % 10));
            unsignedValue /= 10;
            index--;
        }

        if (value < 0) {
            data[index] = '-';
            return { data + index };
        } else {
            return { data + index + 1 };
        }
    }

    std::string toLower(std::string string) {
        for (char &c : string)
            c = std::tolower(c);

        return string;
    }

    std::string toUpper(std::string string) {
        for (char &c : string)
            c = std::toupper(c);

        return string;
    }

    std::vector<u8> parseHexString(std::string string) {
        if (string.empty())
            return { };

        // Remove common hex prefixes and commas
        string = wolv::util::replaceStrings(string, "0x", "");
        string = wolv::util::replaceStrings(string, "0X", "");
        string = wolv::util::replaceStrings(string, ",", "");

        // Check for non-hex characters
        bool isValidHexString = std::find_if(string.begin(), string.end(), [](char c) {
            return !std::isxdigit(c) && !std::isspace(c);
        }) == string.end();

        if (!isValidHexString)
            return { };

        // Remove all whitespace
        std::erase_if(string, [](char c) { return std::isspace(c); });

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
            result = fmt::format("{0:}", value);
        else
            result = fmt::format("{0:.2f}", value);

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
                result += fmt::format("\\x{0:02X}", u8(c));
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

    void startProgram(const std::string &command) {

#if defined(OS_WINDOWS)
        std::ignore = system(fmt::format("start \"\" {0}", command).c_str());
#elif defined(OS_MACOS)
        std::ignore = system(fmt::format("{0}", command).c_str());
#elif defined(OS_LINUX)
        executeCmd({"xdg-open", command});
#elif defined(OS_WEB)
        std::ignore = command;
#endif
    }

    int executeCommand(const std::string &command) {
        return ::system(command.c_str());
    }

    void openWebpage(std::string url) {
        if (!url.contains("://"))
            url = "https://" + url;

#if defined(OS_WINDOWS)
        ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
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
            if (std::isprint(byte) && byte != '\\') {
                result += char(byte);
            } else {
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
                        result += fmt::format("\\x{:02X}", byte);
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

    std::wstring utf8ToUtf16(const std::string& utf8) {
        std::vector<u32> unicodes;

        for (size_t byteIndex = 0; byteIndex < utf8.size();) {
            u32 unicode = 0;
            size_t unicodeSize = 0;

            u8 ch = utf8[byteIndex];
            byteIndex += 1;

            if (ch <= 0x7F) {
                unicode = ch;
                unicodeSize = 0;
            } else if (ch <= 0xBF) {
                return { };
            } else if (ch <= 0xDF) {
                unicode = ch&0x1F;
                unicodeSize = 1;
            } else if (ch <= 0xEF) {
                unicode = ch&0x0F;
                unicodeSize = 2;
            } else if (ch <= 0xF7) {
                unicode = ch&0x07;
                unicodeSize = 3;
            } else {
                return { };
            }

            for (size_t unicodeByteIndex = 0; unicodeByteIndex < unicodeSize; unicodeByteIndex += 1) {
                if (byteIndex == utf8.size())
                    return { };

                u8 byte = utf8[byteIndex];
                if (byte < 0x80 || byte > 0xBF)
                    return { };

                unicode <<= 6;
                unicode += byte & 0x3F;

                byteIndex += 1;
            }

            if (unicode >= 0xD800 && unicode <= 0xDFFF)
                return { };
            if (unicode > 0x10FFFF)
                return { };

            unicodes.push_back(unicode);
        }

        std::wstring utf16;

        for (auto unicode : unicodes) {
            if (unicode <= 0xFFFF) {
                utf16 += static_cast<wchar_t>(unicode);
            } else {
                unicode -= 0x10000;
                utf16 += static_cast<wchar_t>(((unicode >> 10) + 0xD800));
                utf16 += static_cast<wchar_t>(((unicode & 0x3FF) + 0xDC00));
            }
        }
        return utf16;
    }

    std::string utf16ToUtf8(const std::wstring& utf16) {
        std::vector<u32> unicodes;

        for (size_t index = 0; index < utf16.size();) {
            u32 unicode = 0;

            wchar_t wch = utf16[index];
            index += 1;

            if (wch < 0xD800 || wch > 0xDFFF) {
                unicode = static_cast<u32>(wch);
            } else if (wch >= 0xD800 && wch <= 0xDBFF) {
                if (index == utf16.size())
                    return "";

                wchar_t nextWch = utf16[index];
                index += 1;

                if (nextWch < 0xDC00 || nextWch > 0xDFFF)
                    return "";

                unicode = static_cast<u32>(((wch - 0xD800) << 10) + (nextWch - 0xDC00) + 0x10000);
            } else {
                return "";
            }

            unicodes.push_back(unicode);
        }

        std::string utf8;

        for (auto unicode : unicodes) {
            if (unicode <= 0x7F) {
                utf8 += static_cast<char>(unicode);
            } else if (unicode <= 0x7FF) {
                utf8 += static_cast<char>(0xC0 | ((unicode >> 6) & 0x1F));
                utf8 += static_cast<char>(0x80 | (unicode & 0x3F));
            } else if (unicode <= 0xFFFF) {
                utf8 += static_cast<char>(0xE0 | ((unicode >> 12) & 0x0F));
                utf8 += static_cast<char>(0x80 | ((unicode >> 6) & 0x3F));
                utf8 += static_cast<char>(0x80 | (unicode & 0x3F));
            } else if (unicode <= 0x10FFFF) {
                utf8 += static_cast<char>(0xF0 | ((unicode >> 18) & 0x07));
                utf8 += static_cast<char>(0x80 | ((unicode >> 12) & 0x3F));
                utf8 += static_cast<char>(0x80 | ((unicode >> 6) & 0x3F));
                utf8 += static_cast<char>(0x80 | (unicode & 0x3F));
            } else {
                return "";
            }
        }

        return utf8;
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

    [[nodiscard]] std::string limitStringLength(const std::string &string, size_t maxLength) {
        // If the string is shorter than the max length, return it as is
        if (string.size() < maxLength)
            return string;

        // If the string is longer than the max length, find the last space before the max length
        auto it = string.begin() + maxLength / 2;
        while (it != string.begin() && !std::isspace(*it)) --it;

        // If there's no space before the max length, just cut the string
        if (it == string.begin()) {
            it = string.begin() + maxLength / 2;

            // Try to find a UTF-8 character boundary
            while (it != string.begin() && (*it & 0xC0) == 0x80) --it;
        }

        // If we still didn't find a valid boundary, just return the string as is
        if (it == string.begin())
            return string;

        auto result = std::string(string.begin(), it) + "…";

        // If the string is longer than the max length, find the last space before the max length
        it = string.end() - 1 - maxLength / 2;
        while (it != string.end() && !std::isspace(*it)) ++it;

        // If there's no space before the max length, just cut the string
        if (it == string.end()) {
            it = string.end() - 1 - maxLength / 2;

            // Try to find a UTF-8 character boundary
            while (it != string.end() && (*it & 0xC0) == 0x80) ++it;
        }

        return result + std::string(it, string.end());
    }

    static std::optional<std::fs::path> s_fileToOpen;
    extern "C" void openFile(const char *path) {
        log::info("Opening file: {0}", path);
        s_fileToOpen = path;
    }

    std::optional<std::fs::path> getInitialFilePath() {
        return s_fileToOpen;
    }

    static std::map<std::fs::path, std::string> s_fonts;
    extern "C" void registerFont(const char *fontName, const char *fontPath) {
        s_fonts.emplace(fontPath, fontName);
    }

    const std::map<std::fs::path, std::string>& getFonts() {
        return s_fonts;
    }

    namespace {

        std::string generateHexViewImpl(u64 offset, auto begin, auto end) {
            constexpr static auto HeaderLine = "Hex View  00 01 02 03 04 05 06 07  08 09 0A 0B 0C 0D 0E 0F\n";
            std::string result;

            const auto size = std::distance(begin, end);
            result.reserve(std::string(HeaderLine).size() * size / 0x10);

            result += HeaderLine;

            u64 address = offset & ~u64(0x0F);
            std::string asciiRow;
            for (auto it = begin; it != end; ++it) {
                u8 byte = *it;

                if ((address % 0x10) == 0) {
                    result += fmt::format(" {}", asciiRow);
                    result += fmt::format("\n{0:08X}  ", address);

                    asciiRow.clear();

                    if (address == (offset & ~u64(0x0F))) {
                        for (u64 i = 0; i < (offset - address); i++) {
                            result += "   ";
                            asciiRow += " ";
                        }

                        if (offset - address >= 8)
                            result += " ";

                        address = offset;
                    }
                }

                result += fmt::format("{0:02X} ", byte);
                asciiRow += std::isprint(byte) ? char(byte) : '.';
                if ((address % 0x10) == 0x07)
                    result += " ";

                address++;
            }

            if ((address % 0x10) != 0x00)
                for (u32 i = 0; i < (0x10 - (address % 0x10)); i++)
                    result += "   ";

            result += fmt::format(" {}", asciiRow);

            return result;
        }

    }

    std::string generateHexView(u64 offset, u64 size, prv::Provider *provider) {
        auto reader = prv::ProviderReader(provider);
        reader.seek(offset);
        reader.setEndAddress((offset + size) - 1);

        return generateHexViewImpl(offset, reader.begin(), reader.end());
    }

    std::string generateHexView(u64 offset, const std::vector<u8> &data) {
        return generateHexViewImpl(offset, data.begin(), data.end());
    }

    std::string formatSystemError(i32 error) {
#if defined(OS_WINDOWS)
        wchar_t *message = nullptr;
        auto wLength = FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr, error,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (wchar_t*)&message, 0,
            nullptr
        );
        ON_SCOPE_EXIT { LocalFree(message); };

        auto length = ::WideCharToMultiByte(CP_UTF8, 0, message, wLength, nullptr, 0, nullptr, nullptr);
        std::string result(length, '\x00');
        ::WideCharToMultiByte(CP_UTF8, 0, message, wLength, result.data(), length, nullptr, nullptr);

        return result;
#else
        return std::system_category().message(error);
#endif
    }


    void* getContainingModule(void* symbol) {
#if defined(OS_WINDOWS)
        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQuery(symbol, &mbi, sizeof(mbi)))
            return mbi.AllocationBase;

        return nullptr;
#elif !defined(OS_WEB)
        Dl_info info = {};
        if (dladdr(symbol, &info) == 0)
            return nullptr;

        return dlopen(info.dli_fname, RTLD_LAZY);
#else
        std::ignore = symbol;
        return nullptr;
#endif
    }

    std::optional<ImColor> blendColors(const std::optional<ImColor> &a, const std::optional<ImColor> &b) {
        if (!a.has_value() && !b.has_value())
            return std::nullopt;
        else if (a.has_value() && !b.has_value())
            return a;
        else if (!a.has_value() && b.has_value())
            return b;
        else
            return ImAlphaBlendColors(a.value(), b.value());
    }

    std::optional<std::chrono::system_clock::time_point> parseTime(std::string_view format, const std::string &timeString) {
        std::istringstream input(timeString);
        input.imbue(std::locale(std::setlocale(LC_ALL, nullptr)));

        tm time = {};
        input >> std::get_time(&time, format.data());
        if (input.fail()) {
            return std::nullopt;
        }

        return std::chrono::system_clock::from_time_t(std::mktime(&time));
    }

    extern "C" void macOSCloseButtonPressed() {
        EventCloseButtonPressed::post();
    }

    extern "C" void macosEventDataReceived(const u8 *data, size_t length) {
        EventNativeMessageReceived::post(std::vector<u8>(data, data + length));
    }

}