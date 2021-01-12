#pragma once

#include <hex.hpp>

#include "views/view.hpp"
#include "providers/provider.hpp"

#include <string_view>

namespace hex {

    class Plugin {
    public:
        Plugin(std::string_view path);
        ~Plugin();

        void initializePlugin(SharedData &sharedData) const;

    private:
        using InitializePluginFunc = void(*)(SharedData &sharedData);

        void *m_handle = nullptr;

        InitializePluginFunc m_initializePluginFunction = nullptr;
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