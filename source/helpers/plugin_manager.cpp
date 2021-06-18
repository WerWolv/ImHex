#include "helpers/plugin_manager.hpp"

#include <filesystem>

namespace hex {

    namespace fs = std::filesystem;

    // hex::plugin::<pluginName>::internal::initializePlugin()
    constexpr auto InitializePluginSymbol       = "_ZN3hex6plugin{0}{1}8internal16initializePluginEv";
    constexpr auto GetPluginNameSymbol          = "_ZN3hex6plugin{0}{1}8internal13getPluginNameEv";
    constexpr auto GetPluginAuthorSymbol        = "_ZN3hex6plugin{0}{1}8internal15getPluginAuthorEv";
    constexpr auto GetPluginDescriptionSymbol   = "_ZN3hex6plugin{0}{1}8internal20getPluginDescriptionEv";

    Plugin::Plugin(std::string_view path) {
        this->m_handle = dlopen(path.data(), RTLD_LAZY);

        if (this->m_handle == nullptr) {
            hex::log::error("dlopen failed: {}", dlerror());
            return;
        }

        auto pluginName = fs::path(path).stem().string();

        this->m_initializePluginFunction        = getPluginFunction<InitializePluginFunc>(pluginName, InitializePluginSymbol);
        this->m_getPluginNameFunction           = getPluginFunction<GetPluginNameFunc>(pluginName, GetPluginNameSymbol);
        this->m_getPluginAuthorFunction         = getPluginFunction<GetPluginAuthorFunc>(pluginName, GetPluginAuthorSymbol);
        this->m_getPluginDescriptionFunction    = getPluginFunction<GetPluginDescriptionFunc>(pluginName, GetPluginDescriptionSymbol);
    }

    Plugin::Plugin(Plugin &&other) noexcept {
        this->m_handle = other.m_handle;
        this->m_initializePluginFunction        = other.m_initializePluginFunction;
        this->m_getPluginNameFunction           = other.m_getPluginNameFunction;
        this->m_getPluginAuthorFunction         = other.m_getPluginAuthorFunction;
        this->m_getPluginDescriptionFunction    = other.m_getPluginDescriptionFunction;

        other.m_handle = nullptr;
        other.m_initializePluginFunction        = nullptr;
        other.m_getPluginNameFunction           = nullptr;
        other.m_getPluginAuthorFunction         = nullptr;
        other.m_getPluginDescriptionFunction    = nullptr;
    }

    Plugin::~Plugin() {
        if (this->m_handle != nullptr)
            dlclose(this->m_handle);
    }

    void Plugin::initializePlugin() const {
        if (this->m_initializePluginFunction != nullptr)
            this->m_initializePluginFunction();
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

    bool PluginManager::load(std::string_view pluginFolder) {
        if (!std::filesystem::exists(pluginFolder))
            return false;

        PluginManager::s_pluginFolder = pluginFolder;

        for (auto& pluginPath : std::filesystem::directory_iterator(pluginFolder)) {
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
