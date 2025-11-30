#pragma once

#include <hex/api/task_manager.hpp>

#include <hex/ui/view.hpp>
#include <ui/widgets.hpp>

#include <vector>
#include <hex/api/content_registry/disassemblers.hpp>

namespace hex::plugin::disasm {

    class ViewDisassembler : public View::Window {
    public:
        explicit ViewDisassembler();
        ~ViewDisassembler() override;

        void drawContent() override;
        void drawHelpText() override;

    private:
        TaskHolder m_disassemblerTask;

        PerProvider<u64> m_imageLoadAddress;
        PerProvider<u64> m_imageBaseAddress;
        PerProvider<ui::RegionType> m_range;
        PerProvider<Region> m_regionToDisassemble;

        PerProvider<std::unique_ptr<ContentRegistry::Disassemblers::Architecture>> m_currArchitecture;

        PerProvider<std::vector<ContentRegistry::Disassemblers::Instruction>> m_disassembly;
        PerProvider<bool> m_settingsCollapsed;

        void disassemble();
        void exportToFile();
    };

}
