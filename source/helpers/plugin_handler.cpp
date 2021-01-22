#include "helpers/plugin_handler.hpp"

#include <dlfcn.h>
#include <filesystem>

namespace hex {

    // hex::plugin::internal::initializePlugin()
    constexpr auto InitializePluginSymbol   = "_ZN3hex6plugin8internal16initializePluginEv";

    Plugin::Plugin(std::string_view path) {
        this->m_handle = dlopen(path.data(), RTLD_LAZY);

        if (this->m_handle == nullptr)
            return;

        this->m_initializePluginFunction = reinterpret_cast<InitializePluginFunc>(dlsym(this->m_handle, InitializePluginSymbol));
    }

    Plugin::~Plugin() {

    }

    void Plugin::initializePlugin() const {
        if (this->m_initializePluginFunction != nullptr)
            this->m_initializePluginFunction();
    }

    void PluginHandler::load(std::string_view pluginFolder) {
        PluginHandler::unload();

        if (!std::filesystem::exists(pluginFolder))
            throw std::runtime_error("Failed to find plugin folder");

        PluginHandler::s_pluginFolder = pluginFolder;

        for (auto& pluginPath : std::filesystem::directory_iterator(pluginFolder)) {
            if (pluginPath.is_regular_file())
                PluginHandler::s_plugins.emplace_back(pluginPath.path().string());
        }
    }

    void PluginHandler::unload() {
        PluginHandler::s_plugins.clear();
        PluginHandler::s_pluginFolder.clear();
    }

    void PluginHandler::reload() {
        PluginHandler::load(PluginHandler::s_pluginFolder);
    }

}
