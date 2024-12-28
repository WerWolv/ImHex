#include <hex/helpers/utils.hpp>

#include <pl/pattern_language.hpp>
#include <pl/patterns/pattern.hpp>

#include <imgui.h>

#include <capstone/capstone.h>
#include <content/helpers/disassembler.hpp>

#include <hex/ui/imgui_imhex_extensions.h>
#include <hex/api/localization_manager.hpp>

namespace hex::plugin::disasm {

    void drawDisassemblyVisualizer(pl::ptrn::Pattern &, bool shouldReset, std::span<const pl::core::Token::Literal> arguments) {
        struct Disassembly {
            u64 address;
            std::vector<u8> bytes;
            std::string instruction;
        };

        static std::vector<Disassembly> disassembly;
        if (shouldReset) {
            auto pattern  = arguments[0].toPattern();
            auto baseAddress  = arguments[1].toUnsigned();
            const auto [arch, mode] = CapstoneDisassembler::stringToSettings(arguments[2].toString());

            disassembly.clear();

            csh capstone;
            if (cs_open(arch, mode, &capstone) == CS_ERR_OK) {
                cs_option(capstone, CS_OPT_SKIPDATA, CS_OPT_ON);

                auto data = pattern->getBytes();
                cs_insn *instructions = nullptr;

                size_t instructionCount = cs_disasm(capstone, data.data(), data.size(), baseAddress, 0, &instructions);
                for (size_t i = 0; i < instructionCount; i++) {
                    disassembly.push_back({ instructions[i].address, { instructions[i].bytes, instructions[i].bytes + instructions[i].size }, hex::format("{} {}", instructions[i].mnemonic, instructions[i].op_str) });
                }
                cs_free(instructions, instructionCount);
                cs_close(&capstone);
            }
        }

        if (ImGui::BeginTable("##disassembly", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY, scaled(ImVec2(0, 300)))) {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("hex.ui.common.address"_lang);
            ImGui::TableSetupColumn("hex.ui.common.bytes"_lang);
            ImGui::TableSetupColumn("hex.ui.common.instruction"_lang);
            ImGui::TableHeadersRow();

            for (auto &entry : disassembly) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGuiExt::TextFormatted("0x{0:08X}", entry.address);
                ImGui::TableNextColumn();
                std::string bytes;
                for (auto byte : entry.bytes)
                    bytes += hex::format("{0:02X} ", byte);
                ImGui::TextUnformatted(bytes.c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(entry.instruction.c_str());
            }

            ImGui::EndTable();
        }
    }

}