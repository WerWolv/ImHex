#pragma once

#include <hex.hpp>
#include <hex/helpers/fs.hpp>

#include <string>
#include <variant>

#include <nlohmann/json_fwd.hpp>
#include <imgui.h>

namespace hex {

    class LayoutManager {
    public:
        struct Layout {
            std::string name;
            std::fs::path path;
        };

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
         * @brief Load a layout from a string
         * @param content Layout string
         */
        static void loadString(const std::string &content);

        /**
         * @brief Get a list of all layouts
         * @return List of all added layouts
         */
        static std::vector<Layout> getLayouts();

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

    private:
        LayoutManager() = default;
    };

}