#pragma once

#include <hex.hpp>

#include <imgui.h>
#include <hex/ui/view.hpp>

#include <array>
#include <string>

namespace hex::plugin::builtin {

    class ViewTools : public View {
    public:
        ViewTools();
        ~ViewTools() override = default;

        void drawContent() override;
    };

}