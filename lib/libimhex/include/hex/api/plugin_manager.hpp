#pragma once

#include <functional>
#include <list>
#include <span>
#include <string>

#include <wolv/io/fs.hpp>
#include <hex/helpers/logger.hpp>

struct ImGuiContext;

namespace hex {

    struct SubCommand {
        std::string commandKey;
        std::string commandDesc;
        std::function<void(const std::vector<std::string>&)> callback;
    };

    struct Feature {
        std::string name;
        bool enabled;
    };

    struct PluginFunctions {
        using InitializePluginFunc     = void (*)();
        using InitializeLibraryFunc    = void (*)();
        using GetPluginNameFunc        = const char *(*)();
        using GetLibraryNameFunc       = const char *(*)();
        using GetPluginAuthorFunc      = const char *(*)();
        using GetPluginDescriptionFunc = const char *(*)();
        using GetCompatibleVersionFunc = const char *(*)();
        using SetImGuiContextFunc      = void (*)(ImGuiContext *);
        using GetSubCommandsFunc       = void* (*)();
        using GetFeaturesFunc          = void* (*)();

        InitializePluginFunc        initializePluginFunction        = nullptr;
        InitializeLibraryFunc       initializeLibraryFunction       = nullptr;
        GetPluginNameFunc           getPluginNameFunction           = nullptr;
        GetLibraryNameFunc          getLibraryNameFunction          = nullptr;
        GetPluginAuthorFunc         getPluginAuthorFunction         = nullptr;
        GetPluginDescriptionFunc    getPluginDescriptionFunction    = nullptr;
        GetCompatibleVersionFunc    getCompatibleVersionFunction    = nullptr;
        SetImGuiContextFunc         setImGuiContextFunction         = nullptr;
        SetImGuiContextFunc         setImGuiContextLibraryFunction  = nullptr;
        GetSubCommandsFunc          getSubCommandsFunction          = nullptr;
        GetFeaturesFunc             getFeaturesFunction             = nullptr;
    };

    class Plugin {
    public:
        explicit Plugin(const std::fs::path &path);
        explicit Plugin(const std::string &name, const PluginFunctions &functions);

        Plugin(const Plugin &) = delete;
        Plugin(Plugin &&other) noexcept;
        ~Plugin();

        Plugin& operator=(const Plugin &) = delete;
        Plugin& operator=(Plugin &&other) noexcept;

        [[nodiscard]] bool initializePlugin() const;
        [[nodiscard]] std::string getPluginName() const;
        [[nodiscard]] std::string getPluginAuthor() const;
        [[nodiscard]] std::string getPluginDescription() const;
        [[nodiscard]] std::string getCompatibleVersion() const;
        void setImGuiContext(ImGuiContext *ctx) const;

        [[nodiscard]] const std::fs::path &getPath() const;

        [[nodiscard]] bool isValid() const;
        [[nodiscard]] bool isLoaded() const;

        [[nodiscard]] std::span<SubCommand> getSubCommands() const;
        [[nodiscard]] std::span<Feature> getFeatures() const;

        [[nodiscard]] bool isLibraryPlugin() const;

        [[nodiscard]] bool wasAddedManually() const;

    private:
        uintptr_t m_handle = 0;
        std::fs::path m_path;

        mutable bool m_initialized = false;
        bool m_addedManually = false;

        PluginFunctions m_functions = {};

        template<typename T>
        [[nodiscard]] auto getPluginFunction(const std::string &symbol) {
            return reinterpret_cast<T>(this->getPluginFunction(symbol));
        }

        [[nodiscard]] void *getPluginFunction(const std::string &symbol) const;
    };

    class PluginManager {
    public:
        PluginManager() = delete;

        static bool load();
        static bool load(const std::fs::path &pluginFolder);
        static void unload();
        static void reload();
        static void initializeNewPlugins();
        static void addLoadPath(const std::fs::path &path);

        static void addPlugin(const std::string &name, PluginFunctions functions);

        static std::list<Plugin> &getPlugins();
        static Plugin* getPlugin(const std::string &name);
        static std::vector<std::fs::path> &getPluginPaths();
        static std::vector<std::fs::path> &getPluginLoadPaths();

        static bool isPluginLoaded(const std::fs::path &path);
    };

}