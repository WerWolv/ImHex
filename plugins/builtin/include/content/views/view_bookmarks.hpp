#pragma once

#include <hex/ui/view.hpp>
#include <hex/api/imhex_api/bookmarks.hpp>

#include <list>
#include <ui/markdown.hpp>

namespace hex::plugin::builtin {

    class ViewBookmarks : public View::Window {
    public:
        ViewBookmarks();
        ~ViewBookmarks() override;

        void drawContent() override;
        void drawHelpText() override;

    private:
        struct Bookmark {
            ImHexApi::Bookmarks::Entry entry;
            bool highlightVisible;
            ui::Markdown commentDisplay;
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
