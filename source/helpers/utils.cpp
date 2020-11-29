#include "helpers/utils.hpp"

#include <cstdio>
#include <codecvt>
#include <locale>

namespace hex {

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

}