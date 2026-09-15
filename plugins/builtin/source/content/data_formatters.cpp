#include <hex/api/content_registry/data_formatter.hpp>
#include <hex/api/achievement_manager.hpp>

#include <hex/providers/provider.hpp>
#include <hex/providers/buffered_reader.hpp>

#include <hex/helpers/fmt.hpp>
#include <hex/helpers/crypto.hpp>
#include <hex/helpers/utils.hpp>

#include <nlohmann/json.hpp>

#include <wolv/utils/core.hpp>

namespace hex::plugin::builtin {
    using ContentRegistry::DataFormatter::ExportTable;

    namespace {

        std::string formatLanguageArray(prv::Provider *provider, u64 offset, size_t size, const std::string &start, const std::string &byteFormat, const std::string &end, bool removeFinalDelimiter = false, bool newLines = true) {
            constexpr static auto NewLineIndent = "\n    ";
            constexpr static auto LineLength = 16;

            std::string result;
            result.reserve(start.size() + fmt::format(fmt::runtime(byteFormat), 0x00).size() * size + std::string(NewLineIndent).size() / LineLength + end.size());

            result += start;

            auto reader = prv::ProviderReader(provider);
            reader.seek(offset);
            reader.setEndAddress(offset + size - 1);

            u64 index = 0x00;
            for (u8 byte : reader) {

                if (newLines) {
                    if ((index % LineLength) == 0x00)
                        result += NewLineIndent;
                }

                result += fmt::format(fmt::runtime(byteFormat), byte);

                index++;
            }

            // Remove trailing delimiter if required
            if (removeFinalDelimiter && size > 0) {
                result.pop_back();
                result.pop_back();
            }

            if (newLines) result += "\n";
            result += end;

            return result;
        }

        std::string escapeCsvField(const std::string &field) {
            // from spec RFC 4180 Section 2, Item 6: "Fields containing line breaks (CRLF), double quotes, and commas should be enclosed in double-quotes."
            if (field.find_first_of(",\"\n\r") == std::string::npos) {
                return field; // no quoting needed
            }
            std::string escaped;
            escaped.reserve(field.size() + 3 /* first \", last \" + at least single char that needs to be escaped*/);

            escaped.push_back('"');
            for (const char c : field) {
                if (c == '"') {
                    escaped.push_back('"');
                }
                escaped.push_back(c);
            }
            escaped.push_back('"');
            return escaped;
        }

        std::string escapeTsvField(const std::string &field) {
            std::string escaped;
            escaped.reserve(field.size());
            for (const char c : field) {
                if (c == '\t') { escaped += "\\t"; }
                else if (c == '\n') { escaped += "\\n"; }
                else if (c == '\r') { escaped += "\\r"; }
                else { escaped.push_back(c); }
            }
            return escaped;
        }

        std::vector<u8> formatTableDelimited(const ExportTable& table, const char delimiter, const auto& escapeFn) {
            std::string output;

            const auto &headers = table.getHeaders();
            const auto &rows = table.getRows();

            for (size_t i = 0; i < headers.size(); ++i) {
                if (i != 0) {
                    output.push_back(delimiter);
                }
                output += escapeFn(headers[i]);
            }
            output += "\r\n";
            for (const auto& row : rows) {
                for (size_t colIdx = 0; colIdx < row.size(); ++colIdx) {
                    if (colIdx != 0) {
                        output.push_back(delimiter);
                    }
                    output += escapeFn(std::visit([](const auto& x) { return fmt::format("{}", x); }, row[colIdx]));
                }
                output += "\r\n";
            }
            return { output.begin(), output.end() };
        }

        std::vector<u8> formatTableJson(const ExportTable &table) {
            const auto &headers = table.getHeaders();
            const auto &rows = table.getRows();

            nlohmann::json array = nlohmann::json::array();

            for (const auto &row : rows) {
                nlohmann::json object = nlohmann::json::object();
                for (size_t colIdx = 0; colIdx < headers.size(); ++colIdx) {
                    std::visit([&](const auto& x) { object[headers[colIdx]] = x; }, row[colIdx]);
                }
                array.push_back(std::move(object));
            }
            std::string dump = array.dump(4);
            dump.push_back('\n');
            return { dump.begin(), dump.end() };
        }
    }

