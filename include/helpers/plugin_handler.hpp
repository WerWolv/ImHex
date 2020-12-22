#pragma once

#include "views/view.hpp"

#include <string_view>

namespace hex {

    class Plugin {
    public:
        Plugin(std::string_view path);
        ~Plugin();

        void setImGuiContext(ImGuiContext *ctx) const;
        View* createView() const;
        void drawToolsEntry() const;

    private:
        using SetImGuiContextFunc = void(*)(ImGuiContext*);
        using CreateViewFunc = View*(*)();
        using DrawToolsEntryFunc = void(*)();

        void *m_handle = nullptr;

        SetImGuiContextFunc m_setImGuiContextFunction = nullptr;
        CreateViewFunc m_createViewFunction = nullptr;
        DrawToolsEntryFunc m_drawToolsEntryFunction = nullptr;

    };

    class PluginHandler {
    public:
        PluginHandler() = delete;

        static void load(std::string_view pluginFolder);
        static void unload();
        static void reload();

        static const auto& getPlugins() {
            return PluginHandler::s_plugins;
        }

    private:
        static inline std::string s_pluginFolder;
        static inline std::vector<Plugin> s_plugins;
    };

}