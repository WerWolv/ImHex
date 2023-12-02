#pragma once

#include <hex/ui/view.hpp>

#include <TextEditor.h>

#include <list>

namespace hex::plugin::builtin {

    class ViewBookmarks : public View::Window {
    public:
        ViewBookmarks();
        ~ViewBookmarks() override;

        void drawContent() override;

    private:
        bool importBookmarks(hex::prv::Provider *provider, const nlohmann::json &json);
        bool exportBookmarks(hex::prv::Provider *provider, nlohmann::json &json);

        void registerMenuItems();
    private:
        std::string m_currFilter;

        struct Bookmark {
            ImHexApi::Bookmarks::Entry entry;
            TextEditor editor;
        };

        std::list<Bookmark>::iterator m_dragStartIterator;
        PerProvider<std::list<Bookmark>> m_bookmarks;
        PerProvider<u64> m_currBookmarkId;
    };

}