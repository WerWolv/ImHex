#include "helpers/plugin_manager.hpp"

#include <hex/helpers/logger.hpp>
#include <hex/helpers/paths.hpp>

#include <filesystem>

namespace hex {

    // hex::plugin::<pluginName>::internal::initializePlugin()
    constexpr auto InitializePluginSymbol       = "_ZN3hex6plugin{0}{1}8internal16initializePluginEv";
    constexpr auto GetPluginNameSymbol          = "_ZN3hex6plugin{0}{1}8internal13getPluginNameEv";
    constexpr auto GetPluginAuthorSymbol        = "_ZN3hex6plugin{0}{1}8internal15getPluginAuthorEv";
    constexpr auto GetPluginDescriptionSymbol   = "_ZN3hex6plugin{0}{1}8internal20getPluginDescriptionEv";
    constexpr auto SetImGuiContextSymbol        = "_ZN3hex6plugin{0}{1}8internal15setImGuiContextEP12ImGuiContext";

    Plugin::Plugin(const fs::path &path) {
        this->m_handle = dlopen(path.string().c_str(), RTLD_LAZY);

        if (this->m_handle == nullptr) {
            log::error("dlopen failed: {}", dlerror());
            return;
        }

        auto pluginName = fs::path(path).stem().string();

        this->m_initializePluginFunction        = getPluginFunction<InitializePluginFunc>(pluginName, InitializePluginSymbol);
        this->m_getPluginNameFunction           = getPluginFunction<GetPluginNameFunc>(pluginName, GetPluginNameSymbol);
        this->m_getPluginAuthorFunction         = getPluginFunction<GetPluginAuthorFunc>(pluginName, GetPluginAuthorSymbol);
        this->m_getPluginDescriptionFunction    = getPluginFunction<GetPluginDescriptionFunc>(pluginName, GetPluginDescriptionSymbol);
        this->m_setImGuiContextFunction         = getPluginFunction<SetImGuiContextFunc>(pluginName, SetImGuiContextSymbol);
    }

    Plugin::Plugin(Plugin &&other) noexcept {
        this->m_handle = other.m_handle;
        this->m_initializePluginFunction        = other.m_initializePluginFunction;
        this->m_getPluginNameFunction           = other.m_getPluginNameFunction;
        this->m_getPluginAuthorFunction         = other.m_getPluginAuthorFunction;
        this->m_getPluginDescriptionFunction    = other.m_getPluginDescriptionFunction;
        this->m_setImGuiContextFunction         = other.m_setImGuiContextFunction;

        other.m_handle = nullptr;
        other.m_initializePluginFunction        = nullptr;
        other.m_getPluginNameFunction           = nullptr;
        other.m_getPluginAuthorFunction         = nullptr;
        other.m_getPluginDescriptionFunction    = nullptr;
        other.m_setImGuiContextFunction         = nullptr;
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

    void Plugin::setImGuiContext(ImGuiContext *ctx) const {
        if (this->m_setImGuiContextFunction != nullptr)
            this->m_setImGuiContextFunction(ctx);
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
