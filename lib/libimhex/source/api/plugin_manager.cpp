#include <hex/api/plugin_manager.hpp>
#include <hex/api/imhex_api.hpp>

#include <hex/helpers/logger.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/helpers/auto_reset.hpp>
#include <hex/helpers/utils.hpp>

#include <wolv/utils/string.hpp>

#include <filesystem>

#if defined(OS_WINDOWS)
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif

namespace hex {

    Plugin::Plugin(const std::fs::path &path) : m_path(path) {
        log::info("Loading plugin '{}'", wolv::util::toUTF8String(path.filename()));

        #if defined(OS_WINDOWS)
            m_handle = uintptr_t(LoadLibraryW(path.c_str()));

            if (m_handle == uintptr_t(INVALID_HANDLE_VALUE) || m_handle == 0) {
                log::error("Loading plugin '{}' failed: {} {}!", wolv::util::toUTF8String(path.filename()), ::GetLastError(), hex::formatSystemError(::GetLastError()));
                return;
            }
        #else
            m_handle = uintptr_t(dlopen(wolv::util::toUTF8String(path).c_str(), RTLD_LAZY));

            if (m_handle == 0) {
                log::error("Loading plugin '{}' failed: {}!", wolv::util::toUTF8String(path.filename()), dlerror());
                return;
            }
        #endif

        const auto fileName = path.stem().string();

        m_functions.initializePluginFunction        = getPluginFunction<PluginFunctions::InitializePluginFunc>("initializePlugin");
        m_functions.initializeLibraryFunction       = getPluginFunction<PluginFunctions::InitializePluginFunc>(hex::format("initializeLibrary_{}", fileName));
        m_functions.getPluginNameFunction           = getPluginFunction<PluginFunctions::GetPluginNameFunc>("getPluginName");
        m_functions.getLibraryNameFunction          = getPluginFunction<PluginFunctions::GetLibraryNameFunc>(hex::format("getLibraryName_{}", fileName));
        m_functions.getPluginAuthorFunction         = getPluginFunction<PluginFunctions::GetPluginAuthorFunc>("getPluginAuthor");
        m_functions.getPluginDescriptionFunction    = getPluginFunction<PluginFunctions::GetPluginDescriptionFunc>("getPluginDescription");
        m_functions.getCompatibleVersionFunction    = getPluginFunction<PluginFunctions::GetCompatibleVersionFunc>("getCompatibleVersion");
        m_functions.setImGuiContextFunction         = getPluginFunction<PluginFunctions::SetImGuiContextFunc>("setImGuiContext");
        m_functions.setImGuiContextLibraryFunction  = getPluginFunction<PluginFunctions::SetImGuiContextFunc>(hex::format("setImGuiContext_{}", fileName));
        m_functions.getSubCommandsFunction          = getPluginFunction<PluginFunctions::GetSubCommandsFunc>("getSubCommands");
        m_functions.getFeaturesFunction             = getPluginFunction<PluginFunctions::GetSubCommandsFunc>("getFeatures");
    }

    Plugin::Plugin(const std::string &name, const hex::PluginFunctions &functions) {
        m_handle        = 0;
        m_functions     = functions;
        m_path          = name;
        m_addedManually = true;
    }


    Plugin::Plugin(Plugin &&other) noexcept {
        m_handle = other.m_handle;
        other.m_handle = 0;

        m_path = std::move(other.m_path);
        m_addedManually = other.m_addedManually;

        m_functions = other.m_functions;
        other.m_functions = {};
    }

    Plugin& Plugin::operator=(Plugin &&other) noexcept {
        m_handle = other.m_handle;
        other.m_handle = 0;

        m_path = std::move(other.m_path);
        m_addedManually = other.m_addedManually;

        m_functions = other.m_functions;
        other.m_functions = {};

        return *this;
    }

    Plugin::~Plugin() {
        if (isLoaded()) {
            log::debug("Trying to unload plugin '{}'", getPluginName());
        }

        #if defined(OS_WINDOWS)
            if (m_handle != 0)
                if (FreeLibrary(HMODULE(m_handle)) == FALSE) {
                    log::error("Error when unloading plugin '{}': {}!", wolv::util::toUTF8String(m_path.filename()), hex::formatSystemError(::GetLastError()));
                }
        #else
            if (m_handle != 0)
                dlclose(reinterpret_cast<void*>(m_handle));
        #endif
    }

