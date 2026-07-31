#pragma once

#include <hex/ui/view.hpp>
#include <hex/api/imhex_api/bookmarks.hpp>
#include <hex/providers/provider_data.hpp>
#include <hex/providers/file_backed_provider_data.hpp>

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

        using Bookmarks = std::list<Bookmark>;

    private:
        void drawDropTarget(Bookmarks::iterator it, float height);

        static FileBackedProviderData<Bookmarks>::SerializedData encodeBookmarks(const Bookmarks &bookmarks);
        static std::optional<Bookmarks> decodeBookmarks(std::span<const u8> data);
        static nlohmann::json bookmarksToJson(const Bookmarks &bookmarks);
        static std::optional<Bookmarks> bookmarksFromJson(const nlohmann::json &json);
        void refreshBookmarkState(prv::Provider *provider);

        bool importBookmarks(hex::prv::Provider *provider, const nlohmann::json &json);
        bool exportBookmarks(hex::prv::Provider *provider, nlohmann::json &json);

        void registerMenuItems();

    private:
        std::string m_currFilter;

        FileBackedProviderData<Bookmarks> m_bookmarks;
        PerProvider<u64> m_currBookmarkId;
    };

}
