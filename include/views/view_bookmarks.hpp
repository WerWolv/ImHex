#pragma once

#include <hex/views/view.hpp>

#include <vector>
#include <list>

#include <hex/helpers/utils.hpp>

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