    bool Plugin::initializePlugin() const {
        const auto pluginName = wolv::util::toUTF8String(m_path.filename());

        if (this->isLibraryPlugin()) {
            m_functions.initializeLibraryFunction();

            log::info("Library '{}' initialized successfully", pluginName);

            m_initialized = true;
            return true;
        }


        const auto requestedVersion = getCompatibleVersion();
        const auto imhexVersion = ImHexApi::System::getImHexVersion();
        if (!imhexVersion.starts_with(requestedVersion)) {
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
        if (m_functions.getPluginNameFunction != nullptr) {
            return m_functions.getPluginNameFunction();
        } else {
            if (this->isLibraryPlugin())
                return m_functions.getLibraryNameFunction();
            else
                return hex::format("Unknown Plugin @ 0x{0:016X}", m_handle);
        }
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

    const std::fs::path &Plugin::getPath() const {
        return m_path;
    }

    bool Plugin::isValid() const {
        return m_handle != 0 || m_functions.initializeLibraryFunction != nullptr || m_functions.initializePluginFunction != nullptr;
    }

    bool Plugin::isLoaded() const {
        return m_initialized;
    }

    std::span<SubCommand> Plugin::getSubCommands() const {
        if (m_functions.getSubCommandsFunction != nullptr) {
            const auto result = m_functions.getSubCommandsFunction();
            if (result == nullptr)
                return { };

            return *static_cast<std::vector<SubCommand>*>(result);
        } else {
            return { };
        }
    }

    std::span<Feature> Plugin::getFeatures() const {
        if (m_functions.getFeaturesFunction != nullptr) {
            const auto result = m_functions.getFeaturesFunction();
            if (result == nullptr)
                return { };

            return *static_cast<std::vector<Feature>*>(result);
        } else {
            return { };
        }
    }

    bool Plugin::isLibraryPlugin() const {
        return m_functions.initializeLibraryFunction != nullptr &&
               m_functions.initializePluginFunction  == nullptr;
    }

    bool Plugin::wasAddedManually() const {
        return m_addedManually;
    }




    void *Plugin::getPluginFunction(const std::string &symbol) const {
        #if defined(OS_WINDOWS)
            return reinterpret_cast<void *>(GetProcAddress(HMODULE(m_handle), symbol.c_str()));
        #else
            return dlsym(reinterpret_cast<void*>(m_handle), symbol.c_str());
        #endif
    }

    void PluginManager::addLoadPath(const std::fs::path& path) {
        getPluginLoadPaths().emplace_back(path);
    }


    bool PluginManager::load() {
        bool success = true;
        for (const auto &loadPath : getPluginLoadPaths())
            success = PluginManager::load(loadPath) && success;

        return success;
    }


    bool PluginManager::load(const std::fs::path &pluginFolder) {
        if (!wolv::io::fs::exists(pluginFolder))
            return false;

        getPluginPaths().push_back(pluginFolder);

        // Load library plugins first
        for (auto &pluginPath : std::fs::directory_iterator(pluginFolder)) {
            if (pluginPath.is_regular_file() && pluginPath.path().extension() == ".hexpluglib") {
                if (!isPluginLoaded(pluginPath.path())) {
                    getPlugins().emplace_back(pluginPath.path());
                }
            }
        }

        // Load regular plugins afterwards
        for (auto &pluginPath : std::fs::directory_iterator(pluginFolder)) {
            if (pluginPath.is_regular_file() && pluginPath.path().extension() == ".hexplug") {
                if (!isPluginLoaded(pluginPath.path())) {
                    getPlugins().emplace_back(pluginPath.path());
                }
            }
        }

        std::erase_if(getPlugins(), [](const Plugin &plugin) {
            return !plugin.isValid();
        });

        return true;
    }

    void PluginManager::initializeNewPlugins() {
        for (const auto &plugin : getPlugins()) {
            if (!plugin.isLoaded())
                hex::unused(plugin.initializePlugin());
        }
    }

    void PluginManager::unload() {
        getPluginPaths().clear();

        // Unload plugins in reverse order
        auto &plugins = getPlugins();

        std::list<Plugin> savedPlugins;
        while (!plugins.empty()) {
            if (plugins.back().wasAddedManually())
                savedPlugins.emplace_front(std::move(plugins.back()));
            plugins.pop_back();
        }

        getPlugins() = std::move(savedPlugins);
    }

    void PluginManager::addPlugin(const std::string &name, hex::PluginFunctions functions) {
        getPlugins().emplace_back(name, functions);
    }

    std::list<Plugin> &PluginManager::getPlugins() {
        static std::list<Plugin> plugins;

        return plugins;
    }


    Plugin* PluginManager::getPlugin(const std::string &name) {
        for (auto &plugin : getPlugins()) {
            if(plugin.getPluginName() == name) return &plugin;
        }
        return nullptr;
    }

    std::vector<std::fs::path> &PluginManager::getPluginPaths() {
        static AutoReset<std::vector<std::fs::path>> pluginPaths;

        return pluginPaths;
    }

    std::vector<std::fs::path> &PluginManager::getPluginLoadPaths() {
        static AutoReset<std::vector<std::fs::path>> pluginPaths;

        return pluginPaths;
    }

    bool PluginManager::isPluginLoaded(const std::fs::path &path) {
        return std::ranges::any_of(getPlugins(), [&path](const Plugin &plugin) {
            return plugin.getPath().filename() == path.filename();
        });
    }


}
