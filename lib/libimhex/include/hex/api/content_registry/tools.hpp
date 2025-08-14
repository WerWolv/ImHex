#pragma once

#include <hex.hpp>

#include <hex/api/localization_manager.hpp>

#include <functional>
#include <vector>

EXPORT_MODULE namespace hex {

    /* Tools Registry. Allows adding new entries to the tools window */
    namespace ContentRegistry::Tools {

        namespace impl {

            using Callback = std::function<void()>;

            struct Entry {
                UnlocalizedString unlocalizedName;
                const char *icon;
                Callback function;
            };

            const std::vector<Entry>& getEntries();

        }

        /**
         * @brief Adds a new tool to the tools window
         * @param unlocalizedName The unlocalized name of the tool
         * @param function The function that will be called to draw the tool
         */
        void add(const UnlocalizedString &unlocalizedName, const char *icon, const impl::Callback &function);
    }

}