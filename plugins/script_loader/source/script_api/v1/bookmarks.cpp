#include <script_api.hpp>

#include <hex/api/imhex_api.hpp>

#define VERSION V1

SCRIPT_API(void createBookmark, u64 address, u64 size, u32 color, const char *name, const char *description) {
    hex::ImHexApi::Bookmarks::add(address, size, name, description, color);
}