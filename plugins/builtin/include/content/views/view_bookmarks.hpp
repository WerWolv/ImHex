#pragma once

#include <hex/views/view.hpp>

#include <vector>
#include <list>

namespace hex::plugin::builtin {

    class ViewBookmarks : public View {
    public:
        ViewBookmarks();
        ~ViewBookmarks() override;

        void drawContent() override;

    private:
        std::list<ImHexApi::Bookmarks::Entry> m_bookmarks;
    };

}