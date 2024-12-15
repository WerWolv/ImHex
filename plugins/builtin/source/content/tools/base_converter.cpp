#include <hex/helpers/utils.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/api/localization_manager.hpp>

#include <hex/ui/imgui_imhex_extensions.h>

#include <fonts/vscode_icons.hpp>

namespace hex::plugin::builtin {

    void drawBaseConverter() {
        static std::array<std::string, 4> buffers;

        static auto ConvertBases = [](u8 base) {
            u64 number;

            switch (base) {
                case 16:
                    number = std::strtoull(buffers[1].c_str(), nullptr, base);
                    break;
                case 10:
                    number = std::strtoull(buffers[0].c_str(), nullptr, base);
                    break;
                case 8:
                    number = std::strtoull(buffers[2].c_str(), nullptr, base);
                    break;
                case 2:
                    number = std::strtoull(buffers[3].c_str(), nullptr, base);
                    break;
                default:
                    return;
            }

            buffers[0] = std::to_string(number);
            buffers[1] = hex::format("0x{0:X}", number);
            buffers[2]  = hex::format("{0:#o}", number);
            buffers[3]  = hex::toBinaryString(number);
        };

        if (ImGuiExt::InputTextIcon("hex.builtin.tools.base_converter.dec"_lang, ICON_VS_SYMBOL_NUMERIC, buffers[0]))
            ConvertBases(10);

        if (ImGuiExt::InputTextIcon("hex.builtin.tools.base_converter.hex"_lang, ICON_VS_SYMBOL_NUMERIC, buffers[1]))
            ConvertBases(16);

        if (ImGuiExt::InputTextIcon("hex.builtin.tools.base_converter.oct"_lang, ICON_VS_SYMBOL_NUMERIC, buffers[2]))
            ConvertBases(8);

        if (ImGuiExt::InputTextIcon("hex.builtin.tools.base_converter.bin"_lang, ICON_VS_SYMBOL_NUMERIC, buffers[3]))
            ConvertBases(2);
    }

}