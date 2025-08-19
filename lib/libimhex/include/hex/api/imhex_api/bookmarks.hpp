#pragma once

#include <hex.hpp>

#include <string>

EXPORT_MODULE namespace hex {

    /* Functions to interact with Bookmarks */
    namespace ImHexApi::Bookmarks {

        struct Entry {
            Region region;

            std::string name;
            std::string comment;
            u32 color;
            bool locked;
            u64 id;
        };

        /**
         * @brief Adds a new bookmark
         * @param address The address of the bookmark
         * @param size The size of the bookmark
         * @param name The name of the bookmark
         * @param comment The comment of the bookmark
         * @param color The color of the bookmark or 0x00 for the default color
         * @return Bookmark ID
         */
        u64 add(u64 address, size_t size, const std::string &name, const std::string &comment, color_t color = 0x00000000);

        /**
        * @brief Adds a new bookmark
        * @param region The region of the bookmark
        * @param name The name of the bookmark
        * @param comment The comment of the bookmark
        * @param color The color of the bookmark or 0x00 for the default color
        * @return Bookmark ID
        */
        u64 add(Region region, const std::string &name, const std::string &comment, color_t color = 0x00000000);

        /**
        * @brief Removes a bookmark
        * @param id The ID of the bookmark to remove
        */
        void remove(u64 id);

    }


}