#include <hex/helpers/utils.hpp>

#include <cstdio>
#include <codecvt>
#include <locale>
#include <filesystem>

#include <hex/helpers/fmt.hpp>

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

    std::vector<std::string> splitString(const std::string &string, const std::string &delimiter) {
        size_t start = 0, end;
        std::string token;
        std::vector<std::string> res;

        while ((end = string.find(delimiter, start)) != std::string::npos) {
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

        if (url.find("://") == std::string::npos)
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

    void openFileBrowser(const std::string &title, DialogMode mode, const std::vector<nfdfilteritem_t> &validExtensions, const std::function<void(std::string)> &callback) {
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