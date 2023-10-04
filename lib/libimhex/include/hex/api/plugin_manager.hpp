#pragma once

#include <hex.hpp>

#include <hex/helpers/fmt.hpp>
#include <hex/helpers/fs.hpp>

#include <span>
#include <string>

#if defined(OS_WINDOWS)
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif

struct ImGuiContext;

namespace hex {

    struct SubCommand {
        std::string commandKey;
        std::string commandDesc;
        std::function<void(const std::vector<std::string>&)> callback;
    };

    struct PluginFunctions {
        using InitializePluginFunc     = void (*)();
        using GetPluginNameFunc        = const char *(*)();
        using GetPluginAuthorFunc      = const char *(*)();
        using GetPluginDescriptionFunc = const char *(*)();
        using GetCompatibleVersionFunc = const char *(*)();
        using SetImGuiContextFunc      = void (*)(ImGuiContext *);
        using IsBuiltinPluginFunc      = bool (*)();
        using GetSubCommandsFunc       = void* (*)();

        InitializePluginFunc        initializePluginFunction        = nullptr;
        GetPluginNameFunc           getPluginNameFunction           = nullptr;
        GetPluginAuthorFunc         getPluginAuthorFunction         = nullptr;
        GetPluginDescriptionFunc    getPluginDescriptionFunction    = nullptr;
        GetCompatibleVersionFunc    getCompatibleVersionFunction    = nullptr;
        SetImGuiContextFunc         setImGuiContextFunction         = nullptr;
        IsBuiltinPluginFunc         isBuiltinPluginFunction         = nullptr;
        GetSubCommandsFunc          getSubCommandsFunction          = nullptr;
    };

    class Plugin {
    public:
        explicit Plugin(const std::fs::path &path);
        explicit Plugin(PluginFunctions functions);

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

        [[nodiscard]] std::span<SubCommand> getSubCommands() const;

    private:
        #if defined(OS_WINDOWS)
            HMODULE m_handle = nullptr;
        #else
            void *m_handle = nullptr;
        #endif
        std::fs::path m_path;

        mutable bool m_initialized = false;

        PluginFunctions m_functions = {};

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

        static void addPlugin(PluginFunctions functions);

        static std::vector<Plugin> &getPlugins();
        static std::vector<std::fs::path> &getPluginPaths();
    };

}