#pragma once

#include <functional>
#include <string>
#include <vector>

namespace hex::plugin::loader {

    struct Plugin {
        std::string name;
        std::function<void()> entryPoint;
    };

    class PluginLoader {
    public:
        PluginLoader() = default;
        virtual ~PluginLoader() = default;

        virtual bool loadAll() = 0;

        void addPlugin(std::string name, std::function<void()> entryPoint) {
            m_plugins.emplace_back(std::move(name), std::move(entryPoint));
        }

        const auto& getPlugins() const {
            return m_plugins;
        }

    protected:
        void clearPlugins() {
            m_plugins.clear();
        }

    private:
        std::vector<Plugin> m_plugins;
    };

}