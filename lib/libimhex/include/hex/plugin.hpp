#pragma once

#include <hex.hpp>
#include <hex/api/plugin_manager.hpp>
#include <hex/helpers/logger.hpp>

#include <string>

#include <imgui.h>
#include <imgui_internal.h>

#include <wolv/utils/string.hpp>
#include <wolv/utils/preproc.hpp>
#include <wolv/utils/guards.hpp>

#if defined(_MSC_VER)
    #include <windows.h>
    #define PLUGIN_ENTRY_POINT extern "C" BOOL WINAPI DllMain(HINSTANCE, DWORD, LPVOID) { return TRUE; }
#else
    #define PLUGIN_ENTRY_POINT
#endif

namespace {
    struct PluginFunctionHelperInstantiation {};
}

template<typename T>
struct PluginFeatureFunctionHelper {
    static void* getFeatures();
};

template<typename T>
struct PluginSubCommandsFunctionHelper {
    static void* getSubCommands();
};

template<typename T>
void* PluginFeatureFunctionHelper<T>::getFeatures() {
    return nullptr;
}

template<typename T>
void* PluginSubCommandsFunctionHelper<T>::getSubCommands() {
    return nullptr;
}

[[maybe_unused]] static auto& getFeaturesImpl() {
    static hex::AutoReset<std::vector<hex::Feature>> features;
    return *features;
}

#if defined (IMHEX_STATIC_LINK_PLUGINS)
    #define IMHEX_PLUGIN_VISIBILITY_PREFIX static
#else
    #if defined(_MSC_VER)
        #define IMHEX_PLUGIN_VISIBILITY_PREFIX extern "C" __declspec(dllexport)
    #else
        #define IMHEX_PLUGIN_VISIBILITY_PREFIX extern "C" [[gnu::visibility("default")]]
    #endif
#endif

#define IMHEX_FEATURE_ENABLED(feature) WOLV_TOKEN_CONCAT(WOLV_TOKEN_CONCAT(WOLV_TOKEN_CONCAT(IMHEX_PLUGIN_, IMHEX_PLUGIN_NAME), _FEATURE_), feature)
#define IMHEX_DEFINE_PLUGIN_FEATURES() IMHEX_DEFINE_PLUGIN_FEATURES_IMPL()
#define IMHEX_DEFINE_PLUGIN_FEATURES_IMPL()                                                 \
    template<>                                                                              \
    struct PluginFeatureFunctionHelper<PluginFunctionHelperInstantiation> {                 \
        static void* getFeatures();                                                         \
    };                                                                                      \
    void* PluginFeatureFunctionHelper<PluginFunctionHelperInstantiation>::getFeatures() {   \
        return &getFeaturesImpl();                                                          \
    }                                                                                       \
    static auto initFeatures = [] { getFeaturesImpl() = std::vector<hex::Feature>({ IMHEX_PLUGIN_FEATURES_CONTENT }); return 0; }()

#define IMHEX_PLUGIN_FEATURES ::getFeaturesImpl()

/**
 * This macro is used to define all the required entry points for a plugin.
 * Name, Author and Description will be displayed in the plugin list on the Welcome screen.
 */
#define IMHEX_PLUGIN_SETUP(name, author, description) \
    IMHEX_PLUGIN_SETUP_IMPL(name, author, description, nullptr)
#define IMHEX_LIBRARY_SETUP(name) \
    IMHEX_LIBRARY_SETUP_IMPL(name)

#define IMHEX_PLUGIN_SETUP_BUILTIN(name, author, description)               \
    IMHEX_PLUGIN_VISIBILITY_PREFIX bool isBuiltinPlugin() { return true; }  \
    IMHEX_PLUGIN_SETUP_IMPL(name, author, description, isBuiltinPlugin)

#define IMHEX_LIBRARY_SETUP_IMPL(name)                                                                                          \
    IMHEX_PLUGIN_VISIBILITY_PREFIX void WOLV_TOKEN_CONCAT(initializeLibrary_, IMHEX_PLUGIN_NAME)();                             \
    IMHEX_PLUGIN_VISIBILITY_PREFIX const char *WOLV_TOKEN_CONCAT(getLibraryName_, IMHEX_PLUGIN_NAME)() { return name; }         \
    IMHEX_PLUGIN_VISIBILITY_PREFIX void WOLV_TOKEN_CONCAT(setImGuiContext_, IMHEX_PLUGIN_NAME)(ImGuiContext *ctx) {             \
        ImGui::SetCurrentContext(ctx);                                                                                          \
        GImGui = ctx;                                                                                                           \
    }                                                                                                                           \
    extern "C" void WOLV_TOKEN_CONCAT(forceLinkPlugin_, IMHEX_PLUGIN_NAME)() {                                                  \
        hex::PluginManager::addPlugin(name, hex::PluginFunctions {                                                              \
            nullptr,                                                                                                            \
            WOLV_TOKEN_CONCAT(initializeLibrary_, IMHEX_PLUGIN_NAME),                                                           \
            nullptr,                                                                                                            \
            WOLV_TOKEN_CONCAT(getLibraryName_, IMHEX_PLUGIN_NAME),                                                              \
            nullptr,                                                                                                            \
            nullptr,                                                                                                            \
            nullptr,                                                                                                            \
            WOLV_TOKEN_CONCAT(setImGuiContext_, IMHEX_PLUGIN_NAME),                                                             \
            nullptr,                                                                                                            \
            nullptr,                                                                                                            \
            nullptr                                                                                                             \
        });                                                                                                                     \
    }                                                                                                                           \
    PLUGIN_ENTRY_POINT                                                                                                          \
    IMHEX_PLUGIN_VISIBILITY_PREFIX void WOLV_TOKEN_CONCAT(initializeLibrary_, IMHEX_PLUGIN_NAME)()

