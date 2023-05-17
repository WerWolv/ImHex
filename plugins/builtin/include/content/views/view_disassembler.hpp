#pragma once

#include <hex/api/task.hpp>

#include <hex/ui/view.hpp>
#include <ui/widgets.hpp>

#include <hex/helpers/disassembler.hpp>

#include <cstdio>
#include <string>
#include <vector>

namespace hex::plugin::builtin {

    struct Disassembly {
        u64 address;
        u64 offset;
        size_t size;
        std::string bytes;
        std::string mnemonic;
        std::string operators;
    };

    class ViewDisassembler : public View {
    public:
        explicit ViewDisassembler();
        ~ViewDisassembler() override;

        void drawContent() override;

    private:
        TaskHolder m_disassemblerTask;

        u64 m_baseAddress   = 0;
        ui::RegionType m_range = ui::RegionType::EntireData;
        Region m_codeRegion = { 0, 0 };

        Architecture m_architecture = Architecture::ARM;
        cs_mode m_mode              = cs_mode(0);

        std::vector<Disassembly> m_disassembly;

        void disassemble();
    };

}