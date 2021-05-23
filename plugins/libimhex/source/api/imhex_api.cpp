#include <hex/api/imhex_api.hpp>

#include <hex/api/event.hpp>
#include <hex/helpers/shared_data.hpp>

namespace hex {

    void ImHexApi::Bookmarks::add(Region region, std::string_view name, std::string_view comment, u32 color) {
        Entry entry;

        entry.region = region;

        entry.name.reserve(name.length());
        entry.comment.reserve(comment.length());
        std::copy(name.begin(), name.end(), std::back_inserter(entry.name));
        std::copy(comment.begin(), comment.end(), std::back_inserter(entry.comment));
        entry.locked = false;

        entry.color = color;

        EventManager::post<RequestAddBookmark>(entry);
    }

    void ImHexApi::Bookmarks::add(u64 addr, size_t size, std::string_view name, std::string_view comment, u32 color) {
        Bookmarks::add(Region{addr, size}, name, comment, color);
    }

    std::list<ImHexApi::Bookmarks::Entry>& ImHexApi::Bookmarks::getEntries() {
        return SharedData::bookmarkEntries;
    }

}