#include "helpers/plugin_handler.hpp"

#include <dlfcn.h>

#include <filesystem>

namespace hex {

    namespace fs = std::filesystem;

    // hex::plugin::<pluginName>::internal::initializePlugin()
    constexpr auto InitializePluginSymbol   = "_ZN3hex6plugin%d%s8internal16initializePluginEv";

    Plugin::Plugin(std::string_view path) {
        auto fileName = fs::path(path).stem();
        auto symbolName = hex::format(InitializePluginSymbol, fileName.string().length(), fileName.string().c_str());
        this->m_handle = dlopen(path.data(), RTLD_LAZY);
        if (this->m_handle == nullptr)
            return;

        this->m_initializePluginFunction = reinterpret_cast<InitializePluginFunc>(dlsym(this->m_handle, symbolName.c_str()));
    }

    Plugin::Plugin(Plugin &&other) {
        this->m_handle = other.m_handle;
        this->m_initializePluginFunction = other.m_initializePluginFunction;

        other.m_handle = nullptr;
        other.m_initializePluginFunction = nullptr;
    }

    Plugin::~Plugin() {
        dlclose(this->m_handle);
    }

    void Plugin::initializePlugin() const {
        if (this->m_initializePluginFunction != nullptr) {
            this->m_initializePluginFunction();
        }
    }

    void PluginHandler::load(std::string_view pluginFolder) {
        if (!std::filesystem::exists(pluginFolder))
            throw std::runtime_error("Failed to find plugin folder");

        PluginHandler::s_pluginFolder = pluginFolder;

        for (auto& pluginPath : std::filesystem::directory_iterator(pluginFolder)) {
            if (pluginPath.is_regular_file() && pluginPath.path().extension() == ".hexplug")
                PluginHandler::s_plugins.emplace_back(pluginPath.path().string());
        }
    }

    void PluginHandler::unload() {
        PluginHandler::s_plugins.clear();
        PluginHandler::s_pluginFolder.clear();
    }

    void PluginHandler::reload() {
        PluginHandler::unload();
        PluginHandler::load(PluginHandler::s_pluginFolder);
    }

}
