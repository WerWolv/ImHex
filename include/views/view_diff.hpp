#pragma once

#include <hex.hpp>

#include <imgui.h>
#include <hex/views/view.hpp>

#include <array>
#include <string>

namespace hex {

    namespace prv { class Provider; }

    class ViewTools : public View {
    public:
        ViewTools();
        ~ViewTools() override;

        void drawContent() override;
        void drawMenu() override;

    };

}