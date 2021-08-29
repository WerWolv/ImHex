#pragma once

#include <hex.hpp>

#include <list>
#include <vector>

namespace hex {

    struct ImHexApi {
        ImHexApi() = delete;

        struct Common {

            static void closeImHex(bool noQuestions = false);
            static void restartImHex();

        };

        struct Bookmarks {
            Bookmarks() = delete;

            struct Entry {
                Region region;

                std::vector<char> name;
                std::vector<char> comment;
                u32 color;
                bool locked;
            };

            static void add(Region region, std::string_view name, std::string_view comment, u32 color = 0x00000000);
            static void add(u64 addr, size_t size, std::string_view name, std::string_view comment, u32 color = 0x00000000);

            static std::list<Entry>& getEntries();
        };

    };

}
