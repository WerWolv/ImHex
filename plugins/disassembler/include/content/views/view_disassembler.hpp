#pragma once

#include <hex/api/task_manager.hpp>

#include <hex/ui/view.hpp>
#include <ui/widgets.hpp>

#include <content/helpers/disassembler.hpp>

#include <string>
#include <vector>

namespace hex::plugin::disasm {

    struct Disassembly {
        u64 address;
        u64 offset;
        size_t size;
        std::string bytes;
        std::string mnemonic;
        std::string operators;
    };

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

        Architecture m_architecture = Architecture::ARM;
        cs_mode m_mode              = cs_mode(0);

        std::vector<Disassembly> m_disassembly;

        void disassemble();
        void exportToFile();
    };

}