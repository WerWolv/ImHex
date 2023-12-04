#pragma once

#include <hex/api/content_registry.hpp>
#include <hex/api/task_manager.hpp>
#include <hex/ui/view.hpp>
#include <ui/widgets.hpp>

namespace hex::plugin::builtin {

    class ViewDisassembler : public View::Window {
    public:
        explicit ViewDisassembler();

        void drawContent() override;

        ImGuiWindowFlags getWindowFlags() const override {
            return ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
        }

    private:
        struct DisassemblyLine {
            enum class Type {
                Instruction,
                CallInstruction,
                Separator,
            } type;

            ImHexApi::HexEditor::ProviderRegion region;
            std::string bytes;
            std::string mnemonic;
            std::string operands;

            std::optional<u64> extraData;
            ImVec2 linePosition;
        };

        void addLine(prv::Provider *provider, const ContentRegistry::Disassembler::Instruction &instruction);

        bool drawInstructionLine(DisassemblyLine &line);
        bool drawSeparatorLine(DisassemblyLine &line);
    private:
        PerProvider<std::vector<DisassemblyLine>> m_lines;
        ContentRegistry::Disassembler::Architecture *m_currArchitecture = nullptr;

        ui::RegionType m_regionType = ui::RegionType::EntireData;
        Region m_disassembleRegion = { 0, 0 };

        TaskHolder m_disassembleTask;
    };

}
