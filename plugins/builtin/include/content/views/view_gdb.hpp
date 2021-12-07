#pragma once

#include <hex.hpp>

#include <imgui.h>
#include <hex/views/view.hpp>

#include <array>
#include <string>

namespace hex::plugin::builtin {

    class ViewGDB : public hex::View {
    public:
        ViewGDB();

        void drawContent() override;

        bool hasViewMenuItemEntry() override;

        bool isAvailable();

    private:
        std::string m_address;
        int m_port = 0;
    };

}