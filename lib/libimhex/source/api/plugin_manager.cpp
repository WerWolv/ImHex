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

        this->m_functions.initializePluginFunction     = getPluginFunction<PluginFunctions::InitializePluginFunc>("initializePlugin");
        this->m_functions.getPluginNameFunction        = getPluginFunction<PluginFunctions::GetPluginNameFunc>("getPluginName");
        this->m_functions.getPluginAuthorFunction      = getPluginFunction<PluginFunctions::GetPluginAuthorFunc>("getPluginAuthor");
        this->m_functions.getPluginDescriptionFunction = getPluginFunction<PluginFunctions::GetPluginDescriptionFunc>("getPluginDescription");
        this->m_functions.getCompatibleVersionFunction = getPluginFunction<PluginFunctions::GetCompatibleVersionFunc>("getCompatibleVersion");
        this->m_functions.setImGuiContextFunction      = getPluginFunction<PluginFunctions::SetImGuiContextFunc>("setImGuiContext");
        this->m_functions.isBuiltinPluginFunction      = getPluginFunction<PluginFunctions::IsBuiltinPluginFunc>("isBuiltinPlugin");
        this->m_functions.getSubCommandsFunction       = getPluginFunction<PluginFunctions::GetSubCommandsFunc>("getSubCommands");
    }

    Plugin::Plugin(hex::PluginFunctions functions) {
        this->m_handle = nullptr;
        this->m_functions = functions;
    }


    Plugin::Plugin(Plugin &&other) noexcept {
        this->m_handle = other.m_handle;
        other.m_handle = nullptr;

        this->m_path   = std::move(other.m_path);

        this->m_functions = other.m_functions;
        other.m_functions = {};
    }

    Plugin::~Plugin() {
        #if defined(OS_WINDOWS)
            if (this->m_handle != nullptr)
                FreeLibrary(this->m_handle);
        #else
        #endif
    }

    bool Plugin::initializePlugin() const {
        const auto pluginName = wolv::util::toUTF8String(this->m_path.filename());

        const auto requestedVersion = getCompatibleVersion();
        if (requestedVersion != ImHexApi::System::getImHexVersion()) {
            if (requestedVersion.empty()) {
                log::warn("Plugin '{}' did not specify a compatible version, assuming it is compatible with the current version of ImHex.", wolv::util::toUTF8String(this->m_path.filename()));
            } else {
                log::error("Refused to load plugin '{}' which was built for a different version of ImHex: '{}'", wolv::util::toUTF8String(this->m_path.filename()), requestedVersion);
                return false;
            }
        }

        if (this->m_functions.initializePluginFunction != nullptr) {
            try {
                this->m_functions.initializePluginFunction();
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

        this->m_initialized = true;
        return true;
    }

    std::string Plugin::getPluginName() const {
        if (this->m_functions.getPluginNameFunction != nullptr)
            return this->m_functions.getPluginNameFunction();
        else
            return hex::format("Unknown Plugin @ 0x{0:016X}", reinterpret_cast<intptr_t>(this->m_handle));
    }

    std::string Plugin::getPluginAuthor() const {
        if (this->m_functions.getPluginAuthorFunction != nullptr)
            return this->m_functions.getPluginAuthorFunction();
        else
            return "Unknown";
    }

    std::string Plugin::getPluginDescription() const {
        if (this->m_functions.getPluginDescriptionFunction != nullptr)
            return this->m_functions.getPluginDescriptionFunction();
        else
            return "";
    }

    std::string Plugin::getCompatibleVersion() const {
        if (this->m_functions.getCompatibleVersionFunction != nullptr)
            return this->m_functions.getCompatibleVersionFunction();
        else
            return "";
    }

    void Plugin::setImGuiContext(ImGuiContext *ctx) const {
        if (this->m_functions.setImGuiContextFunction != nullptr)
            this->m_functions.setImGuiContextFunction(ctx);
    }

    [[nodiscard]] bool Plugin::isBuiltinPlugin() const {
        if (this->m_functions.isBuiltinPluginFunction != nullptr)
            return this->m_functions.isBuiltinPluginFunction();
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
        if (this->m_functions.getSubCommandsFunction != nullptr) {
            auto result = this->m_functions.getSubCommandsFunction();
            return *reinterpret_cast<std::vector<SubCommand>*>(result);
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
