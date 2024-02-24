#pragma once

#include <hex.hpp>

#include <hex/ui/view.hpp>

namespace hex::plugin::builtin {

    class ViewPatches : public View::Window {
    public:
        explicit ViewPatches();
        ~ViewPatches() override;

        void drawContent() override;
        void drawAlwaysVisibleContent() override;

    private:
        u64 m_selectedPatch = 0x00;
        PerProvider<u32> m_numOperations;
        PerProvider<u32> m_savedOperations;
    };

}