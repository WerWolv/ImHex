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

        u64 m_imageLoadAddress   = 0;
        u64 m_imageBaseAddress = 0;
        ui::RegionType m_range = ui::RegionType::EntireData;
        Region m_regionToDisassemble = { };

        std::unique_ptr<ContentRegistry::Disassembler::Architecture> m_currArchitecture = nullptr;

        std::vector<ContentRegistry::Disassembler::Instruction> m_disassembly;

        void disassemble();
        void exportToFile();
    };

}
