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

        static void save(const std::string &name);
        static void load(const std::fs::path &path);
        static void loadString(const std::string &content);

        static std::vector<Layout> getLayouts();

        static void process();
        static void reload();
        static void reset();

    private:
        LayoutManager() = default;

        static std::optional<std::fs::path> s_layoutPathToLoad;
        static std::optional<std::string> s_layoutStringToLoad;
        static std::vector<Layout> s_layouts;
    };

}