#pragma once

#include "views/view.hpp"

#include <string_view>

namespace hex {

    class Plugin {
    public:
        Plugin(std::string_view path);
        ~Plugin();

        View* createView() const;

    private:
        using CreateViewFunc = View*(*)();

        void *m_handle = nullptr;

        CreateViewFunc m_createViewFunction = nullptr;
    };

    class PluginHandler {
    public:
        PluginHandler() = delete;

        static void load(std::string_view pluginFolder);
        static void unload();
        static void reload();

        static const auto& getPlugins() {
            return PluginHandler::s_plugins;
        }

    private:
        static inline std::string s_pluginFolder;
        static inline std::vector<Plugin> s_plugins;
    };

}