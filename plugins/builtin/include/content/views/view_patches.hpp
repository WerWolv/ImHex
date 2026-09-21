#pragma once

#include <hex.hpp>

#include <hex/ui/view.hpp>

#include <unordered_map>

namespace hex::plugin::builtin {

    class ViewPatches : public View::Window {
    public:
        explicit ViewPatches();
        ~ViewPatches() override;

        void drawContent() override;
        void drawAlwaysVisibleContent() override;
        void drawHelpText() override;

    private:
        u64 m_selectedPatch = 0x00;
        PerProvider<u32> m_numOperations;
        PerProvider<u32> m_savedOperations;
        PerProvider<std::set<u64>> m_insertedAddresses;
        PerProvider<std::unordered_map<u64, u8>> m_originalByteValues;
    };

}