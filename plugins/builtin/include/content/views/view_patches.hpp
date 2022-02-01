#pragma once

#include <hex.hpp>

#include <imgui.h>
#include <hex/ui/view.hpp>

#include <optional>

namespace hex::plugin::builtin {

    namespace prv {
        class Provider;
    }

    class ViewPatches : public View {
    public:
        explicit ViewPatches();
        ~ViewPatches() override;

        void drawContent() override;

    private:
        u64 m_selectedPatch;
    };

}