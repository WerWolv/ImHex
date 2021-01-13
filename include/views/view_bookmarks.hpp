#pragma once

#include "views/view.hpp"

#include <vector>
#include <list>

#include "helpers/utils.hpp"

namespace hex {

    namespace prv { class Provider; }

    class ViewBookmarks : public View {
    public:
        explicit ViewBookmarks(std::list<Bookmark> &bookmarks);
        ~ViewBookmarks() override;

        void drawContent() override;
        void drawMenu() override;

    private:
        std::list<Bookmark> &m_bookmarks;
    };

}