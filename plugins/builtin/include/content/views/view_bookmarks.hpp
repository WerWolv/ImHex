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
        bool importBookmarks(hex::prv::Provider *provider, const nlohmann::json &json);
        bool exportBookmarks(hex::prv::Provider *provider, nlohmann::json &json);

        void registerMenuItems();
    private:
        std::string m_currFilter;

        std::list<ImHexApi::Bookmarks::Entry>::iterator m_dragStartIterator;
        PerProvider<std::list<ImHexApi::Bookmarks::Entry>> m_bookmarks;
    };

}