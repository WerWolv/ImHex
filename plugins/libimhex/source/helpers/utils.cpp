#include <hex/helpers/utils.hpp>

#include <cstdio>
#include <codecvt>
#include <locale>
#include <filesystem>

#if defined(OS_WINDOWS)
    #include <windows.h>
#elif defined(OS_MACOS)
    #include <hex/helpers/utils_mac.h>
#endif


namespace hex {

    std::string to_string(u128 value) {
        char data[45] = { 0 };

        u8 index = sizeof(data) - 2;
        while (value != 0 && index != 0) {
            data[index] = '0' + value % 10;
            value /= 10;
            index--;
        }

        return std::string(data + index + 1);
    }

    std::string to_string(s128 value) {
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
            return std::string(data + index);
        } else
            return std::string(data + index + 1);
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

        std::string result = hex::format("%.2f", value);

        switch (unitIndex) {
            case 0: result += " Bytes"; break;
            case 1: result += " kB"; break;
            case 2: result += " MB"; break;
            case 3: result += " GB"; break;
            case 4: result += " TB"; break;
            case 5: result += " PB"; break;
            case 6: result += " EB"; break;
            default: result = "A lot!";
        }

        return result;
    }

    std::string makePrintable(char c) {
        switch (c) {
            case 0:   return "NUL";
            case 1:   return "SOH";
            case 2:   return "STX";
            case 3:   return "ETX";
            case 4:   return "EOT";
            case 5:   return "ENQ";
            case 6:   return "ACK";
            case 7:   return "BEL";
            case 8:   return "BS";
            case 9:   return "TAB";
            case 10:  return "LF";
            case 11:  return "VT";
            case 12:  return "FF";
            case 13:  return "CR";
            case 14:  return "SO";
            case 15:  return "SI";
            case 16:  return "DLE";
            case 17:  return "DC1";
            case 18:  return "DC2";
            case 19:  return "DC3";
            case 20:  return "DC4";
            case 21:  return "NAK";
            case 22:  return "SYN";
            case 23:  return "ETB";
            case 24:  return "CAN";
            case 25:  return "EM";
            case 26:  return "SUB";
            case 27:  return "ESC";
            case 28:  return "FS";
            case 29:  return "GS";
            case 30:  return "RS";
            case 31:  return "US";
            case 32:  return "Space";
            case 127: return "DEL";
            default:  return std::string() + c;
        }
    }

    std::vector<std::string> splitString(std::string_view string, std::string_view delimiter) {
        size_t start = 0, end;
        std::string token;
        std::vector<std::string> res;

        while ((end = string.find (delimiter, start)) != std::string::npos) {
            token = string.substr(start, end - start);
            start = end + delimiter.length();
            res.push_back(token);
        }

        res.push_back(std::string(string.substr(start)));
        return res;
    }

    std::string toEngineeringString(double value) {
        constexpr std::array Suffixes = { "a", "f", "p", "n", "u", "m", "", "k", "M", "G", "T", "P", "E" };

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

    std::vector<u8> readFile(std::string_view path) {
        FILE *file = fopen(path.data(), "rb");

        if (file == nullptr) return { };

        std::vector<u8> result;

        fseek(file, 0, SEEK_END);
        result.resize(ftell(file));
        rewind(file);

        fread(result.data(), 1, result.size(), file);

        return result;
    }

    void openWebpage(std::string_view url) {

        #if defined(OS_WINDOWS)
            system(hex::format("start %s", url.data()).c_str());
        #elif defined(OS_MACOS)
            system(hex::format("open %s", url.data()).c_str());
        #elif defined(OS_LINUX)
            system(hex::format("xdg-open %s", url.data()).c_str());
        #else
            #warning "Unknown OS, can't open webpages"
        #endif

    }

    std::string getPath(ImHexPath path) {
        #if defined(OS_WINDOWS)
            std::string exePath(MAX_PATH, '\0');
            GetModuleFileName(nullptr, exePath.data(), exePath.length());
            auto parentDir = std::filesystem::path(exePath).parent_path();

            switch (path) {
                case ImHexPath::Patterns:
                    return (parentDir / "patterns").string();
                case ImHexPath::PatternsInclude:
                    return (parentDir / "includes").string();
                case ImHexPath::Magic:
                    return (parentDir / "magic").string();
                case ImHexPath::Python:
                    return parentDir.string();
                case ImHexPath::Plugins:
                    return (parentDir / "plugins").string();
                case ImHexPath::Config:
                    return (parentDir / "config").string();
                case ImHexPath::Resources:
                    return (parentDir / "resources").string();
                default: __builtin_unreachable();
            }
        #elif defined(OS_MACOS)
            return getPathForMac(path);
        #else
            switch (path) {
                case ImHexPath::Patterns:
                    return "/usr/share/imhex/patterns";
                case ImHexPath::PatternsInclude:
                    return "/usr/share/imhex/includes";
                case ImHexPath::Magic:
                    return "/usr/share/imhex/magic";
                case ImHexPath::Python:
                    return "/usr/share/imhex";
                case ImHexPath::Plugins:
                    return "/usr/lib/imhex/plugins";
                case ImHexPath::Config:
                    return "/usr/share/imhex/config";
                case ImHexPath::Resources:
                    return "/usr/share/imhex/resources";
                default: __builtin_unreachable();
            }
        #endif
    }

}