#pragma once

#include <hex/api/content_registry.hpp>
#include <hex/ui/view.hpp>

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
            ImHexApi::HexEditor::ProviderRegion region;
            std::string bytes;
            std::string mnemonic;
            std::string operands;

            std::optional<u64> jumpDestination;
            ImVec2 linePos;
        };

        void addLine(prv::Provider *provider, const ContentRegistry::Disassembler::Instruction &instruction);

    private:
        PerProvider<std::vector<DisassemblyLine>> m_lines;
        ContentRegistry::Disassembler::Architecture *m_currArchitecture = nullptr;
    };

}