#include <hex/api/plugin_manager.hpp>
#include <hex/api/imhex_api.hpp>

#include <hex/helpers/logger.hpp>
#include <hex/helpers/fmt.hpp>

#include <wolv/utils/string.hpp>

#include <filesystem>

#if defined(OS_WINDOWS)
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif

namespace hex {

    Plugin::Plugin(const std::fs::path &path) : m_path(path) {

        #if defined(OS_WINDOWS)
            m_handle = uintptr_t(LoadLibraryW(path.c_str()));

            if (m_handle == uintptr_t(INVALID_HANDLE_VALUE) || m_handle == 0) {
                log::error("LoadLibraryW failed: {}!", std::system_category().message(::GetLastError()));
                return;
            }
        #else
            m_handle = uintptr_t(dlopen(wolv::util::toUTF8String(path).c_str(), RTLD_LAZY));

            if (m_handle == 0) {
                log::error("dlopen failed: {}!", dlerror());
                return;
            }
        #endif

        m_functions.initializePluginFunction     = getPluginFunction<PluginFunctions::InitializePluginFunc>("initializePlugin");
        m_functions.getPluginNameFunction        = getPluginFunction<PluginFunctions::GetPluginNameFunc>("getPluginName");
        m_functions.getPluginAuthorFunction      = getPluginFunction<PluginFunctions::GetPluginAuthorFunc>("getPluginAuthor");
        m_functions.getPluginDescriptionFunction = getPluginFunction<PluginFunctions::GetPluginDescriptionFunc>("getPluginDescription");
        m_functions.getCompatibleVersionFunction = getPluginFunction<PluginFunctions::GetCompatibleVersionFunc>("getCompatibleVersion");
        m_functions.setImGuiContextFunction      = getPluginFunction<PluginFunctions::SetImGuiContextFunc>("setImGuiContext");
        m_functions.isBuiltinPluginFunction      = getPluginFunction<PluginFunctions::IsBuiltinPluginFunc>("isBuiltinPlugin");
        m_functions.getSubCommandsFunction       = getPluginFunction<PluginFunctions::GetSubCommandsFunc>("getSubCommands");
    }

    Plugin::Plugin(hex::PluginFunctions functions) {
        m_handle = 0;
        m_functions = functions;
    }


    Plugin::Plugin(Plugin &&other) noexcept {
        m_handle = other.m_handle;
        other.m_handle = 0;

        m_path   = std::move(other.m_path);

        m_functions = other.m_functions;
        other.m_functions = {};
    }

    Plugin::~Plugin() {
        #if defined(OS_WINDOWS)
            if (m_handle != 0)
                FreeLibrary(HMODULE(m_handle));
        #else
            if (m_handle != 0)
                dlclose(reinterpret_cast<void*>(m_handle));
        #endif
    }

    bool Plugin::initializePlugin() const {
        const auto pluginName = wolv::util::toUTF8String(m_path.filename());

        const auto requestedVersion = getCompatibleVersion();
        if (requestedVersion != ImHexApi::System::getImHexVersion()) {
            if (requestedVersion.empty()) {
                log::warn("Plugin '{}' did not specify a compatible version, assuming it is compatible with the current version of ImHex.", wolv::util::toUTF8String(m_path.filename()));
            } else {
                log::error("Refused to load plugin '{}' which was built for a different version of ImHex: '{}'", wolv::util::toUTF8String(m_path.filename()), requestedVersion);
                return false;
            }
        }

        if (m_functions.initializePluginFunction != nullptr) {
            try {
                m_functions.initializePluginFunction();
            } catch (const std::exception &e) {
                log::error("Plugin '{}' threw an exception on init: {}", pluginName, e.what());
                return false;
            } catch (...) {
                log::error("Plugin '{}' threw an exception on init", pluginName);
                return false;
            }
        } else {
            log::error("Plugin '{}' does not have a proper entrypoint", pluginName);
            return false;
        }

        log::info("Plugin '{}' initialized successfully", pluginName);

        m_initialized = true;
        return true;
    }

    std::string Plugin::getPluginName() const {
        if (m_functions.getPluginNameFunction != nullptr)
            return m_functions.getPluginNameFunction();
        else
            return hex::format("Unknown Plugin @ 0x{0:016X}", m_handle);
    }

    std::string Plugin::getPluginAuthor() const {
        if (m_functions.getPluginAuthorFunction != nullptr)
            return m_functions.getPluginAuthorFunction();
        else
            return "Unknown";
    }

    std::string Plugin::getPluginDescription() const {
        if (m_functions.getPluginDescriptionFunction != nullptr)
            return m_functions.getPluginDescriptionFunction();
        else
            return "";
    }

    std::string Plugin::getCompatibleVersion() const {
        if (m_functions.getCompatibleVersionFunction != nullptr)
            return m_functions.getCompatibleVersionFunction();
        else
            return "";
    }

    void Plugin::setImGuiContext(ImGuiContext *ctx) const {
        if (m_functions.setImGuiContextFunction != nullptr)
            m_functions.setImGuiContextFunction(ctx);
    }

    [[nodiscard]] bool Plugin::isBuiltinPlugin() const {
        if (m_functions.isBuiltinPluginFunction != nullptr)
            return m_functions.isBuiltinPluginFunction();
        else
            return false;
    }

    const std::fs::path &Plugin::getPath() const {
        return m_path;
    }

    bool Plugin::isLoaded() const {
        return m_initialized;
    }

    std::span<SubCommand> Plugin::getSubCommands() const {
        if (m_functions.getSubCommandsFunction != nullptr) {
            auto result = m_functions.getSubCommandsFunction();
            return *static_cast<std::vector<SubCommand>*>(result);
        } else
            return { };
    }


    void *Plugin::getPluginFunction(const std::string &symbol) const {
        #if defined(OS_WINDOWS)
            return reinterpret_cast<void *>(GetProcAddress(HMODULE(m_handle), symbol.c_str()));
        #else
            return dlsym(reinterpret_cast<void*>(m_handle), symbol.c_str());
        #endif
    }

    bool PluginManager::load(const std::fs::path &pluginFolder) {
        if (!wolv::io::fs::exists(pluginFolder))
            return false;

        getPluginPaths().push_back(pluginFolder);

        for (auto &pluginPath : std::fs::directory_iterator(pluginFolder)) {
            if (pluginPath.is_regular_file() && pluginPath.path().extension() == ".hexplug")
                getPlugins().emplace_back(pluginPath.path());
        }

        if (getPlugins().empty())
            return false;

        return true;
    }

    void PluginManager::unload() {
        getPlugins().clear();
        getPluginPaths().clear();
    }

    void PluginManager::reload() {
        auto paths = getPluginPaths();

        PluginManager::unload();
        for (const auto &path : paths)
            PluginManager::load(path);
    }

    void PluginManager::addPlugin(hex::PluginFunctions functions) {
        getPlugins().emplace_back(functions);
    }

    std::vector<Plugin> &PluginManager::getPlugins() {
        static std::vector<Plugin> plugins;

        return plugins;
    }

    std::vector<std::fs::path> &PluginManager::getPluginPaths() {
        static std::vector<std::fs::path> pluginPaths;

        return pluginPaths;
    }

}