    void registerDataFormatters() {

        ContentRegistry::DataFormatter::addExportMenuEntry("hex.builtin.view.hex_editor.copy.c"_unlocalized, [](prv::Provider *provider, u64 offset, size_t size, bool) {
            return formatLanguageArray(provider, offset, size, fmt::format("const uint8_t data[{0}] = {{", size), "0x{0:02X}, ", "};");
        });

        ContentRegistry::DataFormatter::addExportMenuEntry("hex.builtin.view.hex_editor.copy.cpp"_unlocalized, [](prv::Provider *provider, u64 offset, size_t size, bool preview) {
            if (!preview) {
                AchievementManager::unlockAchievement("hex.builtin.achievement.hex_editor"_unlocalized, "hex.builtin.achievement.hex_editor.copy_as.name"_unlocalized);
            }

            return formatLanguageArray(provider, offset, size, fmt::format("constexpr std::array<uint8_t, {0}> data = {{", size), "0x{0:02X}, ", "};");
        });

        ContentRegistry::DataFormatter::addExportMenuEntry("hex.builtin.view.hex_editor.copy.java"_unlocalized, [](prv::Provider *provider, u64 offset, size_t size, bool) {
            return formatLanguageArray(provider, offset, size, "final byte[] data = {", "0x{0:02X}, ", "};");
        });

        ContentRegistry::DataFormatter::addExportMenuEntry("hex.builtin.view.hex_editor.copy.csharp"_unlocalized, [](prv::Provider *provider, u64 offset, size_t size, bool) {
            return formatLanguageArray(provider, offset, size, "byte[] data = {", "0x{0:02X}, ", "};");
        });

        ContentRegistry::DataFormatter::addExportMenuEntry("hex.builtin.view.hex_editor.copy.rust"_unlocalized, [](prv::Provider *provider, u64 offset, size_t size, bool) {
            return formatLanguageArray(provider, offset, size, fmt::format("let data: [u8; 0x{0:02X}] = [", size), "0x{0:02X}, ", "];");
        });

        ContentRegistry::DataFormatter::addExportMenuEntry("hex.builtin.view.hex_editor.copy.python"_unlocalized, [](prv::Provider *provider, u64 offset, size_t size, bool) {
            return formatLanguageArray(provider, offset, size, "data = bytes([", "0x{0:02X}, ", "])");
        });

        ContentRegistry::DataFormatter::addExportMenuEntry("hex.builtin.view.hex_editor.copy.js"_unlocalized, [](prv::Provider *provider, u64 offset, size_t size, bool) {
            return formatLanguageArray(provider, offset, size, "const data = new Uint8Array([", "0x{0:02X}, ", "]);");
        });

        ContentRegistry::DataFormatter::addExportMenuEntry("hex.builtin.view.hex_editor.copy.lua"_unlocalized, [](prv::Provider *provider, u64 offset, size_t size, bool) {
            return formatLanguageArray(provider, offset, size, "data = {", "0x{0:02X}, ", "}");
        });

        ContentRegistry::DataFormatter::addExportMenuEntry("hex.builtin.view.hex_editor.copy.go"_unlocalized, [](prv::Provider *provider, u64 offset, size_t size, bool) {
            return formatLanguageArray(provider, offset, size, "data := [...]byte{", "0x{0:02X}, ", "}", false);
        });

        ContentRegistry::DataFormatter::addExportMenuEntry("hex.builtin.view.hex_editor.copy.crystal"_unlocalized, [](prv::Provider *provider, u64 offset, size_t size, bool) {
            return formatLanguageArray(provider, offset, size, "data = [", "0x{0:02X}, ", "] of UInt8");
        });

        ContentRegistry::DataFormatter::addExportMenuEntry("hex.builtin.view.hex_editor.copy.swift"_unlocalized, [](prv::Provider *provider, u64 offset, size_t size, bool) {
            return formatLanguageArray(provider, offset, size, "let data: [Uint8] = [", "0x{0:02X}, ", "]");
        });

        ContentRegistry::DataFormatter::addExportMenuEntry("hex.builtin.view.hex_editor.copy.pascal"_unlocalized, [](prv::Provider *provider, u64 offset, size_t size, bool) {
            return formatLanguageArray(provider, offset, size, fmt::format("data: array[0..{0}] of Byte = (", size - 1), "${0:02X}, ", ")");
        });

        ContentRegistry::DataFormatter::addExportMenuEntry("hex.builtin.view.hex_editor.copy.base64"_unlocalized, [](prv::Provider *provider, u64 offset, size_t size, bool) {
            std::vector<u8> data(size, 0x00);
            provider->read(offset, data.data(), size);

            auto result = crypt::encode64(data);

            return std::string(result.begin(), result.end());
        });

        ContentRegistry::DataFormatter::addExportMenuEntry("hex.builtin.view.hex_editor.copy.hex_view"_unlocalized, [](prv::Provider *provider, u64 offset, size_t size, bool) {
            return hex::generateHexView(offset, size, provider);
        });

        ContentRegistry::DataFormatter::addExportMenuEntry("hex.builtin.view.hex_editor.copy.html"_unlocalized, [](prv::Provider *provider, u64 offset, size_t size, bool preview) {
            // Don't display a preview for this formatter as it wouldn't make much sense either way.
            if (preview)
                return std::string();

            std::string result =
                "<div>\n"
                "    <style type=\"text/css\">\n"
                "        .offsetheader { color:#0000A0; line-height:200% }\n"
                "        .offsetcolumn { color:#0000A0 }\n"
                "        .hexcolumn { color:#000000 }\n"
                "        .textcolumn { color:#000000 }\n"
                "        .zerobyte { color:#808080 }\n"
                "    </style>\n\n"
                "    <code>\n"
                "        <span class=\"offsetheader\">Hex View&nbsp;&nbsp;00 01 02 03 04 05 06 07&nbsp; 08 09 0A 0B 0C 0D 0E 0F</span>";

            auto html_safe = [](u8 byte) -> std::string {
                if (!std::isprint(byte))
                    return ".";
                char b(byte);
                if (b==' ') return "&nbsp;";
                else if (b=='"') return "&quot;";
                else if (b=='\'') return "&apos;";
                else if (b=='&') return "&amp;";
                else if (b=='<') return "&lt;";
                else if (b=='>') return "&gt;";
                else return std::string{b};
            };

            auto reader = prv::ProviderReader(provider);
            reader.seek(offset);
            reader.setEndAddress((offset + size) - 1);

            u64 address = offset & ~u64(0x0F);

            std::string asciiRow;
            for (u8 byte : reader) {
                if ((address % 0x10) == 0) {
                    result += fmt::format("  {}", asciiRow);
                    result += fmt::format("<br>\n        <span class=\"offsetcolumn\">{0:08X}</span>&nbsp;&nbsp;<span class=\"hexcolumn\">", address);

                    asciiRow.clear();

                    if (address == (offset & ~u64(0x0F))) {
                        for (u64 i = 0; i < (offset - address); i++) {
                            result += "&nbsp;&nbsp;&nbsp;";
                            asciiRow += "&nbsp;";
                        }
                        address = offset;
                    }

                    result += "</span>";
                }

                std::string tagStart, tagEnd;
                if (byte == 0x00) {
                    tagStart = "<span class=\"zerobyte\">";
                    tagEnd = "</span>";
                }

                result += fmt::format("{0}{2:02X}{1} ", tagStart, tagEnd, byte);
                asciiRow += html_safe(byte);
                if ((address % 0x10) == 0x07)
                    result += "&nbsp;";

                address++;
            }

            if (address % 0x10 != 0x00)
                for (u32 i = 0; i < (0x10 - (address % 0x10)); i++)
                    result += "&nbsp;&nbsp;&nbsp;";
            result += asciiRow;

            result +=
                "\n    </code>\n"
                "</div>\n";

            return result;
        });

        ContentRegistry::DataFormatter::addExportMenuEntry("hex.builtin.view.hex_editor.copy.escaped_string"_unlocalized, [](prv::Provider *provider, u64 offset, size_t size, bool) {
            return formatLanguageArray(provider, offset, size, "\"", "\\x{0:02X}", "\"", false, false);
        });

        ContentRegistry::DataFormatter::addExportFormatter("hex.builtin.view.hex_editor.find_export.csv"_unlocalized, "csv", [](const ExportTable& table) {
            return formatTableDelimited(table, ',', escapeCsvField);
        });

        ContentRegistry::DataFormatter::addExportFormatter("hex.builtin.view.hex_editor.find_export.tsv"_unlocalized, "tsv", [](const ExportTable& table) {
            return formatTableDelimited(table, '\t', escapeTsvField);
        });

        ContentRegistry::DataFormatter::addExportFormatter("hex.builtin.view.hex_editor.find_export.json"_unlocalized, "json", [](const ExportTable& table) {
            return formatTableJson(table);
        });
    }

}
