#pragma once

#include <hex/views/view.hpp>

#include <vector>
#include <list>

#include <hex/helpers/utils.hpp>

namespace hex {

    namespace prv { class Provider; }

    class ViewBookmarks : public View {
    public:
        ViewBookmarks();
        ~ViewBookmarks() override;

        void drawContent() override;
        void drawMenu() override;
    };

}