#define IMHEX_PLUGIN_SETUP_IMPL(name, author, description, builtinPluginFunc)                                                                      \
    IMHEX_PLUGIN_VISIBILITY_PREFIX const char *getPluginName() { return name; }                                                 \
    IMHEX_PLUGIN_VISIBILITY_PREFIX const char *getPluginAuthor() { return author; }                                             \
    IMHEX_PLUGIN_VISIBILITY_PREFIX const char *getPluginDescription() { return description; }                                   \
    IMHEX_PLUGIN_VISIBILITY_PREFIX const char *getCompatibleVersion() { return IMHEX_VERSION; }                                 \
    IMHEX_PLUGIN_VISIBILITY_PREFIX void setImGuiContext(ImGuiContext *ctx) {                                                    \
        ImGui::SetCurrentContext(ctx);                                                                                          \
        GImGui = ctx;                                                                                                           \
    }                                                                                                                           \
    IMHEX_DEFINE_PLUGIN_FEATURES();                                                                                             \
    IMHEX_PLUGIN_VISIBILITY_PREFIX void* getFeatures() {                                                                        \
        return PluginFeatureFunctionHelper<PluginFunctionHelperInstantiation>::getFeatures();                                   \
    }                                                                                                                           \
    IMHEX_PLUGIN_VISIBILITY_PREFIX void* getSubCommands() {                                                                     \
        return PluginSubCommandsFunctionHelper<PluginFunctionHelperInstantiation>::getSubCommands();                            \
    }                                                                                                                           \
    IMHEX_PLUGIN_VISIBILITY_PREFIX void initializePlugin();                                                                     \
    extern "C" void WOLV_TOKEN_CONCAT(forceLinkPlugin_, IMHEX_PLUGIN_NAME)() {                                                  \
        hex::PluginManager::addPlugin(name, hex::PluginFunctions {                                                              \
            initializePlugin,                                                                                                   \
            nullptr,                                                                                                            \
            getPluginName,                                                                                                      \
            nullptr,                                                                                                            \
            getPluginAuthor,                                                                                                    \
            getPluginDescription,                                                                                               \
            getCompatibleVersion,                                                                                               \
            setImGuiContext,                                                                                                    \
            nullptr,                                                                                                            \
            getSubCommands,                                                                                                     \
            getFeatures,                                                                                                        \
            builtinPluginFunc                                                                                                   \
        });                                                                                                                     \
    }                                                                                                                           \
    PLUGIN_ENTRY_POINT                                                                                                          \
    IMHEX_PLUGIN_VISIBILITY_PREFIX void initializePlugin()

/**
 * This macro is used to define subcommands defined by the plugin
 * A subcommand consists of a key, a description, and a callback
 * The key is what the first argument to ImHex should be, prefixed by `--`
 * For example, if the key if `help`, ImHex should be started with `--help` as its first argument to trigger the subcommand
 * when the subcommand is triggerred, it's callback will be executed. The callback is executed BEFORE most of ImHex initialization
 * so to do anything meaningful, you should subscribe to an event (like EventImHexStartupFinished) and run your code there.
 */
#define IMHEX_PLUGIN_SUBCOMMANDS() IMHEX_PLUGIN_SUBCOMMANDS_IMPL()

#define IMHEX_PLUGIN_SUBCOMMANDS_IMPL()                                                             \
    extern std::vector<hex::SubCommand> g_subCommands;                                              \
    template<>                                                                                      \
    struct PluginSubCommandsFunctionHelper<PluginFunctionHelperInstantiation> {                     \
        static void* getSubCommands();                                                              \
    };                                                                                              \
    void* PluginSubCommandsFunctionHelper<PluginFunctionHelperInstantiation>::getSubCommands() {    \
        return &g_subCommands;                                                                      \
    }                                                                                               \
    std::vector<hex::SubCommand> g_subCommands
