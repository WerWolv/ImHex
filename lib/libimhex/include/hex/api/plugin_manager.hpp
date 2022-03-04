#pragma once

#include <hex.hpp>

#include <hex/helpers/fmt.hpp>
#include <hex/helpers/fs.hpp>

#include <string>

struct ImGuiContext;

namespace hex {

    class Plugin {
    public:
        explicit Plugin(const std::fs::path &path);
        Plugin(const Plugin &) = delete;
        Plugin(Plugin &&other) noexcept;
        ~Plugin();

        [[nodiscard]] bool initializePlugin() const;
        [[nodiscard]] std::string getPluginName() const;
        [[nodiscard]] std::string getPluginAuthor() const;
        [[nodiscard]] std::string getPluginDescription() const;
        [[nodiscard]] std::string getCompatibleVersion() const;
        void setImGuiContext(ImGuiContext *ctx) const;
        [[nodiscard]] bool isBuiltinPlugin() const;

        [[nodiscard]] const std::fs::path &getPath() const;

        [[nodiscard]] bool isLoaded() const;

    private:
        using InitializePluginFunc     = void (*)();
        using GetPluginNameFunc        = const char *(*)();
        using GetPluginAuthorFunc      = const char *(*)();
        using GetPluginDescriptionFunc = const char *(*)();
        using GetCompatibleVersionFunc = const char *(*)();
        using SetImGuiContextFunc      = void (*)(ImGuiContext *);
        using IsBuiltinPluginFunc      = bool (*)();

        void *m_handle = nullptr;
        std::fs::path m_path;

        mutable bool m_initialized = false;

        InitializePluginFunc m_initializePluginFunction         = nullptr;
        GetPluginNameFunc m_getPluginNameFunction               = nullptr;
        GetPluginAuthorFunc m_getPluginAuthorFunction           = nullptr;
        GetPluginDescriptionFunc m_getPluginDescriptionFunction = nullptr;
        GetCompatibleVersionFunc m_getCompatibleVersionFunction = nullptr;
        SetImGuiContextFunc m_setImGuiContextFunction           = nullptr;
        IsBuiltinPluginFunc m_isBuiltinPluginFunction           = nullptr;

        template<typename T>
        [[nodiscard]] auto getPluginFunction(const std::string &symbol) {
            return reinterpret_cast<T>(this->getPluginFunction(symbol));
        }

    private:
        [[nodiscard]] void *getPluginFunction(const std::string &symbol);
    };

    class PluginManager {
    public:
        PluginManager() = delete;

        static bool load(const std::fs::path &pluginFolder);
        static void unload();
        static void reload();

        static const auto &getPlugins() {
            return PluginManager::s_plugins;
        }

    private:
        static std::fs::path s_pluginFolder;
        static std::vector<Plugin> s_plugins;
    };

}