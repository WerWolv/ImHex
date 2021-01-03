#include "helpers/plugin_handler.hpp"

#include <dlfcn.h>
#include <filesystem>

namespace hex {

    // hex::plugin::createView(void)
    constexpr auto CreateViewSymbol         = "_ZN3hex6plugin10createViewEv";
    // hex::plugin::drawToolsEntry(void)
    constexpr auto DrawToolsEntrySymbol     = "_ZN3hex6plugin14drawToolsEntryEv";
    // hex::plugin::internal::initializePlugin(SharedData&)
    constexpr auto InitializePluginSymbol   = "_ZN3hex6plugin8internal16initializePluginER10SharedData";

    Plugin::Plugin(std::string_view path) {
        this->m_handle = dlopen(path.data(), RTLD_LAZY);

        if (this->m_handle == nullptr)
            return;

        this->m_createViewFunction = reinterpret_cast<CreateViewFunc>(dlsym(this->m_handle, CreateViewSymbol));
        this->m_drawToolsEntryFunction = reinterpret_cast<DrawToolsEntryFunc>(dlsym(this->m_handle, DrawToolsEntrySymbol));
        this->m_initializePluginFunction = reinterpret_cast<InitializePluginFunc>(dlsym(this->m_handle, InitializePluginSymbol));
    }

    Plugin::~Plugin() {
        if (this->m_handle != nullptr)
            dlclose(this->m_handle);
    }

    void Plugin::initializePlugin(SharedData &sharedData) const {
        if (this->m_initializePluginFunction != nullptr)
            this->m_initializePluginFunction(sharedData);
    }

    View* Plugin::createView() const {
        if (this->m_createViewFunction != nullptr)
            return this->m_createViewFunction();

        return nullptr;
    }

    void Plugin::drawToolsEntry() const {
        if (this->m_drawToolsEntryFunction != nullptr)
            this->m_drawToolsEntryFunction();
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
