#include <hex/helpers/utils.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/api/localization_manager.hpp>

#include <hex/ui/imgui_imhex_extensions.h>

#include <charconv>

namespace hex::plugin::builtin {

    static u32 digitsNeeded(u32 numDigits, u32 baseX, u32 baseY) {
        if (numDigits == 0)
            return 0;

        const double ratio = std::log(static_cast<double>(baseX)) / std::log(static_cast<double>(baseY));
        return static_cast<u32>(std::ceil(numDigits * ratio));
    }

    void drawBaseConverter() {
        static std::array<std::string, 4> buffers;

        static auto ConvertBases = [](u8 base) {
            u64 number;

            // Store how many digits were in the source string
            u32 srcDigits = 0;
            switch (base) {
                case 10: srcDigits = static_cast<u32>(buffers[0].size()); break;
                case 16: srcDigits = static_cast<u32>(buffers[1].size()); break;
                case 8:  srcDigits = static_cast<u32>(buffers[2].size()); break;
                case 2:  srcDigits = static_cast<u32>(buffers[3].size()); break;
                default: return;
            }

            // Calculate how many digits each target base should have
            const std::array widths = {
                srcDigits,
                digitsNeeded(srcDigits, base, 16),
                digitsNeeded(srcDigits, base, 8),
                digitsNeeded(srcDigits, base, 2)
            };

            switch (base) {
                case 10:
                    number = wolv::util::from_chars<u64>(buffers[0], base).value_or(0);
                    break;
                case 16:
                    number = wolv::util::from_chars<u64>(buffers[1], base).value_or(0);
                    break;
                case 8:
                    number = wolv::util::from_chars<u64>(buffers[2], base).value_or(0);
                    break;
                case 2:
                    number = wolv::util::from_chars<u64>(buffers[3], base).value_or(0);
                    break;
                default:
                    return;
            }

            buffers[0] = std::to_string(number);
            buffers[1] = fmt::format("{0:X}", number);
            buffers[2] = fmt::format("{0:o}", number);
            buffers[3] = hex::toBinaryString(number);

            // Pad all outputs to match computed widths
            for (u8 i = 0; i < 4; i++) {
                if (buffers[i].size() < widths[i]) {
                    buffers[i] = std::string(widths[i] - buffers[i].size(), '0') + buffers[i];
                }
            }
        };

        ImGui::PushItemWidth(-1);
        {
            if (ImGuiExt::InputPrefix("##Decimal", "  ", buffers[0], ImGuiInputTextFlags_CharsDecimal))
                ConvertBases(10);
            ImGui::SetItemTooltip("%s", "hex.builtin.tools.base_converter.dec"_lang.get());

            if (ImGuiExt::InputPrefix("##Hexadecimal", "0x", buffers[1], ImGuiInputTextFlags_CharsHexadecimal))
                ConvertBases(16);
            ImGui::SetItemTooltip("%s", "hex.builtin.tools.base_converter.hex"_lang.get());

            if (ImGuiExt::InputPrefix("##Octal", "0o", buffers[2], ImGuiInputTextFlags_CharsDecimal))
                ConvertBases(8);
            ImGui::SetItemTooltip("%s", "hex.builtin.tools.base_converter.oct"_lang.get());

            if (ImGuiExt::InputPrefix("##Binary", "0b", buffers[3], ImGuiInputTextFlags_CharsDecimal))
                ConvertBases(2);
            ImGui::SetItemTooltip("%s", "hex.builtin.tools.base_converter.bin"_lang.get());
        }
        ImGui::PopItemWidth();
    }


}