#pragma once

#include <hex/helpers/fs.hpp>

#include <string>

struct ImGuiTextBuffer;

namespace hex {

    class LayoutManager {
    public:
        struct Layout {
            std::string name;
            std::fs::path path;
        };

        using LoadCallback = std::function<void(std::string_view)>;
        using StoreCallback = std::function<void(ImGuiTextBuffer *)>;

        /**
         * @brief Save the current layout
         * @param name Name of the layout
         */
        static void save(const std::string &name);

        /**
         * @brief Load a layout from a file
         * @param path Path to the layout file
         */
        static void load(const std::fs::path &path);

        /**
         * @brief Saves the current layout to a string
         * @return String containing the layout
         */
        static std::string saveToString();

        /**
         * @brief Load a layout from a string
         * @param content Layout string
         */
        static void loadFromString(const std::string &content);

        /**
         * @brief Get a list of all layouts
         * @return List of all added layouts
         */
        static const std::vector<Layout> &getLayouts();

        /**
         * @brief Removes the layout with the given name
         * @param name Name of the layout
         */
        static void removeLayout(const std::string &name);

        /**
         * @brief Handles loading of layouts if needed
         * @note This function should only be called by ImHex
         */
        static void process();

        /**
         * @brief Reload all layouts
         */
        static void reload();

        /**
         * @brief Reset the layout manager
         */
        static void reset();

        /**
         * @brief Checks is the current layout is locked
         */
        static bool isLayoutLocked();

        /**
         * @brief Locks or unlocks the current layout
         * @note If the layout is locked, it cannot be modified by the user anymore
         * @param locked True to lock the layout, false to unlock it
         */
        static void lockLayout(bool locked);

        /**
         * @brief Closes all views
         */
        static void closeAllViews();

        static void registerLoadCallback(const LoadCallback &callback);
        static void registerStoreCallback(const StoreCallback &callback);

        static void onStore(ImGuiTextBuffer *buffer);
        static void onLoad(std::string_view line);

    private:
        LayoutManager() = default;
    };

}