#pragma once

#include <hex.hpp>

#include <imgui.h>
#include <hex/views/view.hpp>

#include <optional>

namespace hex {

    namespace prv { class Provider; }

    class ViewPatches : public View {
    public:
        explicit ViewPatches();
        ~ViewPatches() override;

        void drawContent() override;
        void drawMenu() override;

    private:
        u64 m_selectedPatch;
    };

}