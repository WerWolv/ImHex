#pragma once

#include <hex.hpp>

#include <hex/views/view.hpp>
#include <hex/providers/provider.hpp>
#include <hex/helpers/utils.hpp>

#include <string_view>
#include <dlfcn.h>

namespace hex {

    class Plugin {
    public:
        Plugin(std::string_view path);
        Plugin(const Plugin&) = delete;
        Plugin(Plugin &&other) noexcept;
        ~Plugin();

        void initializePlugin() const;
        std::string getPluginName() const;
        std::string getPluginAuthor() const;
        std::string getPluginDescription() const;


    private:
        using InitializePluginFunc      = void(*)();
        using GetPluginNameFunc         = const char*(*)();
        using GetPluginAuthorFunc       = const char*(*)();
        using GetPluginDescriptionFunc  = const char*(*)();

        void *m_handle = nullptr;

        InitializePluginFunc m_initializePluginFunction             = nullptr;
        GetPluginNameFunc m_getPluginNameFunction                   = nullptr;
        GetPluginAuthorFunc m_getPluginAuthorFunction               = nullptr;
        GetPluginDescriptionFunc m_getPluginDescriptionFunction     = nullptr;

        template<typename T>
        auto getPluginFunction(std::string_view pluginName, std::string_view symbol) {
            auto symbolName = hex::format(symbol.data(), pluginName.length(), pluginName.data());
            return reinterpret_cast<T>(dlsym(this->m_handle, symbolName.c_str()));
        };
    };

    class PluginManager {
    public:
        PluginManager() = delete;

        static bool load(std::string_view pluginFolder);
        static void unload();
        static void reload();

        static const auto& getPlugins() {
            return PluginManager::s_plugins;
        }

    private:
        static inline std::string s_pluginFolder;
        static inline std::vector<Plugin> s_plugins;
    };

}