#include "helpers/plugin_handler.hpp"

#include <dlfcn.h>
#include <filesystem>

namespace hex {

    constexpr auto CreateViewSymbol = "_ZN3hex6plugin10createViewEv"; // hex::plugin::createView(void)

    Plugin::Plugin(std::string_view path) {
        this->m_handle = dlopen(path.data(), RTLD_LAZY);

        if (this->m_handle == nullptr)
            return;

        this->m_createViewFunction = reinterpret_cast<CreateViewFunc>(dlsym(this->m_handle, CreateViewSymbol));
    }

    Plugin::~Plugin() {
        if (this->m_handle != nullptr)
            dlclose(this->m_handle);
    }

    View* Plugin::createView() const {
        if (this->m_createViewFunction != nullptr)
            return this->m_createViewFunction();

        return nullptr;
    }


    void PluginHandler::load(std::string_view pluginFolder) {
        PluginHandler::unload();

        if (!std::filesystem::exists(pluginFolder))
            throw std::runtime_error("Failed to find plugin folder");

        PluginHandler::s_pluginFolder = pluginFolder;

        for (auto& pluginPath : std::filesystem::directory_iterator(pluginFolder))
            PluginHandler::s_plugins.emplace_back(pluginPath.path().string());
    }

    void PluginHandler::unload() {
        PluginHandler::s_plugins.clear();
        PluginHandler::s_pluginFolder.clear();
    }

    void PluginHandler::reload() {
        PluginHandler::load(PluginHandler::s_pluginFolder);
    }

}
