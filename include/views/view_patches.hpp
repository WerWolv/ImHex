#pragma once

#include <hex.hpp>

#include "imgui.h"
#include "views/view.hpp"

#include <optional>

namespace hex {

    namespace prv { class Provider; }

    class ViewPatches : public View {
    public:
        explicit ViewPatches(prv::Provider* &dataProvider);
        ~ViewPatches() override;

        void createView() override;
        void createMenu() override;

    private:
        prv::Provider* &m_dataProvider;


        u64 m_selectedPatch;
    };

}