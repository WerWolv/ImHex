#pragma once

#include <hex/views/view.hpp>

#include "helpers/disassembler.hpp"

#include <cstdio>
#include <string>
#include <vector>

namespace hex {

    namespace prv { class Provider; }

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
        void drawMenu() override;

    private:
        bool m_disassembling = false;

        u64 m_baseAddress = 0;
        u64 m_codeRegion[2] = { 0 };
        bool m_shouldMatchSelection = false;

        Architecture m_architecture = Architecture::ARM;
        cs_mode m_modeBasicARM = cs_mode(0), m_modeExtraARM = cs_mode(0), m_modeBasicMIPS = cs_mode(0), m_modeBasicPPC = cs_mode(0), m_modeBasicX86 = cs_mode(0);
        bool m_littleEndianMode = true, m_micoMode = false, m_sparcV9Mode = false;

        std::vector<Disassembly> m_disassembly;

        void disassemble();
    };

}