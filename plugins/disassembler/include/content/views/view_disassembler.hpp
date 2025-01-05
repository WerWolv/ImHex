#pragma once

#include <hex/api/task_manager.hpp>

#include <hex/ui/view.hpp>
#include <ui/widgets.hpp>

#include <content/helpers/disassembler.hpp>

#include <string>
#include <vector>
#include <hex/api/content_registry.hpp>

namespace hex::plugin::disasm {

    class ViewDisassembler : public View::Window {
    public:
        explicit ViewDisassembler();
        ~ViewDisassembler() override;

        void drawContent() override;

    private:
        TaskHolder m_disassemblerTask;

        PerProvider<u64> m_imageLoadAddress;
        PerProvider<u64> m_imageBaseAddress;
        PerProvider<ui::RegionType> m_range;
        PerProvider<Region> m_regionToDisassemble;

        PerProvider<std::unique_ptr<ContentRegistry::Disassembler::Architecture>> m_currArchitecture;

        PerProvider<std::vector<ContentRegistry::Disassembler::Instruction>> m_disassembly;

        void disassemble();
        void exportToFile();
    };

}
