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
        static bool importBookmarks(prv::Provider *provider, const nlohmann::json &json);
        static bool exportBookmarks(prv::Provider *provider, nlohmann::json &json);

        void registerMenuItems();
    private:
        std::string m_currFilter;

        std::list<ImHexApi::Bookmarks::Entry>::iterator m_dragStartIterator;
    };

}