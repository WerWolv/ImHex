#include <hex/helpers/utils.hpp>

#include <cstdio>
#include <codecvt>
#include <locale>
#include <filesystem>

#include <hex/helpers/fmt.hpp>

#if defined(OS_WINDOWS)
    #include <windows.h>
    #include <shlobj.h>
#elif defined(OS_MACOS)
    #include <hex/helpers/utils_mac.h>
#elif defined(OS_LINUX)
    #include <xdg.hpp>
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

        std::string result = hex::format("{0:.2f}", value);

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

    std::string makePrintable(u8 c) {
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
            case 128 ... 255: return " ";
            default:  return std::string() + static_cast<char>(c);
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

        res.emplace_back(string.substr(start));
        return res;
    }

    std::string combineStrings(const std::vector<std::string> &strings, std::string_view delimiter) {
        std::string result;
        for (const auto &string : strings) {
            result += string;
            result += delimiter;
        }

        return result.substr(0, result.length() - delimiter.length());
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

    void runCommand(const std::string &command) {

        #if defined(OS_WINDOWS)
        system(hex::format("start {0}", command).c_str());
        #elif defined(OS_MACOS)
        system(hex::format("open {0}", command).c_str());
        #elif defined(OS_LINUX)
        system(hex::format("xdg-open {0}", command).c_str());
        #else
            #warning "Unknown OS, can't open webpages"
        #endif

    }

    void openWebpage(std::string url) {

        if (!url.starts_with("http://") && !url.starts_with("https://"))
            url = "https://" + url;

        #if defined(OS_WINDOWS)
            system(hex::format("start {0}", url).c_str());
        #elif defined(OS_MACOS)
            system(hex::format("open {0}", url).c_str());
        #elif defined(OS_LINUX)
            system(hex::format("xdg-open {0}", url).c_str());
        #else
            #warning "Unknown OS, can't open webpages"
        #endif

    }

    std::vector<std::string> getPath(ImHexPath path) {
        #if defined(OS_WINDOWS)
            std::string exePath(MAX_PATH, '\0');
            GetModuleFileName(nullptr, exePath.data(), exePath.length());
            auto parentDir = std::filesystem::path(exePath).parent_path();

            std::filesystem::path appDataDir;
            {
                LPWSTR wAppDataPath = nullptr;
                if (!SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &wAppDataPath)))
                    throw std::runtime_error("Failed to get APPDATA folder path");

                appDataDir = wAppDataPath;
                CoTaskMemFree(wAppDataPath);
            }

            switch (path) {
                case ImHexPath::Patterns:
                    return { (parentDir / "patterns").string() };
                case ImHexPath::PatternsInclude:
                    return { (parentDir / "includes").string() };
                case ImHexPath::Magic:
                    return { (parentDir / "magic").string() };
                case ImHexPath::Python:
                    return { parentDir.string() };
                case ImHexPath::Plugins:
                    return { (parentDir / "plugins").string() };
                case ImHexPath::Yara:
                    return { (parentDir / "yara").string() };
                case ImHexPath::Config:
                    return { (appDataDir / "imhex" / "config").string() };
                case ImHexPath::Resources:
                    return { (parentDir / "resources").string() };
                case ImHexPath::Constants:
                    return { (parentDir / "constants").string() };
                default: __builtin_unreachable();
            }
        #elif defined(OS_MACOS)
            return { getPathForMac(path) };
        #else
            std::vector<std::filesystem::path> configDirs = xdg::ConfigDirs();
            std::vector<std::filesystem::path> dataDirs = xdg::DataDirs();

            configDirs.insert(configDirs.begin(), xdg::ConfigHomeDir());
            dataDirs.insert(dataDirs.begin(), xdg::DataHomeDir());

            std::vector<std::string> result;

            switch (path) {
                case ImHexPath::Patterns:
                    std::transform(dataDirs.begin(), dataDirs.end(), std::back_inserter(result),
                        [](auto p) { return (p / "imhex" / "patterns").string(); });
                    return result;
                case ImHexPath::PatternsInclude:
                    std::transform(dataDirs.begin(), dataDirs.end(), std::back_inserter(result),
                        [](auto p) { return (p / "imhex" / "includes").string(); });
                    return result;
                case ImHexPath::Magic:
                    std::transform(dataDirs.begin(), dataDirs.end(), std::back_inserter(result),
                        [](auto p) { return (p / "imhex" / "magic").string(); });
                    return result;
                case ImHexPath::Python:
                    std::transform(dataDirs.begin(), dataDirs.end(), std::back_inserter(result),
                        [](auto p) { return (p / "imhex").string(); });
                    return result;
                case ImHexPath::Plugins:
                    std::transform(dataDirs.begin(), dataDirs.end(), std::back_inserter(result),
                        [](auto p) { return (p / "imhex" / "plugins").string(); });
                    return result;
                case ImHexPath::Yara:
                    std::transform(dataDirs.begin(), dataDirs.end(), std::back_inserter(result),
                        [](auto p) { return (p / "imhex" / "yara").string(); });
                    return result;
                case ImHexPath::Config:
                    std::transform(configDirs.begin(), configDirs.end(), std::back_inserter(result),
                        [](auto p) { return (p / "imhex").string(); });
                    return result;
                case ImHexPath::Resources:
                    std::transform(dataDirs.begin(), dataDirs.end(), std::back_inserter(result),
                        [](auto p) { return (p / "imhex" / "resources").string(); });
                    return result;
                case ImHexPath::Constants:
                    std::transform(dataDirs.begin(), dataDirs.end(), std::back_inserter(result),
                        [](auto p) { return (p / "imhex" / "constants").string(); });
                    return result;
                default: __builtin_unreachable();
            }
        #endif
    }

    void openFileBrowser(std::string_view title, DialogMode mode, const std::vector<nfdfilteritem_t> &validExtensions, const std::function<void(std::string)> &callback) {
        NFD::Init();

        nfdchar_t *outPath;
        nfdresult_t result;
        switch (mode) {
            case DialogMode::Open:
                result = NFD::OpenDialog(outPath, validExtensions.data(), validExtensions.size(), nullptr);
                break;
            case DialogMode::Save:
                result = NFD::SaveDialog(outPath, validExtensions.data(), validExtensions.size(), nullptr);
                break;
            case DialogMode::Folder:
                result = NFD::PickFolder(outPath, nullptr);
                break;
            default: __builtin_unreachable();
        }

        if (result == NFD_OKAY) {
            callback(outPath);
            NFD::FreePath(outPath);
        }

        NFD::Quit();
    }

    float float16ToFloat32(u16 float16) {
        u32 sign = float16 >> 15;
        u32 exponent = (float16 >> 10) & 0x1F;
        u32 mantissa = float16 & 0x3FF;

        u32 result;

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

        return reinterpret_cast<float&>(result);
    }

}