#include <hex/api/content_registry.hpp>

#include <hex/providers/provider.hpp>
#include <hex/helpers/fmt.hpp>

namespace hex::plugin::builtin {

    static std::string formatLanguageArray(prv::Provider *provider, u64 offset, size_t size, const std::string &start, const std::string &byteFormat, const std::string &end) {
        constexpr auto NewLineIndent = "\n    ";

        std::string result = start;

        std::vector<u8> buffer(0x1'0000, 0x00);
        for (u64 i = 0; i < size; i += buffer.size()) {
            size_t readSize = std::min<u64>(buffer.size(), size - i);
            provider->read(offset, buffer.data(), readSize);

            for (u32 j = 0; j < readSize; j++) {
                if (j % 0x10 == 0)
                    result += NewLineIndent;

                result += hex::format(byteFormat, buffer[j]);
            }

            // Remove trailing comma
            result.pop_back();
            result.pop_back();
        }

        result += "\n";
        result += end;

        return result;
    }

    void registerDataFormatters() {

        ContentRegistry::DataFormatter::add("hex.builtin.view.hexeditor.copy.c", [](prv::Provider *provider, u64 offset, size_t size) {
            return formatLanguageArray(provider, offset, size, hex::format("const uint8_t data[{0}] = {{", size), "0x{0:02X}, ", "};");
        });

        ContentRegistry::DataFormatter::add("hex.builtin.view.hexeditor.copy.cpp", [](prv::Provider *provider, u64 offset, size_t size) {
            return formatLanguageArray(provider, offset, size, hex::format("constexpr std::array<uint8_t, {0}> data = {{", size), "0x{0:02X}, ", "};");
        });

        ContentRegistry::DataFormatter::add("hex.builtin.view.hexeditor.copy.java", [](prv::Provider *provider, u64 offset, size_t size) {
            return formatLanguageArray(provider, offset, size, "final byte[] data = {", "0x{0:02X}, ", "};");
        });

        ContentRegistry::DataFormatter::add("hex.builtin.view.hexeditor.copy.csharp", [](prv::Provider *provider, u64 offset, size_t size) {
            return formatLanguageArray(provider, offset, size, "const byte[] data = {", "0x{0:02X}, ", "};");
        });

        ContentRegistry::DataFormatter::add("hex.builtin.view.hexeditor.copy.rust", [](prv::Provider *provider, u64 offset, size_t size) {
            return formatLanguageArray(provider, offset, size, hex::format("let data: [u8; 0x{0:02X}] = [", size), "0x{0:02X}, ", "];");
        });

        ContentRegistry::DataFormatter::add("hex.builtin.view.hexeditor.copy.python", [](prv::Provider *provider, u64 offset, size_t size) {
            return formatLanguageArray(provider, offset, size, "data = bytes([", "0x{0:02X}, ", "]);");
        });

        ContentRegistry::DataFormatter::add("hex.builtin.view.hexeditor.copy.js", [](prv::Provider *provider, u64 offset, size_t size) {
            return formatLanguageArray(provider, offset, size, "const data = new Uint8Array([", "0x{0:02X}, ", "]);");
        });

        ContentRegistry::DataFormatter::add("hex.builtin.view.hexeditor.copy.ascii", [](prv::Provider *provider, u64 offset, size_t size) {
            std::string result = "Hex View  00 01 02 03 04 05 06 07  08 09 0A 0B 0C 0D 0E 0F\n\n";

            std::vector<u8> buffer(0x1'0000, 0x00);
            for (u64 byte = 0; byte < size; byte += buffer.size()) {
                size_t readSize = std::min<u64>(buffer.size(), size - byte);
                provider->read(offset, buffer.data(), readSize);

                const auto end = (offset + readSize) - 1;

                for (u32 col = offset >> 4; col <= (end >> 4); col++) {
                    result += hex::format("{0:08X}  ", col << 4);
                    for (u64 i = 0; i < 16; i++) {

                        if (col == (offset >> 4) && i < (offset & 0xF) || col == (end >> 4) && i > (end & 0xF))
                            result += "   ";
                        else
                            result += hex::format("{0:02X} ", buffer[((col << 4) - offset) + i]);

                        if ((i & 0xF) == 0x7)
                            result += " ";
                    }

                    result += " ";

                    for (u64 i = 0; i < 16; i++) {

                        if (col == (offset >> 4) && i < (offset & 0xF) || col == (end >> 4) && i > (end & 0xF))
                            result += " ";
                        else {
                            u8 c             = buffer[((col << 4) - offset) + i];
                            char displayChar = (c < 32 || c >= 128) ? '.' : c;
                            result += hex::format("{0}", displayChar);
                        }
                    }

                    result += "\n";
                }
            }
            return result;
        });

        ContentRegistry::DataFormatter::add("hex.builtin.view.hexeditor.copy.html", [](prv::Provider *provider, u64 offset, size_t size) {
            std::string result =
                "<div>\n"
                "    <style type=\"text/css\">\n"
                "        .offsetheader { color:#0000A0; line-height:200% }\n"
                "        .offsetcolumn { color:#0000A0 }\n"
                "        .hexcolumn { color:#000000 }\n"
                "        .textcolumn { color:#000000 }\n"
                "    </style>\n\n"
                "    <code>\n"
                "        <span class=\"offsetheader\">Hex View&nbsp&nbsp00 01 02 03 04 05 06 07&nbsp 08 09 0A 0B 0C 0D 0E 0F</span><br>\n";


            std::vector<u8> buffer(0x1'0000, 0x00);
            for (u64 byte = 0; byte < size; byte += buffer.size()) {
                size_t readSize = std::min<u64>(buffer.size(), size - byte);
                provider->read(offset, buffer.data(), readSize);

                const auto end = (offset + readSize) - 1;

                for (u32 col = offset >> 4; col <= (end >> 4); col++) {
                    result += hex::format("        <span class=\"offsetcolumn\">{0:08X}</span>&nbsp&nbsp<span class=\"hexcolumn\">", col << 4);
                    for (u64 i = 0; i < 16; i++) {

                        if (col == (offset >> 4) && i < (offset & 0xF) || col == (end >> 4) && i > (end & 0xF))
                            result += "&nbsp&nbsp ";
                        else
                            result += hex::format("{0:02X} ", buffer[((col << 4) - offset) + i]);

                        if ((i & 0xF) == 0x7)
                            result += "&nbsp";
                    }

                    result += "</span>&nbsp&nbsp<span class=\"textcolumn\">";

                    for (u64 i = 0; i < 16; i++) {

                        if (col == (offset >> 4) && i < (offset & 0xF) || col == (end >> 4) && i > (end & 0xF))
                            result += "&nbsp";
                        else {
                            u8 c             = buffer[((col << 4) - offset) + i];
                            char displayChar = (c < 32 || c >= 128) ? '.' : c;
                            result += hex::format("{0}", displayChar);
                        }
                    }

                    result += "</span><br>\n";
                }
            }

            result +=
                "    </code>\n"
                "</div>\n";

            return result;
        });
    }

}