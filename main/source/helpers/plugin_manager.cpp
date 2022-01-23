#include "helpers/plugin_manager.hpp"

#include <hex/helpers/logger.hpp>

#include <filesystem>
#include <dlfcn.h>

namespace hex {

    Plugin::Plugin(const fs::path &path) : m_path(path) {
        this->m_handle = dlopen(path.string().c_str(), RTLD_LAZY);

        if (this->m_handle == nullptr) {
            log::error("dlopen failed: {}", dlerror());
            return;
        }

        auto pluginName = fs::path(path).stem().string();

        this->m_initializePluginFunction        = getPluginFunction<InitializePluginFunc>("initializePlugin");
        this->m_getPluginNameFunction           = getPluginFunction<GetPluginNameFunc>("getPluginName");
        this->m_getPluginAuthorFunction         = getPluginFunction<GetPluginAuthorFunc>("getPluginAuthor");
        this->m_getPluginDescriptionFunction    = getPluginFunction<GetPluginDescriptionFunc>("getPluginDescription");
        this->m_getCompatibleVersionFunction    = getPluginFunction<GetCompatibleVersionFunc>("getCompatibleVersion");
        this->m_setImGuiContextFunction         = getPluginFunction<SetImGuiContextFunc>("setImGuiContext");
    }

    Plugin::Plugin(Plugin &&other) noexcept {
        this->m_handle = other.m_handle;
        this->m_path = std::move(other.m_path);

        this->m_initializePluginFunction        = other.m_initializePluginFunction;
        this->m_getPluginNameFunction           = other.m_getPluginNameFunction;
        this->m_getPluginAuthorFunction         = other.m_getPluginAuthorFunction;
        this->m_getPluginDescriptionFunction    = other.m_getPluginDescriptionFunction;
        this->m_getCompatibleVersionFunction    = other.m_getCompatibleVersionFunction;
        this->m_setImGuiContextFunction         = other.m_setImGuiContextFunction;

        other.m_handle = nullptr;
        other.m_initializePluginFunction        = nullptr;
        other.m_getPluginNameFunction           = nullptr;
        other.m_getPluginAuthorFunction         = nullptr;
        other.m_getPluginDescriptionFunction    = nullptr;
        other.m_getCompatibleVersionFunction    = nullptr;
        other.m_setImGuiContextFunction         = nullptr;
    }

    Plugin::~Plugin() {
        if (this->m_handle != nullptr)
            dlclose(this->m_handle);
    }

    bool Plugin::initializePlugin() const {
        const auto requestedVersion = getCompatibleVersion();
        if (requestedVersion != IMHEX_VERSION) {
            log::error("Refused to load plugin '{}' which was built for a different version of ImHex: '{}'", this->m_path.filename().string(), requestedVersion);
            return false;
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

    const fs::path& Plugin::getPath() const {
        return this->m_path;
    }

    bool Plugin::isLoaded() const {
        return this->m_initialized;
    }


    void* Plugin::getPluginFunction(const std::string &symbol) {
        return dlsym(this->m_handle, symbol.c_str());
    }


    bool PluginManager::load(const fs::path &pluginFolder) {
        if (!fs::exists(pluginFolder))
            return false;

        PluginManager::s_pluginFolder = pluginFolder;

        for (auto& pluginPath : fs::directory_iterator(pluginFolder)) {
            if (pluginPath.is_regular_file() && pluginPath.path().extension() == ".hexplug")
                PluginManager::s_plugins.emplace_back(pluginPath.path().string());
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
