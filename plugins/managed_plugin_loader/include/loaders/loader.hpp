#pragma once

#include <functional>
#include <vector>

namespace hex::plugin::loader {

    class PluginLoader {
    public:
        PluginLoader() = default;
        virtual ~PluginLoader() = default;

        virtual bool loadAll() = 0;

        void addEntryPoint(std::function<void()> entryPoint) {
            m_entryPoints.push_back(entryPoint);
        }

        const auto& getPluginEntryPoints() const {
            return m_entryPoints;
        }

    private:
        std::vector<std::function<void()>> m_entryPoints;
    };

}