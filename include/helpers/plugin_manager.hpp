#pragma once

#include <hex.hpp>

#include <hex/views/view.hpp>
#include <hex/providers/provider.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/helpers/paths.hpp>

#include <string_view>
#include <dlfcn.h>

struct ImGuiContext;

namespace hex {

    class Plugin {
    public:
        Plugin(const std::string &path);
        Plugin(const Plugin&) = delete;
        Plugin(Plugin &&other) noexcept;
        ~Plugin();

        void initializePlugin() const;
        [[nodiscard]] std::string getPluginName() const;
        [[nodiscard]] std::string getPluginAuthor() const;
        [[nodiscard]] std::string getPluginDescription() const;
        void setImGuiContext(ImGuiContext *ctx) const;


    private:
        using InitializePluginFunc      = void(*)();
        using GetPluginNameFunc         = const char*(*)();
        using GetPluginAuthorFunc       = const char*(*)();
        using GetPluginDescriptionFunc  = const char*(*)();
        using SetImGuiContextFunc       = void(*)(ImGuiContext*);

        void *m_handle = nullptr;

        InitializePluginFunc m_initializePluginFunction             = nullptr;
        GetPluginNameFunc m_getPluginNameFunction                   = nullptr;
        GetPluginAuthorFunc m_getPluginAuthorFunction               = nullptr;
        GetPluginDescriptionFunc m_getPluginDescriptionFunction     = nullptr;
        SetImGuiContextFunc m_setImGuiContextFunction               = nullptr;

        template<typename T>
        auto getPluginFunction(const std::string &pluginName, const std::string &symbol) {
            auto symbolName = hex::format(symbol.data(), pluginName.length(), pluginName.data());
            return reinterpret_cast<T>(dlsym(this->m_handle, symbolName.c_str()));
        };
    };

    class PluginManager {
    public:
        PluginManager() = delete;

        static bool load(const fs::path &pluginFolder);
        static void unload();
        static void reload();

        static const auto& getPlugins() {
            return PluginManager::s_plugins;
        }

    private:
        static inline fs::path s_pluginFolder;
        static inline std::vector<Plugin> s_plugins;
    };

}