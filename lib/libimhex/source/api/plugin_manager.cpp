#include <hex/api/plugin_manager.hpp>
#include <hex/api/imhex_api.hpp>

#include <hex/helpers/logger.hpp>
#include <hex/helpers/utils.hpp>

#include <wolv/utils/string.hpp>

#include <filesystem>
#include <system_error>

namespace hex {

    Plugin::Plugin(const std::fs::path &path) : m_path(path) {
        #if defined(OS_WINDOWS)
            this->m_handle = LoadLibraryW(path.c_str());

            if (this->m_handle == INVALID_HANDLE_VALUE || this->m_handle == nullptr) {
                log::error("LoadLibraryW failed: {}!", std::system_category().message(::GetLastError()));
                return;
            }
        #else
            this->m_handle = dlopen(wolv::util::toUTF8String(path).c_str(), RTLD_LAZY);

            if (this->m_handle == nullptr) {
                log::error("dlopen failed: {}!", dlerror());
                return;
            }
        #endif

        this->m_initializePluginFunction     = getPluginFunction<InitializePluginFunc>("initializePlugin");
        this->m_getPluginNameFunction        = getPluginFunction<GetPluginNameFunc>("getPluginName");
        this->m_getPluginAuthorFunction      = getPluginFunction<GetPluginAuthorFunc>("getPluginAuthor");
        this->m_getPluginDescriptionFunction = getPluginFunction<GetPluginDescriptionFunc>("getPluginDescription");
        this->m_getCompatibleVersionFunction = getPluginFunction<GetCompatibleVersionFunc>("getCompatibleVersion");
        this->m_setImGuiContextFunction      = getPluginFunction<SetImGuiContextFunc>("setImGuiContext");
        this->m_isBuiltinPluginFunction      = getPluginFunction<IsBuiltinPluginFunc>("isBuiltinPlugin");
        this->m_getSubCommandsFunction       = getPluginFunction<GetSubCommandsFunc>("getSubCommands");
    }

    Plugin::Plugin(Plugin &&other) noexcept {
        this->m_handle = other.m_handle;
        this->m_path   = std::move(other.m_path);

        this->m_initializePluginFunction     = other.m_initializePluginFunction;
        this->m_getPluginNameFunction        = other.m_getPluginNameFunction;
        this->m_getPluginAuthorFunction      = other.m_getPluginAuthorFunction;
        this->m_getPluginDescriptionFunction = other.m_getPluginDescriptionFunction;
        this->m_getCompatibleVersionFunction = other.m_getCompatibleVersionFunction;
        this->m_setImGuiContextFunction      = other.m_setImGuiContextFunction;
        this->m_isBuiltinPluginFunction      = other.m_isBuiltinPluginFunction;
        this->m_getSubCommandsFunction       = other.m_getSubCommandsFunction;

        other.m_handle                       = nullptr;
        other.m_initializePluginFunction     = nullptr;
        other.m_getPluginNameFunction        = nullptr;
        other.m_getPluginAuthorFunction      = nullptr;
        other.m_getPluginDescriptionFunction = nullptr;
        other.m_getCompatibleVersionFunction = nullptr;
        other.m_setImGuiContextFunction      = nullptr;
        other.m_isBuiltinPluginFunction      = nullptr;
        other.m_getSubCommandsFunction       = nullptr;
    }

    Plugin::~Plugin() {
        #if defined(OS_WINDOWS)
            if (this->m_handle != nullptr)
                FreeLibrary(this->m_handle);
        #else
            if (this->m_handle != nullptr)
                dlclose(this->m_handle);
        #endif
    }

    bool Plugin::initializePlugin() const {
        if (this->m_handle == nullptr)
            return false;

        const auto requestedVersion = getCompatibleVersion();
        if (requestedVersion != ImHexApi::System::getImHexVersion()) {
            if (requestedVersion.empty()) {
                log::warn("Plugin '{}' did not specify a compatible version, assuming it is compatible with the current version of ImHex.", wolv::util::toUTF8String(this->m_path.filename()));
            } else {
                log::error("Refused to load plugin '{}' which was built for a different version of ImHex: '{}'", wolv::util::toUTF8String(this->m_path.filename()), requestedVersion);
                return false;
            }
        }

        if (this->m_initializePluginFunction != nullptr) {
            this->m_initializePluginFunction();
        } else {
            return false;
        }

        this->m_initialized = true;
        return true;
    }

    std::string Plugin::getPluginName() const {
        if (this->m_getPluginNameFunction != nullptr)
            return this->m_getPluginNameFunction();
        else
            return hex::format("Unknown Plugin @ 0x{0:016X}", reinterpret_cast<intptr_t>(this->m_handle));
    }

    std::string Plugin::getPluginAuthor() const {
        if (this->m_getPluginAuthorFunction != nullptr)
            return this->m_getPluginAuthorFunction();
        else
            return "Unknown";
    }

    std::string Plugin::getPluginDescription() const {
        if (this->m_getPluginDescriptionFunction != nullptr)
            return this->m_getPluginDescriptionFunction();
        else
            return "";
    }

    std::string Plugin::getCompatibleVersion() const {
        if (this->m_getCompatibleVersionFunction != nullptr)
            return this->m_getCompatibleVersionFunction();
        else
            return "";
    }

    void Plugin::setImGuiContext(ImGuiContext *ctx) const {
        if (this->m_setImGuiContextFunction != nullptr)
            this->m_setImGuiContextFunction(ctx);
    }

    [[nodiscard]] bool Plugin::isBuiltinPlugin() const {
        if (this->m_isBuiltinPluginFunction != nullptr)
            return this->m_isBuiltinPluginFunction();
        else
            return false;
    }

    const std::fs::path &Plugin::getPath() const {
        return this->m_path;
    }

    bool Plugin::isLoaded() const {
        return this->m_initialized;
    }

    std::span<SubCommand> Plugin::getSubCommands() const {
        if (this->m_getSubCommandsFunction != nullptr) {
            auto result = this->m_getSubCommandsFunction();
            return { result->subCommands, result->size };
        } else
            return { };
    }


    void *Plugin::getPluginFunction(const std::string &symbol) {
        #if defined(OS_WINDOWS)
            return reinterpret_cast<void *>(GetProcAddress(this->m_handle, symbol.c_str()));
        #else
            return dlsym(this->m_handle, symbol.c_str());
        #endif
    }


    std::fs::path PluginManager::s_pluginFolder;
    std::vector<Plugin> PluginManager::s_plugins;

    bool PluginManager::load(const std::fs::path &pluginFolder) {
        if (!wolv::io::fs::exists(pluginFolder))
            return false;

        PluginManager::s_pluginFolder = pluginFolder;

        for (auto &pluginPath : std::fs::directory_iterator(pluginFolder)) {
            if (pluginPath.is_regular_file() && pluginPath.path().extension() == ".hexplug")
                PluginManager::s_plugins.emplace_back(pluginPath.path());
        }

        if (PluginManager::s_plugins.empty())
            return false;

        return true;
    }

    void PluginManager::unload() {
        PluginManager::s_plugins.clear();
        PluginManager::s_pluginFolder.clear();
    }

    void PluginManager::reload() {
        PluginManager::unload();
        PluginManager::load(PluginManager::s_pluginFolder);
    }

}
