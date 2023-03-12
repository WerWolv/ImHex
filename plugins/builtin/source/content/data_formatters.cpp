#include <hex/api/content_registry.hpp>

#include <hex/providers/provider.hpp>
#include <hex/providers/buffered_reader.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/helpers/crypto.hpp>

namespace hex::plugin::builtin {

    static std::string formatLanguageArray(prv::Provider *provider, u64 offset, size_t size, const std::string &start, const std::string &byteFormat, const std::string &end) {
        constexpr static auto NewLineIndent = "\n    ";
        constexpr static auto LineLength = 16;

        std::string result;
        result.reserve(start.size() + hex::format(byteFormat, 0x00).size() * size +  + std::string(NewLineIndent).size() / LineLength + end.size());

        result += start;

        auto reader = prv::ProviderReader(provider);
        reader.seek(offset);
        reader.setEndAddress(offset + size - 1);

        u64 index = 0x00;
        for (u8 byte : reader) {
            if ((index % LineLength) == 0x00)
                result += NewLineIndent;

            result += hex::format(byteFormat, byte);

            index++;
        }

        // Remove trailing comma
        if (provider->getActualSize() > 0) {
            result.pop_back();
            result.pop_back();
        }

        result += "\n" + end;

        return result;
    }

    void registerDataFormatters() {

        ContentRegistry::DataFormatter::add("hex.builtin.view.hex_editor.copy.c", [](prv::Provider *provider, u64 offset, size_t size) {
            return formatLanguageArray(provider, offset, size, hex::format("const uint8_t data[{0}] = {{", size), "0x{0:02X}, ", "};");
        });

        ContentRegistry::DataFormatter::add("hex.builtin.view.hex_editor.copy.cpp", [](prv::Provider *provider, u64 offset, size_t size) {
            return formatLanguageArray(provider, offset, size, hex::format("constexpr std::array<uint8_t, {0}> data = {{", size), "0x{0:02X}, ", "};");
        });

        ContentRegistry::DataFormatter::add("hex.builtin.view.hex_editor.copy.java", [](prv::Provider *provider, u64 offset, size_t size) {
            return formatLanguageArray(provider, offset, size, "final byte[] data = {", "0x{0:02X}, ", "};");
        });

        ContentRegistry::DataFormatter::add("hex.builtin.view.hex_editor.copy.csharp", [](prv::Provider *provider, u64 offset, size_t size) {
            return formatLanguageArray(provider, offset, size, "const byte[] data = {", "0x{0:02X}, ", "};");
        });

        ContentRegistry::DataFormatter::add("hex.builtin.view.hex_editor.copy.rust", [](prv::Provider *provider, u64 offset, size_t size) {
            return formatLanguageArray(provider, offset, size, hex::format("let data: [u8; 0x{0:02X}] = [", size), "0x{0:02X}, ", "];");
        });

        ContentRegistry::DataFormatter::add("hex.builtin.view.hex_editor.copy.python", [](prv::Provider *provider, u64 offset, size_t size) {
            return formatLanguageArray(provider, offset, size, "data = bytes([", "0x{0:02X}, ", "])");
        });

        ContentRegistry::DataFormatter::add("hex.builtin.view.hex_editor.copy.js", [](prv::Provider *provider, u64 offset, size_t size) {
            return formatLanguageArray(provider, offset, size, "const data = new Uint8Array([", "0x{0:02X}, ", "]);");
        });

        ContentRegistry::DataFormatter::add("hex.builtin.view.hex_editor.copy.lua", [](prv::Provider *provider, u64 offset, size_t size) {
            return formatLanguageArray(provider, offset, size, "data = {", "0x{0:02X}, ", "}");
        });

        ContentRegistry::DataFormatter::add("hex.builtin.view.hex_editor.copy.go", [](prv::Provider *provider, u64 offset, size_t size) {
            return formatLanguageArray(provider, offset, size, "data := [...]byte {", "0x{0:02X}, ", "}");
        });

        ContentRegistry::DataFormatter::add("hex.builtin.view.hex_editor.copy.crystal", [](prv::Provider *provider, u64 offset, size_t size) {
            return formatLanguageArray(provider, offset, size, "data = [", "0x{0:02X}, ", "] of UInt8");
        });

        ContentRegistry::DataFormatter::add("hex.builtin.view.hex_editor.copy.swift", [](prv::Provider *provider, u64 offset, size_t size) {
            return formatLanguageArray(provider, offset, size, "let data: [Uint8] = [", "0x{0:02X}, ", "]");
        });

        ContentRegistry::DataFormatter::add("hex.builtin.view.hex_editor.copy.pascal", [](prv::Provider *provider, u64 offset, size_t size) {
            return formatLanguageArray(provider, offset, size, hex::format("data: array[0..{0}] of Byte = (", size - 1), "${0:02X}, ", ")");
        });

        ContentRegistry::DataFormatter::add("hex.builtin.view.hex_editor.copy.base64", [](prv::Provider *provider, u64 offset, size_t size) {
            std::vector<u8> data(size, 0x00);
            provider->read(offset, data.data(), size);

            auto result = crypt::encode64(data);

            return std::string(result.begin(), result.end());
        });

        ContentRegistry::DataFormatter::add("hex.builtin.view.hex_editor.copy.hex_view", [](prv::Provider *provider, u64 offset, size_t size) {
            constexpr static auto HeaderLine = "Hex View  00 01 02 03 04 05 06 07  08 09 0A 0B 0C 0D 0E 0F\n";
            std::string result;
            result.reserve(std::string(HeaderLine).size() * size / 0x10);

            result += HeaderLine;

            auto reader = prv::ProviderReader(provider);
            reader.seek(offset);
            reader.setEndAddress((offset + size) - 1);

            u64 address = offset & ~u64(0x0F);
            std::string asciiRow;
            for (u8 byte : reader) {
                if ((address % 0x10) == 0) {
                    result += hex::format(" {}", asciiRow);
                    result += hex::format("\n{0:08X}  ", address);

                    asciiRow.clear();

                    if (address == (offset & ~u64(0x0F))) {
                        for (u64 i = 0; i < (offset - address); i++) {
                            result += "   ";
                            asciiRow += " ";
                        }
                        address = offset;
                    }
                }

                result += hex::format("{0:02X} ", byte);
                asciiRow += std::isprint(byte) ? char(byte) : '.';
                if ((address % 0x10) == 0x07)
                    result += " ";

                address++;
            }

            if ((address % 0x10) != 0x00)
                for (u32 i = 0; i < (0x10 - (address % 0x10)); i++)
                    result += "   ";

            result += hex::format(" {}", asciiRow);

            return result;
        });

        ContentRegistry::DataFormatter::add("hex.builtin.view.hex_editor.copy.html", [](prv::Provider *provider, u64 offset, size_t size) {
            std::string result =
                "<div>\n"
                "    <style type=\"text/css\">\n"
                "        .offsetheader { color:#0000A0; line-height:200% }\n"
                "        .offsetcolumn { color:#0000A0 }\n"
                "        .hexcolumn { color:#000000 }\n"
                "        .textcolumn { color:#000000 }\n"
                "    </style>\n\n"
                "    <code>\n"
                "        <span class=\"offsetheader\">Hex View&nbsp&nbsp00 01 02 03 04 05 06 07&nbsp 08 09 0A 0B 0C 0D 0E 0F</span>";

            auto reader = prv::ProviderReader(provider);
            reader.seek(offset);
            reader.setEndAddress((offset + size) - 1);

            u64 address = offset & ~u64(0x0F);
            std::string asciiRow;
            for (u8 byte : reader) {
                if ((address % 0x10) == 0) {
                    result += hex::format("  {}", asciiRow);
                    result +=  hex::format("<br>\n        <span class=\"offsetcolumn\">{0:08X}</span>&nbsp&nbsp<span class=\"hexcolumn\">", address);

                    asciiRow.clear();

                    if (address == (offset & ~u64(0x0F))) {
                        for (u64 i = 0; i < (offset - address); i++) {
                            result += "&nbsp&nbsp&nbsp";
                            asciiRow += "&nbsp";
                        }
                        address = offset;
                    }

                    result += "</span>";
                }

                result += hex::format("{0:02X} ", byte);
                asciiRow += std::isprint(byte) ? char(byte) : '.';
                if ((address % 0x10) == 0x07)
                    result += "&nbsp";

                address++;
            }

            for (u32 i = 0; i < (0x10 - (address % 0x10)); i++)
                result += "&nbsp&nbsp&nbsp";
            result += asciiRow;

            result +=
                "\n    </code>\n"
                "</div>\n";

            return result;
        });
    }

}