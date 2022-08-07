#pragma once

#include <hex/ui/view.hpp>

#include <vector>
#include <list>

namespace hex::plugin::builtin {

    class ViewBookmarks : public View {
    public:
        ViewBookmarks();
        ~ViewBookmarks() override;

        void drawContent() override;

    private:
        std::string m_currFilter;
    };

}