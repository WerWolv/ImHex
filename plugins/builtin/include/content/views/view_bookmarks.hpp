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
        struct Bookmark {
            ImHexApi::Bookmarks::Entry entry;
            TextEditor editor;
            bool highlightVisible;
        };

    private:
        void drawDropTarget(std::list<Bookmark>::iterator it, float height);

        bool importBookmarks(hex::prv::Provider *provider, const nlohmann::json &json);
        bool exportBookmarks(hex::prv::Provider *provider, nlohmann::json &json);

        void registerMenuItems();

    private:
        std::string m_currFilter;

        PerProvider<std::list<Bookmark>> m_bookmarks;
        PerProvider<u64> m_currBookmarkId;
    };

}