#pragma once

#include "views/view.hpp"

#include <capstone/capstone.h>

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
        std::string opcodeString;
    };

    class ViewDisassembler : public View {
    public:
        explicit ViewDisassembler(prv::Provider* &dataProvider);
        ~ViewDisassembler() override;

        void createView() override;
        void createMenu() override;

    private:
        prv::Provider* &m_dataProvider;
        bool m_windowOpen = true;

        bool m_shouldInvalidate = false;

        u64 m_baseAddress = 0;
        u64 m_codeOffset = 0;
        u64 m_codeSize = 0;

        cs_arch m_architecture = CS_ARCH_ARM;
        cs_mode m_modeBasicARM = cs_mode(0), m_modeExtraARM = cs_mode(0), m_modeBasicMIPS = cs_mode(0), m_modeBasicPPC = cs_mode(0), m_modeBasicX86 = cs_mode(0);
        bool m_littleEndianMode = true, m_micoMode = false, m_sparcV9Mode = false;

        std::vector<Disassembly> m_disassembly;


    };

}