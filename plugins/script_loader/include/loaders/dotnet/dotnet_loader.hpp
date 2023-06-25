#pragma once

#include <loaders/loader.hpp>

#include <wolv/io/fs.hpp>

#include <functional>

namespace hex::plugin::loader {

    class DotNetLoader : public PluginLoader {
    public:
        DotNetLoader();
        ~DotNetLoader() override = default;

        bool loadAll() override;

    private:
        std::function<bool(const std::fs::path&)> m_loadAssembly;
    };

}