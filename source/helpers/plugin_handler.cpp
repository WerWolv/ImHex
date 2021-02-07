#include "helpers/plugin_handler.hpp"

#include <dlfcn.h>

#include <filesystem>

namespace hex {

    namespace fs = std::filesystem;

    // hex::plugin::internal::initializePlugin()
    constexpr auto InitializePluginSymbol   = "_ZN3hex6plugin%d%s8internal16initializePluginEv";

    Plugin::Plugin(std::string_view path) {
        auto fileName = fs::path(path).stem();
        auto symbolName = hex::format(InitializePluginSymbol, fileName.string().length(), fileName.string().c_str());
        this->m_handle = dlopen(path.data(), RTLD_LAZY);
        if (this->m_handle == nullptr)
            return;

        printf("Loaded plugin %s\n", path.data());


        this->m_initializePluginFunction = reinterpret_cast<InitializePluginFunc>(dlsym(this->m_handle, symbolName.c_str()));
        printf("Symbol %s at %p\n", symbolName.c_str(), this->m_initializePluginFunction);

    }

    Plugin::~Plugin() {
        printf("Plugin unloaded\n");
        dlclose(this->m_handle);
    }

    void Plugin::initializePlugin() const {
        if (this->m_initializePluginFunction != nullptr) {
            printf("Initializing plugin\n");
            this->m_initializePluginFunction();
            printf("Initialized plugin\n");
        }
    }

    void PluginHandler::load(std::string_view pluginFolder) {
        if (!std::filesystem::exists(pluginFolder))
            throw std::runtime_error("Failed to find plugin folder");

        PluginHandler::s_pluginFolder = pluginFolder;

        for (auto& pluginPath : std::filesystem::directory_iterator(pluginFolder)) {
            if (pluginPath.is_regular_file() && pluginPath.path().extension() == ".hexplug")
                PluginHandler::s_plugins.push_back(new Plugin(pluginPath.path().string()));
        }
    }

    void PluginHandler::unload() {
        for (auto &plugin : PluginHandler::s_plugins)
            delete plugin;

        PluginHandler::s_plugins.clear();
        PluginHandler::s_pluginFolder.clear();
    }

    void PluginHandler::reload() {
        PluginHandler::unload();
        PluginHandler::load(PluginHandler::s_pluginFolder);
    }

}
