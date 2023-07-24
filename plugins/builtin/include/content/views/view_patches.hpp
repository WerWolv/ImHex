#pragma once

#include <hex.hpp>

#include <imgui.h>
#include <hex/ui/view.hpp>

#include <optional>

namespace hex::plugin::builtin {

    class ViewPatches : public View {
    public:
        explicit ViewPatches();
        ~ViewPatches() override = default;

        void drawContent() override;
        void drawAlwaysVisible() override;

    private:
        u64 m_selectedPatch = 0x00;
        PerProvider<u32> m_numPatches;
    };

}